"""IDA headless report for a network-oriented PE database.

Run with:
    idat.exe -A -Sida_network_inventory.py target.i64

The report is printed between TDX_IDA_REPORT markers so the caller can capture
stdout without creating a second evidence file.
"""

from __future__ import annotations

import json
import re

import ida_auto
import ida_bytes
import ida_entry
import ida_funcs
import ida_hexrays
import ida_ida
import ida_name
import ida_nalt
import ida_pro
import idautils


NETWORK_IMPORTS = {
    "connect",
    "getaddrinfo",
    "freeaddrinfo",
    "recv",
    "send",
    "WSAConnect",
    "WSAIoctl",
    "WSARecv",
    "WSASend",
    "WSASocketA",
    "WSASocketW",
    "CreateIoCompletionPort",
    "GetQueuedCompletionStatus",
    "PostQueuedCompletionStatus",
}
TRACKED_IMPORT_FRAGMENTS = (
    "MakeUserCommModule",
    "DelUserCommModule",
)
STRING_PATTERN = re.compile(
    r"(?:https?://|(?:\d{1,3}\.){3}\d{1,3}|"
    r"\b(?:connect|disconnect|heartbeat|login|proxy|socket|timeout)\b)",
    re.IGNORECASE,
)


def function_at(ea: int) -> dict[str, int | str] | None:
    function = ida_funcs.get_func(ea)
    if function is None:
        return None
    return {
        "address": function.start_ea,
        "name": ida_funcs.get_func_name(function.start_ea),
    }


def xref_functions(ea: int) -> list[dict[str, int | str]]:
    found: dict[int, dict[str, int | str]] = {}
    for reference in idautils.XrefsTo(ea):
        function = function_at(reference.frm)
        if function is None:
            continue
        found[int(function["address"])] = function
    return [found[address] for address in sorted(found)]


def imports_report() -> tuple[list[dict[str, object]], dict[str, int]]:
    modules: list[dict[str, object]] = []
    network_addresses: dict[str, int] = {}
    for index in range(ida_nalt.get_import_module_qty()):
        entries: list[dict[str, object]] = []

        def callback(ea: int, name: str | None, ordinal: int) -> bool:
            display = name or f"ordinal_{ordinal}"
            entry = {"address": ea, "name": display, "ordinal": ordinal}
            entries.append(entry)
            if display in NETWORK_IMPORTS or any(
                fragment in display for fragment in TRACKED_IMPORT_FRAGMENTS
            ):
                network_addresses[display] = ea
            return True

        ida_nalt.enum_import_names(index, callback)
        modules.append(
            {
                "name": ida_nalt.get_import_module_name(index),
                "entries": entries,
            }
        )
    return modules, network_addresses


def exports_report() -> list[dict[str, object]]:
    exports: list[dict[str, object]] = []
    for index in range(ida_entry.get_entry_qty()):
        ordinal = ida_entry.get_entry_ordinal(index)
        ea = ida_entry.get_entry(ordinal)
        exports.append(
            {
                "ordinal": ordinal,
                "address": ea,
                "name": ida_entry.get_entry_name(ordinal) or "",
                "xref_functions": xref_functions(ea),
            }
        )
    return exports


def decompile_exports(exports: list[dict[str, object]]) -> list[dict[str, object]]:
    results: list[dict[str, object]] = []
    if not ida_hexrays.init_hexrays_plugin():
        return results
    for item in exports:
        name = str(item["name"])
        if "UserComm" not in name:
            continue
        ea = int(item["address"])
        try:
            pseudocode = str(ida_hexrays.decompile(ea))
        except Exception as error:
            pseudocode = f"<decompile failed: {error}>"
        results.append({"name": name, "address": ea, "pseudocode": pseudocode})
    return results


def decompile_xref_functions(addresses: dict[str, int]) -> list[dict[str, object]]:
    results: list[dict[str, object]] = []
    seen: set[int] = set()
    if not ida_hexrays.init_hexrays_plugin():
        return results
    for import_name, ea in addresses.items():
        if not any(fragment in import_name for fragment in TRACKED_IMPORT_FRAGMENTS):
            continue
        for function in xref_functions(ea):
            function_ea = int(function["address"])
            if function_ea in seen:
                continue
            seen.add(function_ea)
            results.append(
                {
                    "import": import_name,
                    "address": function_ea,
                    "name": function["name"],
                    "pseudocode": decompile_one(function_ea),
                }
            )
    return results


def interesting_names() -> list[dict[str, int | str]]:
    keywords = ("UserComm", "asio", "socket", "completion", "session")
    results: list[dict[str, int | str]] = []
    for ea, name in idautils.Names():
        if any(keyword.casefold() in name.casefold() for keyword in keywords):
            results.append({"address": ea, "name": name})
    return results[:300]


def usercomm_vtables() -> list[dict[str, object]]:
    results: list[dict[str, object]] = []
    can_decompile = ida_hexrays.init_hexrays_plugin()
    for ea, name in idautils.Names():
        if not name.startswith("??_7") or "UserComm@@6B@" not in name:
            continue
        entries: list[dict[str, object]] = []
        misses = 0
        for slot in range(64):
            target = ida_bytes.get_dword(ea + slot * 4)
            function = function_at(target)
            target_name = ida_name.get_name(target) or ""
            if function is None and not target_name:
                misses += 1
                if misses >= 2:
                    break
            else:
                misses = 0
            entries.append(
                {
                    "slot": slot,
                    "address": target,
                    "name": (
                        str(function["name"])
                        if function is not None
                        else target_name
                    ),
                    "pseudocode": (
                        decompile_one(target)
                        if can_decompile and function is not None and slot < 15
                        else ""
                    ),
                }
            )
        results.append({"address": ea, "name": name, "entries": entries})
    return results


def decompile_one(ea: int) -> str:
    try:
        return str(ida_hexrays.decompile(ea))
    except Exception as error:
        return f"<decompile failed: {error}>"


def interesting_strings() -> list[dict[str, object]]:
    results: list[dict[str, object]] = []
    strings = idautils.Strings()
    strings.setup(strtypes=[0, 1], minlen=5)
    for item in strings:
        text = str(item)
        if not STRING_PATTERN.search(text):
            continue
        results.append(
            {
                "address": item.ea,
                "text": text[:300],
                "xref_functions": xref_functions(item.ea),
            }
        )
        if len(results) >= 200:
            break
    return results


def main() -> None:
    ida_auto.auto_wait()
    imports, network_addresses = imports_report()
    exports = exports_report()
    report = {
        "imagebase": ida_nalt.get_imagebase(),
        "min_ea": ida_ida.inf_get_min_ea(),
        "max_ea": ida_ida.inf_get_max_ea(),
        "function_count": sum(1 for _ in idautils.Functions()),
        "imports": imports,
        "exports": exports,
        "network_xrefs": {
            name: {"address": ea, "xref_functions": xref_functions(ea)}
            for name, ea in sorted(network_addresses.items())
        },
        "usercomm_vtables": usercomm_vtables(),
        "tracked_import_callers": decompile_xref_functions(network_addresses),
        "interesting_names": interesting_names(),
        "interesting_strings": interesting_strings(),
        "export_pseudocode": decompile_exports(exports),
    }
    print("TDX_IDA_REPORT_BEGIN")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    print("TDX_IDA_REPORT_END")
    ida_pro.qexit(0)


main()

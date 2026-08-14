"""Generic IDA headless inventory for a completed PE database.

The script prints a JSON report between TDX_IDA_MODULE_REPORT markers.  It is
read-only apart from IDA's normal database bookkeeping.
"""

from __future__ import annotations

import json
import re

import ida_auto
import ida_entry
import ida_funcs
import ida_hexrays
import ida_ida
import ida_name
import ida_nalt
import ida_pro
import idautils


STRING_PATTERN = re.compile(
    r"(?:https?://|tpbus|taapi|tdataparse|tbigdata|"
    r"\b(?:callback|channel|compress|currentgp|data|decode|encode|event|"
    r"frame|heartbeat|image|inflate|json|login|market|message|msg|packet|"
    r"parse|price|quote|request|response|scheme|server|stock|subscribe|"
    r"topic|volume|zlib)\b)",
    re.IGNORECASE,
)
MAX_CALL_NODES = 120


def decompile(ea: int) -> str:
    try:
        return str(ida_hexrays.decompile(ea))
    except Exception as error:
        return f"<decompile failed: {error}>"


def direct_callees(ea: int) -> list[int]:
    function = ida_funcs.get_func(ea)
    if function is None:
        return []
    minimum = ida_ida.inf_get_min_ea()
    maximum = ida_ida.inf_get_max_ea()
    results: set[int] = set()
    for item_ea in idautils.FuncItems(function.start_ea):
        for target in idautils.CodeRefsFrom(item_ea, False):
            target_function = ida_funcs.get_func(target)
            if (
                target_function is not None
                and minimum <= target_function.start_ea < maximum
                and target_function.start_ea != function.start_ea
            ):
                results.add(target_function.start_ea)
    return sorted(results)


def call_tree(root: int, max_depth: int = 2) -> list[dict[str, object]]:
    queue: list[tuple[int, int]] = [(root, 0)]
    seen: set[int] = set()
    nodes: list[dict[str, object]] = []
    while queue and len(nodes) < MAX_CALL_NODES:
        ea, depth = queue.pop(0)
        function = ida_funcs.get_func(ea)
        if function is None:
            continue
        ea = function.start_ea
        if ea in seen:
            continue
        seen.add(ea)
        callees = direct_callees(ea)
        nodes.append(
            {
                "address": ea,
                "name": ida_funcs.get_func_name(ea),
                "depth": depth,
                "callees": callees,
                "pseudocode": decompile(ea),
            }
        )
        if depth < max_depth:
            queue.extend((target, depth + 1) for target in callees)
    return nodes


def imports_report() -> list[dict[str, object]]:
    modules: list[dict[str, object]] = []
    for index in range(ida_nalt.get_import_module_qty()):
        entries: list[dict[str, object]] = []

        def callback(ea: int, name: str | None, ordinal: int) -> bool:
            entries.append(
                {
                    "address": ea,
                    "name": name or f"ordinal_{ordinal}",
                    "ordinal": ordinal,
                }
            )
            return True

        ida_nalt.enum_import_names(index, callback)
        modules.append(
            {
                "name": ida_nalt.get_import_module_name(index),
                "entries": entries,
            }
        )
    return modules


def exports_report() -> list[dict[str, object]]:
    exports: list[dict[str, object]] = []
    for index in range(ida_entry.get_entry_qty()):
        ordinal = ida_entry.get_entry_ordinal(index)
        ea = ida_entry.get_entry(ordinal)
        name = ida_entry.get_entry_name(ordinal) or ""
        function = ida_funcs.get_func(ea)
        if not name or function is None:
            continue
        exports.append(
            {
                "ordinal": ordinal,
                "address": function.start_ea,
                "name": name,
                "call_tree": call_tree(function.start_ea),
            }
        )
    return exports


def string_xref_functions(ea: int) -> list[dict[str, object]]:
    results: dict[int, dict[str, object]] = {}
    for reference in idautils.XrefsTo(ea):
        function = ida_funcs.get_func(reference.frm)
        if function is None:
            continue
        results[function.start_ea] = {
            "address": function.start_ea,
            "name": ida_funcs.get_func_name(function.start_ea),
        }
    return [results[address] for address in sorted(results)]


def strings_report() -> list[dict[str, object]]:
    results: list[dict[str, object]] = []
    strings = idautils.Strings()
    strings.setup(strtypes=[0, 1], minlen=4)
    for item in strings:
        text = str(item)
        if not STRING_PATTERN.search(text):
            continue
        results.append(
            {
                "address": item.ea,
                "text": text[:500],
                "xref_functions": string_xref_functions(item.ea),
            }
        )
        if len(results) >= 500:
            break
    return results


def named_symbols() -> list[dict[str, object]]:
    results: list[dict[str, object]] = []
    for ea, name in idautils.Names():
        if (
            name.startswith("sub_")
            or name.startswith("loc_")
            or name.startswith("unk_")
            or name.startswith("byte_")
            or name.startswith("word_")
            or name.startswith("dword_")
            or name.startswith("off_")
            or name.startswith("asc_")
            or name.startswith("a")
        ):
            continue
        results.append({"address": ea, "name": name})
        if len(results) >= 1000:
            break
    return results


def main() -> None:
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    report = {
        "imagebase": ida_nalt.get_imagebase(),
        "min_ea": ida_ida.inf_get_min_ea(),
        "max_ea": ida_ida.inf_get_max_ea(),
        "function_count": sum(1 for _ in idautils.Functions()),
        "imports": imports_report(),
        "exports": exports_report(),
        "strings": strings_report(),
        "named_symbols": named_symbols(),
    }
    print("TDX_IDA_MODULE_REPORT_BEGIN")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    print("TDX_IDA_MODULE_REPORT_END")
    ida_pro.qexit(0)


main()

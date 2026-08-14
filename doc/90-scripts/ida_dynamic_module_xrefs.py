"""Locate dynamic DLL/export references in an IDA database.

Run this against the main executable (or another module) after auto-analysis.
The report is printed as JSON between TDX_IDA_DYNAMIC_XREFS markers.
"""

from __future__ import annotations

import json
import re

import ida_auto
import ida_funcs
import ida_hexrays
import ida_ida
import ida_nalt
import ida_pro
import idautils


TARGET_PATTERN = re.compile(
    r"(?:"
    r"(?:tdx)?asiocomm\.dll|"
    r"tdataparse\.dll|tbigdata\.dll|tencrypt\.dll|tpbus\.dll|taapi\.dll|"
    r"tpool\.dll|tpool_[a-z0-9_]+|"
    r"addinminiquote(?:ex)?\.dll|addin_getobject|miniquote_getversion|"
    r"tjyaid\.dll|"
    r"fn_sync_getdata|fn_tgetimagedata|"
    r"bigdata(?:unit)?_[a-z0-9_]+|"
    r"t_(?:downloadfile(?:clean|init|proxyinit)?|encode|encrypt|"
    r"posturlverify|rsaencode2?)|"
    r"tp_(?:createappcore|createtpdata|destroyappcore|destroytpdata|"
    r"exit|getfile|init)|"
    r"taapi_(?:createappcore|createinstanceex?|destroyappcore)"
    r")",
    re.IGNORECASE,
)
MAX_FUNCTIONS = 160


def decompile(ea: int) -> str:
    try:
        return str(ida_hexrays.decompile(ea))
    except Exception as error:
        return f"<decompile failed: {error}>"


def containing_functions(string_ea: int) -> list[int]:
    results: set[int] = set()
    for reference in idautils.XrefsTo(string_ea):
        function = ida_funcs.get_func(reference.frm)
        if function is not None:
            results.add(function.start_ea)
    return sorted(results)


def code_callers(function_ea: int) -> list[dict[str, object]]:
    callers: dict[int, dict[str, object]] = {}
    for reference in idautils.CodeRefsTo(function_ea, False):
        function = ida_funcs.get_func(reference)
        if function is None:
            continue
        callers[function.start_ea] = {
            "address": function.start_ea,
            "name": ida_funcs.get_func_name(function.start_ea),
        }
    return [callers[address] for address in sorted(callers)]


def imports_report() -> list[dict[str, object]]:
    modules: list[dict[str, object]] = []
    for index in range(ida_nalt.get_import_module_qty()):
        entries: list[dict[str, object]] = []

        def callback(ea: int, name: str | None, ordinal: int) -> bool:
            text = name or f"ordinal_{ordinal}"
            if TARGET_PATTERN.search(text):
                entries.append(
                    {"address": ea, "name": text, "ordinal": ordinal}
                )
            return True

        ida_nalt.enum_import_names(index, callback)
        module_name = ida_nalt.get_import_module_name(index) or ""
        if entries or TARGET_PATTERN.search(module_name):
            modules.append({"name": module_name, "entries": entries})
    return modules


def main() -> None:
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()

    hits: list[dict[str, object]] = []
    functions: dict[int, dict[str, object]] = {}
    strings = idautils.Strings()
    strings.setup(strtypes=[0, 1], minlen=4)
    for item in strings:
        text = str(item)
        if not TARGET_PATTERN.search(text):
            continue
        owners = containing_functions(item.ea)
        hits.append(
            {
                "address": item.ea,
                "text": text[:500],
                "functions": owners,
            }
        )
        for function_ea in owners:
            if function_ea in functions or len(functions) >= MAX_FUNCTIONS:
                continue
            functions[function_ea] = {
                "address": function_ea,
                "name": ida_funcs.get_func_name(function_ea),
                "callers": code_callers(function_ea),
                "pseudocode": decompile(function_ea),
            }

    report = {
        "imagebase": ida_nalt.get_imagebase(),
        "min_ea": ida_ida.inf_get_min_ea(),
        "max_ea": ida_ida.inf_get_max_ea(),
        "imports": imports_report(),
        "string_hits": hits,
        "xref_functions": [
            functions[address] for address in sorted(functions)
        ],
    }
    print("TDX_IDA_DYNAMIC_XREFS_BEGIN")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    print("TDX_IDA_DYNAMIC_XREFS_END")
    ida_pro.qexit(0)


main()

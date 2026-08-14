"""Search selected strings and report their IDA cross-reference functions.

Set ``TDX_IDA_STRING_PATTERN`` to a regular expression before launching IDA.
The script is read-only apart from IDA's normal database bookkeeping.
"""

from __future__ import annotations

import json
import os
import re
from pathlib import Path

import ida_auto
import ida_funcs
import ida_hexrays
import ida_nalt
import ida_pro
import idautils
import idc


DEFAULT_PATTERN = (
    r"reqformat|pb_rpc_req|reqbyte|pbrpc|cloud_cfg|datasource|"
    r"hqserv|tqlex|json2grid|xml2grid"
)
MAX_STRINGS = 200
MAX_FUNCTIONS = 80


def decompile(function_ea: int) -> str:
    try:
        return str(ida_hexrays.decompile(function_ea))
    except Exception as error:
        return f"<decompile failed: {error}>"


def main() -> None:
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    pattern = re.compile(
        os.environ.get("TDX_IDA_STRING_PATTERN", DEFAULT_PATTERN),
        re.IGNORECASE,
    )

    matches: list[dict[str, object]] = []
    functions: dict[int, dict[str, object]] = {}
    strings = idautils.Strings()
    strings.setup(strtypes=[0, 1], minlen=4)
    for item in strings:
        value = str(item)
        if not pattern.search(value):
            continue
        xrefs: list[dict[str, object]] = []
        for reference in idautils.XrefsTo(item.ea):
            function = ida_funcs.get_func(reference.frm)
            if function is None:
                continue
            function_ea = function.start_ea
            xrefs.append(
                {
                    "from": reference.frm,
                    "function": function_ea,
                    "name": ida_funcs.get_func_name(function_ea),
                }
            )
            if function_ea not in functions and len(functions) < MAX_FUNCTIONS:
                functions[function_ea] = {
                    "address": function_ea,
                    "name": ida_funcs.get_func_name(function_ea),
                    "pseudocode": decompile(function_ea),
                }
        matches.append(
            {
                "address": item.ea,
                "text": value[:1000],
                "xrefs": xrefs,
            }
        )
        if len(matches) >= MAX_STRINGS:
            break

    report = {
        "input_file": ida_nalt.get_input_file_path(),
        "pattern": pattern.pattern,
        "matches": matches,
        "functions": list(functions.values()),
    }
    output_path = next(
        (
            Path(value.partition("=")[2])
            for value in idc.ARGV[1:]
            if value.startswith("--output=")
        ),
        None,
    )
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(rendered, encoding="utf-8")
    print("TDX_IDA_STRING_XREFS_BEGIN")
    if output_path is None:
        print(rendered)
    else:
        print(
            json.dumps(
                {
                    "output": str(output_path),
                    "match_count": len(matches),
                    "function_count": len(functions),
                },
                ensure_ascii=False,
            )
        )
    print("TDX_IDA_STRING_XREFS_END")
    ida_pro.qexit(0)


main()

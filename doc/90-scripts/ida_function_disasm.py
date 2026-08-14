"""Dump IDA disassembly for selected functions.

Pass one or more function addresses after the script name::

    idat.exe -A "-Sida_function_disasm.py 0x69EFF0" target.i64

Use ``--output=PATH`` to write the JSON report without printing the full
disassembly to IDA's console.
"""

from __future__ import annotations

import json
from pathlib import Path

import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_pro
import idautils
import idc


def parse_arguments() -> tuple[list[int], Path | None]:
    addresses: list[int] = []
    output_path: Path | None = None
    for value in idc.ARGV[1:]:
        if value.startswith("--output="):
            output_path = Path(value.partition("=")[2])
            continue
        try:
            addresses.append(int(value, 0))
        except ValueError:
            print(f"Skipping invalid address: {value}")
    return addresses, output_path


def dump_function(address: int) -> dict[str, object]:
    function = ida_funcs.get_func(address)
    if function is None:
        return {"address": address, "error": "not a function"}
    instructions = []
    for ea in idautils.FuncItems(function.start_ea):
        rendered = idc.generate_disasm_line(ea, 0) or ""
        item_size = idc.get_item_size(ea)
        raw = ida_bytes.get_bytes(ea, item_size) or b""
        instructions.append(
            {
                "address": ea,
                "bytes": raw.hex(" "),
                "text": ida_lines.tag_remove(rendered),
            }
        )
    return {
        "address": function.start_ea,
        "end_address": function.end_ea,
        "name": ida_funcs.get_func_name(function.start_ea),
        "instructions": instructions,
    }


def main() -> None:
    ida_auto.auto_wait()
    addresses, output_path = parse_arguments()
    report = [dump_function(address) for address in addresses]
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(rendered, encoding="utf-8")
    print("TDX_IDA_FUNCTION_DISASM_BEGIN")
    if output_path is None:
        print(rendered)
    else:
        print(json.dumps({"output": str(output_path), "function_count": len(report)}))
    print("TDX_IDA_FUNCTION_DISASM_END")
    ida_pro.qexit(0)


main()

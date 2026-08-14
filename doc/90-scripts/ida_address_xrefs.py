"""Decompile functions that reference selected IDA addresses.

Pass one or more hexadecimal/decimal addresses after the script name:

    idat.exe -A "-Sida_address_xrefs.py 0x1041568C" target.i64
"""

from __future__ import annotations

import json
from pathlib import Path

import ida_auto
import ida_funcs
import ida_hexrays
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


def decompile(ea: int) -> str:
    try:
        return str(ida_hexrays.decompile(ea))
    except Exception as error:
        return f"<decompile failed: {error}>"


def referring_functions(target: int) -> list[dict[str, object]]:
    functions: dict[int, dict[str, object]] = {}
    references = list(idautils.CodeRefsTo(target, False))
    references.extend(idautils.DataRefsTo(target))
    references.extend(reference.frm for reference in idautils.XrefsTo(target))
    for reference in references:
        function = ida_funcs.get_func(reference)
        if function is None:
            continue
        functions[function.start_ea] = {
            "address": function.start_ea,
            "name": ida_funcs.get_func_name(function.start_ea),
            "pseudocode": decompile(function.start_ea),
        }
    return [functions[address] for address in sorted(functions)]


def main() -> None:
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    addresses, output_path = parse_arguments()
    report = []
    for address in addresses:
        function = ida_funcs.get_func(address)
        report.append(
            {
                "target": address,
                "target_function": (
                    {
                        "address": function.start_ea,
                        "name": ida_funcs.get_func_name(function.start_ea),
                        "pseudocode": decompile(function.start_ea),
                    }
                    if function is not None
                    else None
                ),
                "functions": referring_functions(address),
            }
        )
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(rendered, encoding="utf-8")
    print("TDX_IDA_ADDRESS_XREFS_BEGIN")
    if output_path is None:
        print(rendered)
    else:
        print(
            json.dumps(
                {
                    "output": str(output_path),
                    "target_count": len(report),
                },
                ensure_ascii=False,
            )
        )
    print("TDX_IDA_ADDRESS_XREFS_END")
    ida_pro.qexit(0)


main()

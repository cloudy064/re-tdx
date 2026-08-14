"""Find IDA instructions that use selected immediate/displacement values.

Example:

    idat.exe -A \
      "-Sida_immediate_xrefs.py --output=C:\\tmp\\refs.json 4045 9239" \
      TdxW.exe.i64
"""

from __future__ import annotations

import json
from pathlib import Path

import ida_auto
import ida_funcs
import ida_hexrays
import ida_pro
import ida_ua
import idautils
import idc


MAX_REFERENCES_PER_VALUE = 500


def parse_arguments() -> tuple[list[int], Path | None, bool]:
    values: list[int] = []
    output_path: Path | None = None
    should_decompile = True
    for argument in idc.ARGV[1:]:
        if argument.startswith("--output="):
            output_path = Path(argument.partition("=")[2])
            continue
        if argument == "--no-decompile":
            should_decompile = False
            continue
        try:
            values.append(int(argument, 0))
        except ValueError:
            print(f"Skipping invalid value: {argument}")
    return values, output_path, should_decompile


def decompile(ea: int) -> str:
    try:
        return str(ida_hexrays.decompile(ea))
    except Exception as error:
        return f"<decompile failed: {error}>"


def main() -> None:
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    values, output_path, should_decompile = parse_arguments()
    wanted = set(values)
    references: dict[int, list[dict[str, object]]] = {
        value: [] for value in values
    }

    for function_ea in idautils.Functions():
        function = ida_funcs.get_func(function_ea)
        if function is None:
            continue
        for ea in idautils.FuncItems(function_ea):
            for operand_index in range(8):
                operand_type = idc.get_operand_type(ea, operand_index)
                if operand_type == idc.o_void:
                    break
                if operand_type not in (idc.o_imm, idc.o_displ):
                    continue
                value = idc.get_operand_value(ea, operand_index)
                if value not in wanted:
                    continue
                entries = references[value]
                if len(entries) >= MAX_REFERENCES_PER_VALUE:
                    continue
                entries.append(
                    {
                        "address": ea,
                        "operand_index": operand_index,
                        "operand_type": operand_type,
                        "instruction": idc.generate_disasm_line(ea, 0),
                        "function_address": function.start_ea,
                        "function_name": ida_funcs.get_func_name(
                            function.start_ea
                        ),
                    }
                )

    report: list[dict[str, object]] = []
    for value in values:
        functions: dict[int, dict[str, object]] = {}
        for reference in references[value]:
            function_ea = int(reference["function_address"])
            if function_ea not in functions:
                functions[function_ea] = {
                    "address": function_ea,
                    "name": reference["function_name"],
                    "pseudocode": (
                        decompile(function_ea) if should_decompile else None
                    ),
                }
        report.append(
            {
                "value": value,
                "references": references[value],
                "functions": [
                    functions[address] for address in sorted(functions)
                ],
            }
        )

    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(rendered, encoding="utf-8")
    print("TDX_IDA_IMMEDIATE_XREFS_BEGIN")
    if output_path is None:
        print(rendered)
    else:
        print(
            json.dumps(
                {
                    "output": str(output_path),
                    "value_count": len(report),
                    "reference_counts": {
                        str(item["value"]): len(item["references"])
                        for item in report
                    },
                },
                ensure_ascii=False,
            )
        )
    print("TDX_IDA_IMMEDIATE_XREFS_END")
    ida_pro.qexit(0)


main()

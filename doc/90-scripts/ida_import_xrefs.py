"""Export selected imports and their referring functions from an IDA database.

Set ``TDX_IDA_IMPORT_PATTERN`` to a case-insensitive regular expression.  This
is useful when strings contain import names but ordinary string xrefs do not
reach the IAT entries used by code.
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
    r"DrawText|TextOut|CreateFont|SelectObject|SetTextColor|SetBkMode|"
    r"GetTextExtent|AlphaBlend|GradientFill|TransparentBlt"
)


def decompile(address: int) -> str:
    try:
        return str(ida_hexrays.decompile(address))
    except Exception as error:
        return f"<decompile failed: {error}>"


def output_path() -> Path | None:
    return next(
        (
            Path(value.partition("=")[2])
            for value in idc.ARGV[1:]
            if value.startswith("--output=")
        ),
        None,
    )


def main() -> None:
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    pattern = re.compile(
        os.environ.get("TDX_IDA_IMPORT_PATTERN", DEFAULT_PATTERN), re.IGNORECASE
    )
    imports: list[dict[str, object]] = []
    functions: dict[int, dict[str, object]] = {}

    for module_index in range(ida_nalt.get_import_module_qty()):
        module_name = ida_nalt.get_import_module_name(module_index) or ""

        def visit(address: int, name: str | None, ordinal: int) -> bool:
            import_name = name or f"ordinal_{ordinal}"
            if not pattern.search(module_name) and not pattern.search(import_name):
                return True
            references: list[dict[str, object]] = []
            for xref in idautils.XrefsTo(address):
                function = ida_funcs.get_func(xref.frm)
                if function is None:
                    continue
                references.append(
                    {
                        "from": xref.frm,
                        "function": function.start_ea,
                        "function_name": ida_funcs.get_func_name(function.start_ea),
                    }
                )
                if function.start_ea not in functions:
                    functions[function.start_ea] = {
                        "address": function.start_ea,
                        "name": ida_funcs.get_func_name(function.start_ea),
                        "pseudocode": decompile(function.start_ea),
                    }
            imports.append(
                {
                    "module": module_name,
                    "name": import_name,
                    "ordinal": ordinal,
                    "address": address,
                    "xrefs": references,
                }
            )
            return True

        ida_nalt.enum_import_names(module_index, visit)

    report = {
        "input_file": ida_nalt.get_input_file_path(),
        "pattern": pattern.pattern,
        "imports": imports,
        "functions": [functions[key] for key in sorted(functions)],
    }
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    destination = output_path()
    if destination is None:
        print(rendered)
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(rendered, encoding="utf-8")
        print(
            json.dumps(
                {
                    "output": str(destination),
                    "import_count": len(imports),
                    "function_count": len(functions),
                }
            )
        )
    ida_pro.qexit(0)


main()

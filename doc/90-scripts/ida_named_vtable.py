"""Dump and decompile an IDA vtable selected by a name substring.

Usage:

    idat.exe -A "-Sida_named_vtable.py CFetchRapidJson --output=out.json" file.i64
"""

from __future__ import annotations

import json
from pathlib import Path

import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_ida
import ida_name
import ida_pro
import idautils
import idc


def decompile(ea: int) -> str:
    try:
        return str(ida_hexrays.decompile(ea))
    except Exception as error:
        return f"<decompile failed: {error}>"


def arguments() -> tuple[str, int, Path | None]:
    needle = ""
    count = 32
    output: Path | None = None
    for value in idc.ARGV[1:]:
        if value.startswith("--count="):
            count = int(value.partition("=")[2], 0)
        elif value.startswith("--output="):
            output = Path(value.partition("=")[2])
        elif not needle:
            needle = value
    if not needle:
        raise ValueError("provide a vtable name substring")
    return needle.casefold(), count, output


def display_name(ea: int, raw_name: str) -> str:
    demangled = ida_name.demangle_name(
        raw_name,
        ida_name.MNG_SHORT_FORM,
    )
    return demangled or raw_name or ida_name.get_name(ea)


def main() -> None:
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    needle, count, output = arguments()
    pointer_size = 8 if ida_ida.inf_is_64bit() else 4
    tables: list[dict[str, object]] = []
    for ea, raw_name in idautils.Names():
        shown = display_name(ea, raw_name)
        if needle not in raw_name.casefold() and needle not in shown.casefold():
            continue
        entries: list[dict[str, object]] = []
        for index in range(count):
            slot = ea + pointer_size * index
            target = (
                ida_bytes.get_qword(slot)
                if pointer_size == 8
                else ida_bytes.get_dword(slot)
            )
            function = ida_funcs.get_func(target)
            if function is None:
                if index:
                    break
                continue
            entries.append(
                {
                    "index": index,
                    "offset": index * pointer_size,
                    "address": function.start_ea,
                    "name": ida_funcs.get_func_name(function.start_ea),
                    "pseudocode": decompile(function.start_ea),
                }
            )
        if entries:
            tables.append(
                {
                    "address": ea,
                    "raw_name": raw_name,
                    "name": shown,
                    "entries": entries,
                }
            )
    report = {"needle": needle, "tables": tables}
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered, encoding="utf-8")
        print(json.dumps({"output": str(output), "tables": len(tables)}))
    else:
        print(rendered)
    ida_pro.qexit(0)


main()

"""Find raw 32-bit pointers to matching strings in an IDA database.

IDA does not always create data xrefs for packed registration tables. Set
``TDX_IDA_STRING_PATTERN`` to a regular expression and run, for example::

    idat.exe -A \
      "-Sida_string_pointer_scan.py --output=report.json" target.i64

The script is read-only apart from IDA's normal database bookkeeping.
"""

from __future__ import annotations

import json
import os
import re
import struct
from pathlib import Path

import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_nalt
import ida_pro
import ida_segment
import idautils
import idc


def decompile(function_ea: int) -> str:
    try:
        return str(ida_hexrays.decompile(function_ea))
    except Exception as error:
        return f"<decompile failed: {error}>"


def main() -> None:
    ida_auto.auto_wait()
    ida_hexrays.init_hexrays_plugin()
    pattern = re.compile(
        os.environ.get("TDX_IDA_STRING_PATTERN", r"DRAWGBK_DIV"),
        re.IGNORECASE,
    )
    output_path = next(
        (
            Path(argument.partition("=")[2])
            for argument in idc.ARGV[1:]
            if argument.startswith("--output=")
        ),
        None,
    )

    strings = []
    ida_strings = idautils.Strings()
    ida_strings.setup(strtypes=[0, 1], minlen=4)
    for item in ida_strings:
        text = str(item)
        if pattern.search(text):
            strings.append({"address": int(item.ea), "text": text[:1000]})

    needles: dict[bytes, int] = {
        struct.pack("<I", item["address"]): item["address"] for item in strings
    }
    raw_pointers: list[dict[str, object]] = []
    for segment_ea in idautils.Segments():
        segment = ida_segment.getseg(segment_ea)
        if segment is None:
            continue
        data = ida_bytes.get_bytes(segment.start_ea, segment.end_ea - segment.start_ea)
        if not data:
            continue
        for needle, string_ea in needles.items():
            offset = data.find(needle)
            while offset >= 0:
                pointer_ea = segment.start_ea + offset
                surrounding = ida_bytes.get_bytes(max(segment.start_ea, pointer_ea - 32), 68) or b""
                xrefs = []
                functions: dict[int, dict[str, object]] = {}
                for reference in idautils.XrefsTo(pointer_ea):
                    function = ida_funcs.get_func(reference.frm)
                    function_ea = function.start_ea if function is not None else None
                    xrefs.append(
                        {
                            "from": int(reference.frm),
                            "type": int(reference.type),
                            "function": function_ea,
                        }
                    )
                    if function_ea is not None and function_ea not in functions:
                        functions[function_ea] = {
                            "address": int(function_ea),
                            "name": ida_funcs.get_func_name(function_ea),
                            "pseudocode": decompile(function_ea),
                        }
                raw_pointers.append(
                    {
                        "address": int(pointer_ea),
                        "string_address": int(string_ea),
                        "aligned_4": pointer_ea % 4 == 0,
                        "surrounding_start": int(max(segment.start_ea, pointer_ea - 32)),
                        "surrounding_bytes": surrounding.hex(" "),
                        "xrefs": xrefs,
                        "functions": list(functions.values()),
                    }
                )
                offset = data.find(needle, offset + 1)

    report = {
        "input_file": ida_nalt.get_input_file_path(),
        "pattern": pattern.pattern,
        "strings": strings,
        "raw_pointers": raw_pointers,
    }
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(rendered, encoding="utf-8")
    print("TDX_IDA_STRING_POINTER_SCAN_BEGIN")
    if output_path is None:
        print(rendered)
    else:
        print(
            json.dumps(
                {
                    "output": str(output_path),
                    "string_count": len(strings),
                    "pointer_count": len(raw_pointers),
                },
                ensure_ascii=False,
            )
        )
    print("TDX_IDA_STRING_POINTER_SCAN_END")
    ida_pro.qexit(0)


main()

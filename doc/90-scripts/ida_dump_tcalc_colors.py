"""Export TCalc's built-in COLOR* directive table from an IDA database.

The TCalc parser walks fixed-size records starting at ``aColorblack`` and
stores ``dword_1030A468[index]`` as a Windows COLORREF.  Keeping this probe in
the repository makes the browser palette traceable to the processed DLL
instead of to UI guesses.
"""

from __future__ import annotations

import json
from pathlib import Path

import ida_auto
import ida_bytes
import ida_name
import ida_nalt
import ida_pro
import idc


NAME_RECORD_SIZE = 34


def named_address(name: str) -> int:
    address = ida_name.get_name_ea(idc.BADADDR, name)
    if address == idc.BADADDR:
        raise RuntimeError(f"IDA name not found: {name}")
    return address


def read_c_string(address: int) -> str:
    value = ida_bytes.get_strlit_contents(address, -1, idc.STRTYPE_C)
    if value is None:
        return ""
    return value.decode("ascii", errors="replace")


def css_from_colorref(value: int) -> str:
    red = value & 0xFF
    green = (value >> 8) & 0xFF
    blue = (value >> 16) & 0xFF
    return f"#{red:02x}{green:02x}{blue:02x}"


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
    name_base = named_address("aColorblack")
    count_address = named_address("word_10128464")
    metadata_base = named_address("word_10128468")
    color_base = named_address("dword_1030A468")
    count = ida_bytes.get_word(count_address)

    colors: list[dict[str, object]] = []
    for index in range(count):
        colorref = ida_bytes.get_dword(color_base + index * 4)
        colors.append(
            {
                "index": index,
                "token": read_c_string(name_base + index * NAME_RECORD_SIZE),
                "colorref": colorref,
                "colorref_hex": f"0x{colorref:08x}",
                "css_rgb": css_from_colorref(colorref),
                "metadata_word": ida_bytes.get_word(
                    metadata_base + index * NAME_RECORD_SIZE
                ),
            }
        )

    report = {
        "input_file": ida_nalt.get_input_file_path(),
        "evidence": {
            "name_base": name_base,
            "name_record_size": NAME_RECORD_SIZE,
            "count_address": count_address,
            "colorref_base": color_base,
            "parser_function": "sub_10090590",
        },
        "count": count,
        "colors": colors,
    }
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    destination = output_path()
    if destination is None:
        print(rendered)
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(rendered, encoding="utf-8")
        print(json.dumps({"output": str(destination), "count": count}))
    ida_pro.qexit(0)


main()

"""Inspect named IDA data items and list the functions that reference them.

Example::

    idat.exe -A "-Sida_data_inspect.py 0xEFE5E8 0xEFE598 --bytes=64 --output=report.json" target.i64

This is intentionally a small, generic evidence helper.  It reads the bytes as
they exist in the IDB and does not attempt to infer runtime values after config
files or theme code have modified the globals.
"""

from __future__ import annotations

import json
from pathlib import Path

import ida_auto
import ida_bytes
import ida_funcs
import ida_name
import ida_pro
import ida_segment
import idautils
import idc


def parse_arguments() -> tuple[list[str], int, Path | None]:
    selectors: list[str] = []
    byte_count = 64
    output_path: Path | None = None
    for value in idc.ARGV[1:]:
        if value.startswith("--bytes="):
            byte_count = max(1, min(4096, int(value.partition("=")[2], 0)))
        elif value.startswith("--output="):
            output_path = Path(value.partition("=")[2])
        else:
            selectors.append(value)
    return selectors, byte_count, output_path


def resolve_selector(value: str) -> int:
    try:
        return int(value, 0)
    except ValueError:
        address = idc.get_name_ea_simple(value)
        if address == idc.BADADDR:
            raise ValueError(f"IDA name not found: {value}")
        return address


def decode_c_string(raw: bytes) -> str | None:
    prefix = raw.partition(b"\0")[0]
    if not prefix:
        return ""
    if any(value < 0x20 and value not in (0x09, 0x0A, 0x0D) for value in prefix):
        return None
    for encoding in ("gb18030", "utf-8", "latin-1"):
        try:
            return prefix.decode(encoding)
        except UnicodeDecodeError:
            continue
    return None


def references_to(address: int) -> list[dict[str, object]]:
    records: dict[tuple[int, int], dict[str, object]] = {}
    for xref in idautils.XrefsTo(address):
        function = ida_funcs.get_func(xref.frm)
        start = function.start_ea if function is not None else idc.BADADDR
        key = (start, xref.frm)
        records[key] = {
            "from": xref.frm,
            "from_hex": hex(xref.frm),
            "type": xref.type,
            "function_address": None if function is None else start,
            "function_address_hex": None if function is None else hex(start),
            "function_name": None if function is None else ida_funcs.get_func_name(start),
        }
    return [records[key] for key in sorted(records)]


def inspect(address: int, byte_count: int) -> dict[str, object]:
    raw = ida_bytes.get_bytes(address, byte_count) or b""
    segment = ida_segment.getseg(address)
    unsigned = ida_bytes.get_dword(address)
    signed = unsigned if unsigned < 0x80000000 else unsigned - 0x100000000
    return {
        "address": address,
        "address_hex": hex(address),
        "name": ida_name.get_name(address),
        "segment": None if segment is None else ida_segment.get_segm_name(segment),
        "item_size": ida_bytes.get_item_size(address),
        "dword_unsigned": unsigned,
        "dword_signed": signed,
        "bytes_hex": raw.hex(),
        "c_string": decode_c_string(raw),
        "references": references_to(address),
    }


def main() -> None:
    ida_auto.auto_wait()
    selectors, byte_count, output_path = parse_arguments()
    addresses = [resolve_selector(value) for value in selectors]
    report = {
        "input_file": idc.get_input_file_path(),
        "byte_count": byte_count,
        "items": [inspect(address, byte_count) for address in addresses],
    }
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(rendered, encoding="utf-8")
    print("TDX_IDA_DATA_INSPECT_BEGIN")
    print(
        rendered
        if output_path is None
        else json.dumps(
            {"output": str(output_path), "item_count": len(addresses)},
            ensure_ascii=False,
        )
    )
    print("TDX_IDA_DATA_INSPECT_END")
    ida_pro.qexit(0)


main()

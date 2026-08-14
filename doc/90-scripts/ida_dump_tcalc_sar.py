"""Read-only IDA batch probe for the TCalc SAR/SARTURN registrations.

Usage: idat.exe -A -S"ida_dump_tcalc_sar.py OUTPUT.json" TCalc.dll.i64
"""

import json
import sys

import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_kernwin
import ida_lines
import idaapi
import idautils
import idc


def decode(raw):
    if isinstance(raw, str):
        return raw
    for encoding in ("gbk", "utf-8", "latin1"):
        try:
            return bytes(raw).decode(encoding)
        except Exception:
            pass
    return bytes(raw).hex()


def pseudocode(ea):
    try:
        cfunc = ida_hexrays.decompile(ea)
        return "\n".join(ida_lines.tag_remove(line.line) for line in cfunc.get_pseudocode())
    except Exception as error:
        return "<decompile failed: %s>" % error


def function_row(ea):
    function = ida_funcs.get_func(ea)
    if not function:
        return None
    return {
        "start_ea": hex(function.start_ea),
        "end_ea": hex(function.end_ea),
        "name": ida_funcs.get_func_name(function.start_ea),
        "pseudocode": pseudocode(function.start_ea),
        "callers": [
            {
                "xref_ea": hex(xref.frm),
                "function_ea": hex(ida_funcs.get_func(xref.frm).start_ea),
                "function_name": ida_funcs.get_func_name(ida_funcs.get_func(xref.frm).start_ea),
            }
            for xref in idautils.XrefsTo(function.start_ea)
            if ida_funcs.get_func(xref.frm)
        ],
    }


def main():
    arguments = list(getattr(idc, "ARGV", [])) or sys.argv
    output = arguments[1]
    ida_auto.auto_wait()
    rows = []
    seen_functions = set()
    strings = idautils.Strings()
    strings.setup(strtypes=[0, 1], minlen=3)
    for item in strings:
        text = decode(ida_bytes.get_strlit_contents(item.ea, item.length, item.strtype) or b"")
        upper = text.upper()
        normalized = upper.strip(" \r\n\t:.,")
        if "SARTURN" not in upper and "SAR(" not in upper and normalized not in {
            "SAR", "NEWSAR", "SARTURN"
        }:
            continue
        xrefs = []
        for xref in idautils.XrefsTo(item.ea):
            function = ida_funcs.get_func(xref.frm)
            row = {"from": hex(xref.frm), "type": int(xref.type)}
            if function:
                row["function"] = hex(function.start_ea)
                row["function_name"] = ida_funcs.get_func_name(function.start_ea)
                seen_functions.add(function.start_ea)
            row["parents"] = []
            for parent in idautils.XrefsTo(xref.frm):
                parent_function = ida_funcs.get_func(parent.frm)
                parent_row = {"from": hex(parent.frm), "type": int(parent.type)}
                if parent_function:
                    parent_row["function"] = hex(parent_function.start_ea)
                    parent_row["function_name"] = ida_funcs.get_func_name(parent_function.start_ea)
                    seen_functions.add(parent_function.start_ea)
                row["parents"].append(parent_row)
            row["nearby_dwords"] = [
                {"ea": hex(xref.frm + delta), "value": hex(ida_bytes.get_dword(xref.frm + delta))}
                for delta in range(-32, 36, 4)
            ]
            xrefs.append(row)
        rows.append({"ea": hex(item.ea), "text": text, "xrefs": xrefs})
    functions = [function_row(ea) for ea in sorted(seen_functions)]
    document = {"strings": rows, "functions": [row for row in functions if row]}
    with open(output, "w", encoding="utf-8") as stream:
        json.dump(document, stream, ensure_ascii=False, indent=2)
    idc.qexit(0)


main()

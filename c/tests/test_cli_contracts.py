"""Five offline CLI contracts checked with Python's independent JSON decoder.

Only temporary synthetic .day/.lc1/.dat/ZIP fixtures are used; no market server,
terminal installation or externally downloaded dataset is required.
"""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile
import zlib


EXE = str(Path(sys.argv[1]).resolve())


def reject_constant(value):
    raise ValueError("non-JSON numeric constant: " + value)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON member: " + key)
        result[key] = value
    return result


def execute(*arguments, succeeds=True):
    result = subprocess.run([EXE, *map(str, arguments)], capture_output=True, timeout=15)
    if (result.returncode == 0) != succeeds:
        raise AssertionError(f"unexpected exit {result.returncode} for {arguments}: "
                             + result.stderr.decode("utf-8", errors="replace"))
    if not succeeds:
        assert result.stderr, "a refused command must explain the failure"
        return result
    lines = result.stdout.decode("utf-8").splitlines()
    assert lines and all(lines), "successful JSONL must contain complete nonempty lines"
    return [json.loads(line, parse_constant=reject_constant, object_pairs_hook=unique_object)
            for line in lines], lines


def finance_member():
    header = bytearray(20)
    struct.pack_into("<H", header, 0, 1)
    struct.pack_into("<I", header, 2, 20260630)
    struct.pack_into("<H", header, 6, 1)
    struct.pack_into("<H", header, 10, 11)
    struct.pack_into("<I", header, 12, 184 * 4)
    values = [0.0] * 184
    values[0], values[182], values[183] = 2.5, 1.25, float("nan")
    return bytes(header) + b"000001\0" + struct.pack("<I", 31) + struct.pack("<184f", *values)


def main():
    catalog, _ = execute("panorama", "--view", "catalog", "--max-records", "2", "--quiet")
    assert [row["type"] for row in catalog] == ["panorama_view", "panorama_view", "panorama_summary"]
    assert catalog[-1]["view"] == "catalog" and catalog[-1]["rows"] == 2
    for index, view in enumerate(catalog[:-1]):
        assert view["index"] == index and view["field_count"] == len(view["fields"])

    with tempfile.TemporaryDirectory(prefix="tdx-cli-contract-") as temporary:
        # Windows supplies backslashes in the absolute path. POSIX additionally
        # exercises literal quotes and backslashes as legal filename characters.
        name = "space ' path" if os.name == "nt" else 'space "quote" \\ path'
        root = Path(temporary) / name
        root.mkdir()
        daily = root / "sample.day"
        daily.write_bytes(struct.pack("<IiiiifII", 20260630, 1000, 1100, 900, 1050, 12345.5, 100, 0))
        rows, _ = execute("daily", "--input", daily, "--security", "sh600000", "--quiet")
        assert [row["type"] for row in rows] == ["daily_summary", "daily_bar"]
        assert rows[0]["source"] == str(daily) and rows[0]["bars"] == 1
        assert rows[1]["security_id"] == "SH600000" and rows[1]["scale_divisor"] == 100
        assert rows[1]["open"] == 10 and rows[1]["close"] == 10.5 and rows[1]["volume"] == 100

        minute = root / "sample.lc1"
        date_word = (2026 - 2004) * 2048 + 630
        minute.write_bytes(struct.pack("<HHfffffIHH", date_word, 570, 10, 11, 9, 10.5, 12345.5, 100, 7, 11))
        rows, _ = execute("minute", "--input", minute, "--security", "sh600000", "--quiet")
        assert [row["type"] for row in rows] == ["minute_summary", "minute_bar"]
        assert rows[0]["source"] == str(minute) and rows[0]["bars"] == 1
        assert rows[1]["date"] == 20260630 and rows[1]["time"] == "09:30"
        assert rows[1]["close"] == 10.5 and (rows[1]["extra_1"], rows[1]["extra_2"]) == (7, 11)

        trading = root / "sample.dat"
        trading.write_bytes(struct.pack("<BIff", 1, 20260630, 1.25, float("nan"))
                            + struct.pack("<BIff", 99, 0, 2.5, 3.5))
        rows, _ = execute("professional", "--input", trading, "--kind", "stock", "--quiet")
        assert rows[0]["type"] == "professional_document" and rows[0]["source"] == str(trading)
        assert rows[0]["records"] == 2 and rows[0]["selected"] == 2
        records = [row for row in rows if row["type"] == "professional_record"]
        assert records[0]["first"] == 1.25 and records[0]["second"] is None
        assert records[1]["id"] == 99 and records[1]["name"] is None and records[1]["date"] is None

        finance = root / "quarter.zip"
        member_name = 'finance"quoted\\quarter.dat'
        member = finance_member()
        with zipfile.ZipFile(finance, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            entry = zipfile.ZipInfo("placeholder.dat")
            # ZipInfo's constructor normalizes Windows path separators. Set the
            # wire name explicitly so both platforms exercise identical bytes.
            entry.filename = member_name
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, member)
        arguments = ("professional", "--zip", finance, "--member", member_name, "--quiet")
        rows, lines = execute(*arguments, "--field", "1")
        assert rows[0]["source"] == str(finance) and rows[0]["member"] == member_name
        assert rows[0]["member_crc"] == f"{zlib.crc32(member):08x}" and rows[0]["field_count"] == 184
        assert rows[1] == {"type": "finance_row", "code": "000001", "market_id": 0,
                           "report_date": 20260630, "revenue_yoy": 1.25, "profit_yoy": None, "field_1": 2.5}
        assert lines[1] == ('{"type":"finance_row","code":"000001","market_id":0,'
                            '"report_date":20260630,"revenue_yoy":1.250000,"profit_yoy":null,"field_1":2.500000}')
        rows, _ = execute(*arguments, "--code", "000001", "--field", "183")
        assert [row["type"] for row in rows] == ["finance_document", "finance_record", "finance_field"]
        assert rows[2]["field"] == 183 and rows[2]["name"] and rows[2]["value"] == 1.25

        # Rejections must finish before any network use or misleading success.
        for bad in [("professional", "--field", "1junk"),
                    ("professional", "--from", "20260230"),
                    ("daily", "--max-records", "-1"),
                    ("daily", "--port", "8789"),
                    ("professional", "--input", trading, "--output", root)]:
            execute(*bad, succeeds=False)
        if os.name != "nt" and Path("/dev/full").exists():
            execute("professional", "--input", trading, "--output", "/dev/full", succeeds=False)
    print("5 offline CLI contracts passed (catalog, day, minute, trading, finance ZIP)")


if __name__ == "__main__":
    main()

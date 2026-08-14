from __future__ import annotations

import struct
import unittest
from dataclasses import replace

import tdx_category_quotes as category


def encode_varint(value: int) -> bytes:
    remaining = abs(value)
    first = remaining & 0x3F
    remaining >>= 6
    if value < 0:
        first |= 0x40
    output = bytearray((first,))
    if remaining:
        output[0] |= 0x80
    while remaining:
        byte = remaining & 0x7F
        remaining >>= 7
        if remaining:
            byte |= 0x80
        output.append(byte)
    return bytes(output)


def make_record(market_id: int, code: str, close_raw: int = 1000) -> bytes:
    output = bytearray(bytes((market_id,)) + code.encode("ascii"))
    output.extend(struct.pack("<H", 7))
    for value in (close_raw, -10, 1, 2, -2, 150000, 0, 1000, 10):
        output.extend(encode_varint(value))
    output.extend(struct.pack("<I", 0))
    for value in (400, 600, 0, 77, 0, 1, 123, 45):
        output.extend(encode_varint(value))
    output.extend(struct.pack(
        "<Hhhfh10sff24sH",
        9,
        25,
        123,
        456.5,
        -75,
        b"pair-data!",
        3.5,
        4.5,
        b"meta".ljust(24, b"\x00"),
        8,
    ))
    return bytes(output)


class CategoryQuoteTests(unittest.TestCase):
    def test_builds_exact_054b_request_data(self) -> None:
        self.assertEqual(
            category.build_category_request_data(
                "a-shares", "seal-amount", start=3, count=80,
            ).hex(" "),
            "06 00 1c 00 03 00 50 00 01 00 05 00 00 00 01 00 00 00",
        )
        ascending = category.build_category_request_data(
            6, "opening-rush", start=0, count=30, ascending=True,
        )
        self.assertEqual(struct.unpack("<9H", ascending)[4], 2)

    def test_parses_category_records_and_fixed_tail(self) -> None:
        payload = (
            struct.pack("<HH", 11, 2)
            + make_record(0, "000001")
            + make_record(1, "600000", 900)
        )
        page = category.parse_category_payload(
            payload,
            category=6,
            sort_type=0x001C,
            start=0,
            request_count=80,
            ascending=False,
            filter_raw=0,
        )
        self.assertEqual(page.header, 11)
        self.assertEqual([item.key for item in page.records], [
            (0, "000001"), (1, "600000"),
        ])
        item = page.records[0]
        self.assertAlmostEqual(item.last_price, 10.0)
        self.assertAlmostEqual(item.pre_close_price, 9.9)
        self.assertAlmostEqual(item.open_amount_yuan, 7700.0)
        self.assertAlmostEqual(item.bid1_price, 10.0)
        self.assertEqual(item.bid1_volume_hand, 123)
        self.assertAlmostEqual(item.seal_amount_yuan, 123_000.0)
        self.assertAlmostEqual(item.rise_speed, 0.25)
        self.assertAlmostEqual(item.short_turnover, 1.23)
        self.assertAlmostEqual(item.opening_rush, -0.75)
        self.assertAlmostEqual(item.two_minute_amount, 456.5)
        self.assertEqual(item.active2, 8)
        self.assertFalse(item.is_sealed)
        self.assertTrue(replace(item, ask1_volume_hand=0).is_sealed)

    def test_applies_etf_price_divisor(self) -> None:
        payload = struct.pack("<HH", 0, 1) + make_record(1, "510300", 43210)
        item = category.parse_category_payload(
            payload,
            category=6,
            sort_type=0,
            start=0,
            request_count=1,
            ascending=False,
            filter_raw=0,
        ).records[0]
        self.assertAlmostEqual(item.last_price, 43.21)


if __name__ == "__main__":
    unittest.main()

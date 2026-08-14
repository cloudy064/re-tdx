from __future__ import annotations

import struct
import unittest

import tdx_market_depth as depth
import tdx_market_snapshot as snapshot


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


def make_record(market_id: int, code: str, current: int = 1000) -> bytes:
    output = bytearray(bytes((market_id,)) + code.encode("ascii"))
    output.extend(struct.pack("<H", 7))
    for value in (current, -10, 1, 2, -2):
        output.extend(encode_varint(value))
    output.extend(struct.pack("<I", 123456))
    for value in (0, 1000, 10):
        output.extend(encode_varint(value))
    output.extend(struct.pack("<I", 0))
    for value in (400, 600, 0, 77):
        output.extend(encode_varint(value))
    for level in range(5):
        for value in (-(level + 1), level + 1, 100 + level, 200 + level):
            output.extend(encode_varint(value))
    output.extend(b"\xaa\xbb")
    return bytes(output)


class MarketDepthTests(unittest.TestCase):
    def test_builds_exact_0547_request_data(self) -> None:
        codes = (
            snapshot.QuoteCode(0, "000001"),
            snapshot.QuoteCode(1, "600000"),
        )
        self.assertEqual(
            depth.build_depth_request_data(codes).hex(" "),
            "02 00 00 30 30 30 30 30 31 00 00 00 00 "
            "01 36 30 30 30 30 30 00 00 00 00",
        )

    def test_decodes_xor_and_five_levels(self) -> None:
        codes = (snapshot.QuoteCode(0, "000001"),)
        decoded = struct.pack("<H", 1) + make_record(0, "000001")
        payload = bytes(byte ^ depth.RESPONSE_XOR for byte in decoded)
        parsed = depth.parse_depth_payload(payload, codes)
        self.assertEqual(len(parsed), 1)
        item = parsed[0]
        self.assertEqual(item.key, (0, "000001"))
        self.assertAlmostEqual(item.last_price, 10.0)
        self.assertAlmostEqual(item.pre_close_price, 9.9)
        self.assertEqual([level.volume_hand for level in item.buy_levels], [100, 101, 102, 103, 104])
        self.assertAlmostEqual(item.buy_levels[0].price, 9.99)
        self.assertAlmostEqual(item.sell_levels[4].price, 10.05)
        self.assertAlmostEqual(item.bid1_amount_yuan or 0, 99_900.0)
        self.assertEqual(item.open_amount_yuan, 770.0)
        self.assertEqual(item.tail_hex, "aabb")

    def test_splits_multiple_records(self) -> None:
        codes = (
            snapshot.QuoteCode(0, "000001"),
            snapshot.QuoteCode(1, "600000"),
        )
        decoded = (
            struct.pack("<H", 2)
            + make_record(0, "000001")
            + make_record(1, "600000", 900)
        )
        payload = bytes(byte ^ depth.RESPONSE_XOR for byte in decoded)
        parsed = depth.parse_depth_payload(payload, codes)
        self.assertEqual([item.key for item in parsed], [(0, "000001"), (1, "600000")])


if __name__ == "__main__":
    unittest.main()

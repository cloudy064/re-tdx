from __future__ import annotations

import struct
import unittest

import tdx_auction_series as auction
import tdx_market_snapshot as snapshots


def point(
    minute: int,
    second: int,
    price: float,
    matched: int,
    unmatched: int,
    reserved: int = 0,
) -> bytes:
    return (
        struct.pack("<HfIi", minute, price, matched, unmatched)
        + bytes((reserved, second))
    )


class AuctionSeriesTests(unittest.TestCase):
    def test_builds_exact_056a_request(self) -> None:
        request = auction.build_auction_request_data(
            snapshots.QuoteCode(0, "000988")
        )
        self.assertEqual(
            request.hex(),
            "000030303039383800000000030000000000000000000000f4010000",
        )
        self.assertEqual(len(request), 28)

    def test_parses_fixed_records_and_signed_unmatched(self) -> None:
        payload = (
            struct.pack("<H", 3)
            + point(555, 0, 10.0, 100, 80)
            + point(555, 5, 10.1, 120, -60)
            + point(565, 0, 10.2, 150, 0)
        )
        item = auction.parse_auction_payload(
            payload, snapshots.QuoteCode(0, "000001")
        )
        self.assertEqual(item.security_id, "SZ000001")
        self.assertEqual(len(item.points), 3)
        self.assertEqual(item.points[0].time_label, "09:15:00")
        self.assertAlmostEqual(item.points[-1].price, 10.2, places=5)
        self.assertEqual(item.points[0].unmatched_direction_raw, 1)
        self.assertEqual(item.points[1].unmatched_direction_raw, -1)
        self.assertEqual(item.points[2].unmatched_direction_raw, 0)
        self.assertEqual(item.points[0].unmatched_direction, "buy")
        self.assertEqual(item.points[1].unmatched_direction, "sell")
        self.assertEqual(item.points[2].unmatched_direction, "balanced")
        self.assertAlmostEqual(
            item.points[-1].matched_amount_yuan, 153_000.0, places=2
        )

        summary = auction.summarize_series(item)
        opening = summary["opening"]
        self.assertEqual(opening["start_time"], "09:15:00")
        self.assertEqual(opening["end_time"], "09:25:00")
        self.assertEqual(opening["unmatched_direction_flips"], 1)
        self.assertEqual(opening["matched_volume_monotonic_violations"], 0)
        self.assertEqual(summary["closing"]["point_count"], 0)

    def test_rejects_bad_length_and_time(self) -> None:
        code = snapshots.QuoteCode(0, "000001")
        with self.assertRaises(auction.AuctionSeriesError):
            auction.parse_auction_payload(b"\x01\x00", code)
        payload = struct.pack("<H", 1) + point(555, 60, 10.0, 1, 0)
        with self.assertRaises(auction.AuctionSeriesError):
            auction.parse_auction_payload(payload, code)


if __name__ == "__main__":
    unittest.main()

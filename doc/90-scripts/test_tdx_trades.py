from __future__ import annotations

import struct
import unittest

import tdx_market_snapshot as snapshots
import tdx_trades as trades


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


def record(
    minute: int,
    price_delta: int,
    volume: int,
    orders: int,
    status: int,
    tail: int = 0,
) -> bytes:
    output = bytearray(struct.pack("<H", minute))
    for value in (price_delta, volume, orders, status, tail):
        output.extend(encode_varint(value))
    return bytes(output)


class TradeTests(unittest.TestCase):
    def test_builds_exact_today_and_history_requests(self) -> None:
        code = snapshots.QuoteCode(0, "000001")
        self.assertEqual(
            trades.build_today_request_data(code, count=115).hex(),
            "000030303030303100007300",
        )
        self.assertEqual(
            trades.build_history_request_data(
                snapshots.QuoteCode(0, "300308"),
                "2026-05-11",
                count=900,
            ).hex(),
            "9f263501000033303033303800008403",
        )

    def test_parses_price_deltas_status_and_absolute_index(self) -> None:
        payload = (
            struct.pack("<H", 3)
            + record(565, 1000, 20, 3, 0, 7)
            + record(565, 1, 10, 2, 1, 8)
            + record(900, -1, 30, 5, 2, 9)
        )
        page = trades.parse_today_payload(
            payload,
            snapshots.QuoteCode(0, "000001"),
            start=100,
            request_count=3,
            trading_date="20260801",
        )
        self.assertEqual([item.absolute_index for item in page.ticks], [100, 101, 102])
        self.assertEqual([item.time_label for item in page.ticks], [
            "09:25", "09:25", "15:00",
        ])
        self.assertEqual([item.price for item in page.ticks], [10.0, 10.01, 10.0])
        self.assertEqual([item.side for item in page.ticks], ["buy", "sell", "neutral"])
        self.assertEqual([item.tail_raw for item in page.ticks], [7, 8, 9])
        self.assertAlmostEqual(page.ticks[0].amount_yuan, 20_000.0)

    def test_parses_history_header_and_aggregates_auction_minutes(self) -> None:
        payload = (
            struct.pack("<Hf", 3, 10.0)
            + record(565, 1000, 20, 3, 0)
            + record(565, 1, 10, 2, 1)
            + record(900, 1001, 30, 5, 2)
        )
        page = trades.parse_history_payload(
            payload,
            snapshots.QuoteCode(0, "000001"),
            "20260801",
            start=0,
            request_count=2000,
        )
        self.assertAlmostEqual(page.price_base_raw or 0, 10.0)
        summary = trades.aggregate_ticks(page.ticks)
        opening = summary["auction_0925"]
        closing = summary["closing_1500"]
        self.assertEqual(opening["tick_count"], 2)
        self.assertEqual(opening["volume_hand"], 30)
        self.assertAlmostEqual(opening["amount_yuan"], 30_010.0)
        self.assertEqual(opening["buy_volume_hand"], 20)
        self.assertEqual(opening["sell_volume_hand"], 10)
        self.assertEqual(closing["tick_count"], 1)
        self.assertEqual(closing["volume_hand"], 30)
        self.assertAlmostEqual(closing["last_price"], 20.02)

    def test_uses_three_decimal_prices_for_funds_and_bonds(self) -> None:
        payload = struct.pack("<H", 1) + record(565, 4680, 20, 3, 2)
        fund = trades.parse_today_payload(
            payload,
            snapshots.QuoteCode(1, "510300"),
            start=0,
            request_count=1,
        )
        self.assertEqual(fund.price_divisor, 1000)
        self.assertAlmostEqual(fund.ticks[0].price, 4.680)
        self.assertAlmostEqual(fund.ticks[0].amount_yuan, 9_360.0)

        bond_payload = struct.pack("<H", 1) + record(565, 164510, 2, 1, 2)
        bond = trades.parse_today_payload(
            bond_payload,
            snapshots.QuoteCode(1, "113039"),
            start=0,
            request_count=1,
        )
        self.assertEqual(bond.price_divisor, 1000)
        self.assertAlmostEqual(bond.ticks[0].price, 164.510)

    def test_separates_post_close_status_5(self) -> None:
        payload = (
            struct.pack("<H", 3)
            + record(900, 1000, 20, 3, 2)
            + record(905, 0, 7, 2, 5)
            + record(929, 0, 3, 1, 5)
        )
        page = trades.parse_today_payload(
            payload,
            snapshots.QuoteCode(0, "000001"),
            start=0,
            request_count=3,
        )
        summary = trades.aggregate_ticks(page.ticks)
        post_close = summary["post_close_status_5"]
        self.assertEqual(post_close["tick_count"], 2)
        self.assertEqual(post_close["volume_hand"], 10)
        self.assertEqual(post_close["first_time"], "15:05")
        self.assertEqual(post_close["last_time"], "15:29")
        self.assertEqual(
            summary["auction_0925"]["execution_status"], "no-0925-trade"
        )
        self.assertEqual(
            summary["closing_1500"]["execution_status"], "executed-1500"
        )

    def test_rejects_truncated_payload(self) -> None:
        with self.assertRaises(trades.TradeError):
            trades.parse_today_payload(
                b"\x01\x00",
                snapshots.QuoteCode(0, "000001"),
                start=0,
                request_count=1,
            )

    def test_download_uses_returned_count_and_reverses_page_order(self) -> None:
        class Response:
            def __init__(self, data: bytes) -> None:
                self.data = data

        class Connection:
            def __init__(self) -> None:
                self.starts: list[int] = []

            def call(self, command: int, request: bytes) -> Response:
                self.assert_command(command)
                start = int.from_bytes(request[-4:-2], "little")
                self.starts.append(start)
                rows = {
                    0: (record(700, 1000, 1, 1, 0), record(701, 1001, 1, 1, 0)),
                    2: (record(600, 900, 1, 1, 1), record(601, 901, 1, 1, 1)),
                    4: (),
                }[start]
                return Response(struct.pack("<Hf", len(rows), 10.0) + b"".join(rows))

            @staticmethod
            def assert_command(command: int) -> None:
                if command != trades.TYPE_HISTORICAL_TICKS:
                    raise AssertionError(command)

        connection = Connection()
        series = trades._download_one(
            connection,  # type: ignore[arg-type]
            snapshots.QuoteCode(0, "000001"),
            trading_date="20260801",
            server_trade_date="20260801",
            page_size=3,
            max_pages=2,
            progress=None,
        )
        self.assertEqual(connection.starts, [0, 2, 4])
        self.assertEqual(series.pages, 2)
        self.assertEqual(
            [item.time_minutes for item in series.ticks],
            [600, 601, 700, 701],
        )
        self.assertEqual(
            [item.absolute_index for item in series.ticks],
            [2, 3, 0, 1],
        )


if __name__ == "__main__":
    unittest.main()

from __future__ import annotations

import struct
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import download_tdx_minute as transport
import tdx_market_snapshot as snapshot
import update_tdx_market as updater


def encode_varint(value: int) -> bytes:
    negative = value < 0
    remaining = abs(value)
    first = remaining & 0x3F
    remaining >>= 6
    if negative:
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


def make_snapshot_record(
    market_id: int,
    code: str,
    *,
    current_delta: int,
    pre_close_delta: int,
) -> bytes:
    output = bytearray(bytes((market_id,)) + code.encode("ascii"))
    output.extend(struct.pack("<H", 7))
    for value in (
        current_delta,
        pre_close_delta,
        10,
        20,
        -20,
        93_100,
        0,
        12345,
        321,
    ):
        output.extend(encode_varint(value))
    output.extend(struct.pack("<I", 0))
    for value in (4000, 5000, 0, 66):
        output.extend(encode_varint(value))
    output.extend(b"\x00\x00\x00\x00")
    return bytes(output)


class SnapshotProtocolTests(unittest.TestCase):
    def test_uses_extra_price_precision_for_funds_and_bonds(self) -> None:
        self.assertEqual(snapshot._price_divisor("510300"), 10)
        self.assertEqual(snapshot._price_divisor("113039"), 100)
        self.assertEqual(snapshot._price_divisor("000001"), 1)

    def test_builds_exact_054c_request_data(self) -> None:
        codes = (
            snapshot.QuoteCode(0, "000001"),
            snapshot.QuoteCode(1, "880471"),
        )
        data = snapshot.build_snapshot_request_data(codes)
        self.assertEqual(
            data.hex(" "),
            "05 00 00 00 00 00 00 00 02 00 "
            "00 30 30 30 30 30 31 01 38 38 30 34 37 31",
        )

    def test_parses_variable_length_records_by_requested_markers(self) -> None:
        requested = (
            snapshot.QuoteCode(0, "000001"),
            snapshot.QuoteCode(1, "880471"),
        )
        records = (
            make_snapshot_record(
                0,
                "000001",
                current_delta=1000,
                pre_close_delta=-10,
            ),
            make_snapshot_record(
                1,
                "880471",
                current_delta=250_000,
                pre_close_delta=-2500,
            ),
        )
        payload = b"\x00\x00" + struct.pack("<H", 2) + b"".join(records)
        quotes = snapshot.parse_snapshots_payload(payload, requested)

        self.assertEqual([quote.key for quote in quotes], [
            (0, "000001"),
            (1, "880471"),
        ])
        self.assertAlmostEqual(quotes[0].last_price, 10.0)
        self.assertAlmostEqual(quotes[0].pre_close_price, 9.9)
        self.assertAlmostEqual(quotes[0].change_pct or 0, 1.010101, places=5)
        self.assertAlmostEqual(quotes[1].last_price, 2500.0)
        self.assertEqual(quotes[1].total_hand, 12345)
        self.assertEqual(quotes[1].open_amount_yuan, 6600.0)

    def test_resumes_at_unfinished_batch_after_host_failure(self) -> None:
        codes = tuple(
            snapshot.QuoteCode(0, code)
            for code in ("000001", "000002", "000003")
        )
        calls: list[tuple[str, tuple[str, ...]]] = []

        class FakeConnection:
            def __init__(self, endpoint, _timeout) -> None:
                self.endpoint = endpoint
                self.server_name = f"server-{endpoint.host}"

            def __enter__(self):
                return self

            def __exit__(self, *_args) -> None:
                return None

            def call(self, _message_type, data):
                count = int.from_bytes(data[8:10], "little")
                requested = tuple(
                    data[11 + index * 7 : 17 + index * 7].decode("ascii")
                    for index in range(count)
                )
                calls.append((self.endpoint.host, requested))
                if self.endpoint.host == "first" and requested == ("000003",):
                    raise transport.DownloadError("connection lost")
                records = b"".join(
                    make_snapshot_record(
                        0,
                        code,
                        current_delta=1000,
                        pre_close_delta=-10,
                    )
                    for code in requested
                )
                return SimpleNamespace(
                    data=b"\x00\x00" + struct.pack("<H", count) + records
                )

        endpoints = (
            transport.HostEndpoint("first"),
            transport.HostEndpoint("second"),
        )
        with patch.object(transport, "QuoteConnection", FakeConnection):
            result = snapshot.download_snapshots(
                endpoints,
                codes,
                batch_size=2,
            )

        self.assertEqual(calls, [
            ("first", ("000001", "000002")),
            ("first", ("000003",)),
            ("second", ("000003",)),
        ])
        self.assertEqual(len(result.quotes), 3)
        self.assertEqual(result.completed_batches, 2)
        self.assertEqual(result.endpoint.host, "second")


class MarketDocumentTests(unittest.TestCase):
    def test_computes_breadth_leader_laggard_and_alignment(self) -> None:
        document: dict[str, object] = {
            "v": 1,
            "generated_at": "2026-07-31T12:00:00+08:00",
            "source_date": "20260731",
            "stats": {"blocks": 1, "memberships": 2, "securities": 2},
            "families": [["industry", "通达信行业", 1, 2]],
            "blocks": [[0, "银行", "880471", -1, 2, 1, 2, "20260731", "T0101"]],
            "securities": [[0, "000001", "平安银行"], [1, "600000", "浦发银行"]],
            "members": [[1, 3]],
            "reverse": [[1], [1]],
        }
        quotes = (
            snapshot.QuoteSnapshot(
                0, "000001", 1, 10.5, 10.0, 10.1, 10.6, 10.0,
                93100, 100, 1, 2_000_000.0, 40, 60, 100_000.0,
            ),
            snapshot.QuoteSnapshot(
                1, "600000", 1, 9.8, 10.0, 10.0, 10.1, 9.7,
                93100, 200, 1, 3_000_000.0, 60, 40, 200_000.0,
            ),
            snapshot.QuoteSnapshot(
                1, "880471", 1, 1200.0, 1190.0, 1195.0, 1205.0, 1180.0,
                93100, 300, 1, 9_000_000.0, 50, 50, 300_000.0,
            ),
        )
        updater.attach_market_data(
            document,
            quotes,
            {"880471": 1},
            captured_at="2026-07-31T13:30:00+08:00",
            endpoint="127.0.0.1:7709",
            server_name="test",
            requested=3,
        )
        market = document["market"]
        self.assertIsInstance(market, dict)
        metric = market["block_metrics"][0]
        self.assertEqual(metric[:4], [2, 1, 1, 0])
        self.assertEqual(metric[5], 0)
        self.assertEqual(metric[7], 1)
        self.assertEqual(metric[9], 5_000_000.0)
        self.assertAlmostEqual(metric[4], 1.5)
        self.assertAlmostEqual(market["block_quotes"][0][10], 100 / 119, places=4)


if __name__ == "__main__":
    unittest.main()

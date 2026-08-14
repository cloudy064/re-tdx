from __future__ import annotations

import json
import struct
import tempfile
import unittest
from pathlib import Path

import tdx_level2 as l2


def encode_varint(value: int) -> bytes:
    magnitude = abs(value)
    first = magnitude & 0x3F
    magnitude >>= 6
    if value < 0:
        first |= 0x40
    output = bytearray((first | (0x80 if magnitude else 0),))
    while magnitude:
        byte = magnitude & 0x7F
        magnitude >>= 7
        output.append(byte | (0x80 if magnitude else 0))
    return bytes(output)


class DirectRequestTests(unittest.TestCase):
    def test_build_and_inspect_transaction_request(self) -> None:
        payload = l2.build_direct_request(
            "transaction", 1, "600000", cursor=0x11223344, count=1500
        )
        self.assertEqual(len(payload), 26)
        self.assertEqual(
            payload.hex(),
            "000000000000100010005405010036303030303044332211dc05",
        )
        request = l2.inspect_direct_request(payload)
        self.assertEqual(request.command, 1364)
        self.assertEqual(request.code, "600000")
        self.assertEqual(request.cursor, 0x11223344)

    def test_order_command_and_limit(self) -> None:
        payload = l2.build_direct_request("order", 0, "000001", count=1)
        self.assertEqual(struct.unpack_from("<H", payload, 10)[0], 1374)
        with self.assertRaises(l2.Level2Error):
            l2.build_direct_request("order", 0, "000001", count=1501)

    def test_redirect_bodies(self) -> None:
        tick = l2.build_redirect_body_4655(
            1, "600000", wantnum=600, has_attachinfo=True
        )
        self.assertEqual(len(tick), 46)
        self.assertEqual(struct.unpack_from("<HH", tick), (4655, 1))
        self.assertEqual(struct.unpack_from("<H", tick, 34)[0], 80)
        self.assertEqual(tick[36], 1)
        depth = l2.build_redirect_body_4680(0, "000001", depth=10)
        self.assertEqual(len(depth), 37)
        self.assertEqual(depth[26], 10)

    def test_fast_hq_subscription_fields(self) -> None:
        self.assertEqual(
            l2.fast_hq_subscribe_fields(0, "000001", 2),
            {
                "CODE": "000001",
                "SC": 0,
                "LX": 2,
                "PkgType": 0,
                "OperType": 1,
                "PushType": 3,
                "BatchPush": 1,
            },
        )


class DirectParserTests(unittest.TestCase):
    def test_parse_transactions_and_xor(self) -> None:
        body = bytearray(struct.pack("<HI", 2, 77))
        body += struct.pack("<H", 12600)
        body += encode_varint(100_000)
        body += encode_varint(5)
        body += encode_varint(2)
        body += encode_varint(0)
        body += encode_varint(10)
        body += struct.pack("<H", 12601)
        body += encode_varint(5)
        body += encode_varint(7)
        body += encode_varint(1)
        body += encode_varint(1)
        body += encode_varint(11)

        page = l2.parse_direct_payload(bytes(body), "transaction")
        self.assertEqual(page.next_cursor, 77)
        self.assertEqual(page.records[0].time_label, "09:30:00")
        self.assertEqual(page.records[0].price, 10.0)
        self.assertEqual(page.records[1].price, 10.0005)
        self.assertEqual(page.records[1].side, "sell")

        encrypted = l2.xor_payload(bytes(body), 0x5A)
        self.assertEqual(
            l2.parse_direct_payload(encrypted, "transaction", xor_key=0x5A),
            page,
        )

    def test_parse_orders(self) -> None:
        body = bytearray(struct.pack("<HIH", 1, 99, 12605))
        body += encode_varint(123_450)
        body += encode_varint(8)
        body += bytes((ord("2"), ord("B"), ord("C")))
        body += encode_varint(98765)
        page = l2.parse_direct_payload(bytes(body), "order")
        record = page.records[0]
        self.assertEqual(record.price, 12.345)
        self.assertEqual(record.order_type, "2")
        self.assertEqual(record.side, "B")
        self.assertEqual(record.action, "C")
        self.assertEqual(record.order_id_raw, 98765)

    def test_rejects_trailing_and_bad_count(self) -> None:
        with self.assertRaises(l2.Level2Error):
            l2.parse_direct_payload(struct.pack("<HI", 0, 0) + b"x", "order")
        with self.assertRaises(l2.Level2Error):
            l2.parse_direct_payload(struct.pack("<HI", 1501, 0), "order")


class SdkAdapterTests(unittest.TestCase):
    def test_transactions_reverse_sdk_order(self) -> None:
        records = l2.parse_sdk_transactions({
            "Data": [
                {
                    "transactionPrice": 10.01,
                    "transactionTime": 9300100,
                    "singleVolume": 100,
                    "transactionStatus": "B",
                },
                {
                    "transactionPrice": 10.02,
                    "transactionTime": 9300200,
                    "singleVolume": 200,
                    "transactionStatus": "S",
                },
            ]
        })
        self.assertEqual([record.time_label for record in records], ["09:30:02", "09:30:01"])
        self.assertEqual([record.side for record in records], ["sell", "buy"])

    def test_queue_selects_first_buy_and_last_sell_level(self) -> None:
        queue = l2.parse_sdk_queue({
            "Data": {
                "buyList": [{"QUANTITY_": [100, 250]}],
                "sellList": [
                    {"QUANTITY_": [999]},
                    {"QUANTITY_": [300, 400]},
                ],
            }
        })
        self.assertEqual(queue.buy_quantities_hand, (1, 2))
        self.assertEqual(queue.sell_quantities_hand, (3, 4))

    def test_depth_matches_adapter_orientation_and_scale(self) -> None:
        depth = l2.parse_sdk_depth({
            "Data": json.dumps({
                "datetime": "20260801153000",
                "preClosePrice": 9.9,
                "openPrice": 10,
                "highPrice": 10.5,
                "lowPrice": 9.8,
                "lastPrice": 10.2,
                "volume": 1234,
                "amount": 5678.5,
                "openInterest": 9,
                "buyPrices": [10.1, 10.0, 9.9],
                "buyVolumes": [1, 2, 3],
                "sellPrices": [10.2, 10.3, 10.4],
                "sellVolumes": [4, 5, 6],
            })
        }, requested_depth=5)
        self.assertEqual(depth.time_label, "15:30:00")
        self.assertEqual(depth.buy_prices, (9.9, 10.0, 10.1))
        self.assertEqual(depth.buy_volumes_raw, (3000, 2000, 1000))
        self.assertEqual(depth.sell_prices, (10.2, 10.3, 10.4))
        self.assertEqual(depth.sell_volumes_raw, (4000, 5000, 6000))


class SdkBinaryCallbackTests(unittest.TestCase):
    def test_parse_1803_multi_level_snapshot(self) -> None:
        payload = bytearray(l2.SDK_MULTI_LEVEL_SIZE)
        struct.pack_into("<IIII", payload, 0, 7, 8, 2, 2)
        for index, (price, volume, auxiliary) in enumerate(
            ((10.01, 100, 3), (10.00, 200, 4))
        ):
            struct.pack_into("<d", payload, 16 + index * 8, price)
            struct.pack_into("<I", payload, 8016 + index * 4, volume)
            struct.pack_into("<H", payload, 12016 + index * 4, auxiliary)
        for index, (price, volume, auxiliary) in enumerate(
            ((10.02, 300, 5), (10.03, 400, 6))
        ):
            struct.pack_into("<d", payload, 16016 + index * 8, price)
            struct.pack_into("<I", payload, 24016 + index * 4, volume)
            struct.pack_into("<H", payload, 28016 + index * 4, auxiliary)

        snapshot = l2.parse_sdk_callback_binary(1803, bytes(payload), limit=3)
        self.assertEqual((snapshot.header_u32_0, snapshot.header_u32_1), (7, 8))
        self.assertEqual((snapshot.first_side_count, snapshot.second_side_count), (2, 2))
        self.assertEqual([item.volume_raw for item in snapshot.first_side_levels], [100, 200])
        self.assertEqual([item.price for item in snapshot.second_side_levels], [10.02])
        self.assertEqual(snapshot.second_side_levels[0].auxiliary_raw, 5)

    def test_parse_18031_selected_price_queue(self) -> None:
        payload = bytearray(l2.SDK_ORDER_QUEUE_SIZE)
        struct.pack_into("<III4I", payload, 0, 11, 12, 4, 100, 250, 300, 450)
        snapshot = l2.parse_sdk_callback_binary(18031, bytes(payload), limit=3)
        self.assertEqual((snapshot.header_u32_0, snapshot.header_u32_1), (11, 12))
        self.assertEqual(snapshot.count, 4)
        self.assertEqual(snapshot.quantities_raw, (100, 250, 300))

    def test_binary_callback_rejects_short_or_invalid_counts(self) -> None:
        with self.assertRaises(l2.Level2Error):
            l2.parse_sdk_callback_binary(1803, bytes(100))
        payload = bytearray(l2.SDK_ORDER_QUEUE_SIZE)
        struct.pack_into("<III", payload, 0, 0, 0, 5001)
        with self.assertRaises(l2.Level2Error):
            l2.parse_sdk_callback_binary(18031, bytes(payload))


class TpbusPushParserTests(unittest.TestCase):
    def test_parse_111_quote_and_depth_levels(self) -> None:
        payload = bytearray(l2.TPBUS_QUOTE_HEADER_SIZE + 2 * l2.TPBUS_QUOTE_LEVEL_SIZE)
        struct.pack_into("<H", payload, 0, 1)
        payload[2:24] = b"600000" + bytes(16)
        struct.pack_into("<b", payload, 24, 2)
        struct.pack_into("<II", payload, 35, 9300100, 77)
        struct.pack_into("<6f", payload, 43, 9.9, 10.0, 10.5, 9.8, 10.2, 0.3)
        struct.pack_into("<II", payload, 67, 1234, 56)
        struct.pack_into("<fIf", payload, 75, 9876.5, 321, 4.5)
        payload[87:90] = bytes((1, 2, 100))
        struct.pack_into("<f", payload, 90, 6.5)
        struct.pack_into("<fIHfIH", payload, 99, 10.1, 100, 3, 10.2, 200, 4)
        struct.pack_into("<fIHfIH", payload, 119, 10.0, 300, 5, 10.3, 400, 6)

        push = l2.parse_tpbus_push_binary(111, bytes(payload), limit=1)
        self.assertEqual((push.market_id, push.code, push.depth_count), (1, "600000", 2))
        self.assertEqual((push.hq_time_raw, push.item_number), (9300100, 77))
        self.assertAlmostEqual(push.last, 10.2, places=5)
        self.assertEqual(len(push.levels), 1)
        self.assertEqual((push.levels[0].buy_volume_raw, push.levels[0].sell_seat_count), (100, 4))

    def test_parse_112_best_bid_and_ask_queues(self) -> None:
        payload = bytearray(l2.TPBUS_QUEUE_HEADER_SIZE + 3 * 4)
        struct.pack_into("<H", payload, 0, 0)
        payload[2:24] = b"000001" + bytes(16)
        struct.pack_into("<IffII", payload, 24, 8, 10.01, 10.02, 2, 1)
        struct.pack_into("<3I", payload, 54, 100, 250, 300)

        push = l2.parse_tpbus_push_binary(112, bytes(payload), limit=2)
        self.assertEqual((push.market_id, push.code, push.refresh_number), (0, "000001", 8))
        self.assertAlmostEqual(push.buy_price, 10.01, places=5)
        self.assertEqual((push.buy_count, push.sell_count), (2, 1))
        self.assertEqual(push.buy_quantities_raw, (100,))
        self.assertEqual(push.sell_quantities_raw, (300,))

    def test_tpbus_push_rejects_truncated_body(self) -> None:
        payload = bytearray(l2.TPBUS_QUEUE_HEADER_SIZE)
        struct.pack_into("<IffII", payload, 24, 0, 1.0, 1.1, 1, 0)
        with self.assertRaises(l2.Level2Error):
            l2.parse_tpbus_push_binary(112, bytes(payload))


class ProtobufWireInspectorTests(unittest.TestCase):
    def test_reports_confirmed_wire_structure_without_schema_names(self) -> None:
        payload = (
            b"\x08\x96\x01"
            b"\x12\x03ABC"
            b"\x1d\x78\x56\x34\x12"
            b"\x21\x08\x07\x06\x05\x04\x03\x02\x01"
        )
        result = l2.inspect_protobuf_wire(payload)
        self.assertEqual(result["field_count_reported"], 4)
        self.assertEqual(result["fields"][0]["value_unsigned"], 150)
        self.assertEqual(result["fields"][1]["text_candidate"], "ABC")
        self.assertEqual(result["fields"][2]["wire_type_name"], "fixed32")
        self.assertEqual(result["fields"][3]["value_unsigned"], 0x0102030405060708)
        self.assertNotIn("field_name", result["fields"][0])

    def test_reports_bounded_length_sample_and_field_limit(self) -> None:
        result = l2.inspect_protobuf_wire(
            b"\x0a\x05abcde\x10\x01", max_fields=1, max_sample_bytes=2
        )
        self.assertEqual(result["fields"][0]["sample_hex"], "6162")
        self.assertTrue(result["fields"][0]["sample_truncated"])
        self.assertTrue(result["field_limit_reached"])

    def test_rejects_truncated_or_invalid_wire_payload(self) -> None:
        with self.assertRaises(l2.Level2Error):
            l2.inspect_protobuf_wire(b"\x0a\x05ab")
        with self.assertRaises(l2.Level2Error):
            l2.inspect_protobuf_wire(b"\x0b")


class ProbeSummaryTests(unittest.TestCase):
    def test_correlates_label_lx_data_type_and_push(self) -> None:
        events = [
            {
                "event": "fasthq-subscribe",
                "capture_label": "order_queue",
                "code": "600000",
                "lx": 32,
                "operation": "subscribe",
            },
            {
                "event": "sdk-callback",
                "capture_label": "order_queue",
                "code": "600000",
                "data_type": 18031,
            },
        ]
        events.extend(
            {
                "event": "fasthq-push",
                "capture_label": "order_queue",
                "code": "600000",
                "push_type": 112,
                "recent_subscriptions": [{"lx": 32, "age_ms": age}],
            }
            for age in (100, 200, 300)
        )
        summary = l2.summarize_probe_events(events)
        mapping = summary["mappings"]["lx_to_push_type"][0]
        self.assertEqual(mapping["lx"], 32)
        self.assertEqual(mapping["push_type"], 112)
        self.assertEqual(mapping["confidence"], "high")
        sdk_mapping = summary["mappings"]["lx_to_sdk_data_type"][0]
        self.assertEqual(sdk_mapping["data_type"], 18031)
        self.assertEqual(sdk_mapping["data_type_name"], "order_queue_at_price")
        self.assertEqual(sdk_mapping["confidence"], "contextual")

    def test_probe_file_summary_reports_invalid_lines(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.jsonl"
            capture.write_text(
                '{"event":"ready","capture_label":"depth"}\nnot-json\n',
                encoding="utf-8",
            )
            summary = l2.summarize_probe_files([capture])
            self.assertEqual(summary["events_total"], 1)
            self.assertEqual(summary["invalid_line_count"], 1)
            self.assertIn("depth", summary["labels"])

    def test_counts_opaque_protobuf_push_types_without_claiming_code_mapping(self) -> None:
        summary = l2.summarize_probe_events([
            {
                "event": "fasthq-protobuf-push",
                "capture_label": "unknown_pb",
                "push_type": 113,
                "recent_subscriptions": [{"code": "600000", "lx": 32}],
            }
        ])
        item = summary["labels"]["unknown_pb"]["protobuf_push_types"][0]
        self.assertEqual(item["push_type"], 113)
        self.assertEqual(item["name"], "opaque_protobuf_113")
        self.assertEqual(summary["mappings"]["lx_to_push_type"], [])


class CliTests(unittest.TestCase):
    def test_build_direct_hex_cli(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "request.hex"
            result = l2.main([
                "build-direct",
                "--kind", "order",
                "--market", "0",
                "--code", "000001",
                "--count", "1",
                "--output", str(output),
            ])
            self.assertEqual(result, 0)
            self.assertEqual(len(bytes.fromhex(output.read_text("utf-8"))), 26)

    def test_parse_tpbus_push_cli(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "push-112.bin"
            output = Path(temporary) / "push-112.json"
            payload = bytearray(l2.TPBUS_QUEUE_HEADER_SIZE + 4)
            payload[2:24] = b"600000" + bytes(16)
            struct.pack_into("<IffII", payload, 24, 1, 10.0, 10.1, 1, 0)
            struct.pack_into("<I", payload, 54, 500)
            source.write_bytes(payload)
            result = l2.main([
                "parse-tpbus-push",
                "--push-type", "112",
                "--input", str(source),
                "--output", str(output),
            ])
            self.assertEqual(result, 0)
            document = json.loads(output.read_text("utf-8"))
            self.assertEqual(document["code"], "600000")
            self.assertEqual(document["buy_quantities_raw"], [500])

    def test_inspect_protobuf_cli(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "push-113.hex"
            output = Path(temporary) / "push-113.json"
            source.write_text("08011206363030303030", encoding="ascii")
            result = l2.main([
                "inspect-protobuf",
                "--input", str(source),
                "--encoding", "hex",
                "--output", str(output),
            ])
            self.assertEqual(result, 0)
            document = json.loads(output.read_text("utf-8"))
            self.assertEqual(document["fields"][0]["field_number"], 1)
            self.assertEqual(document["fields"][1]["text_candidate"], "600000")


if __name__ == "__main__":
    unittest.main()

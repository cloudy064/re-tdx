from __future__ import annotations

import struct
import tempfile
import unittest
import zlib
from pathlib import Path

import download_tdx_minute as downloader
import extract_tdx_minute as local


def encode_date(year: int, month: int, day: int) -> int:
    return (year - 2004) * 2048 + month * 100 + day


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


def make_record(
    minute: int,
    open_delta: int,
    close_delta: int,
    high_delta: int,
    low_delta: int,
    *,
    index_mode: bool = True,
    up: int = 22,
    down: int = 15,
) -> bytes:
    output = bytearray(
        struct.pack("<HH", encode_date(2026, 7, 31), minute)
    )
    for value in (open_delta, close_delta, high_delta, low_delta):
        output.extend(encode_varint(value))
    output.extend(struct.pack("<II", 0, 0))
    if index_mode:
        output.extend(struct.pack("<HH", up, down))
    return bytes(output)


class FakeSocket:
    def __init__(self, data: bytes, chunk_size: int = 7) -> None:
        self.data = data
        self.chunk_size = chunk_size

    def recv(self, size: int) -> bytes:
        if not self.data:
            return b""
        take = min(size, self.chunk_size)
        result = self.data[:take]
        self.data = self.data[take:]
        return result


class ProtocolTests(unittest.TestCase):
    def test_builds_current_long_kline_request(self) -> None:
        data = downloader.build_kline_request_data(1, "880471", 0, 800)
        frame = downloader.build_request_frame(
            0x01640801,
            downloader.TYPE_KLINES,
            data,
        )
        self.assertEqual(len(frame), 54)
        self.assertEqual(
            frame[:20].hex(" "),
            "0c 01 08 64 01 01 2c 00 2c 00 2d 05 "
            "01 00 38 38 30 34 37 31",
        )
        self.assertEqual(frame[20:28].hex(" "), "07 00 01 00 00 00 20 03")

    def test_reads_and_decompresses_response(self) -> None:
        payload = b"minute-bars" * 12
        compressed = zlib.compress(payload)
        header = downloader.RESPONSE_HEADER.pack(
            downloader.RESPONSE_PREFIX,
            12,
            0x01640802,
            0,
            downloader.TYPE_KLINES,
            len(compressed),
            len(payload),
        )
        response = downloader.read_response(FakeSocket(header + compressed))
        self.assertEqual(response.message_id, 0x01640802)
        self.assertEqual(response.message_type, downloader.TYPE_KLINES)
        self.assertEqual(response.data, payload)

    def test_parses_index_bars_and_previous_close_base(self) -> None:
        first = make_record(571, 3_500_000, 100, 200, -50)
        second = make_record(572, 10, -20, 40, -30, up=21, down=16)
        bars = downloader.parse_kline_payload(
            struct.pack("<H", 2) + first + second,
            index_mode=True,
        )
        self.assertEqual(len(bars), 2)
        self.assertEqual((bars[0].date, bars[0].time), (20260731, "09:31"))
        self.assertEqual(
            (bars[0].open, bars[0].close, bars[0].high, bars[0].low),
            (3500.0, 3500.1, 3500.2, 3499.95),
        )
        self.assertEqual((bars[0].extra_1, bars[0].extra_2), (22, 15))
        self.assertEqual(bars[1].open, 3500.11)
        self.assertEqual(bars[1].close, 3500.09)
        self.assertEqual((bars[1].extra_1, bars[1].extra_2), (21, 16))

    def test_parses_stock_bar_without_index_breadth(self) -> None:
        record = make_record(
            571,
            10_000,
            20,
            30,
            -10,
            index_mode=False,
        )
        bars = downloader.parse_kline_payload(
            struct.pack("<H", 1) + record,
            index_mode=False,
        )
        self.assertEqual(len(bars), 1)
        self.assertEqual((bars[0].extra_1, bars[0].extra_2), (0, 0))

    def test_rejects_legacy_two_byte_error_payload(self) -> None:
        with self.assertRaisesRegex(
            downloader.DownloadError,
            "不兼容的短请求格式",
        ):
            downloader.parse_kline_payload(b"\x20\x03", index_mode=True)


class StorageAndConfigTests(unittest.TestCase):
    def test_loads_only_hq_hosts(self) -> None:
        content = """
[HQHOST]
HostName01=深圳主站
IPAddress01=110.41.147.114
Port01=7709
[EXHOST]
HostName01=扩展主站
IPAddress01=1.2.3.4
Port01=7721
""".strip()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "newhost.lst"
            path.write_bytes(content.encode("gb18030"))
            endpoints = downloader.load_hq_hosts(path)
        self.assertEqual(
            endpoints,
            (
                downloader.HostEndpoint(
                    "110.41.147.114",
                    7709,
                    "深圳主站",
                ),
            ),
        )

    def test_downloaded_bar_overrides_existing_and_round_trips_lc1(self) -> None:
        existing = local.MinuteBar(
            date=20260731,
            time="09:31",
            minute=571,
            open=10.0,
            high=10.2,
            low=9.9,
            close=10.1,
            amount=1000.0,
            volume=10,
            extra_1=1,
            extra_2=2,
        )
        downloaded = local.MinuteBar(
            date=20260731,
            time="09:31",
            minute=571,
            open=11.0,
            high=11.2,
            low=10.9,
            close=11.1,
            amount=2000.0,
            volume=20,
            extra_1=3,
            extra_2=4,
        )
        merged = downloader.merge_bars((existing,), (downloaded,))
        self.assertEqual(merged, (downloaded,))
        decoded = local.parse_lc1(downloader.pack_lc1(merged))
        self.assertEqual(len(decoded), 1)
        self.assertAlmostEqual(decoded[0].close, 11.1, places=5)
        self.assertEqual((decoded[0].extra_1, decoded[0].extra_2), (3, 4))


if __name__ == "__main__":
    unittest.main()

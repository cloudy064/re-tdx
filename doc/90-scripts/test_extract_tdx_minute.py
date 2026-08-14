from __future__ import annotations

import struct
import tempfile
import unittest
from pathlib import Path

import extract_tdx_minute as minute


def encoded_date(year: int, month: int, day: int) -> int:
    return (year - 2004) * 2048 + month * 100 + day


class MinuteDataTest(unittest.TestCase):
    def make_record(
        self,
        *,
        year: int = 2026,
        month: int = 7,
        day: int = 31,
        clock: int = 571,
        open_price: float = 10.0,
        high_price: float = 10.5,
        low_price: float = 9.8,
        close_price: float = 10.2,
        amount: float = 12345.0,
        volume: int = 678,
        extra_1: int = 7,
        extra_2: int = 3,
    ) -> bytes:
        return minute.RECORD.pack(
            encoded_date(year, month, day),
            clock,
            open_price,
            high_price,
            low_price,
            close_price,
            amount,
            volume,
            extra_1,
            extra_2,
        )

    def test_parse_lc1_record(self) -> None:
        bars = minute.parse_lc1(self.make_record())
        self.assertEqual(len(bars), 1)
        self.assertEqual(bars[0].date, 20260731)
        self.assertEqual(bars[0].time, "09:31")
        self.assertAlmostEqual(bars[0].close, 10.2, places=5)
        self.assertEqual((bars[0].extra_1, bars[0].extra_2), (7, 3))

    def test_rejects_partial_record(self) -> None:
        with self.assertRaisesRegex(minute.MinuteDataError, "整数倍"):
            minute.parse_lc1(self.make_record() + b"\0")

    def test_rejects_non_finite_price(self) -> None:
        with self.assertRaisesRegex(minute.MinuteDataError, "非有限"):
            minute.parse_lc1(self.make_record(open_price=float("nan")))

    def test_resolves_board_market_and_name(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "T0002" / "hq_cache"
            source = root / "vipdoc" / "sh" / "minline"
            cache.mkdir(parents=True)
            source.mkdir(parents=True)
            (cache / "tdxzs3.cfg").write_bytes(
                "银行|880471|2|1|1|T1001\n".encode("gb18030")
            )
            (cache / "tdxzsbase.cfg").write_text(
                "1|880471|0\n", encoding="ascii"
            )
            path = source / "sh880471.lc1"
            path.write_bytes(self.make_record())

            series = minute.load_series(root, "880471")

            self.assertEqual(series.name, "银行")
            self.assertEqual(series.market, "sh")
            self.assertEqual(series.source, path)

    def test_explicit_market_is_strict(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "vipdoc" / "sh" / "minline"
            (root / "T0002" / "hq_cache").mkdir(parents=True)
            source.mkdir(parents=True)
            (source / "sh880471.lc1").write_bytes(self.make_record())
            with self.assertRaisesRegex(minute.MinuteDataError, "未找到"):
                minute.load_series(root, "880471", market="sz")

    def test_selects_latest_day(self) -> None:
        data = self.make_record(day=30) + self.make_record(day=31)
        bars = minute.parse_lc1(data)
        selected = minute.select_bars(bars, "latest")
        self.assertEqual([bar.date for bar in selected], [20260731])

    def test_html_is_self_contained(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "sh880471.lc1"
            path.write_bytes(self.make_record())
            series = minute.MinuteSeries(
                code="880471",
                name="银行",
                market="sh",
                source=path,
                bars=minute.parse_lc1(path.read_bytes()),
            )
            rendered = minute.render_html(series, series.bars)
            self.assertIn("银行 880471", rendered)
            self.assertIn("<canvas", rendered)
            self.assertNotIn("https://", rendered)


if __name__ == "__main__":
    unittest.main()

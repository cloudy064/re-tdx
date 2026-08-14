from __future__ import annotations

import struct
import unittest
from io import BytesIO
from zipfile import ZipFile

import tdx_stats as stats


def stat_line(code: str = "000001", market_id: int = 0) -> str:
    parts = [""] * 35
    parts[0] = str(market_id)
    parts[1] = code
    parts[2] = "1.25"
    parts[3] = "8.75"
    parts[4] = "20260730"
    parts[11] = "200.00"
    parts[26] = "9"
    parts[31] = "7"
    parts[32] = "5"
    parts[33] = "3"
    return "|".join(parts)


def stat2_line(code: str = "000001", market_id: int = 0) -> str:
    parts = [""] * 21
    parts[0] = str(market_id)
    parts[1] = code
    parts[2] = "20260730"
    parts[3] = "242501.84"
    parts[5] = "254664.58"
    parts[6] = "851.80"
    parts[7] = "134819.47"
    parts[8] = "3468.47"
    parts[9] = "11141"
    parts[10] = "9513"
    parts[14] = "6002.06"
    parts[15] = "4291.39"
    return "|".join(parts)


def archive(stat_lines: list[str], stat2_lines: list[str]) -> bytes:
    output = BytesIO()
    with ZipFile(output, "w") as value:
        value.writestr("tdxstat.cfg", "\n".join(stat_lines).encode("gbk"))
        value.writestr("tdxstat2.cfg", "\n".join(stat2_lines).encode("gbk"))
    return output.getvalue()


class StatsTests(unittest.TestCase):
    def test_builds_exact_06b9_request(self) -> None:
        request = stats.build_file_request_data("zhb.zip", offset=123, size=30000)
        self.assertEqual(len(request), 308)
        self.assertEqual(struct.unpack_from("<II", request), (123, 30000))
        self.assertEqual(request[8:15], b"zhb.zip")
        self.assertEqual(request[15:], b"\x00" * 293)

    def test_parses_file_chunk_and_rejects_length_mismatch(self) -> None:
        self.assertEqual(
            stats.parse_file_chunk(struct.pack("<I", 3) + b"abc", request_size=4),
            b"abc",
        )
        with self.assertRaises(stats.StatsError):
            stats.parse_file_chunk(struct.pack("<I", 3) + b"ab", request_size=4)

    def test_parses_stats_archive_columns(self) -> None:
        resource = stats.parse_stats_archive(archive(
            [stat_line(), stat_line("600000", 1)],
            [stat2_line(), stat2_line("600000", 1)],
        ))
        self.assertEqual(len(resource.stat), 2)
        self.assertEqual(len(resource.stat2), 2)
        self.assertEqual(resource.stats_date, "20260730")
        self.assertEqual(resource.stats_date_coverage, 1.0)
        row = resource.stat[(0, "000001")]
        row2 = resource.stat2[(0, "000001")]
        self.assertAlmostEqual(row.beta_60d or 0, 1.25)
        self.assertAlmostEqual(row.free_float_shares_10k or 0, 200.0)
        self.assertEqual(row.limit_up_streak_days, 3)
        self.assertAlmostEqual(row2.amount_10k or 0, 242501.84)
        self.assertIsNone(row2.seal_amount_10k)
        self.assertAlmostEqual(row2.prev_seal_amount_10k or 0, 851.80)
        self.assertAlmostEqual(row2.open_amount_10k or 0, 6002.06)

    def test_rejects_missing_or_duplicate_members(self) -> None:
        output = BytesIO()
        with ZipFile(output, "w") as value:
            value.writestr("tdxstat.cfg", stat_line())
        with self.assertRaises(stats.StatsError):
            stats.parse_stats_archive(output.getvalue())

        with self.assertRaises(stats.StatsError):
            stats.parse_stats_archive(archive(
                [stat_line(), stat_line()], [stat2_line()],
            ))


if __name__ == "__main__":
    unittest.main()

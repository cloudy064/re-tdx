from __future__ import annotations

import unittest

import extract_tdx_blocks as blocks
import tdx_market_depth as depth
import tdx_stats as stats
import update_tdx_limit_quality as quality


def quote(code: str, *, sealed: bool) -> depth.MarketDepth:
    buy = (depth.QuoteLevel(11.0 if sealed else 10.99, 100),)
    sell = (depth.QuoteLevel(0.0, 0),) if sealed else (depth.QuoteLevel(11.0, 20),)
    return depth.MarketDepth(
        0, code, 1, 11.0, 10.0, 10.1, 11.0, 10.0,
        150000, 0, 1000, 10, 10000.0, 400, 600, 0, 1000.0,
        buy, sell, "",
    )


class LimitQualityTests(unittest.TestCase):
    def test_builds_current_seals_ladders_and_industry_links(self) -> None:
        event_rows = [
            {"$SC": "0", "$ZQDM": "000001", "ztrq": "20260801", "yy": "A+B", "lbts": "3"},
            {"$SC": "0", "$ZQDM": "000002", "ztrq": "20260801", "yy": "C", "lbts": "1"},
        ]
        daily_rows = [{"$ZQDM": "20260801", "ztjs2": "1", "ztjs3": "1"}]
        industries = [{
            "$SC": "1", "$ZQDM": "881001", "mc": "煤炭", "rq": "20260801",
            "ztjs": "1", "zbs": "1", "zrzt": "0", "lbs": "1",
            "lbgd": "3", "hzgd": "3", "lbjjl": "50.0",
        }]
        master = {
            (0, "000001"): blocks.Security(0, "SZ", "深圳", "000001", "甲"),
            (0, "000002"): blocks.Security(0, "SZ", "深圳", "000002", "乙"),
        }
        assignments = {
            (0, "000001"): [{
                "code": "881001", "name": "煤炭",
                "family": "research-industry", "family_name": "研究行业",
                "level": 2, "is_leaf": True,
            }],
        }
        model = quality.build_model(
            event_rows,
            daily_rows,
            industries,
            [quote("000001", sealed=True), quote("000002", sealed=False)],
            master,
            assignments,
            generated_at="2026-08-01T12:00:00+08:00",
        )
        self.assertEqual(model["counts"]["sealed"], 1)
        self.assertEqual(model["counts"]["opened"], 1)
        self.assertTrue(model["counts"]["sealed_count_matches_native"])
        self.assertEqual(model["ladders"][0]["consecutive_boards"], 3)
        self.assertEqual(model["block_ladders"][0]["linked_sealed_count"], 1)
        self.assertEqual(model["block_ladders"][0]["family"], "research-industry")
        self.assertAlmostEqual(
            model["current_depth_aggregate"]["total_bid1_amount_yuan"],
            110_000.0,
        )

    def test_seal_requires_last_price_bid_and_empty_ask(self) -> None:
        self.assertTrue(quality.is_sealed(quote("000001", sealed=True)))
        self.assertFalse(quality.is_sealed(quote("000001", sealed=False)))
        self.assertFalse(quality.is_sealed(None))

    def test_aligns_same_day_stats_and_computes_seal_quality(self) -> None:
        stat = stats.TdxStatRow(
            0, "000001", "20260801", 1.2, 8.0, 1.0,
            3, 5, 2, 1,
        )
        stat2 = stats.TdxStat2Row(
            0, "000001", "20260801",
            100.0, 11.0, 80.0, 5.0, 70.0, 2.5,
            1000.0, 900.0, 10.0, 9.0,
        )
        resource = stats.TdxStatsResource(
            {(0, "000001"): stat},
            {(0, "000001"): stat2},
            "test://zhb.zip",
        )
        metrics = quality.build_stats_metrics(
            (0, "000001"),
            quote("000001", sealed=True),
            True,
            resource,
            target_date="20260801",
        )
        self.assertEqual(metrics["alignment_status"], "same-day")
        self.assertAlmostEqual(metrics["current_seal_amount_yuan"], 110_000.0)
        self.assertAlmostEqual(metrics["previous_seal_amount_yuan"], 50_000.0)
        self.assertAlmostEqual(metrics["seal_to_float_ratio_pct"], 100.0)
        self.assertAlmostEqual(metrics["seal_prev_ratio"], 2.2)
        self.assertAlmostEqual(metrics["seal_decay_pct"], -120.0)
        self.assertTrue(metrics["stats_vs_depth_matches_100_yuan"])

    def test_shifts_previous_resource_day_once_and_ignores_negative_seal(self) -> None:
        row = stats.TdxStat2Row(
            0, "000001", "20260731",
            100.0, -5.0, 80.0, 3.0, 70.0, 2.0,
            1000.0, 900.0, 10.0, 9.0,
        )
        aligned = quality.align_stat2(row, target_date="20260801")
        self.assertEqual(aligned["status"], "previous-resource-day")
        self.assertAlmostEqual(aligned["previous_amount_yuan"], 1_000_000.0)
        self.assertAlmostEqual(aligned["previous_seal_amount_yuan"], -50_000.0)
        metrics = quality.build_stats_metrics(
            (0, "000001"), quote("000001", sealed=True), True,
            stats.TdxStatsResource({}, {(0, "000001"): row}, "test"),
            target_date="20260801",
        )
        self.assertIsNone(metrics["seal_prev_ratio"])
        self.assertFalse(metrics["previous_seal_was_positive"])


if __name__ == "__main__":
    unittest.main()

from __future__ import annotations

import unittest

import tdx_auction_series as auctions
import tdx_category_quotes as categories
import tdx_stats as stats
import tdx_trades as trades
import update_tdx_auction_quality as quality


def category_quote() -> categories.CategoryQuote:
    return categories.CategoryQuote(
        market_id=0,
        code="000001",
        active1=1,
        active2=2,
        last_price=12.0,
        pre_close_price=10.0,
        open_price=11.0,
        high_price=12.0,
        low_price=9.8,
        server_time_raw=150000,
        neg_price_raw=0,
        total_hand=1000,
        current_hand=10,
        amount=1_000_000.0,
        amount_raw=0,
        inside_dish=400,
        outer_disc=600,
        after_outer_raw=0,
        open_amount_yuan=220_000.0,
        bid1_price=12.0,
        ask1_price=12.01,
        bid1_volume_hand=100,
        ask1_volume_hand=200,
        status_or_sort_raw=0,
        rise_speed=0.0,
        short_turnover=0.0,
        two_minute_amount=0.0,
        opening_rush=10.0,
        volume_rise_speed=0.0,
        depth=0.0,
        extra_pair_hex="",
        extra_meta_hex="",
    )


def point(
    index: int,
    minute: int,
    second: int,
    price: float,
    matched: int,
    unmatched: int,
) -> auctions.AuctionPoint:
    direction = 1 if unmatched > 0 else -1 if unmatched < 0 else 0
    return auctions.AuctionPoint(
        index=index,
        minute_of_day_raw=minute,
        second_raw=second,
        time_label=auctions.minute_label(minute, second),
        time_seconds=minute * 60 + second,
        price=price,
        matched_volume_hand=matched,
        unmatched_signed_hand=unmatched,
        unmatched_volume_hand=abs(unmatched),
        unmatched_direction_raw=direction,
        unmatched_direction=(
            "buy" if direction > 0 else "sell" if direction < 0 else "balanced"
        ),
        reserved_zero_0e=0,
    )


def series() -> auctions.AuctionSeries:
    return auctions.AuctionSeries(
        market_id=0,
        code="000001",
        selector=3,
        start_raw=0,
        limit=500,
        points=(
            point(0, 555, 0, 9.8, 80, -20),
            point(1, 564, 57, 10.0, 100, 50),
            point(2, 897, 0, 11.9, 20, -10),
            point(3, 899, 51, 12.0, 30, 5),
        ),
    )


def resource() -> stats.TdxStatsResource:
    stat = stats.TdxStatRow(
        0, "000001", "20260801", 1.0, 10.0, 10.0,
        2, 5, 2, 1,
    )
    stat2 = stats.TdxStat2Row(
        0, "000001", "20260801",
        100.0, None, 80.0, None, 70.0, None,
        200.0, 100.0, 22.0, 10.0,
    )
    return stats.TdxStatsResource(
        {(0, "000001"): stat},
        {(0, "000001"): stat2},
        "test://zhb.zip",
    )


def trade_tick(
    index: int,
    minute: int,
    price: float,
    volume: int,
    status: int,
) -> trades.TradeTick:
    raw = round(price * 100)
    return trades.TradeTick(
        index=index,
        absolute_index=index,
        time_minutes=minute,
        time_label=f"{minute // 60:02d}:{minute % 60:02d}",
        price=price,
        volume_hand=volume,
        order_count=3,
        status_raw=status,
        side={0: "buy", 1: "sell", 2: "neutral"}.get(status, f"status_{status}"),
        price_delta_raw=raw,
        price_acc_raw=raw,
        tail_raw=0,
    )


def trade_series() -> trades.TradeSeries:
    return trades.TradeSeries(
        market_id=0,
        code="000001",
        trading_date="20260801",
        source_mode="history",
        pages=1,
        page_size=1800,
        price_divisor=100,
        price_base_raw=10.0,
        ticks=(
            trade_tick(2, 565, 11.0, 200, 2),
            trade_tick(1, 900, 12.0, 40, 2),
            trade_tick(0, 905, 12.0, 5, 5),
        ),
    )


class AuctionQualityTests(unittest.TestCase):
    def test_recovers_opening_rush_and_joins_stats(self) -> None:
        item = build = quality.build_security_record(
            category_quote(),
            series(),
            resource().stat[(0, "000001")],
            resource().stat2[(0, "000001")],
            target_date="20260801",
            name="甲",
        )
        self.assertEqual(build["opening_series"]["last_sample_time"], "09:24:57")
        self.assertAlmostEqual(build["opening_series"]["derived_opening_rush_pct"], 10.0)
        self.assertTrue(build["opening_series"]["native_formula_matches_0_02_pct"])
        self.assertIsNone(build["opening_series"]["official_open_volume_hand"])
        self.assertEqual(
            build["opening_series"]["auction_reference_volume_hand"], 200.0
        )
        self.assertEqual(
            build["opening_series"]["opening_execution_status"], "no-0925-trade"
        )
        self.assertEqual(build["closing_series"]["last_sample_time"], "14:59:51")
        self.assertTrue(build["closing_series"]["official_price_unchanged_0_001"])
        self.assertAlmostEqual(build["closing_series"]["derived_closing_rush_pct"], 0.0)
        self.assertAlmostEqual(build["stats"]["open_prev_amount_ratio"], 2.2)
        self.assertAlmostEqual(build["stats"]["auction_prev_volume_ratio"], 2.0)
        self.assertAlmostEqual(build["stats"]["open_turnover_pct"], 20.0)
        self.assertEqual(item["points"][1]["unmatched_direction"], "buy")

    def test_builds_consistency_counts(self) -> None:
        model = quality.build_model(
            [category_quote()],
            [series()],
            resource(),
            {},
            [trade_series()],
            target_date="20260801",
            generated_at="2026-08-02T12:00:00+08:00",
        )
        self.assertEqual(model["counts"]["ranked"], 1)
        self.assertEqual(model["counts"]["opening_rush_formula_matches"], 1)
        self.assertEqual(model["counts"]["open_amount_matches_stats"], 1)
        self.assertEqual(model["counts"]["open_volume_matches_stats"], 1)
        self.assertEqual(model["counts"]["closing_price_unchanged"], 1)
        self.assertEqual(model["counts"]["trade_series_received"], 1)
        self.assertEqual(model["counts"]["no_0925_trade"], 0)
        self.assertEqual(model["counts"]["trade_open_price_matches_native"], 1)
        self.assertEqual(model["counts"]["trade_open_amount_matches_native"], 1)
        self.assertEqual(
            model["counts"]["trade_open_amount_matches_native_1000_yuan"], 1
        )
        self.assertEqual(model["counts"]["trade_open_volume_matches_stats"], 1)
        self.assertEqual(model["counts"]["trade_close_price_matches_native"], 1)
        self.assertEqual(model["counts"]["post_close_status_5_securities"], 1)
        item = model["securities"][0]
        self.assertEqual(
            item["opening_series"]["opening_execution_status"], "executed-0925"
        )
        self.assertEqual(item["opening_series"]["official_open_volume_hand"], 200)
        self.assertEqual(
            item["closing_series"]["official_close_volume_hand"], 40
        )
        self.assertEqual(
            item["closing_series"]["official_close_amount_yuan"], 48_000.0
        )
        self.assertEqual(
            item["trade_details"]["post_close_status_5"]["volume_hand"], 5
        )

    def test_aligns_previous_resource_day_without_claiming_current_stats(self) -> None:
        row = stats.TdxStat2Row(
            0, "000001", "20260731",
            100.0, None, 80.0, None, 70.0, None,
            200.0, 100.0, 22.0, 10.0,
        )
        aligned = quality.align_open_stats(row, target_date="20260801")
        self.assertEqual(aligned["status"], "previous-resource-day")
        self.assertIsNone(aligned["current_open_amount_yuan"])
        self.assertEqual(aligned["previous_open_amount_yuan"], 220_000.0)
        self.assertEqual(aligned["previous_open_volume_hand"], 200.0)


if __name__ == "__main__":
    unittest.main()

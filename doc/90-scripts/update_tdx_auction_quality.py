#!/usr/bin/env python3
"""Join TDX opening-rush ranking, auction points, and historical auction stats."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Mapping, Sequence

import download_tdx_minute as transport
import extract_tdx_blocks as blocks
import tdx_auction_series as auctions
import tdx_category_quotes as categories
import tdx_market_depth as depth
import tdx_market_snapshot as snapshots
import tdx_stats as statistics
import tdx_trades as trades
import update_tdx_blocks as updater


class AuctionQualityError(RuntimeError):
    """Raised when auction ranking sources cannot be aligned safely."""


def _tenk(value: object) -> float | None:
    if value is None:
        return None
    return float(value) * 10_000.0


def _ratio(numerator: object, denominator: object) -> float | None:
    if numerator is None or denominator is None:
        return None
    right = float(denominator)
    if right <= 0:
        return None
    return float(numerator) / right


def align_open_stats(
    row: statistics.TdxStat2Row | None,
    *,
    target_date: str,
) -> dict[str, object]:
    empty = {
        "status": "stats-row-missing" if row is None else "stats-date-unaligned",
        "stats_date": row.stats_date if row else None,
        "current_open_volume_hand": None,
        "current_open_amount_yuan": None,
        "previous_open_volume_hand": None,
        "previous_open_amount_yuan": None,
    }
    if row is None or not row.stats_date:
        return empty
    try:
        target = datetime.strptime(target_date, "%Y%m%d").date()
        row_date = datetime.strptime(row.stats_date, "%Y%m%d").date()
    except ValueError:
        empty["status"] = "invalid-date"
        return empty
    if row_date == target:
        return {
            "status": "same-day",
            "stats_date": row.stats_date,
            "current_open_volume_hand": row.open_volume_hand,
            "current_open_amount_yuan": _tenk(row.open_amount_10k),
            "previous_open_volume_hand": row.prev_open_volume_hand,
            "previous_open_amount_yuan": _tenk(row.prev_open_amount_10k),
        }
    if 1 <= (target - row_date).days <= 7:
        return {
            "status": "previous-resource-day",
            "stats_date": row.stats_date,
            "current_open_volume_hand": None,
            "current_open_amount_yuan": None,
            "previous_open_volume_hand": row.open_volume_hand,
            "previous_open_amount_yuan": _tenk(row.open_amount_10k),
        }
    return empty


def _segment_points(
    series: auctions.AuctionSeries,
) -> tuple[tuple[auctions.AuctionPoint, ...], tuple[auctions.AuctionPoint, ...]]:
    opening = tuple(
        point for point in series.points if point.minute_of_day_raw < 12 * 60
    )
    closing = tuple(
        point for point in series.points if point.minute_of_day_raw >= 12 * 60
    )
    return opening, closing


def build_security_record(
    ranking: categories.CategoryQuote,
    series: auctions.AuctionSeries | None,
    stat: statistics.TdxStatRow | None,
    stat2: statistics.TdxStat2Row | None,
    trade_series: trades.TradeSeries | None = None,
    *,
    target_date: str,
    name: str = "",
) -> dict[str, object]:
    aligned = align_open_stats(stat2, target_date=target_date)
    opening, closing = _segment_points(series) if series else ((), ())
    last_preopen = opening[-1] if opening else None
    last_preclose = closing[-1] if closing else None
    trade_summary = (
        trades.aggregate_ticks(trade_series.ticks) if trade_series else None
    )
    opening_trade = trade_summary["auction_0925"] if trade_summary else None
    closing_trade = trade_summary["closing_1500"] if trade_summary else None
    post_close_trade = (
        trade_summary["post_close_status_5"] if trade_summary else None
    )
    if opening_trade and not opening_trade["tick_count"]:
        opening_trade = None
    if closing_trade and not closing_trade["tick_count"]:
        closing_trade = None
    if post_close_trade and not post_close_trade["tick_count"]:
        post_close_trade = None
    official_open_trade_price = (
        float(opening_trade["last_price"]) if opening_trade else None
    )
    official_open_trade_volume = (
        int(opening_trade["volume_hand"]) if opening_trade else None
    )
    official_open_trade_amount = (
        float(opening_trade["amount_yuan"]) if opening_trade else None
    )
    official_close_price = (
        float(closing_trade["last_price"]) if closing_trade else ranking.last_price
    )
    derived_rush = (
        (ranking.open_price / last_preopen.price - 1.0) * 100.0
        if last_preopen is not None and last_preopen.price > 0
        else None
    )
    rush_delta = (
        derived_rush - ranking.opening_rush
        if derived_rush is not None
        else None
    )
    inferred_open_volume = (
        ranking.open_amount_yuan / ranking.open_price / 100.0
        if ranking.open_price > 0
        else None
    )
    stats_open_volume = aligned["current_open_volume_hand"]
    auction_reference_volume = (
        float(official_open_trade_volume)
        if official_open_trade_volume is not None
        else float(stats_open_volume)
        if stats_open_volume is not None
        else float(last_preopen.matched_volume_hand)
        if last_preopen is not None
        else inferred_open_volume
        if inferred_open_volume is not None else None
    )
    auction_reference_amount = (
        official_open_trade_amount
        if official_open_trade_amount is not None
        else ranking.open_amount_yuan
    )
    stats_open_amount = aligned["current_open_amount_yuan"]
    amount_delta = (
        ranking.open_amount_yuan - float(stats_open_amount)
        if stats_open_amount is not None
        else None
    )
    volume_delta = (
        inferred_open_volume - float(stats_open_volume)
        if inferred_open_volume is not None and stats_open_volume is not None
        else None
    )
    last_virtual_amount = (
        last_preopen.matched_amount_yuan if last_preopen else None
    )
    late_volume = (
        official_open_trade_volume - last_preopen.matched_volume_hand
        if official_open_trade_volume is not None and last_preopen is not None
        else None
    )
    free_float_shares = _tenk(stat.free_float_shares_10k) if stat else None
    open_turnover_ratio = _ratio(
        auction_reference_volume * 100.0
        if auction_reference_volume is not None else None,
        free_float_shares,
    )
    previous_open_amount = aligned["previous_open_amount_yuan"]
    previous_open_volume = aligned["previous_open_volume_hand"]
    amount_prev_ratio = _ratio(auction_reference_amount, previous_open_amount)
    volume_prev_ratio = _ratio(auction_reference_volume, previous_open_volume)
    official_close_delta = (
        official_close_price - last_preclose.price if last_preclose else None
    )
    derived_closing_rush = (
        (official_close_price / last_preclose.price - 1.0) * 100.0
        if last_preclose is not None and last_preclose.price > 0
        else None
    )
    series_summary = auctions.summarize_series(series) if series else None
    return {
        "market_id": ranking.market_id,
        "code": ranking.code,
        "security_id": ranking.security_id,
        "name": name,
        "target_trade_date": target_date,
        "native": {
            "opening_rush_pct": ranking.opening_rush,
            "pre_close_price": ranking.pre_close_price,
            "official_open_price": ranking.open_price,
            "last_price": ranking.last_price,
            "open_auction_reference_amount_yuan": ranking.open_amount_yuan,
            "change_pct": ranking.change_pct,
        },
        "opening_series": {
            "point_count": len(opening),
            "first_time": opening[0].time_label if opening else None,
            "last_sample_time": last_preopen.time_label if last_preopen else None,
            "last_sample_price": last_preopen.price if last_preopen else None,
            "last_sample_matched_volume_hand": (
                last_preopen.matched_volume_hand if last_preopen else None
            ),
            "last_sample_matched_amount_yuan": last_virtual_amount,
            "last_sample_unmatched_signed_hand": (
                last_preopen.unmatched_signed_hand if last_preopen else None
            ),
            "last_sample_unmatched_direction": (
                last_preopen.unmatched_direction if last_preopen else None
            ),
            "derived_opening_rush_pct": derived_rush,
            "native_vs_derived_delta_pct": rush_delta,
            "native_formula_matches_0_02_pct": (
                abs(rush_delta) <= 0.02 if rush_delta is not None else None
            ),
            "opening_execution_status": (
                "executed-0925" if opening_trade else "no-0925-trade"
            ),
            "official_open_volume_hand": official_open_trade_volume,
            "official_open_amount_yuan": official_open_trade_amount,
            "auction_reference_volume_hand": auction_reference_volume,
            "auction_reference_amount_yuan": auction_reference_amount,
            "auction_reference_source": (
                "0x0FC5/0x0FC6@09:25"
                if official_open_trade_volume is not None
                else "tdxstat2"
                if stats_open_volume is not None
                else "0x056A-last-sample"
                if last_preopen is not None
                else "0x054B-open-amount/open-price"
                if inferred_open_volume is not None
                else None
            ),
            "official_open_price_from_trade": official_open_trade_price,
            "official_open_volume_hand_from_trade": official_open_trade_volume,
            "official_open_amount_yuan_from_trade": official_open_trade_amount,
            "trade_vs_native_open_price_delta": (
                official_open_trade_price - ranking.open_price
                if official_open_trade_price is not None else None
            ),
            "trade_vs_native_open_price_matches_0_001": (
                abs(official_open_trade_price - ranking.open_price) <= 0.001
                if official_open_trade_price is not None else None
            ),
            "trade_vs_native_open_amount_delta_yuan": (
                official_open_trade_amount - ranking.open_amount_yuan
                if official_open_trade_amount is not None else None
            ),
            "trade_vs_native_open_amount_matches_100_yuan": (
                abs(official_open_trade_amount - ranking.open_amount_yuan) <= 100.0
                if official_open_trade_amount is not None else None
            ),
            "trade_vs_native_open_amount_matches_1000_yuan": (
                abs(official_open_trade_amount - ranking.open_amount_yuan) <= 1000.0
                if official_open_trade_amount is not None else None
            ),
            "official_minus_last_sample_volume_hand": late_volume,
            "official_vs_last_sample_volume_ratio": _ratio(
                official_open_trade_volume,
                last_preopen.matched_volume_hand if last_preopen else None,
            ),
            "official_minus_last_virtual_amount_yuan": (
                official_open_trade_amount - last_virtual_amount
                if official_open_trade_amount is not None
                and last_virtual_amount is not None else None
            ),
        },
        "closing_series": {
            "point_count": len(closing),
            "first_time": closing[0].time_label if closing else None,
            "last_sample_time": last_preclose.time_label if last_preclose else None,
            "last_sample_price": last_preclose.price if last_preclose else None,
            "last_sample_matched_volume_hand": (
                last_preclose.matched_volume_hand if last_preclose else None
            ),
            "last_sample_matched_amount_yuan": (
                last_preclose.matched_amount_yuan if last_preclose else None
            ),
            "last_sample_unmatched_signed_hand": (
                last_preclose.unmatched_signed_hand if last_preclose else None
            ),
            "last_sample_unmatched_direction": (
                last_preclose.unmatched_direction if last_preclose else None
            ),
            "official_close_price_from_trade": (
                float(closing_trade["last_price"]) if closing_trade else None
            ),
            "official_close_volume_hand": (
                int(closing_trade["volume_hand"]) if closing_trade else None
            ),
            "official_close_amount_yuan": (
                float(closing_trade["amount_yuan"]) if closing_trade else None
            ),
            "official_close_order_count": (
                int(closing_trade["order_count"]) if closing_trade else None
            ),
            "trade_vs_native_close_price_delta": (
                float(closing_trade["last_price"]) - ranking.last_price
                if closing_trade else None
            ),
            "trade_vs_native_close_price_matches_0_001": (
                abs(float(closing_trade["last_price"]) - ranking.last_price) <= 0.001
                if closing_trade else None
            ),
            "official_minus_last_sample_volume_hand": (
                int(closing_trade["volume_hand"])
                - last_preclose.matched_volume_hand
                if closing_trade and last_preclose else None
            ),
            "official_minus_last_sample_amount_yuan": (
                float(closing_trade["amount_yuan"])
                - last_preclose.matched_amount_yuan
                if closing_trade and last_preclose else None
            ),
            "official_minus_last_sample_price": official_close_delta,
            "derived_closing_rush_pct": derived_closing_rush,
            "official_price_unchanged_0_001": (
                abs(official_close_delta) <= 0.001
                if official_close_delta is not None else None
            ),
        },
        "stats": {
            "stats_date": aligned["stats_date"],
            "alignment_status": aligned["status"],
            "stats_current_open_amount_yuan": stats_open_amount,
            "category_vs_stats_open_amount_delta_yuan": amount_delta,
            "category_vs_stats_open_amount_matches_100_yuan": (
                abs(amount_delta) <= 100.0 if amount_delta is not None else None
            ),
            "stats_current_open_volume_hand": stats_open_volume,
            "inferred_open_volume_hand": inferred_open_volume,
            "inferred_vs_stats_open_volume_delta_hand": volume_delta,
            "inferred_vs_stats_open_volume_matches_1_hand": (
                abs(volume_delta) <= 1.0 if volume_delta is not None else None
            ),
            "trade_vs_stats_open_volume_delta_hand": (
                official_open_trade_volume - float(stats_open_volume)
                if official_open_trade_volume is not None
                and stats_open_volume is not None else None
            ),
            "trade_vs_stats_open_volume_matches_1_hand": (
                abs(official_open_trade_volume - float(stats_open_volume)) <= 1.0
                if official_open_trade_volume is not None
                and stats_open_volume is not None else None
            ),
            "trade_vs_stats_open_amount_delta_yuan": (
                official_open_trade_amount - float(stats_open_amount)
                if official_open_trade_amount is not None
                and stats_open_amount is not None else None
            ),
            "trade_vs_stats_open_amount_matches_1000_yuan": (
                abs(official_open_trade_amount - float(stats_open_amount)) <= 1000.0
                if official_open_trade_amount is not None
                and stats_open_amount is not None else None
            ),
            "previous_open_amount_yuan": previous_open_amount,
            "previous_open_volume_hand": previous_open_volume,
            "open_prev_amount_ratio": amount_prev_ratio,
            "auction_prev_volume_ratio": volume_prev_ratio,
            "free_float_shares": free_float_shares,
            "open_turnover_pct": (
                open_turnover_ratio * 100.0
                if open_turnover_ratio is not None else None
            ),
        },
        "trade_details": {
            "source_mode": trade_series.source_mode if trade_series else None,
            "page_count": trade_series.pages if trade_series else 0,
            "tick_count": len(trade_series.ticks) if trade_series else 0,
            "price_divisor": trade_series.price_divisor if trade_series else None,
            "post_close_status_5_amount_yuan": (
                float(post_close_trade["amount_yuan"])
                if post_close_trade else None
            ),
            "post_close_status_5": post_close_trade,
        },
        "series_summary": series_summary,
        "points": [
            {
                "time": point.time_label,
                "price": point.price,
                "matched_volume_hand": point.matched_volume_hand,
                "matched_amount_yuan": point.matched_amount_yuan,
                "unmatched_signed_hand": point.unmatched_signed_hand,
                "unmatched_volume_hand": point.unmatched_volume_hand,
                "unmatched_direction": point.unmatched_direction,
            }
            for point in series.points
        ] if series else [],
    }


def build_model(
    ranking: Sequence[categories.CategoryQuote],
    series: Sequence[auctions.AuctionSeries],
    resource: statistics.TdxStatsResource,
    security_master: Mapping[tuple[int, str], blocks.Security],
    trade_series: Sequence[trades.TradeSeries] = (),
    *,
    target_date: str,
    category_endpoint: str = "",
    auction_endpoint: str = "",
    trade_endpoint: str = "",
    stats_endpoint: str = "",
    generated_at: str | None = None,
) -> dict[str, object]:
    by_series = {item.key: item for item in series}
    by_trades = {item.key: item for item in trade_series}
    records = [
        build_security_record(
            item,
            by_series.get(item.key),
            resource.stat.get(item.key),
            resource.stat2.get(item.key),
            by_trades.get(item.key),
            target_date=target_date,
            name=(security_master[item.key].name if item.key in security_master else ""),
        )
        for item in ranking
    ]

    def rank_row(item: Mapping[str, object]) -> dict[str, object]:
        return {
            "security_id": item["security_id"],
            "name": item["name"],
            "opening_rush_pct": item["native"]["opening_rush_pct"],
            "open_prev_amount_ratio": item["stats"]["open_prev_amount_ratio"],
            "auction_prev_volume_ratio": item["stats"]["auction_prev_volume_ratio"],
            "open_turnover_pct": item["stats"]["open_turnover_pct"],
            "official_vs_last_sample_volume_ratio": (
                item["opening_series"]["official_vs_last_sample_volume_ratio"]
            ),
            "derived_closing_rush_pct": (
                item["closing_series"]["derived_closing_rush_pct"]
            ),
            "closing_auction_volume_hand": (
                item["closing_series"]["official_close_volume_hand"]
            ),
            "closing_auction_amount_yuan": (
                item["closing_series"]["official_close_amount_yuan"]
            ),
            "post_close_status_5_amount_yuan": (
                item["trade_details"]["post_close_status_5"]["amount_yuan"]
                if item["trade_details"]["post_close_status_5"] else None
            ),
            "last_sample_unmatched_direction": (
                item["opening_series"]["last_sample_unmatched_direction"]
            ),
        }

    def ranked(path: tuple[str, str]) -> list[dict[str, object]]:
        left, right = path
        usable = [item for item in records if item[left][right] is not None]
        return [
            rank_row(item)
            for item in sorted(
                usable,
                key=lambda item: float(item[left][right]),
                reverse=True,
            )
        ]

    comparable_rush = [
        item for item in records
        if item["opening_series"]["native_vs_derived_delta_pct"] is not None
    ]
    comparable_amount = [
        item for item in records
        if item["stats"]["category_vs_stats_open_amount_delta_yuan"] is not None
    ]
    comparable_volume = [
        item for item in records
        if item["stats"]["inferred_vs_stats_open_volume_delta_hand"] is not None
    ]
    comparable_close = [
        item for item in records
        if item["closing_series"]["derived_closing_rush_pct"] is not None
    ]
    trade_open = [
        item for item in records
        if item["opening_series"]["official_open_price_from_trade"] is not None
    ]
    trade_close = [
        item for item in records
        if item["closing_series"]["official_close_price_from_trade"] is not None
    ]
    return {
        "schema": "tdx-auction-quality-v2",
        "generated_at": generated_at or datetime.now().astimezone().isoformat(),
        "target_trade_date": target_date,
        "source": {
            "category_command": "0x054B/0x010A",
            "auction_command": "0x056A/selector=3",
            "trade_command": "0x0FC6",
            "stats_command": "0x06B9/zhb.zip",
            "category_endpoint": category_endpoint,
            "auction_endpoint": auction_endpoint,
            "trade_endpoint": trade_endpoint,
            "stats_endpoint": stats_endpoint,
            "stats_date": resource.stats_date,
            "stats_date_coverage": resource.stats_date_coverage,
            "opening_rush_definition": (
                "(正式开盘价/0x056A 最后一个开盘竞价样本价-1)×100%"
            ),
            "series_scope_note": (
                "selector=3 同时返回 09:15—09:25 开盘竞价和 "
                "14:57—15:00 收盘竞价，中间为大时间断层"
            ),
            "last_sample_note": (
                "深市常见最后样本为 09:24:57，不能冒充 09:25 正式成交；"
                "正式开盘竞价量额仅在 0x0FC6 存在 09:25 成交时确认"
            ),
            "open_volume_note": (
                "0x054B 与 tdxstat2 的开盘量额是竞价参考口径；"
                "若当日无 09:25 成交，它们可能保留 09:24:57 虚拟匹配值，"
                "不能宣称为已执行量额"
            ),
            "closing_sample_note": (
                "收盘段常见最后样本为 14:59:51；正式收盘价相对该样本的"
                "变化另算 derived_closing_rush_pct；0x0FC6 的 15:00 记录"
                "给出正式收盘竞价价、量、额"
            ),
            "trade_scope_note": (
                "0x0FC6 为分钟精度的公开 L1 成交明细，不是 Level2 秒级逐笔；"
                "存在时，09:25 和 15:00 集中撮合可闭合正式开/收盘竞价量额"
            ),
            "unmatched_direction_note": (
                "正数=买方未匹配，负数=卖方未匹配，零=平衡"
            ),
        },
        "counts": {
            "ranked": len(records),
            "series_received": sum(bool(item["points"]) for item in records),
            "trade_series_received": sum(
                bool(item["trade_details"]["tick_count"]) for item in records
            ),
            "stats_joined": sum(bool(item["stats"]["stats_date"]) for item in records),
            "opening_rush_comparable": len(comparable_rush),
            "opening_rush_formula_matches": sum(
                item["opening_series"]["native_formula_matches_0_02_pct"] is True
                for item in comparable_rush
            ),
            "open_amount_comparable": len(comparable_amount),
            "open_amount_matches_stats": sum(
                item["stats"]["category_vs_stats_open_amount_matches_100_yuan"] is True
                for item in comparable_amount
            ),
            "open_volume_comparable": len(comparable_volume),
            "open_volume_matches_stats": sum(
                item["stats"]["inferred_vs_stats_open_volume_matches_1_hand"] is True
                for item in comparable_volume
            ),
            "sh_sz_open_volume_comparable": sum(
                int(item["market_id"]) in (0, 1) for item in comparable_volume
            ),
            "sh_sz_open_volume_matches_stats": sum(
                int(item["market_id"]) in (0, 1)
                and item["stats"]["inferred_vs_stats_open_volume_matches_1_hand"] is True
                for item in comparable_volume
            ),
            "closing_rush_comparable": len(comparable_close),
            "closing_price_unchanged": sum(
                item["closing_series"]["official_price_unchanged_0_001"] is True
                for item in comparable_close
            ),
            "trade_open_comparable": len(trade_open),
            "no_0925_trade": len(records) - len(trade_open),
            "trade_open_price_matches_native": sum(
                item["opening_series"]["trade_vs_native_open_price_matches_0_001"]
                is True
                for item in trade_open
            ),
            "trade_open_amount_matches_native": sum(
                item["opening_series"]["trade_vs_native_open_amount_matches_100_yuan"]
                is True
                for item in trade_open
            ),
            "trade_open_amount_matches_native_1000_yuan": sum(
                item["opening_series"]["trade_vs_native_open_amount_matches_1000_yuan"]
                is True
                for item in trade_open
            ),
            "trade_open_volume_matches_stats": sum(
                item["stats"]["trade_vs_stats_open_volume_matches_1_hand"] is True
                for item in trade_open
            ),
            "trade_open_amount_matches_stats_1000_yuan": sum(
                item["stats"]["trade_vs_stats_open_amount_matches_1000_yuan"] is True
                for item in trade_open
            ),
            "trade_close_comparable": len(trade_close),
            "trade_close_price_matches_native": sum(
                item["closing_series"]["trade_vs_native_close_price_matches_0_001"]
                is True
                for item in trade_close
            ),
            "post_close_status_5_securities": sum(
                bool(
                    item["trade_details"]["post_close_status_5"]
                    and item["trade_details"]["post_close_status_5"]["tick_count"]
                )
                for item in records
            ),
        },
        "rankings": {
            "native_opening_rush": [rank_row(item) for item in records],
            "open_prev_amount_ratio": ranked(("stats", "open_prev_amount_ratio")),
            "auction_prev_volume_ratio": ranked(("stats", "auction_prev_volume_ratio")),
            "open_turnover_pct": ranked(("stats", "open_turnover_pct")),
            "late_volume_ratio": ranked((
                "opening_series", "official_vs_last_sample_volume_ratio"
            )),
            "derived_closing_rush": ranked((
                "closing_series", "derived_closing_rush_pct"
            )),
            "closing_auction_amount": ranked((
                "closing_series", "official_close_amount_yuan"
            )),
            "closing_auction_volume": ranked((
                "closing_series", "official_close_volume_hand"
            )),
            "post_close_status_5_amount": ranked((
                "trade_details", "post_close_status_5_amount_yuan"
            )),
        },
        "securities": records,
        "security_index": {
            str(item["security_id"]): index for index, item in enumerate(records)
        },
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="更新通达信开盘抢筹榜、开/收盘竞价逐点和历史竞价对比。"
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument("--count", type=int, default=30)
    parser.add_argument("--ascending", action="store_true", help="改取抢筹最低榜")
    parser.add_argument("--host", action="append", default=[])
    parser.add_argument("--max-hosts", type=int, default=5)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument(
        "--stats-chunk-size", type=int, default=statistics.DEFAULT_CHUNK_SIZE,
    )
    parser.add_argument("--download", action="store_true", help="明确允许联网")
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-auction-quality.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        if not 1 <= args.count <= 80:
            raise AuctionQualityError("count 必须为 1..80")
        output = args.output.expanduser().resolve()
        if not args.download:
            print(json.dumps({
                "action": "dry-run",
                "commands": [
                    "0x054B/0x010A", "0x056A", "0x0FC6", "0x06B9"
                ],
                "count": args.count,
                "ascending": args.ascending,
                "output": str(output),
            }, ensure_ascii=False, indent=2))
            return 0
        endpoints = depth.endpoint_candidates(root, args.host, args.max_hosts)
        category_result = categories.download_category_quotes(
            endpoints,
            category=categories.CATEGORY_A_SHARES,
            sort_type="opening-rush",
            count=args.count,
            ascending=args.ascending,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        codes = tuple(
            snapshots.QuoteCode(item.market_id, item.code)
            for item in category_result.page.records
        )
        auction_result = auctions.download_auction_series(
            endpoints,
            codes,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        trade_result = trades.download_trades(
            endpoints,
            codes,
            trading_date=auction_result.server_trade_date,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        stats_result = statistics.download_stats(
            endpoints,
            chunk_size=args.stats_chunk_size,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        master = blocks.load_security_master(root / "T0002" / "hq_cache")
        model = build_model(
            category_result.page.records,
            auction_result.series,
            stats_result.resource,
            master,
            trade_result.series,
            target_date=auction_result.server_trade_date,
            category_endpoint=category_result.endpoint.address,
            auction_endpoint=auction_result.endpoint.address,
            trade_endpoint=trade_result.endpoint.address,
            stats_endpoint=stats_result.endpoint.address,
        )
        rendered = json.dumps(
            model,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        updater.atomic_write_text(output, rendered + "\n", "utf-8")
        counts = model["counts"]
        print(
            f"已更新 {counts['ranked']} 只开盘抢筹证券："
            f"公式匹配 {counts['opening_rush_formula_matches']}/"
            f"{counts['opening_rush_comparable']}，"
            f"竞价量额匹配统计 {counts['open_amount_matches_stats']}/"
            f"{counts['open_amount_comparable']}，"
            f"正式收盘竞价 {counts['trade_close_comparable']} 条：{output}"
        )
    except (
        OSError,
        UnicodeError,
        ValueError,
        blocks.BlockFormatError,
        updater.UpdateError,
        transport.DownloadError,
        snapshots.SnapshotError,
        categories.CategoryQuoteError,
        auctions.AuctionSeriesError,
        trades.TradeError,
        statistics.StatsError,
        AuctionQualityError,
    ) as error:
        print(f"竞价质量更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

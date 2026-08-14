#!/usr/bin/env python3
"""Update TDX price-limit events, live seal amounts, and industry ladders."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Mapping, Sequence

import download_tdx_jsn as jsn
import download_tdx_minute as transport
import extract_tdx_blocks as blocks
import tdx_market_depth as depth
import tdx_market_snapshot as snapshots
import tdx_stats as statistics
import update_tdx_blocks as updater


RESOURCES = (
    "list/func_zdtfx101_1.jsn",
    "list/func_zdtfx107_1.jsn",
    "list/func_lbtt101_1.jsn",
)


class LimitQualityError(RuntimeError):
    """Raised when price-limit resources cannot be joined safely."""


def read_jsn_rows(path: Path) -> list[dict[str, object]]:
    try:
        value = json.loads(path.read_bytes().decode("gb18030"))
    except FileNotFoundError as error:
        raise LimitQualityError(f"缺少 JSN 资源：{path}") from error
    if not isinstance(value, list) or not value or not isinstance(value[0], dict):
        raise LimitQualityError(f"JSN 根结构无效：{path}")
    group = value[0]
    columns = group.get("colheader", [])
    rows = group.get("data", [])
    if not isinstance(columns, list) or not isinstance(rows, list):
        raise LimitQualityError(f"JSN 表结构无效：{path}")
    names = [str(item) for item in columns]
    records: list[dict[str, object]] = []
    for row in rows:
        if not isinstance(row, list) or len(row) != len(names):
            raise LimitQualityError(f"JSN 行宽与表头不符：{path}")
        records.append(dict(zip(names, row)))
    return records


def refresh_resources(
    root: Path,
    output_dir: Path,
    *,
    hosts: Sequence[str],
    timeout: float,
) -> tuple[str, str]:
    endpoints = (
        transport.unique_endpoints(transport.parse_endpoint(item) for item in hosts)
        if hosts
        else transport.load_hq_hosts(root / "connect.cfg")
    )
    connection, _ = jsn.connect_first(endpoints, timeout)
    try:
        for resource in RESOURCES:
            destination = output_dir.joinpath(*resource.split("/"))
            jsn.download_to_path(
                connection,
                jsn.make_remote_path(resource),
                destination,
            )
        return connection.endpoint.address, connection.server_name
    finally:
        connection.close()


def load_block_memberships(
    root: Path,
) -> tuple[
    dict[tuple[int, str], blocks.Security],
    dict[tuple[int, str], list[dict[str, object]]],
]:
    securities, catalog, members = blocks.extract(root, set(blocks.FAMILY_CHOICES))[:3]
    by_id = {item.block_id: item for item in catalog}
    assignments: dict[tuple[int, str], list[dict[str, object]]] = {}
    for member in members:
        block = by_id.get(member.block_id)
        if block is None:
            continue
        assignments.setdefault((member.market_id, member.code), []).append({
            "code": block.block_code,
            "name": block.name,
            "family": block.family,
            "family_name": block.family_name,
            "level": block.level,
            "is_leaf": block.is_leaf,
        })
    for values in assignments.values():
        values.sort(key=lambda item: (int(item["level"]), str(item["code"])))
    return securities, assignments


def is_sealed(depth_item: depth.MarketDepth | None) -> bool:
    if depth_item is None or depth_item.last_price <= 0 or not depth_item.buy_levels:
        return False
    bid1 = depth_item.buy_levels[0]
    ask1 = depth_item.sell_levels[0] if depth_item.sell_levels else None
    at_bid = abs(bid1.price - depth_item.last_price) < 0.000001
    no_ask = ask1 is None or ask1.price <= 0 or ask1.volume_hand <= 0
    return at_bid and no_ask


def positive_int(value: object) -> int:
    try:
        return max(0, int(str(value).strip() or "0"))
    except ValueError:
        return 0


def _number(value: object) -> float | None:
    if value is None:
        return None
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    return result


def _tenk(value: object) -> float | None:
    result = _number(value)
    return None if result is None else result * 10_000.0


def _ratio(numerator: object, denominator: object) -> float | None:
    left = _number(numerator)
    right = _number(denominator)
    if left is None or right is None or right <= 0:
        return None
    return left / right


def align_stat2(
    row: statistics.TdxStat2Row | None,
    *,
    target_date: str,
) -> dict[str, object]:
    """Align a stats2 row to the event date without shifting it twice."""

    empty = {
        "status": "stats-row-missing" if row is None else "stats-date-unaligned",
        "stats_date": row.stats_date if row else None,
        "stats_current_amount_yuan": None,
        "stats_current_seal_amount_yuan": None,
        "previous_amount_yuan": None,
        "previous_seal_amount_yuan": None,
        "previous2_seal_amount_yuan": None,
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
            "stats_current_amount_yuan": _tenk(row.amount_10k),
            "stats_current_seal_amount_yuan": _tenk(row.seal_amount_10k),
            "previous_amount_yuan": _tenk(row.prev_amount_10k),
            "previous_seal_amount_yuan": _tenk(row.prev_seal_amount_10k),
            "previous2_seal_amount_yuan": _tenk(row.prev2_seal_amount_10k),
            "previous_open_volume_hand": row.prev_open_volume_hand,
            "previous_open_amount_yuan": _tenk(row.prev_open_amount_10k),
        }
    day_gap = (target - row_date).days
    if 1 <= day_gap <= 7:
        return {
            "status": "previous-resource-day",
            "stats_date": row.stats_date,
            "stats_current_amount_yuan": None,
            "stats_current_seal_amount_yuan": None,
            "previous_amount_yuan": _tenk(row.amount_10k),
            "previous_seal_amount_yuan": _tenk(row.seal_amount_10k),
            "previous2_seal_amount_yuan": _tenk(row.prev_seal_amount_10k),
            "previous_open_volume_hand": row.open_volume_hand,
            "previous_open_amount_yuan": _tenk(row.open_amount_10k),
        }
    return empty


def build_stats_metrics(
    key: tuple[int, str],
    quote: depth.MarketDepth | None,
    sealed: bool,
    resource: statistics.TdxStatsResource | None,
    *,
    target_date: str,
) -> dict[str, object]:
    stat = resource.stat.get(key) if resource else None
    stat2 = resource.stat2.get(key) if resource else None
    aligned = align_stat2(stat2, target_date=target_date)
    free_float_shares = _tenk(stat.free_float_shares_10k) if stat else None
    free_float_market_value = (
        free_float_shares * quote.last_price
        if free_float_shares is not None and quote is not None
        else None
    )
    current_seal = (
        float(quote.bid1_amount_yuan or 0.0)
        if sealed and quote is not None
        else 0.0 if quote is not None else None
    )
    previous_seal = aligned["previous_seal_amount_yuan"]
    retention = _ratio(current_seal, previous_seal)
    seal_to_float = _ratio(current_seal, free_float_market_value)
    stats_current = _number(aligned["stats_current_seal_amount_yuan"])
    comparison_delta = (
        current_seal - stats_current
        if current_seal is not None and stats_current is not None and stats_current > 0
        else None
    )
    return {
        "stats_date": aligned["stats_date"],
        "alignment_status": aligned["status"],
        "beta_60d": stat.beta_60d if stat else None,
        "pe_ttm": stat.pe_ttm if stat else None,
        "free_float_shares": free_float_shares,
        "free_float_market_value_yuan": free_float_market_value,
        "current_seal_amount_yuan": current_seal,
        "stats_current_seal_amount_yuan": stats_current,
        "stats_vs_depth_delta_yuan": comparison_delta,
        "stats_vs_depth_matches_100_yuan": (
            abs(comparison_delta) <= 100.0 if comparison_delta is not None else None
        ),
        "previous_amount_yuan": aligned["previous_amount_yuan"],
        "previous_seal_amount_yuan": previous_seal,
        "previous2_seal_amount_yuan": aligned["previous2_seal_amount_yuan"],
        "previous_open_volume_hand": aligned["previous_open_volume_hand"],
        "previous_open_amount_yuan": aligned["previous_open_amount_yuan"],
        "previous_seal_was_positive": (
            _number(previous_seal) is not None and float(previous_seal) > 0
        ),
        "seal_to_float_ratio_pct": (
            seal_to_float * 100.0 if seal_to_float is not None else None
        ),
        "seal_prev_ratio": retention,
        "seal_change_pct": (
            (retention - 1.0) * 100.0 if retention is not None else None
        ),
        "seal_decay_pct": (
            (1.0 - retention) * 100.0 if retention is not None else None
        ),
        "limit_stat_days": stat.limit_stat_days if stat else None,
        "limit_up_count_in_stat_days": (
            stat.limit_up_count_in_stat_days if stat else None
        ),
        "limit_up_streak_days": stat.limit_up_streak_days if stat else None,
        "year_limit_up_days": stat.year_limit_up_days if stat else None,
    }


def build_model(
    event_rows: Sequence[Mapping[str, object]],
    daily_rows: Sequence[Mapping[str, object]],
    native_block_rows: Sequence[Mapping[str, object]],
    depths: Sequence[depth.MarketDepth],
    security_master: Mapping[tuple[int, str], blocks.Security],
    assignments: Mapping[tuple[int, str], Sequence[Mapping[str, object]]],
    *,
    stats_resource: statistics.TdxStatsResource | None = None,
    generated_at: str | None = None,
    jsn_endpoint: str = "",
    depth_endpoint: str = "",
    stats_endpoint: str = "",
) -> dict[str, object]:
    depth_by_key = {item.key: item for item in depths}
    events: list[dict[str, object]] = []
    target_date = str(
        daily_rows[0].get("$ZQDM", "") if daily_rows
        else event_rows[0].get("ztrq", "") if event_rows
        else ""
    )
    for raw in event_rows:
        market_text = str(raw.get("$SC", "")).strip()
        code = str(raw.get("$ZQDM", "")).strip()
        try:
            key = (int(market_text), code)
        except ValueError as error:
            raise LimitQualityError(f"涨停记录市场号无效：{market_text!r}") from error
        quote = depth_by_key.get(key)
        sealed = is_sealed(quote)
        stats_metrics = build_stats_metrics(
            key,
            quote,
            sealed,
            stats_resource,
            target_date=target_date,
        )
        security = security_master.get(key)
        bid1 = quote.buy_levels[0] if quote and quote.buy_levels else None
        ask1 = quote.sell_levels[0] if quote and quote.sell_levels else None
        events.append({
            "market": market_text,
            "code": code,
            "security_id": snapshots.QuoteCode(*key).display,
            "name": security.name if security else "",
            "name_resolved": security is not None,
            "date": str(raw.get("ztrq", "")),
            "status": "sealed" if sealed else ("opened" if quote else "missing-depth"),
            "board_type": raw.get("bx", ""),
            "reason": raw.get("yy", ""),
            "reason_tags": [
                item.strip()
                for item in str(raw.get("yy", "")).split("+")
                if item.strip()
            ],
            "first_limit_time": raw.get("ztsj1", ""),
            "last_limit_time": raw.get("ztsj2", ""),
            "open_count": positive_int(raw.get("ztcs")),
            "consecutive_boards": positive_int(raw.get("lbts")),
            "recent_limit_count": positive_int(raw.get("ztcs1")),
            "quote": None if quote is None else {
                "last": quote.last_price,
                "pre_close": quote.pre_close_price,
                "change_pct": quote.change_pct,
                "update_time_raw": quote.update_time_raw,
                "bid1_price": bid1.price if bid1 else None,
                "bid1_volume_hand": bid1.volume_hand if bid1 else None,
                "bid1_amount_yuan": quote.bid1_amount_yuan,
                "ask1_price": ask1.price if ask1 else None,
                "ask1_volume_hand": ask1.volume_hand if ask1 else None,
                "buy_levels": [
                    {
                        "price": item.price,
                        "volume_hand": item.volume_hand,
                        "amount_yuan": item.amount_yuan,
                    }
                    for item in quote.buy_levels
                ],
                "sell_levels": [
                    {
                        "price": item.price,
                        "volume_hand": item.volume_hand,
                        "amount_yuan": item.amount_yuan,
                    }
                    for item in quote.sell_levels
                ],
            },
            "stats": stats_metrics,
            "block_memberships": list(assignments.get(key, ())),
            "research_industries": [
                item
                for item in assignments.get(key, ())
                if item.get("family") == "research-industry"
            ],
            "raw": dict(raw),
        })
    events.sort(key=lambda item: (str(item["market"]), str(item["code"])))
    event_by_key = {
        str(item["security_id"]): index
        for index, item in enumerate(events)
    }
    sealed_events = [item for item in events if item["status"] == "sealed"]
    opened_events = [item for item in events if item["status"] == "opened"]

    ladder_groups: dict[int, list[dict[str, object]]] = {}
    for item in sealed_events:
        level = int(item["consecutive_boards"])
        ladder_groups.setdefault(level, []).append({
            "market": item["market"],
            "code": item["code"],
            "security_id": item["security_id"],
            "name": item["name"],
            "reason": item["reason"],
            "bid1_amount_yuan": item["quote"]["bid1_amount_yuan"],
            "seal_to_float_ratio_pct": item["stats"]["seal_to_float_ratio_pct"],
            "seal_prev_ratio": item["stats"]["seal_prev_ratio"],
            "seal_decay_pct": item["stats"]["seal_decay_pct"],
        })
    ladders = [
        {
            "consecutive_boards": level,
            "security_count": len(items),
            "securities": sorted(
                items,
                key=lambda item: float(item["bid1_amount_yuan"] or 0),
                reverse=True,
            ),
        }
        for level, items in sorted(ladder_groups.items(), reverse=True)
    ]

    industry_members: dict[str, list[dict[str, object]]] = {}
    for item in sealed_events:
        for block in item["block_memberships"]:
            industry_members.setdefault(str(block["code"]), []).append({
                "market": item["market"],
                "code": item["code"],
                "security_id": item["security_id"],
                "name": item["name"],
                "consecutive_boards": item["consecutive_boards"],
                "bid1_amount_yuan": item["quote"]["bid1_amount_yuan"],
                "seal_to_float_ratio_pct": item["stats"]["seal_to_float_ratio_pct"],
                "seal_prev_ratio": item["stats"]["seal_prev_ratio"],
                "reason": item["reason"],
            })
    block_family_by_code: dict[str, Mapping[str, object]] = {}
    for values in assignments.values():
        for item in values:
            block_family_by_code.setdefault(str(item["code"]), item)
    block_ladders: list[dict[str, object]] = []
    for raw in native_block_rows:
        code = str(raw.get("$ZQDM", ""))
        members = industry_members.get(code, [])
        block_meta = block_family_by_code.get(code, {})
        block_ladders.append({
            "market": str(raw.get("$SC", "")),
            "code": code,
            "name": raw.get("mc", ""),
            "family": block_meta.get("family", ""),
            "family_name": block_meta.get("family_name", ""),
            "level": block_meta.get("level"),
            "date": raw.get("rq", ""),
            "native": {
                "sealed_count": positive_int(raw.get("ztjs")),
                "opened_count": positive_int(raw.get("zbs")),
                "previous_limit_count": positive_int(raw.get("zrzt")),
                "consecutive_count": positive_int(raw.get("lbs")),
                "ladder_height": positive_int(raw.get("lbgd")),
                "total_height": positive_int(raw.get("hzgd")),
                "promotion_pct": raw.get("lbjjl", ""),
            },
            "linked_sealed_count": len(members),
            "securities": sorted(
                members,
                key=lambda item: (
                    int(item["consecutive_boards"]),
                    float(item["bid1_amount_yuan"] or 0),
                ),
                reverse=True,
            ),
        })
    block_ladders.sort(
        key=lambda item: (
            int(item["native"]["sealed_count"]),
            int(item["native"]["ladder_height"]),
            int(item["linked_sealed_count"]),
        ),
        reverse=True,
    )
    daily = dict(daily_rows[0]) if daily_rows else {}
    current_amounts = [
        float(item["quote"]["bid1_amount_yuan"] or 0)
        for item in sealed_events
    ]
    native_sealed = positive_int(daily.get("ztjs2"))
    native_opened = positive_int(daily.get("ztjs3"))
    resolved_blocks = [item for item in block_ladders if item["family"]]
    with_stat = [item for item in events if item["stats"]["stats_date"]]
    comparable = [
        item for item in events
        if item["stats"]["stats_vs_depth_delta_yuan"] is not None
    ]
    previous_positive = [
        item for item in events
        if item["stats"]["previous_seal_was_positive"]
    ]
    ratio_rows = [
        item for item in sealed_events
        if item["stats"]["seal_to_float_ratio_pct"] is not None
    ]
    retention_rows = [
        item for item in events
        if item["stats"]["seal_prev_ratio"] is not None
    ]

    def rank_item(item: Mapping[str, object]) -> dict[str, object]:
        return {
            "security_id": item["security_id"],
            "name": item["name"],
            "status": item["status"],
            "current_seal_amount_yuan": item["stats"]["current_seal_amount_yuan"],
            "previous_seal_amount_yuan": item["stats"]["previous_seal_amount_yuan"],
            "seal_to_float_ratio_pct": item["stats"]["seal_to_float_ratio_pct"],
            "seal_prev_ratio": item["stats"]["seal_prev_ratio"],
            "seal_change_pct": item["stats"]["seal_change_pct"],
            "seal_decay_pct": item["stats"]["seal_decay_pct"],
        }

    return {
        "schema": "tdx-limit-quality-v2",
        "generated_at": generated_at or datetime.now().astimezone().isoformat(),
        "date": str(daily.get("$ZQDM", event_rows[0].get("ztrq", "") if event_rows else "")),
        "source": {
            "event_resource": RESOURCES[0],
            "daily_resource": RESOURCES[1],
            "industry_ladder_resource": RESOURCES[2],
            "depth_command": "0x0547",
            "stats_command": "0x06B9",
            "stats_resource": "zhb.zip:tdxstat.cfg+tdxstat2.cfg",
            "jsn_endpoint": jsn_endpoint,
            "depth_endpoint": depth_endpoint,
            "stats_endpoint": stats_endpoint,
            "stats_source_path": stats_resource.source_path if stats_resource else "",
            "stats_date": stats_resource.stats_date if stats_resource else None,
            "stats_date_coverage": (
                stats_resource.stats_date_coverage if stats_resource else 0.0
            ),
            "current_seal_definition": "收盘/当前买一价等于现价且卖一为空；金额=买一价×买一量(手)×100",
            "stats_alignment_note": "统计日等于事件日时，昨日值取 prev_*；统计日是更早且相差不超过 7 个自然日时，昨日值取当行 amount/seal/open_*",
            "signed_seal_note": "tdxstat2 的历史封单字段可能为负；只对正的历史封单计算封昨比和衰减率",
            "seal_decay_definition": "(1-当前封单/昨日正封单)×100%；正数表示减弱，负数表示增强，炸板当前封单按 0",
            "daily_seal_note": "ztjs6/ztjs7 是通达信日统计口径，不等同于当前或收盘买一金额",
            "block_link_note": "linked_sealed_count 使用当前本地板块成员缓存；成员更新时间不同会使其与同日 native.sealed_count 有少量差异",
        },
        "counts": {
            "touched_limit_up": len(events),
            "sealed": len(sealed_events),
            "opened": len(opened_events),
            "missing_depth": sum(item["status"] == "missing-depth" for item in events),
            "native_sealed": native_sealed,
            "native_opened": native_opened,
            "sealed_count_matches_native": len(sealed_events) == native_sealed,
            "opened_count_matches_native": len(opened_events) == native_opened,
            "native_blocks": len(block_ladders),
            "linked_blocks": sum(bool(item["securities"]) for item in block_ladders),
            "resolved_native_blocks": len(resolved_blocks),
            "blocks_matching_native_sealed_count": sum(
                item["linked_sealed_count"] == item["native"]["sealed_count"]
                for item in resolved_blocks
            ),
            "stats_rows_joined": len(with_stat),
            "positive_previous_seal": len(previous_positive),
            "stats_current_seal_comparable": len(comparable),
            "stats_current_seal_matches_depth": sum(
                item["stats"]["stats_vs_depth_matches_100_yuan"] is True
                for item in comparable
            ),
        },
        "current_depth_aggregate": {
            "total_bid1_amount_yuan": sum(current_amounts),
            "max_bid1_amount_yuan": max(current_amounts, default=0),
        },
        "native_daily": daily,
        "seal_quality_rankings": {
            "seal_to_float_top": [
                rank_item(item)
                for item in sorted(
                    ratio_rows,
                    key=lambda item: float(
                        item["stats"]["seal_to_float_ratio_pct"] or 0
                    ),
                    reverse=True,
                )[:30]
            ],
            "retention_top": [
                rank_item(item)
                for item in sorted(
                    retention_rows,
                    key=lambda item: float(item["stats"]["seal_prev_ratio"] or 0),
                    reverse=True,
                )[:30]
            ],
            "decay_top": [
                rank_item(item)
                for item in sorted(
                    retention_rows,
                    key=lambda item: float(item["stats"]["seal_decay_pct"] or 0),
                    reverse=True,
                )[:30]
            ],
        },
        "ladders": ladders,
        "block_ladders": block_ladders,
        "securities": events,
        "security_index": event_by_key,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "更新通达信涨停/炸板明细、实时五档封单、连板梯队和行业/概念板块梯队。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument("--host", action="append", default=[])
    parser.add_argument("--max-hosts", type=int, default=5)
    parser.add_argument("--batch-size", type=int, default=80)
    parser.add_argument(
        "--stats-chunk-size", type=int, default=statistics.DEFAULT_CHUNK_SIZE,
    )
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument(
        "--download", action="store_true", help="明确允许更新 JSN 和 0x0547 五档",
    )
    parser.add_argument(
        "--jsn-dir", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-jsn",
    )
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-limit-quality.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        output = args.output.expanduser().resolve()
        jsn_dir = args.jsn_dir.expanduser().resolve()
        if not args.download:
            print(json.dumps({
                "action": "dry-run",
                "resources": list(RESOURCES),
                "depth_command": "0x0547",
                "stats_command": "0x06B9",
                "stats_resource": "zhb.zip",
                "output": str(output),
            }, ensure_ascii=False, indent=2))
            return 0
        jsn_endpoint, _ = refresh_resources(
            root,
            jsn_dir,
            hosts=args.host,
            timeout=args.timeout,
        )
        event_rows = read_jsn_rows(jsn_dir / RESOURCES[0])
        daily_rows = read_jsn_rows(jsn_dir / RESOURCES[1])
        industry_rows = read_jsn_rows(jsn_dir / RESOURCES[2])
        codes = tuple(
            snapshots.QuoteCode(int(str(item["$SC"])), str(item["$ZQDM"]))
            for item in event_rows
        )
        endpoints = depth.endpoint_candidates(root, args.host, args.max_hosts)
        stats_result = statistics.download_stats(
            endpoints,
            chunk_size=args.stats_chunk_size,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        depth_result = depth.download_depths(
            endpoints,
            codes,
            batch_size=args.batch_size,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        security_master, assignments = load_block_memberships(root)
        model = build_model(
            event_rows,
            daily_rows,
            industry_rows,
            depth_result.depths,
            security_master,
            assignments,
            stats_resource=stats_result.resource,
            jsn_endpoint=jsn_endpoint,
            depth_endpoint=depth_result.endpoint.address,
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
            f"已更新 {counts['touched_limit_up']} 只触板股："
            f"封板 {counts['sealed']}、炸板 {counts['opened']}，"
            f"统计关联 {counts['stats_rows_joined']} 只，"
            f"{counts['linked_blocks']} 个板块关联到封板股：{output}"
        )
    except (
        OSError,
        UnicodeError,
        ValueError,
        json.JSONDecodeError,
        blocks.BlockFormatError,
        updater.UpdateError,
        jsn.JsnDownloadError,
        transport.DownloadError,
        snapshots.SnapshotError,
        depth.MarketDepthError,
        statistics.StatsError,
        LimitQualityError,
    ) as error:
        print(f"涨停质量更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

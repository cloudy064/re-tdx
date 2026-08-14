#!/usr/bin/env python3
"""Update TDX LHB summary views and optional per-event broker details."""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

import catalog_tdx_jsn as catalog
import download_tdx_jsn as downloader
import download_tdx_minute as transport
import extract_tdx_blocks as blocks
import update_tdx_blocks as updater


@dataclass(frozen=True)
class LhbView:
    name: str
    resource: str


LHB_VIEWS = (
    LhbView("龙虎榜还原", "list/func_lhbfx101_1.jsn"),
    LhbView("机构参与", "list/func_lhbfx103_1.jsn"),
    LhbView("市场风口", "list/func_lhbfx104_1.jsn"),
    LhbView("合力封板", "list/func_lhbfx105_1.jsn"),
    LhbView("一家独大", "list/func_lhbfx106_1.jsn"),
    LhbView("量化席位", "list/func_lhbfx107_1.jsn"),
    LhbView("同城携手", "list/func_lhbfx108_1.jsn"),
    LhbView("游资席位", "list/func_lhbfx110_1.jsn"),
)

MASTER_REQUIRED = {"$ZQDM", "$ZQDM1", "$SC1"}
DETAIL_REQUIRED = {
    "$ZQDM", "sc", "date", "lb", "yyb", "yyb1", "yyb2",
    "bje", "sje", "jmr", "zb", "mrcgl1", "mrcgl3", "mrcgl5",
    "ygcb", "ygsy",
}


class LhbUpdateError(ValueError):
    """Raised when LHB master/detail data is inconsistent."""


def security_id(market: str, code: str) -> str:
    try:
        market_id = int(market)
    except ValueError:
        return f"M{market}{code}"
    prefix = blocks.MARKETS.get(market_id, (f"M{market_id}", ""))[0]
    return f"{prefix}{code}"


def table_records(path: Path, required: set[str]) -> list[dict[str, str]]:
    result: list[dict[str, str]] = []
    for headers, rows in catalog.load_tables(path):
        missing = required - set(headers)
        if missing:
            raise LhbUpdateError(
                f"{path.name}: 缺少字段 {', '.join(sorted(missing))}"
            )
        for row in rows:
            result.append({
                name: "" if value is None else str(value)
                for name, value in zip(headers, row)
            })
    return result


def load_master_events(
    input_dir: Path,
    views: Sequence[LhbView] = LHB_VIEWS,
) -> list[dict[str, object]]:
    aggregate: dict[str, dict[str, object]] = {}
    for priority, view in enumerate(views):
        path = input_dir.joinpath(*Path(view.resource).parts)
        if not path.is_file():
            raise LhbUpdateError(
                f"缺少龙虎榜主表：{path}；请先使用 --download-masters"
            )
        for record in table_records(path, MASTER_REQUIRED):
            event_id = record["$ZQDM"].strip()
            market = record["$SC1"].strip()
            code = record["$ZQDM1"].strip()
            date = (record.get("date") or record.get("rq") or "").strip()
            if not event_id or not market or not code:
                raise LhbUpdateError(f"{path.name}: 事件或证券主键为空")
            event = aggregate.setdefault(
                event_id,
                {
                    "event_id": event_id,
                    "key_candidates": [],
                    "dates": [],
                    "views": [],
                    "event_types": [],
                    "master_records": [],
                },
            )
            candidate = (market, code)
            if candidate not in event["key_candidates"]:
                event["key_candidates"].append(candidate)
            if date and date not in event["dates"]:
                event["dates"].append(date)
            if view.name not in event["views"]:
                event["views"].append(view.name)
            event_type = (record.get("lx") or record.get("sblx") or "").strip()
            if event_type and event_type not in event["event_types"]:
                event["event_types"].append(event_type)
            event["master_records"].append(
                {
                    "view": view.name,
                    "view_priority": priority,
                    "resource": view.resource,
                    "record": record,
                }
            )
    return list(aggregate.values())


def load_detail(path: Path) -> list[dict[str, str]]:
    records = table_records(path, DETAIL_REQUIRED)
    keys = {(item["sc"], item["$ZQDM"], item["date"]) for item in records}
    if len(keys) > 1:
        raise LhbUpdateError(f"{path.name}: 详情包含多个证券或日期：{keys}")
    return records


def select_events(
    events: Sequence[dict[str, object]],
    event_ids: Sequence[str],
    market: str | None,
    code: str | None,
    all_details: bool,
) -> list[dict[str, object]]:
    if all_details:
        return list(events)
    ids = set(event_ids)
    selected: list[dict[str, object]] = []
    for event in events:
        candidates = {
            (str(item[0]), str(item[1]))
            for item in event["key_candidates"]
        }
        if (
            str(event["event_id"]) in ids
            or (market is not None and code is not None and (market, code) in candidates)
        ):
            selected.append(event)
    return selected


def build_model(
    root: Path,
    input_dir: Path,
    views: Sequence[LhbView] = LHB_VIEWS,
) -> dict[str, object]:
    events = load_master_events(input_dir, views)
    security_master = blocks.load_security_master(root / "T0002" / "hq_cache")
    stock_events: dict[tuple[str, str], list[str]] = defaultdict(list)
    event_records: list[dict[str, object]] = []
    detailed_events = 0
    detail_rows = 0
    conflicting_master_keys = 0
    for event in events:
        event_id = str(event["event_id"])
        candidates = [
            (str(item[0]), str(item[1]))
            for item in event["key_candidates"]
        ]
        if len(candidates) > 1:
            conflicting_master_keys += 1
        detail_path = input_dir / "lhbfx" / f"{event_id}.jsn"
        raw_details = load_detail(detail_path) if detail_path.is_file() else []
        if raw_details:
            detailed_events += 1
            detail_rows += len(raw_details)
            market = raw_details[0]["sc"]
            code = raw_details[0]["$ZQDM"]
            date = raw_details[0]["date"]
        else:
            market, code = candidates[0]
            date = str(event["dates"][0]) if event["dates"] else ""
        try:
            market_id = int(market)
        except ValueError:
            market_id = -1
        security = security_master.get((market_id, code))
        stock_events[(market, code)].append(event_id)
        details = [
            {
                "broker": item["yyb"],
                "side": item["yyb1"],
                "rank": item["yyb2"],
                "buy_amount": item["bje"],
                "sell_amount": item["sje"],
                "net_buy": item["jmr"],
                "share_percent": item["zb"],
                "buy_success_rate_1d": item["mrcgl1"],
                "buy_success_rate_3d": item["mrcgl3"],
                "buy_success_rate_5d": item["mrcgl5"],
                "estimated_cost": item["ygcb"],
                "estimated_return": item["ygsy"],
                "tag": item.get("yzbq", ""),
                "operation_url": item.get("czjl", ""),
            }
            for item in raw_details
        ]
        event_records.append(
            {
                "event_id": event_id,
                "date": date,
                "security_id": security_id(market, code),
                "market": market,
                "code": code,
                "name": security.name if security else "",
                "views": event["views"],
                "event_types": event["event_types"],
                "master_key_candidates": [
                    {"market": item[0], "code": item[1]}
                    for item in candidates
                ],
                "master_key_conflict": len(candidates) > 1,
                "detail_available": bool(raw_details),
                "detail_resource": f"lhbfx/{event_id}.jsn",
                "master_records": event["master_records"],
                "details": details,
            }
        )
    event_records.sort(
        key=lambda item: (str(item["date"]), str(item["event_id"])),
        reverse=True,
    )
    stock_records: list[dict[str, object]] = []
    for (market, code), event_ids in sorted(stock_events.items()):
        try:
            market_id = int(market)
        except ValueError:
            market_id = -1
        security = security_master.get((market_id, code))
        stock_records.append(
            {
                "security_id": security_id(market, code),
                "market": market,
                "code": code,
                "name": security.name if security else "",
                "event_ids": event_ids,
            }
        )
    master_rows = sum(len(event["master_records"]) for event in events)
    return {
        "schema": "tdx-lhb-events-v1",
        "counts": {
            "master_views": len(views),
            "master_rows": master_rows,
            "events": len(event_records),
            "event_view_memberships": master_rows,
            "stocks": len(stock_records),
            "conflicting_master_keys": conflicting_master_keys,
            "detailed_events": detailed_events,
            "detail_rows": detail_rows,
        },
        "views": [
            {"name": view.name, "resource": view.resource}
            for view in views
        ],
        "events": event_records,
        "stocks": stock_records,
    }


def connect(root: Path, hosts: Sequence[str], timeout: float) -> transport.QuoteConnection:
    endpoints = (
        transport.unique_endpoints(transport.parse_endpoint(value) for value in hosts)
        if hosts
        else transport.load_hq_hosts(root / "connect.cfg")
    )
    connection, failures = downloader.connect_first(endpoints, timeout)
    if failures:
        print(
            f"前 {len(failures)} 个主站连接失败，已切换到 "
            f"{connection.endpoint.address}",
            file=sys.stderr,
        )
    return connection


def download_resources(
    connection: transport.QuoteConnection,
    input_dir: Path,
    resources: Sequence[str],
) -> None:
    for resource in resources:
        normalized = downloader.normalize_resource_path(resource)
        destination = input_dir.joinpath(*Path(normalized).parts)
        info = downloader.download_to_path(
            connection,
            downloader.make_remote_path(normalized),
            destination,
        )
        print(
            f"已更新 {normalized}: {info.size} bytes, md5={info.md5}",
            file=sys.stderr,
        )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "更新通达信 8 张龙虎榜分析主表，可按事件或证券下载逐营业部明细，"
            "并生成事件—股票—营业部关系模型。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录；默认自动寻找")
    parser.add_argument("--download-masters", action="store_true", help="联网更新 8 张龙虎榜主表")
    parser.add_argument("--download-details", action="store_true", help="联网更新选中事件的营业部明细")
    parser.add_argument("--event", action="append", default=[], help="按事件 ID 选择；可重复")
    parser.add_argument("--market", help="与 --code 一起按证券选择事件")
    parser.add_argument("--code", help="与 --market 一起按证券选择事件")
    parser.add_argument("--all-details", action="store_true", help="选择当前主表中的全部事件")
    parser.add_argument(
        "--max-details", type=int, default=50,
        help="单次详情下载上限；0 表示不限制（默认 50）",
    )
    parser.add_argument("--list", action="store_true", help="列出选中事件")
    parser.add_argument("--host", action="append", default=[], help="指定 host[:port]；可重复")
    parser.add_argument("--timeout", type=float, default=8.0, help="网络超时秒数")
    parser.add_argument(
        "--input-dir", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-jsn",
        help="JSN 下载目录",
    )
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-lhb-events.json",
        help="关系模型输出",
    )
    parser.add_argument("--compact", action="store_true", help="输出紧凑 JSON")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    connection: transport.QuoteConnection | None = None
    try:
        if args.max_details < 0:
            raise LhbUpdateError("--max-details 不能为负数")
        if (args.market is None) != (args.code is None):
            raise LhbUpdateError("--market 和 --code 必须同时使用")
        root = updater.find_tdx_root(args.root)
        input_dir = args.input_dir.expanduser().resolve()
        if args.download_masters:
            connection = connect(root, args.host, args.timeout)
            download_resources(
                connection,
                input_dir,
                [view.resource for view in LHB_VIEWS],
            )
            connection.close()
            connection = None
        events = load_master_events(input_dir)
        selected = select_events(
            events, args.event, args.market, args.code, args.all_details
        )
        if args.list:
            visible = selected if (args.event or args.code or args.all_details) else events
            for event in sorted(
                visible,
                key=lambda item: (
                    str(item["dates"][0]) if item["dates"] else "",
                    str(item["event_id"]),
                ),
                reverse=True,
            ):
                keys = ",".join(f"{item[0]}|{item[1]}" for item in event["key_candidates"])
                print(
                    f"{event['event_id']}\t"
                    f"{event['dates'][0] if event['dates'] else ''}\t"
                    f"{keys}\t{','.join(event['views'])}\t"
                    f"{'；'.join(event['event_types'])}"
                )
        if args.download_details:
            if not selected:
                raise LhbUpdateError(
                    "请用 --event、--market/--code 或 --all-details 选择详情"
                )
            if args.max_details and len(selected) > args.max_details:
                raise LhbUpdateError(
                    f"选中 {len(selected)} 个事件，超过 --max-details "
                    f"{args.max_details}；请缩小范围或显式调高上限"
                )
            connection = connect(root, args.host, args.timeout)
            download_resources(
                connection,
                input_dir,
                [f"lhbfx/{item['event_id']}.jsn" for item in selected],
            )
            connection.close()
            connection = None
        model = build_model(root, input_dir)
        output = args.output.expanduser().resolve()
        text = json.dumps(
            model,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        updater.atomic_write_text(output, text + "\n", "utf-8")
        counts = model["counts"]
        print(
            f"已导出 {counts['events']} 个龙虎榜事件、{counts['stocks']} 只证券；"
            f"其中 {counts['detailed_events']} 个事件有营业部明细：{output}",
            file=sys.stderr if args.list else sys.stdout,
        )
    except (
        OSError,
        UnicodeError,
        json.JSONDecodeError,
        catalog.CatalogError,
        downloader.JsnDownloadError,
        transport.DownloadError,
        updater.UpdateError,
        blocks.BlockFormatError,
        LhbUpdateError,
    ) as error:
        print(f"龙虎榜更新失败：{error}", file=sys.stderr)
        return 1
    finally:
        if connection is not None:
            connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Query every downloaded TDX JSN feature associated with one security."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Sequence

import catalog_tdx_jsn as catalog
import download_tdx_jsn as downloader
import update_tdx_blocks as updater
import update_tdx_lhb as lhb_logic
import update_tdx_theme_logic as theme_logic


def row_matches(
    headers: Sequence[str],
    row: Sequence[object],
    market: str,
    code: str,
) -> list[list[str]]:
    matches: list[list[str]] = []
    for market_field, code_field in (
        ("$SC", "$ZQDM"),
        ("$SC1", "$ZQDM1"),
        ("sc", "$ZQDM"),
    ):
        if market_field not in headers or code_field not in headers:
            continue
        if (
            str(row[headers.index(market_field)]) == market
            and str(row[headers.index(code_field)]) == code
        ):
            matches.append([market_field, code_field])
    if "$S_ZQDM" in headers:
        value = row[headers.index("$S_ZQDM")]
        if (market, code) in catalog.member_security_keys(value):
            matches.append(["$S_ZQDM"])
    return matches


def load_stock_connect_holding_lookup(
    input_dir: Path,
) -> dict[str, dict[str, str]]:
    """Map composite quarterly holding keys back to their A-share security."""
    path = input_dir / "list" / "func_hsgt212_1.jsn"
    if not path.is_file():
        return {}
    result: dict[str, dict[str, str]] = {}
    required = {"$ZQDM", "$SC1", "$ZQDM1"}
    for headers, rows in catalog.load_tables(path):
        if not required.issubset(headers):
            continue
        for row in rows:
            record = {
                name: "" if value is None else str(value)
                for name, value in zip(headers, row)
            }
            holding_key = record["$ZQDM"].strip()
            market = record["$SC1"].strip()
            code = record["$ZQDM1"].strip()
            if holding_key and market and code:
                result[holding_key] = {
                    "holding_key": holding_key,
                    "market": market,
                    "code": code,
                    "report_date": record.get("N001", ""),
                    "channel": record.get("jylx", ""),
                }
    return result


def records_from(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        return []
    records: list[dict[str, str]] = []
    for headers, rows in catalog.load_tables(path):
        records.extend(
            {
                name: "" if value is None else str(value)
                for name, value in zip(headers, row)
            }
            for row in rows
        )
    return records


def load_relation_lookups(
    input_dir: Path,
) -> dict[str, dict[str, dict[str, str]]]:
    """Resolve opaque dynamic JSN keys back to client-visible concepts."""
    futures: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "func_qhtj101_1.jsn"):
        market = record.get("$SC", "").strip()
        code = record.get("$ZQDM", "").strip()
        if market and code:
            futures[market + code] = {
                "market": market,
                "contract_code": code,
                "name": record.get("SPQH", ""),
                "date": record.get("DATE", ""),
                "spot_price": record.get("xhjg", ""),
            }

    ipo_industries: dict[str, dict[str, str]] = {}
    for path in sorted((input_dir / "ipotj102").glob("*.jsn")):
        for record in records_from(path):
            key = record.get("$ZQDM", "").strip()
            if key:
                ipo_industries[key] = {
                    "ipo_industry_key": key,
                    "year": record.get("nf", ""),
                    "industry_code": record.get("$ZQDM1", ""),
                    "industry": record.get("hy", ""),
                    "listed_count": record.get("ssjs", ""),
                    "funds_raised": record.get("zmz", ""),
                }

    premium_groups: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "func_bygtj102_1.jsn"):
        key = record.get("$ZQDM", "").strip()
        if key:
            premium_groups[key] = {
                "premium_group_key": key,
                "date": record.get("rq", ""),
                "hundred_yuan_count": record.get("zjs", ""),
                "thousand_yuan_count": record.get("qyjs", ""),
            }

    price_themes: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "func_zjtc101_1.jsn"):
        key = record.get("$ZQDM", "").strip()
        if key:
            price_themes[key] = {
                "price_theme_id": key,
                "name": record.get("name", ""),
                "logic": record.get("qdlj", ""),
                "latest_event": record.get("title", ""),
                "latest_event_date": record.get("date1", ""),
                "commodity_id": record.get("gldm", ""),
            }

    price_events: dict[str, dict[str, str]] = {}
    for path in sorted((input_dir / "zjtc2").glob("*.jsn")):
        theme_id = path.stem
        for record in records_from(path):
            key = record.get("$ZQDM", "").strip()
            if key:
                price_events[key] = {
                    "price_event_id": key,
                    "price_theme_id": theme_id,
                    "date": record.get("date", ""),
                    "name": record.get("name", ""),
                }

    commodities: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "func_zjtc103_1.jsn"):
        key = record.get("$ZQDM", "").strip()
        if key:
            commodities[key] = {
                "commodity_id": key,
                "name": record.get("mc", ""),
                "category": record.get("yjhy", ""),
                "date": record.get("bjrq", ""),
                "latest_price": record.get("zxjg", ""),
                "unit": record.get("jjdw", ""),
            }

    economic_indicators: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "func_jjzb101_1.jsn"):
        key = record.get("$ZQDM", "").strip()
        if key:
            economic_indicators[key] = {
                "economic_indicator_id": key,
                "name": record.get("NAME", ""),
                "category": record.get("lb", ""),
                "date": record.get("date", ""),
                "report_period": record.get("bgq", ""),
                "value": record.get("sz", ""),
                "unit": record.get("dw", ""),
            }

    opportunity_groups: dict[str, dict[str, str]] = {}
    for filename, name_field, category_field, group_type in (
        ("func_ydyl101_1.jsn", "ZLHY", "DLHY", "industry"),
        ("func_ydyl102_1.jsn", "HXDQ", "DLQY", "region"),
    ):
        for record in records_from(input_dir / "list" / filename):
            key = record.get("$ZQDM", "").strip()
            if key:
                opportunity_groups[key] = {
                    "opportunity_group_id": key,
                    "type": group_type,
                    "name": record.get(name_field, ""),
                    "category": record.get(category_field, ""),
                }

    topics: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "func_ztxx101_1.jsn"):
        key = record.get("$ZQDM", "").strip()
        if key:
            topics[key] = {
                "topic_id": key,
                "name": record.get("MC", ""),
                "type": record.get("LX", ""),
                "created_date": record.get("DATE", ""),
                "updated_date": record.get("gxdate", ""),
                "description": record.get("MS", ""),
            }

    strength_intervals: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "func_ygzl101_1.jsn"):
        key = record.get("$ZQDM", "").strip()
        if key:
            strength_intervals[key] = {
                "strength_interval_id": key,
                "market": record.get("$SC1", ""),
                "code": record.get("$ZQDM1", ""),
                "start_date": record.get("sj1", ""),
                "end_date": record.get("sj2", ""),
                "limit_pattern": record.get("jtjb", ""),
                "stock_return": record.get("zf1", ""),
                "index_return": record.get("zf2", ""),
            }

    news_events: dict[str, dict[str, str]] = {}
    for filename, source in (
        ("func_sjqd101_1.jsn", "news_event"),
        ("func_bwyq101_1.jsn", "ministry_event"),
    ):
        for record in records_from(input_dir / "list" / filename):
            key = record.get("$ZQDM", "").strip()
            if key:
                news_events[key] = {
                    "news_event_id": key,
                    "source": source,
                    "date": record.get("date", ""),
                    "title": record.get("title", ""),
                    "type": record.get("type", ""),
                    "ministry": record.get("bw", ""),
                }

    forecast_groups: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "func_yjygtj101_1.jsn"):
        key = record.get("$ZQDM", "").strip()
        if key:
            forecast_groups[key] = {
                "forecast_group_key": key,
                "industry_market": record.get("$SC1", ""),
                "industry_code": record.get("$ZQDM1", ""),
                "report_period": record.get("BGQ", ""),
                "industry_company_count": record.get("HYSL", ""),
            }

    convertible_bonds: dict[str, dict[str, str]] = {}
    for record in records_from(input_dir / "list" / "kzz_kzzsy201_1.jsn"):
        market = record.get("$SC", "").strip()
        code = record.get("$ZQDM", "").strip()
        if market and code:
            convertible_bonds[market + code] = {
                "market": market,
                "code": code,
                "name": record.get("ZQJC", ""),
                "underlying_market": record.get("$SC1", ""),
                "underlying_code": record.get("$ZQDM1", ""),
                "conversion_code": record.get("ZGDM", ""),
                "conversion_price": record.get("ZGJ", ""),
                "maturity_date": record.get("DQRQ", ""),
                "balance": record.get("ZQYE", ""),
            }
    return {
        "futures": futures,
        "ipo_industries": ipo_industries,
        "premium_groups": premium_groups,
        "price_themes": price_themes,
        "price_events": price_events,
        "commodities": commodities,
        "economic_indicators": economic_indicators,
        "opportunity_groups": opportunity_groups,
        "topics": topics,
        "strength_intervals": strength_intervals,
        "news_events": news_events,
        "forecast_groups": forecast_groups,
        "convertible_bonds": convertible_bonds,
    }


def query(
    root: Path,
    input_dir: Path,
    market: str,
    code: str,
) -> dict[str, object]:
    resources = downloader.merge_resource_inventories(
        downloader.inventory_resources(root),
        downloader.inventory_cfg_resources(root),
    )
    try:
        categories, themes = theme_logic.load_theme_masters(input_dir)
        theme_lookup = {
            str(item["theme_id"]): item
            for item in themes
        }
        category_lookup = {
            downloader.normalize_resource_path(str(item["resource"])): item
            for item in categories
        }
    except theme_logic.ThemeLogicError:
        theme_lookup = {}
        category_lookup = {}
    try:
        lhb_events = lhb_logic.load_master_events(input_dir)
        lhb_event_lookup = {
            str(item["event_id"]): item
            for item in lhb_events
        }
    except lhb_logic.LhbUpdateError:
        lhb_event_lookup = {}
    stock_connect_lookup = load_stock_connect_holding_lookup(input_dir)
    relation_lookups = load_relation_lookups(input_dir)
    results: list[dict[str, object]] = []
    for path in sorted(
        input_dir.rglob("*.jsn"),
        key=lambda item: item.relative_to(input_dir).as_posix().casefold(),
    ):
        relative_path = path.relative_to(input_dir).as_posix()
        resource = catalog.identify_resource(relative_path, resources)
        template = resource.resource if resource else relative_path
        source_files = resource.source_files if resource else ()
        parameters = catalog.template_parameters(template, relative_path)
        stock_connect_holding = stock_connect_lookup.get(
            parameters.get("holding_key", "")
        )
        futures_contract = relation_lookups["futures"].get(
            parameters.get("market", "") + parameters.get("code", "")
        ) if template.startswith(("qhtj1/", "qhtj2/")) else None
        ipo_industry = relation_lookups["ipo_industries"].get(
            parameters.get("ipo_industry_key", "")
        )
        premium_group = relation_lookups["premium_groups"].get(
            parameters.get("premium_group_key", "")
        )
        price_event = relation_lookups["price_events"].get(
            parameters.get("price_event_id", "")
        )
        price_theme_id = parameters.get("price_theme_id", "")
        if not price_theme_id and price_event is not None:
            price_theme_id = price_event["price_theme_id"]
        price_theme = relation_lookups["price_themes"].get(price_theme_id)
        commodity_id = parameters.get("commodity_id", "")
        if not commodity_id and price_theme is not None:
            commodity_id = price_theme["commodity_id"]
        commodity = relation_lookups["commodities"].get(commodity_id)
        economic_indicator = relation_lookups["economic_indicators"].get(
            parameters.get("economic_indicator_id", "")
        )
        opportunity_group = relation_lookups["opportunity_groups"].get(
            parameters.get("opportunity_group_id", "")
        )
        topic = relation_lookups["topics"].get(
            parameters.get("topic_id", "")
        )
        strength_interval = relation_lookups["strength_intervals"].get(
            parameters.get("strength_interval_id", "")
        )
        news_event = relation_lookups["news_events"].get(
            parameters.get("news_event_id", "")
        )
        forecast_group = relation_lookups["forecast_groups"].get(
            parameters.get("forecast_group_key", "")
        )
        convertible_bond = (
            relation_lookups["convertible_bonds"].get(
                parameters.get("market", "") + parameters.get("code", "")
            )
            if template.startswith(("kzz_hstk/", "kzz_shtk/", "kzz_xztk/"))
            else None
        )
        bound_match = (
            (
                parameters.get("market") == market
                and parameters.get("code") == code
            )
            or (
                stock_connect_holding is not None
                and stock_connect_holding["market"] == market
                and stock_connect_holding["code"] == code
            )
            or (
                convertible_bond is not None
                and convertible_bond["underlying_market"] == market
                and convertible_bond["underlying_code"] == code
            )
        )
        metadata = catalog.column_metadata(root, source_files, template)
        records: list[dict[str, object]] = []
        match_fields: list[list[str]] = []
        for headers, rows in catalog.load_tables(path):
            for row in rows:
                matches = row_matches(headers, row, market, code)
                if not (matches or bound_match):
                    continue
                records.append(
                    {
                        name: "" if value is None else value
                        for name, value in zip(headers, row)
                    }
                )
                for fields in matches:
                    if fields not in match_fields:
                        match_fields.append(fields)
        if not records:
            continue
        theme = theme_lookup.get(parameters.get("theme_id", ""))
        lhb_event = lhb_event_lookup.get(parameters.get("event_id", ""))
        theme_category = category_lookup.get(relative_path)
        used_fields = list(
            dict.fromkeys(name for record in records for name in record)
        )
        results.append(
            {
                "resource": relative_path,
                "template": template,
                "description": (
                    f"{theme['name']}：逐股入选逻辑"
                    if theme is not None
                    else (
                        f"战略主题大类：{theme_category['name']}"
                        if theme_category is not None
                        else catalog.description_for(template)
                    )
                ),
                "theme": (
                    {
                        "theme_id": theme["theme_id"],
                        "name": theme["name"],
                        "categories": theme["categories"],
                    }
                    if theme is not None
                    else None
                ),
                "theme_category": (
                    {
                        "block_id": theme_category["block_id"],
                        "name": theme_category["name"],
                    }
                    if theme_category is not None
                    else None
                ),
                "lhb_event": (
                    {
                        "event_id": lhb_event["event_id"],
                        "dates": lhb_event["dates"],
                        "views": lhb_event["views"],
                        "event_types": lhb_event["event_types"],
                    }
                    if lhb_event is not None
                    else None
                ),
                "stock_connect_holding": stock_connect_holding,
                "futures_contract": futures_contract,
                "ipo_industry": ipo_industry,
                "premium_group": premium_group,
                "price_theme": price_theme,
                "price_event": price_event,
                "commodity": commodity,
                "economic_indicator": economic_indicator,
                "opportunity_group": opportunity_group,
                "topic": topic,
                "strength_interval": strength_interval,
                "news_event": news_event,
                "forecast_group": forecast_group,
                "convertible_bond": convertible_bond,
                "match_fields": match_fields,
                "template_parameters": parameters,
                "fields": {
                    name: metadata.get(name, {}).get("caption", "")
                    for name in used_fields
                },
                "records": records,
            }
        )
    return {
        "schema": "tdx-jsn-security-features-v1",
        "security": {
            "market": market,
            "code": code,
        },
        "counts": {
            "resources": len(results),
            "records": sum(len(item["records"]) for item in results),
        },
        "resources": results,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "在已下载的 reqformat=11 资源中查询一只证券关联的行业/主题、"
            "财务、资金、两融、公司行动、大宗交易、事件和高管明细。"
            "该工具不联网。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录；默认自动寻找")
    parser.add_argument("--market", required=True, help="通达信市场号，如 0/1/2")
    parser.add_argument("--code", required=True, help="六位证券代码")
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-jsn",
        help="已下载 JSN 目录",
    )
    parser.add_argument("--output", type=Path, help="输出 JSON；默认写标准输出")
    parser.add_argument("--compact", action="store_true", help="输出紧凑 JSON")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        input_dir = args.input_dir.expanduser().resolve()
        if not input_dir.is_dir():
            raise catalog.CatalogError(f"JSN 下载目录不存在：{input_dir}")
        result = query(root, input_dir, str(args.market), args.code)
        text = json.dumps(
            result,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        if args.output:
            output = args.output.expanduser().resolve()
            updater.atomic_write_text(output, text + "\n", "utf-8")
            print(
                f"命中 {result['counts']['resources']} 个资源、"
                f"{result['counts']['records']} 条记录：{output}"
            )
        else:
            print(text)
    except (
        OSError,
        UnicodeError,
        json.JSONDecodeError,
        updater.UpdateError,
        downloader.JsnDownloadError,
        catalog.CatalogError,
        lhb_logic.LhbUpdateError,
        theme_logic.ThemeLogicError,
    ) as error:
        print(f"查询失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

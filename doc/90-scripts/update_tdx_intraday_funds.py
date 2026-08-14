#!/usr/bin/env python3
"""Download TDX intraday main-fund flows and build industry/security indexes."""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
from datetime import datetime
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Mapping, Sequence

import extract_tdx_blocks as blocks
import tdx_pbrpc as pbrpc
import update_tdx_blocks as updater


class IntradayFundError(RuntimeError):
    """Raised when the segmented-fund service result cannot be normalized."""


PERIODS = (
    ("today", "今日", 1, None),
    ("before-09:35", "9:35之前", 7, 5),
    ("before-10:30", "10:30之前", 2, 60),
    ("10:30-11:30", "10:30-11:30", 3, 60),
    ("13:00-14:00", "13:00-14:00", 4, 60),
    ("14:00-15:00", "14:00-15:00", 5, 60),
    ("last-30-minutes", "尾盘30分钟", 6, 30),
)


def response_rows(value: object) -> list[dict[str, object]]:
    if not isinstance(value, dict):
        raise IntradayFundError("PBRPC result is not an object")
    error_code = value.get("ErrorCode", 0)
    if error_code not in (0, "0", None):
        raise IntradayFundError(
            f"server returned {error_code}: {value.get('ErrorInfo', '')}"
        )
    result_sets = value.get("ResultSets", [])
    if not isinstance(result_sets, list):
        raise IntradayFundError("ResultSets is not a list")
    records: list[dict[str, object]] = []
    for result_set in result_sets:
        if not isinstance(result_set, dict):
            continue
        columns = result_set.get("ColDes", [])
        rows = result_set.get("Content", [])
        if not isinstance(columns, list) or not isinstance(rows, list):
            raise IntradayFundError("invalid result-set columns or rows")
        names = [
            str(item.get("Name", "")) if isinstance(item, dict) else str(item)
            for item in columns
        ]
        if not names or any(not name for name in names):
            raise IntradayFundError("result set has an unnamed column")
        for row in rows:
            if not isinstance(row, list) or len(row) != len(names):
                raise IntradayFundError("result row width does not match columns")
            records.append(dict(zip(names, row)))
    return records


def decimal_value(value: object) -> Decimal | None:
    try:
        return Decimal(str(value))
    except (InvalidOperation, ValueError):
        return None


def relative_volume(
    volume: object,
    baseline: object,
    minutes: int | None,
    *,
    detail: bool,
) -> float | None:
    if minutes is None:
        return None
    actual = decimal_value(volume)
    base = decimal_value(baseline)
    if actual is None or base in (None, 0):
        return None
    # The client XML explicitly multiplies stock detail volumes by 100 because
    # cjl is in shares while q5rjl is in lots. Index/master rows use one unit.
    multiplier = Decimal(100 if detail else 1)
    return float(actual * multiplier / Decimal(minutes) / base)


def normalize_record(
    record: Mapping[str, object],
    security_master: Mapping[tuple[int, str], blocks.Security],
    *,
    detail: bool,
) -> dict[str, object]:
    market_text = str(record.get("market", "")).strip()
    code = str(record.get("code", "")).strip()
    try:
        market_id = int(market_text)
    except ValueError as error:
        raise IntradayFundError(f"invalid market value: {market_text!r}") from error
    if not code:
        raise IntradayFundError("fund-flow row has no code")
    security = security_master.get((market_id, code))
    market_prefix = blocks.MARKETS.get(market_id, (str(market_id), ""))[0]
    periods: dict[str, object] = {}
    for key, name, suffix, minutes in PERIODS:
        period: dict[str, object] = {
            "name": name,
            "net_main_inflow": record.get(f"jlr_{suffix}", ""),
            "turnover": record.get(f"cje_{suffix}", ""),
            "net_main_share_pct": record.get(f"zlzb_{suffix}", ""),
        }
        if suffix != 1:
            period.update({
                "change_pct": record.get(f"zf_{suffix}", ""),
                "volume": record.get(f"cjl_{suffix}", ""),
                "relative_volume": relative_volume(
                    record.get(f"cjl_{suffix}"),
                    record.get("q5rjl"),
                    minutes,
                    detail=detail,
                ),
            })
        periods[key] = period
    return {
        "market": market_text,
        "code": code,
        "security_id": f"{market_prefix}{code}",
        "name": security.name if security else "",
        "name_resolved": security is not None,
        "quote": {
            "last": record.get("xj", ""),
            "change_pct": record.get("zdf", ""),
            "change": record.get("zd", ""),
            "previous_5day_minute_volume": record.get("q5rjl", ""),
        },
        "periods": periods,
    }


def industry_keys(master_rows: Sequence[Mapping[str, object]]) -> list[tuple[str, str]]:
    return [
        (str(row.get("market", "")).strip(), str(row.get("code", "")).strip())
        for row in master_rows
        if str(row.get("code", "")).startswith("881")
    ]


def parse_requested_industries(
    values: Sequence[str],
    available: Sequence[tuple[str, str]],
) -> list[tuple[str, str]]:
    available_set = set(available)
    by_code: dict[str, list[tuple[str, str]]] = {}
    for item in available:
        by_code.setdefault(item[1], []).append(item)
    selected: list[tuple[str, str]] = []
    for value in values:
        text = value.strip()
        if ":" in text:
            market, code = text.split(":", 1)
            item = (market.strip(), code.strip())
            if item not in available_set:
                raise IntradayFundError(f"industry is not in ReqId 200340: {text}")
        else:
            matches = by_code.get(text, [])
            if len(matches) != 1:
                raise IntradayFundError(
                    f"industry code is missing or ambiguous in ReqId 200340: {text}"
                )
            item = matches[0]
        if item not in selected:
            selected.append(item)
    return selected


def build_model(
    master_response: object,
    detail_responses: Mapping[tuple[str, str], object],
    security_master: Mapping[tuple[int, str], blocks.Security],
    *,
    generated_at: str | None = None,
) -> dict[str, object]:
    raw_master = response_rows(master_response)
    normalized_master = [
        normalize_record(row, security_master, detail=False)
        for row in raw_master
    ]
    master_by_key = {
        (str(row["market"]), str(row["code"])): row
        for row in normalized_master
    }
    industries: list[dict[str, object]] = []
    reverse: dict[tuple[str, str], dict[str, object]] = {}
    component_records = 0
    for key, response in detail_responses.items():
        if key not in master_by_key:
            raise IntradayFundError(
                f"detail response has no matching 200340 industry: {key[0]}:{key[1]}"
            )
        components = [
            normalize_record(row, security_master, detail=True)
            for row in response_rows(response)
        ]
        component_records += len(components)
        master = master_by_key[key]
        industries.append({
            "market": key[0],
            "code": key[1],
            "security_id": master["security_id"],
            "name": master["name"],
            "summary": master,
            "component_count": len(components),
            "components": components,
        })
        for component_index, component in enumerate(components):
            security_key = (str(component["market"]), str(component["code"]))
            item = reverse.setdefault(security_key, {
                "market": component["market"],
                "code": component["code"],
                "security_id": component["security_id"],
                "name": component["name"],
                "name_resolved": component["name_resolved"],
                "matches": [],
            })
            item["matches"].append({
                "industry_market": key[0],
                "industry_code": key[1],
                "industry_name": master["name"],
                "component_index": component_index,
            })
    industries.sort(key=lambda item: (str(item["market"]), str(item["code"])))
    securities = [reverse[key] for key in sorted(reverse)]
    return {
        "schema": "tdx-intraday-funds-v1",
        "generated_at": generated_at or datetime.now().astimezone().isoformat(),
        "source": {
            "config": "T0002/cloud_cfg/sszjtj.xml",
            "master_request_id": "200340",
            "detail_request_id": "200341",
            "module": "mod_peg.dll",
            "periods": [
                {"key": key, "name": name, "field_suffix": suffix}
                for key, name, suffix, _ in PERIODS
            ],
        },
        "counts": {
            "market_rows": len(normalized_master),
            "available_industries": len(industry_keys(raw_master)),
            "expanded_industries": len(industries),
            "component_records": component_records,
            "securities": len(securities),
            "multi_industry_securities": sum(
                len(item["matches"]) > 1 for item in securities
            ),
        },
        "market_rows": normalized_master,
        "industries": industries,
        "securities": securities,
    }


def query(
    spec: pbrpc.RequestSpec,
    request: Mapping[str, object],
    *,
    base_url: str,
    timeout: float,
    max_rounds: int,
    retry_delay: float,
    verbose: bool,
) -> object:
    raw = pbrpc.query_pbrpc(
        spec.entry,
        spec.module,
        dict(request),
        base_url=base_url,
        timeout=timeout,
        max_rounds=max_rounds,
        retry_delay=retry_delay,
        verbose=verbose,
    )
    return pbrpc.decode_result(raw)


def fetch(
    root: Path,
    requested: Sequence[str],
    *,
    master_only: bool,
    base_url: str,
    timeout: float,
    max_rounds: int,
    retry_delay: float,
    verbose: bool,
) -> tuple[object, dict[tuple[str, str], object]]:
    master_spec = pbrpc.find_config_spec(root, "200340", source_file="sszjtj.xml")
    master_request = dict(master_spec.request)
    master_request["ReqId"] = "200340"
    master_response = query(
        master_spec,
        master_request,
        base_url=base_url,
        timeout=timeout,
        max_rounds=max_rounds,
        retry_delay=retry_delay,
        verbose=verbose,
    )
    available = industry_keys(response_rows(master_response))
    if master_only:
        selected: list[tuple[str, str]] = []
    elif requested:
        selected = parse_requested_industries(requested, available)
    else:
        selected = available
    # Resolve the XML master-row placeholders first; the actual values are
    # supplied per request below through code_hy/market_hy.
    detail_spec = pbrpc.find_config_spec(
        root,
        "200341",
        source_file="sszjtj.xml",
        replacements={"code": "0", "market": "0"},
    )
    details: dict[tuple[str, str], object] = {}
    for market, code in selected:
        request = dict(detail_spec.request)
        request.update({
            "ReqId": "200341",
            "code_hy": code,
            "market_hy": market,
            "Page": "-1",
        })
        details[(market, code)] = query(
            detail_spec,
            request,
            base_url=base_url,
            timeout=timeout,
            max_rounds=max_rounds,
            retry_delay=retry_delay,
            verbose=verbose,
        )
    return master_response, details


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "更新通达信市场/行业及成分股的分时段主力资金，生成行业和股票双向索引。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument(
        "--industry", action="append", default=[], metavar="[MARKET:]CODE",
        help="只展开指定行业；可重复。默认展开 200340 返回的全部 881xxx 行业",
    )
    parser.add_argument(
        "--master-only", action="store_true", help="只更新市场/行业总表，不查询成分股",
    )
    parser.add_argument(
        "--download", action="store_true", help="明确允许联网；省略时只显示计划",
    )
    parser.add_argument("--base-url", default=pbrpc.DEFAULT_BASE_URL)
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--max-rounds", type=int, default=32)
    parser.add_argument("--retry-delay", type=float, default=0.15)
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-intraday-funds.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        output = args.output.expanduser().resolve()
        if not args.download:
            print(json.dumps({
                "action": "dry-run",
                "master_request_id": "200340",
                "detail_request_id": None if args.master_only else "200341",
                "industries": args.industry or "all 881xxx rows returned by 200340",
                "output": str(output),
            }, ensure_ascii=False, indent=2))
            return 0
        security_master = blocks.load_security_master(root / "T0002" / "hq_cache")
        master_response, details = fetch(
            root,
            args.industry,
            master_only=args.master_only,
            base_url=args.base_url,
            timeout=args.timeout,
            max_rounds=args.max_rounds,
            retry_delay=args.retry_delay,
            verbose=args.verbose,
        )
        model = build_model(master_response, details, security_master)
        rendered = json.dumps(
            model,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        updater.atomic_write_text(output, rendered + "\n", "utf-8")
        counts = model["counts"]
        print(
            f"已更新 {counts['market_rows']} 条市场/行业资金、"
            f"{counts['expanded_industries']} 个行业、"
            f"{counts['component_records']} 条成分记录、"
            f"{counts['securities']} 只证券：{output}"
        )
    except (
        OSError,
        UnicodeError,
        ValueError,
        json.JSONDecodeError,
        urllib.error.URLError,
        blocks.BlockFormatError,
        updater.UpdateError,
        pbrpc.PBRPCError,
        IntradayFundError,
    ) as error:
        print(f"分时段资金更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Update per-stock institution details and cross-stock holder histories."""

from __future__ import annotations

import argparse
import json
import re
import sys
import urllib.parse
from collections import defaultdict
from pathlib import Path
from typing import Sequence

import catalog_tdx_jsn as catalog
import download_tdx_jsn as downloader
import download_tdx_minute as transport
import extract_tdx_blocks as blocks
import tdx_tqlex
import update_tdx_blocks as updater


SUMMARY_TEMPLATE = "cgfxmx1/$$$SC$$$$$ZQDM$$.jsn"
HOLDERS_TEMPLATE = "cgfxmx2/$$$SC$$$$$ZQDM$$.jsn"
HOLDER_ENTRY = "CWServ.tdxf10_gg_gdyjcgmx"
DEFAULT_HOLDER_BASE_URL = "http://page1.tdx.com.cn:7615/TQLEX"
DETAIL_STEM_RE = re.compile(r"^(?P<market>\d)(?P<code>\d{6})$")
SUMMARY_REQUIRED = {
    "bgq", "lb", "jgs", "ccgs", "cczlt", "cczzg", "ccsz",
    "zjgs", "zjbl", "zjzlt", "zjzzg",
}
HOLDERS_REQUIRED = {
    "gdpm", "gdlx", "gdmc", "gdjc", "cgsl", "zb1", "zjgs", "zl",
}
HISTORY_FIELDS = {
    "T001": "row_id",
    "bh": "currently_held",
    "cnt": "stock_record_count",
    "rq": "report_date",
    "sc": "market",
    "zqdm": "code",
    "zqjc": "name",
    "T006": "holding_shares",
    "T007": "share_percent",
    "stype": "shareholder_list_type",
    "T012": "share_nature",
    "T008": "change_shares",
    "T009": "change_type",
}
CHANGE_TYPE_LABELS = {
    "1": "新进",
    "2": "不变",
    "3": "增持",
    "4": "减持",
}


class InstitutionUpdateError(ValueError):
    """Raised when institution detail or holder history data is inconsistent."""


def repair_detail_text(value: object) -> object:
    """Repair UTF-8 text decoded as GB18030 by the ``gdjcxq`` endpoint."""
    if not isinstance(value, str):
        return value
    try:
        return value.encode("gb18030").decode("utf-8")
    except (UnicodeEncodeError, UnicodeDecodeError):
        return value


def validate_security(market: str, code: str) -> tuple[str, str]:
    if not re.fullmatch(r"\d", market):
        raise InstitutionUpdateError(f"市场号必须是一位数字：{market!r}")
    if not re.fullmatch(r"\d{6}", code):
        raise InstitutionUpdateError(f"证券代码必须是六位数字：{code!r}")
    return market, code


def detail_resource(template: str, market: str, code: str) -> str:
    validate_security(market, code)
    return downloader.normalize_resource_path(
        downloader.expand_template(template, market=int(market), code=code)
    )


def table_records(path: Path, required: set[str]) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for headers, rows in catalog.load_tables(path):
        missing = required - set(headers)
        if missing:
            raise InstitutionUpdateError(
                f"{path.name}: 缺少字段 {', '.join(sorted(missing))}"
            )
        result.extend(
            {
                name: "" if value is None else value
                for name, value in zip(headers, row)
            }
            for row in rows
        )
    return result


def parse_holder_reference(url: str) -> dict[str, str]:
    parsed = urllib.parse.urlsplit(url)
    query = urllib.parse.parse_qs(parsed.query, keep_blank_values=True)

    def first(name: str) -> str:
        values = query.get(name, [])
        return values[0].strip() if values else ""

    return {
        "holder_id": first("gdid"),
        "holder_variant_id": first("tdxid"),
        "holder_name": first("gdname"),
        "reference_code": first("gp"),
        "source_url": url,
    }


def holder_key(holder_id: str, variant_id: str) -> str:
    return f"{holder_id}:{variant_id}" if variant_id else holder_id


def load_stock_detail(
    input_dir: Path,
    market: str,
    code: str,
) -> dict[str, object]:
    validate_security(market, code)
    summary_resource = detail_resource(SUMMARY_TEMPLATE, market, code)
    holders_resource = detail_resource(HOLDERS_TEMPLATE, market, code)
    summary_path = input_dir.joinpath(*Path(summary_resource).parts)
    holders_path = input_dir.joinpath(*Path(holders_resource).parts)
    if not summary_path.is_file() or not holders_path.is_file():
        raise InstitutionUpdateError(
            f"缺少 {market}|{code} 机构详情；请先使用 --download-details"
        )
    summaries = table_records(summary_path, SUMMARY_REQUIRED)
    raw_holders = table_records(holders_path, HOLDERS_REQUIRED)
    holders: list[dict[str, object]] = []
    for row in raw_holders:
        reference = parse_holder_reference(str(row["gdjc"]))
        key = holder_key(
            reference["holder_id"], reference["holder_variant_id"]
        )
        holders.append(
            {
                "holder_key": key,
                **reference,
                "rank": row["gdpm"],
                "holder_type": row["gdlx"],
                "name": row["gdmc"],
                "holding_shares": row["cgsl"],
                "float_share": row["zb1"],
                "change_shares": row["zjgs"],
                "change_status": row["zl"],
            }
        )
    return {
        "market": market,
        "code": code,
        "summary_resource": summary_resource,
        "holders_resource": holders_resource,
        "history": summaries,
        "current_holders": holders,
    }


def discover_local_securities(input_dir: Path) -> list[tuple[str, str]]:
    stems: set[str] = set()
    for directory_name in ("cgfxmx1", "cgfxmx2"):
        directory = input_dir / directory_name
        if directory.is_dir():
            stems.update(path.stem for path in directory.glob("*.jsn"))
    result: list[tuple[str, str]] = []
    for stem in sorted(stems):
        match = DETAIL_STEM_RE.fullmatch(stem)
        if not match:
            continue
        market, code = match.group("market"), match.group("code")
        if (
            (input_dir / "cgfxmx1" / f"{stem}.jsn").is_file()
            and (input_dir / "cgfxmx2" / f"{stem}.jsn").is_file()
        ):
            result.append((market, code))
    return result


def normalize_holder_history(
    response: dict[str, object],
    holder: dict[str, object],
) -> dict[str, object]:
    result_sets = response.get("ResultSets", [])
    if not isinstance(result_sets, list) or not result_sets:
        raise InstitutionUpdateError("股东持仓响应没有 ResultSets")
    result_set = result_sets[0]
    if not isinstance(result_set, dict):
        raise InstitutionUpdateError("股东持仓结果集不是对象")
    headers = result_set.get("ColName", [])
    rows = result_set.get("Content", [])
    if not isinstance(headers, list) or not isinstance(rows, list):
        raise InstitutionUpdateError("股东持仓结果集缺少 ColName/Content")
    records: list[dict[str, object]] = []
    for index, row in enumerate(rows):
        if not isinstance(row, list) or len(row) != len(headers):
            raise InstitutionUpdateError(f"股东持仓第 {index} 行列数不符")
        raw = dict(zip((str(item) for item in headers), row))
        record = {
            HISTORY_FIELDS.get(name, name): value
            for name, value in raw.items()
        }
        record["change_status"] = CHANGE_TYPE_LABELS.get(
            str(record.get("change_type", "")), ""
        )
        records.append(record)
    unique_stocks = {
        (str(item.get("market", "")), str(item.get("code", "")))
        for item in records
        if item.get("code")
    }
    return {
        "schema": "tdx-holder-history-v1",
        "holder": {
            key: holder.get(key, "")
            for key in (
                "holder_key", "holder_id", "holder_variant_id", "name",
                "holder_type", "reference_code",
            )
        },
        "counts": {
            "records": len(records),
            "stocks": len(unique_stocks),
            "detailed_stocks": 0,
            "detail_records": 0,
        },
        "records": records,
        "stock_details": {},
    }


def normalize_holder_stock_detail(
    response: dict[str, object],
    code: str,
) -> list[dict[str, object]]:
    if not re.fullmatch(r"\d{6}", code):
        raise InstitutionUpdateError(f"证券代码必须是六位数字：{code!r}")
    result_sets = response.get("ResultSets", [])
    if not isinstance(result_sets, list) or not result_sets:
        raise InstitutionUpdateError("股东单证券明细响应没有 ResultSets")
    result_set = result_sets[0]
    if not isinstance(result_set, dict):
        raise InstitutionUpdateError("股东单证券结果集不是对象")
    headers = result_set.get("ColName", [])
    rows = result_set.get("Content", [])
    if not isinstance(headers, list) or not isinstance(rows, list):
        raise InstitutionUpdateError("股东单证券结果集缺少 ColName/Content")
    records: list[dict[str, object]] = []
    for index, row in enumerate(rows):
        if not isinstance(row, list) or len(row) != len(headers):
            raise InstitutionUpdateError(f"股东单证券第 {index} 行列数不符")
        raw = dict(zip((str(item) for item in headers), row))
        record = {
            HISTORY_FIELDS.get(name, name): repair_detail_text(value)
            for name, value in raw.items()
        }
        record["code"] = code
        record["change_status"] = CHANGE_TYPE_LABELS.get(
            str(record.get("change_type", "")), ""
        )
        records.append(record)
    return records


def select_holders(
    stocks: Sequence[dict[str, object]],
    selectors: Sequence[str],
    all_holders: bool,
) -> list[dict[str, object]]:
    unique: dict[str, dict[str, object]] = {}
    for stock in stocks:
        for holder in stock["current_holders"]:
            key = str(holder["holder_key"])
            if not key:
                continue
            candidate = dict(holder)
            candidate["seed_market"] = stock["market"]
            candidate["seed_code"] = stock["code"]
            unique.setdefault(key, candidate)
    if all_holders:
        return list(unique.values())
    needles = [item.casefold() for item in selectors]
    return [
        holder
        for holder in unique.values()
        if any(
            needle == str(holder["holder_key"]).casefold()
            or needle == str(holder["holder_id"]).casefold()
            or needle in str(holder["name"]).casefold()
            for needle in needles
        )
    ]


def safe_holder_filename(key: str) -> str:
    name = re.sub(r"[^A-Za-z0-9_.-]+", "_", key).strip("._")
    if not name:
        raise InstitutionUpdateError(f"股东键不能生成文件名：{key!r}")
    return f"{name}.json"


def download_holder_history(
    holder: dict[str, object],
    output_dir: Path,
    *,
    base_url: str,
    timeout: float,
    stock_codes: Sequence[str] = (),
) -> Path:
    response = tdx_tqlex.query_tqlex(
        HOLDER_ENTRY,
        {
            "Params": [
                "gdjc",
                str(holder["seed_code"]),
                str(holder["holder_id"]),
                str(holder["holder_variant_id"]),
                "1",
            ]
        },
        base_url=base_url,
        timeout=timeout,
    )
    normalized = normalize_holder_history(response, holder)
    for code in stock_codes:
        detail_response = tdx_tqlex.query_tqlex(
            HOLDER_ENTRY,
            {
                "Params": [
                    "gdjcxq",
                    code,
                    str(holder["holder_id"]),
                    str(holder["holder_variant_id"]),
                    "1",
                ]
            },
            base_url=base_url,
            timeout=timeout,
        )
        normalized["stock_details"][code] = normalize_holder_stock_detail(
            detail_response, code
        )
    normalized["counts"]["detailed_stocks"] = len(
        normalized["stock_details"]
    )
    normalized["counts"]["detail_records"] = sum(
        len(records) for records in normalized["stock_details"].values()
    )
    output = output_dir / safe_holder_filename(str(holder["holder_key"]))
    updater.atomic_write_text(
        output,
        json.dumps(normalized, ensure_ascii=False, indent=2) + "\n",
        "utf-8",
    )
    return output


def connect(
    root: Path,
    hosts: Sequence[str],
    timeout: float,
) -> transport.QuoteConnection:
    endpoints = (
        transport.unique_endpoints(
            transport.parse_endpoint(value) for value in hosts
        )
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


def load_holder_history(path: Path) -> dict[str, object]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("schema") != "tdx-holder-history-v1":
        raise InstitutionUpdateError(f"股东历史文件格式无效：{path}")
    return value


def build_model(
    root: Path,
    input_dir: Path,
    holder_dir: Path,
) -> dict[str, object]:
    stocks = [
        load_stock_detail(input_dir, market, code)
        for market, code in discover_local_securities(input_dir)
    ]
    security_master = blocks.load_security_master(root / "T0002" / "hq_cache")
    holder_positions: dict[str, list[dict[str, object]]] = defaultdict(list)
    holder_metadata: dict[str, dict[str, object]] = {}
    for stock in stocks:
        market, code = str(stock["market"]), str(stock["code"])
        security = security_master.get((int(market), code))
        stock["name"] = security.name if security else ""
        for holder in stock["current_holders"]:
            key = str(holder["holder_key"])
            if not key:
                continue
            holder_metadata.setdefault(key, dict(holder))
            holder_positions[key].append(
                {
                    "market": market,
                    "code": code,
                    "name": stock["name"],
                    "rank": holder["rank"],
                    "holding_shares": holder["holding_shares"],
                    "float_share": holder["float_share"],
                    "change_shares": holder["change_shares"],
                    "change_status": holder["change_status"],
                }
            )
    holders: list[dict[str, object]] = []
    holder_history_rows = 0
    holder_history_stocks: set[tuple[str, str]] = set()
    for key in sorted(holder_metadata, key=str.casefold):
        metadata = holder_metadata[key]
        history_path = holder_dir / safe_holder_filename(key)
        history = load_holder_history(history_path) if history_path.is_file() else None
        if history:
            holder_history_rows += int(history["counts"]["records"])
            holder_history_stocks.update(
                (str(item.get("market", "")), str(item.get("code", "")))
                for item in history["records"]
                if item.get("code")
            )
        holders.append(
            {
                "holder_key": key,
                "holder_id": metadata["holder_id"],
                "holder_variant_id": metadata["holder_variant_id"],
                "name": metadata["name"],
                "holder_type": metadata["holder_type"],
                "current_positions": holder_positions[key],
                "history_file": str(history_path) if history else "",
                "history": history,
            }
        )
    return {
        "schema": "tdx-institution-details-v1",
        "counts": {
            "stocks": len(stocks),
            "stock_history_rows": sum(len(stock["history"]) for stock in stocks),
            "current_holder_rows": sum(
                len(stock["current_holders"]) for stock in stocks
            ),
            "unique_holders": len(holders),
            "holders_with_history": sum(
                1 for holder in holders if holder["history"] is not None
            ),
            "holder_history_rows": holder_history_rows,
            "holder_history_stocks": len(holder_history_stocks),
        },
        "stocks": stocks,
        "holders": holders,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "更新单证券历期机构分类和十大流通股东，并可按股东 ID 查询其"
            "跨股票、跨报告期持仓轨迹。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录；默认自动寻找")
    parser.add_argument("--download-masters", action="store_true", help="更新 24 张机构持仓主表")
    parser.add_argument("--download-details", action="store_true", help="更新指定证券的两张机构详情")
    parser.add_argument("--market", help="与 --code 一起指定证券市场号")
    parser.add_argument("--code", help="与 --market 一起指定六位证券代码")
    parser.add_argument(
        "--download-holder-history", action="store_true",
        help="下载 --holder 或 --all-holders 选中的跨股票持仓历史",
    )
    parser.add_argument(
        "--holder", action="append", default=[],
        help="按股东 ID、股东键或名称子串选择；可重复",
    )
    parser.add_argument("--all-holders", action="store_true", help="选择本地详情中的全部股东")
    parser.add_argument(
        "--holder-stock", action="append", default=[], metavar="CODE",
        help="为选中的股东继续下载某只股票的逐报告期持仓；可重复",
    )
    parser.add_argument(
        "--max-holders", type=int, default=10,
        help="单次股东历史请求上限；0 表示不限制（默认 10）",
    )
    parser.add_argument("--list-holders", action="store_true", help="列出选中的股东")
    parser.add_argument("--host", action="append", default=[], help="指定 7709 host[:port]；可重复")
    parser.add_argument("--timeout", type=float, default=15.0, help="网络超时秒数")
    parser.add_argument(
        "--holder-base-url", default=DEFAULT_HOLDER_BASE_URL,
        help="股东历史 TQLEX 地址",
    )
    parser.add_argument(
        "--input-dir", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-jsn",
        help="JSN 下载目录",
    )
    parser.add_argument(
        "--holder-dir", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-holder-history",
        help="股东历史目录",
    )
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-institution-details.json",
        help="机构—股票—股东模型输出",
    )
    parser.add_argument("--compact", action="store_true", help="输出紧凑 JSON")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    connection: transport.QuoteConnection | None = None
    try:
        if args.max_holders < 0:
            raise InstitutionUpdateError("--max-holders 不能为负数")
        if (args.market is None) != (args.code is None):
            raise InstitutionUpdateError("--market 和 --code 必须同时使用")
        if args.market is not None:
            validate_security(args.market, args.code)
        for code in args.holder_stock:
            if not re.fullmatch(r"\d{6}", code):
                raise InstitutionUpdateError(
                    f"--holder-stock 必须是六位证券代码：{code!r}"
                )
        root = updater.find_tdx_root(args.root)
        input_dir = args.input_dir.expanduser().resolve()
        holder_dir = args.holder_dir.expanduser().resolve()
        resources: list[str] = []
        if args.download_masters:
            resources.extend(
                f"list/{name}"
                for name in downloader.VERIFIED_CFG_FAMILIES["institution-holdings"]
            )
        if args.download_details:
            if args.market is None:
                raise InstitutionUpdateError(
                    "--download-details 需要 --market 和 --code"
                )
            resources.extend(
                detail_resource(template, args.market, args.code)
                for template in (SUMMARY_TEMPLATE, HOLDERS_TEMPLATE)
            )
        if resources:
            connection = connect(root, args.host, args.timeout)
            download_resources(connection, input_dir, resources)
            connection.close()
            connection = None
        stocks = [
            load_stock_detail(input_dir, market, code)
            for market, code in discover_local_securities(input_dir)
        ]
        selected = select_holders(stocks, args.holder, args.all_holders)
        if args.list_holders:
            visible = selected if (args.holder or args.all_holders) else select_holders(
                stocks, (), True
            )
            for holder in visible:
                print(
                    f"{holder['holder_key']}\t{holder['name']}\t"
                    f"{holder['holder_type']}\t"
                    f"{holder['seed_market']}|{holder['seed_code']}"
                )
        if args.download_holder_history:
            if not selected:
                raise InstitutionUpdateError(
                    "请用 --holder 或 --all-holders 选择股东"
                )
            if args.max_holders and len(selected) > args.max_holders:
                raise InstitutionUpdateError(
                    f"选中 {len(selected)} 个股东，超过 --max-holders "
                    f"{args.max_holders}；请缩小范围或显式调高上限"
                )
            for holder in selected:
                output = download_holder_history(
                    holder,
                    holder_dir,
                    base_url=args.holder_base_url,
                    timeout=args.timeout,
                    stock_codes=args.holder_stock,
                )
                print(f"已更新股东 {holder['name']}：{output}", file=sys.stderr)
        model = build_model(root, input_dir, holder_dir)
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
            f"已导出 {counts['stocks']} 只证券、{counts['unique_holders']} 个股东；"
            f"股东历史 {counts['holder_history_rows']} 条：{output}",
            file=sys.stderr if args.list_holders else sys.stdout,
        )
    except (
        OSError,
        UnicodeError,
        json.JSONDecodeError,
        updater.UpdateError,
        downloader.JsnDownloadError,
        catalog.CatalogError,
        tdx_tqlex.TQLEXError,
        InstitutionUpdateError,
    ) as error:
        print(f"更新失败：{error}", file=sys.stderr)
        return 1
    finally:
        if connection is not None:
            connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

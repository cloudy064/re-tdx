#!/usr/bin/env python3
"""Update TDX strategic-theme masters and optional per-stock logic details."""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

import download_tdx_jsn as downloader
import download_tdx_minute as transport
import extract_tdx_blocks as blocks
import update_tdx_blocks as updater


@dataclass(frozen=True)
class ThemeCategory:
    block_id: str
    name: str
    config_name: str
    resource: str


THEME_CATEGORIES = (
    ThemeCategory("Z01", "国防军工", "func_gfjg101", "list/func_gfjg101_1.jsn"),
    ThemeCategory("Z03", "健康中国", "func_jkzg101", "list/func_jkzg101_1.jsn"),
    ThemeCategory("Z04", "美丽中国", "func_mlzg101", "list/func_mlzg101_1.jsn"),
    ThemeCategory("Z10", "交通强国", "func_jtqg101", "list/func_jtqg101_1.jsn"),
    ThemeCategory("Z05", "工业4.0", "func_gy101", "list/func_gy101_1.jsn"),
    ThemeCategory("Z06", "汽车产业", "func_qccy101", "list/func_qccy101_1.jsn"),
    ThemeCategory("Z07", "人工智能", "func_rgzn101", "list/func_rgzn101_1.jsn"),
    ThemeCategory("Z08", "5G6G", "func_5G101", "list/func_5G101_1.jsn"),
    ThemeCategory("Z09", "新基建", "func_xjj101", "list/func_xjj101_1.jsn"),
    ThemeCategory("Z11", "大金融", "func_djr101", "list/func_djr101_1.jsn"),
    ThemeCategory("Z12", "非接触经济", "func_fjcjj101", "list/func_fjcjj101_1.jsn"),
    ThemeCategory("Z13", "半导体", "func_bdt101", "list/func_bdt101_1.jsn"),
    ThemeCategory("Z14", "碳中和", "func_tzh_1", "list/func_tzh_1.jsn"),
    ThemeCategory("Z16", "数字经济", "func_szjj101", "list/func_szjj101_1.jsn"),
    ThemeCategory("Z17", "算力产业", "func_slcy101", "list/func_slcy101_1.jsn"),
    ThemeCategory("Z18", "机器人", "func_jqr101", "list/func_jqr101_1.jsn"),
    ThemeCategory("Z19", "大消费", "func_dxf101", "list/func_dxf101_1.jsn"),
    ThemeCategory("Z20", "风光锂储", "func_fglc101", "list/func_fglc101_1.jsn"),
    ThemeCategory("Z23", "新材料", "func_xcl101", "list/func_xcl101_1.jsn"),
    ThemeCategory("Z24", "新型电力", "func_xxdl101", "list/func_xxdl101_1.jsn"),
    ThemeCategory("Z25", "国产软件", "func_gcrj101", "list/func_gcr101_1.jsn"),
    ThemeCategory("Z27", "大周期", "func_dzq101", "list/func_dzq101_1.jsn"),
    ThemeCategory("Z30", "农业安全", "func_nyaq101", "list/func_nyaq101_1.jsn"),
    ThemeCategory("Z31", "地产链", "func_dcl101", "list/func_dcl101_1.jsn"),
)

REQUIRED_MASTER_HEADERS = {"gname", "$S_ZQDM", "S_NUM", "$ZQDM"}
REQUIRED_DETAIL_HEADERS = {
    "$ZQDM",
    "$SC",
    "fqprice_d3",
    "fqprice_d5",
    "fqprice_d20",
    "fqprice_d60",
    "price1",
    "tzlj",
    "xxsm",
}


class ThemeLogicError(ValueError):
    """Raised when theme configuration or JSN data is inconsistent."""


def parse_member_tokens(value: str) -> tuple[tuple[int, str], ...]:
    members: list[tuple[int, str]] = []
    for token in value.split(","):
        token = token.strip()
        if not token:
            continue
        market_text, separator, code = token.partition("|")
        if not separator or not code.strip():
            raise ThemeLogicError(f"主题成分格式无效：{token!r}")
        try:
            market_id = int(market_text.strip())
        except ValueError as error:
            raise ThemeLogicError(f"主题市场号无效：{token!r}") from error
        members.append((market_id, code.strip()))
    return tuple(members)


def parse_members(value: str) -> tuple[tuple[int, str], ...]:
    return tuple(dict.fromkeys(parse_member_tokens(value)))


def load_group(path: Path, required: set[str]) -> tuple[list[str], list[list[object]]]:
    value = json.loads(path.read_bytes().decode("gb18030"))
    if not isinstance(value, list) or len(value) != 1:
        raise ThemeLogicError(f"{path.name}: 期望一个结果集")
    group = value[0]
    if not isinstance(group, dict):
        raise ThemeLogicError(f"{path.name}: 结果集不是对象")
    headers = group.get("colheader")
    rows = group.get("data")
    if not isinstance(headers, list) or not isinstance(rows, list):
        raise ThemeLogicError(f"{path.name}: 缺少 colheader/data")
    names = [str(item) for item in headers]
    missing = required - set(names)
    if missing:
        raise ThemeLogicError(
            f"{path.name}: 缺少字段 {', '.join(sorted(missing))}"
        )
    normalized_rows: list[list[object]] = []
    for row_index, row in enumerate(rows):
        if not isinstance(row, list) or len(row) != len(names):
            raise ThemeLogicError(f"{path.name}: 第 {row_index} 行列数不符")
        normalized_rows.append(row)
    return names, normalized_rows


def load_master(path: Path, category: ThemeCategory) -> list[dict[str, object]]:
    headers, rows = load_group(path, REQUIRED_MASTER_HEADERS)
    result: list[dict[str, object]] = []
    seen_ids: set[str] = set()
    for row in rows:
        value = {name: str(cell) for name, cell in zip(headers, row)}
        theme_id = value["$ZQDM"].strip()
        name = value["gname"].strip()
        if not theme_id or not name:
            raise ThemeLogicError(f"{path.name}: 主题 ID 或名称为空")
        if theme_id in seen_ids:
            raise ThemeLogicError(f"{path.name}: 主题 ID 重复：{theme_id}")
        seen_ids.add(theme_id)
        raw_members = parse_member_tokens(value["$S_ZQDM"])
        members = tuple(dict.fromkeys(raw_members))
        try:
            declared_count = int(value["S_NUM"])
        except ValueError as error:
            raise ThemeLogicError(
                f"{path.name}: {theme_id} 的 S_NUM 无效"
            ) from error
        if declared_count != len(raw_members):
            raise ThemeLogicError(
                f"{path.name}: {theme_id} 声明 {declared_count} 只，"
                f"原始列表 {len(raw_members)} 条"
            )
        result.append(
            {
                "theme_id": theme_id,
                "name": name,
                "category": category.name,
                "category_block_id": category.block_id,
                "declared_member_count": declared_count,
                "duplicate_member_count": len(raw_members) - len(members),
                "members": members,
            }
        )
    return result


def load_detail(path: Path) -> list[dict[str, object]]:
    headers, rows = load_group(path, REQUIRED_DETAIL_HEADERS)
    result: list[dict[str, object]] = []
    seen: set[tuple[int, str]] = set()
    for row in rows:
        value = {name: "" if cell is None else str(cell) for name, cell in zip(headers, row)}
        try:
            market_id = int(value["$SC"])
        except ValueError as error:
            raise ThemeLogicError(f"{path.name}: 市场号无效") from error
        code = value["$ZQDM"].strip()
        key = (market_id, code)
        if not code or key in seen:
            raise ThemeLogicError(f"{path.name}: 证券代码为空或重复：{key}")
        seen.add(key)
        result.append(
            {
                "market_id": market_id,
                "code": code,
                "logic": value["tzlj"],
                "description": value["xxsm"],
                "fqprice_d3": value["fqprice_d3"],
                "fqprice_d5": value["fqprice_d5"],
                "fqprice_d20": value["fqprice_d20"],
                "fqprice_d60": value["fqprice_d60"],
                "price_3m": value["price1"],
            }
        )
    return result


def security_id(market_id: int, code: str) -> str:
    market = blocks.MARKETS.get(market_id, (f"M{market_id}", ""))[0]
    return f"{market}{code}"


def load_theme_masters(
    input_dir: Path,
    categories: Sequence[ThemeCategory] = THEME_CATEGORIES,
) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    category_records: list[dict[str, object]] = []
    aggregate: dict[str, dict[str, object]] = {}
    for category in categories:
        path = input_dir.joinpath(*Path(category.resource).parts)
        if not path.is_file():
            raise ThemeLogicError(
                f"缺少主题主表：{path}；请先使用 --download-masters"
            )
        rows = load_master(path, category)
        category_records.append(
            {
                "block_id": category.block_id,
                "name": category.name,
                "config_name": category.config_name,
                "resource": category.resource,
                "theme_count": len(rows),
                "theme_ids": [str(item["theme_id"]) for item in rows],
            }
        )
        for row in rows:
            theme_id = str(row["theme_id"])
            members = tuple(row["members"])
            current = aggregate.get(theme_id)
            if current is None:
                aggregate[theme_id] = {
                    "theme_id": theme_id,
                    "name": row["name"],
                    "categories": [row["category"]],
                    "category_block_ids": [row["category_block_id"]],
                    "master_declared_member_count": row["declared_member_count"],
                    "master_duplicate_member_count": row["duplicate_member_count"],
                    "master_members": members,
                }
            else:
                if (
                    current["name"] != row["name"]
                    or current["master_members"] != members
                    or current["master_declared_member_count"]
                    != row["declared_member_count"]
                ):
                    raise ThemeLogicError(
                        f"主题 {theme_id} 在多个大类中的名称或成分不一致"
                    )
                current["categories"].append(row["category"])
                current["category_block_ids"].append(row["category_block_id"])
    return category_records, list(aggregate.values())


def build_model(
    root: Path,
    input_dir: Path,
    categories: Sequence[ThemeCategory] = THEME_CATEGORIES,
) -> dict[str, object]:
    category_records, themes = load_theme_masters(input_dir, categories)
    security_master = blocks.load_security_master(root / "T0002" / "hq_cache")
    stock_themes: dict[tuple[int, str], set[str]] = defaultdict(set)
    theme_records: list[dict[str, object]] = []
    detailed_themes = 0
    detailed_records = 0
    for theme in sorted(themes, key=lambda item: (str(item["name"]), str(item["theme_id"]))):
        theme_id = str(theme["theme_id"])
        detail_path = input_dir / "zttzty" / f"{theme_id}.jsn"
        detail_rows = load_detail(detail_path) if detail_path.is_file() else []
        master_members = tuple(theme["master_members"])
        if detail_rows:
            detailed_themes += 1
            detailed_records += len(detail_rows)
            active_members = tuple(
                (int(item["market_id"]), str(item["code"]))
                for item in detail_rows
            )
        else:
            active_members = master_members
        for market_id, code in active_members:
            stock_themes[(market_id, code)].add(theme_id)
        details: list[dict[str, object]] = []
        for item in detail_rows:
            market_id = int(item["market_id"])
            code = str(item["code"])
            security = security_master.get((market_id, code))
            details.append(
                {
                    "security_id": security_id(market_id, code),
                    "market_id": market_id,
                    "code": code,
                    "name": security.name if security else "",
                    **{
                        key: item[key]
                        for key in (
                            "logic",
                            "description",
                            "fqprice_d3",
                            "fqprice_d5",
                            "fqprice_d20",
                            "fqprice_d60",
                            "price_3m",
                        )
                    },
                }
            )
        theme_records.append(
            {
                "theme_id": theme_id,
                "name": theme["name"],
                "categories": theme["categories"],
                "category_block_ids": theme["category_block_ids"],
                "master_declared_member_count": theme[
                    "master_declared_member_count"
                ],
                "master_member_count": len(master_members),
                "master_duplicate_member_count": theme[
                    "master_duplicate_member_count"
                ],
                "member_count": len(active_members),
                "detail_available": bool(detail_rows),
                "detail_resource": f"zttzty/{theme_id}.jsn",
                "members": [
                    security_id(market_id, code)
                    for market_id, code in active_members
                ],
                "details": details,
            }
        )
    stock_records: list[dict[str, object]] = []
    for (market_id, code), theme_ids in sorted(stock_themes.items()):
        security = security_master.get((market_id, code))
        stock_records.append(
            {
                "security_id": security_id(market_id, code),
                "market_id": market_id,
                "code": code,
                "name": security.name if security else "",
                "theme_ids": sorted(theme_ids),
            }
        )
    return {
        "schema": "tdx-theme-logic-v1",
        "counts": {
            "categories": len(category_records),
            "themes": len(theme_records),
            "category_theme_memberships": sum(
                int(item["theme_count"]) for item in category_records
            ),
            "theme_stock_memberships": sum(
                int(item["member_count"]) for item in theme_records
            ),
            "stocks": len(stock_records),
            "detailed_themes": detailed_themes,
            "detailed_records": detailed_records,
            "duplicate_master_memberships": sum(
                int(item["master_duplicate_member_count"])
                for item in theme_records
            ),
        },
        "categories": category_records,
        "themes": theme_records,
        "stocks": stock_records,
    }


def select_themes(
    themes: Sequence[dict[str, object]],
    selectors: Sequence[str],
    category_names: Sequence[str],
    all_details: bool,
) -> list[dict[str, object]]:
    if all_details:
        return list(themes)
    selected: list[dict[str, object]] = []
    selector_values = [value.casefold() for value in selectors]
    categories = set(category_names)
    for theme in themes:
        matches_selector = any(
            value == str(theme["theme_id"]).casefold()
            or value in str(theme["name"]).casefold()
            for value in selector_values
        )
        matches_category = bool(categories & set(theme["categories"]))
        if matches_selector or matches_category:
            selected.append(theme)
    return selected


def connect(
    root: Path,
    hosts: Sequence[str],
    timeout: float,
) -> transport.QuoteConnection:
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
    resources: Iterable[str],
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


def write_model(path: Path, model: dict[str, object], compact: bool) -> None:
    text = json.dumps(
        model,
        ensure_ascii=False,
        indent=None if compact else 2,
        separators=(",", ":") if compact else None,
    )
    updater.atomic_write_text(path, text + "\n", "utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "更新通达信 24 个战略主题主表，可选下载 zttzty 逐股入选逻辑，"
            "并生成主题、大类、股票双向关系模型。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录；默认自动寻找")
    parser.add_argument("--download-masters", action="store_true", help="联网更新 24 个主题主表")
    parser.add_argument("--download-details", action="store_true", help="联网更新选中主题的逐股逻辑")
    parser.add_argument("--theme", action="append", default=[], help="按主题 ID 或名称片段选择；可重复")
    parser.add_argument(
        "--category",
        action="append",
        default=[],
        choices=tuple(item.name for item in THEME_CATEGORIES),
        help="选择大类下的全部主题；可重复",
    )
    parser.add_argument("--all-details", action="store_true", help="选择全部主题详情")
    parser.add_argument(
        "--max-details",
        type=int,
        default=50,
        help="单次详情下载上限；0 表示不限制（默认 50）",
    )
    parser.add_argument("--list", action="store_true", help="列出主题，不下载详情")
    parser.add_argument("--host", action="append", default=[], help="指定 host[:port]；可重复")
    parser.add_argument("--timeout", type=float, default=8.0, help="网络超时秒数")
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-jsn",
        help="JSN 下载目录",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-theme-logic.json",
        help="关系模型输出",
    )
    parser.add_argument("--compact", action="store_true", help="输出紧凑 JSON")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    connection: transport.QuoteConnection | None = None
    try:
        if args.max_details < 0:
            raise ThemeLogicError("--max-details 不能为负数")
        root = updater.find_tdx_root(args.root)
        input_dir = args.input_dir.expanduser().resolve()
        if args.download_masters:
            connection = connect(root, args.host, args.timeout)
            download_resources(
                connection,
                input_dir,
                (item.resource for item in THEME_CATEGORIES),
            )
            connection.close()
            connection = None
        category_records, themes = load_theme_masters(input_dir)
        selected = select_themes(
            themes,
            args.theme,
            args.category,
            args.all_details,
        )
        if args.list:
            visible = selected if (args.theme or args.category or args.all_details) else themes
            for theme in sorted(visible, key=lambda item: (str(item["name"]), str(item["theme_id"]))):
                print(
                    f"{theme['theme_id']}\t{theme['name']}\t"
                    f"{len(theme['master_members'])}\t"
                    f"{','.join(theme['categories'])}"
                )
            print(
                f"大类 {len(category_records)}，主题 {len(visible)}",
                file=sys.stderr,
            )
        if args.download_details:
            if not selected:
                raise ThemeLogicError(
                    "请用 --theme、--category 或 --all-details 选择详情"
                )
            if args.max_details and len(selected) > args.max_details:
                raise ThemeLogicError(
                    f"选中 {len(selected)} 个主题，超过 --max-details "
                    f"{args.max_details}；请缩小范围或显式调高上限"
                )
            connection = connect(root, args.host, args.timeout)
            download_resources(
                connection,
                input_dir,
                (f"zttzty/{item['theme_id']}.jsn" for item in selected),
            )
            connection.close()
            connection = None
        model = build_model(root, input_dir)
        output = args.output.expanduser().resolve()
        write_model(output, model, args.compact)
        counts = model["counts"]
        print(
            f"已导出 {counts['categories']} 个大类、{counts['themes']} 个主题、"
            f"{counts['stocks']} 只股票；其中 {counts['detailed_themes']} 个主题"
            f"有逐股逻辑：{output}",
            file=sys.stderr if args.list else sys.stdout,
        )
    except (
        OSError,
        UnicodeError,
        json.JSONDecodeError,
        blocks.BlockFormatError,
        updater.UpdateError,
        transport.DownloadError,
        downloader.JsnDownloadError,
        ThemeLogicError,
    ) as error:
        print(f"主题更新失败：{error}", file=sys.stderr)
        return 1
    finally:
        if connection is not None:
            connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

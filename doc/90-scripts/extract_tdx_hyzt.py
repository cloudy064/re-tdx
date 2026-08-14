#!/usr/bin/env python3
"""Normalize the reqformat=11 industry/theme resource into searchable relations."""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from typing import Sequence

import extract_tdx_blocks as blocks
import update_tdx_blocks as updater


REQUIRED_HEADERS = {
    "$ZQDM",
    "$SC",
    "TDXHY",
    "$ZQDM1",
    "$SC1",
    "hyPE",
    "hyPB",
    "$S_ZQDM",
    "sszt",
}
MARKET_NAMES = {0: "SZ", 1: "SH", 2: "BJ"}


class HyztFormatError(ValueError):
    """Raised when the industry/theme JSN structure is inconsistent."""


def security_id(market_id: int, code: str) -> str:
    prefix = MARKET_NAMES.get(market_id, str(market_id))
    return f"{prefix}{code}"


def parse_market(value: object, field: str) -> int:
    try:
        market_id = int(str(value))
    except ValueError as error:
        raise HyztFormatError(f"{field} 市场号无效：{value!r}") from error
    if market_id < 0:
        raise HyztFormatError(f"{field} 市场号不能为负数：{market_id}")
    return market_id


def parse_member_list(value: str) -> tuple[tuple[int, str], ...]:
    members: list[tuple[int, str]] = []
    seen: set[tuple[int, str]] = set()
    for item in value.split(","):
        item = item.strip()
        if not item:
            continue
        market_text, separator, code = item.partition("|")
        if not separator or not code:
            raise HyztFormatError(f"行业成分证券格式无效：{item!r}")
        member = (parse_market(market_text, "$S_ZQDM"), code)
        if member not in seen:
            members.append(member)
            seen.add(member)
    return tuple(members)


def parse_themes(value: str) -> tuple[str, ...]:
    return tuple(
        dict.fromkeys(
            theme.strip()
            for theme in value.split("、")
            if theme.strip()
        )
    )


def load_rows(path: Path) -> list[dict[str, str]]:
    value = json.loads(path.read_bytes().decode("gb18030"))
    if not isinstance(value, list):
        raise HyztFormatError("JSN 根节点不是数组")
    rows: list[dict[str, str]] = []
    for group_index, group in enumerate(value):
        if not isinstance(group, dict):
            raise HyztFormatError(f"第 {group_index} 个结果集不是对象")
        header = group.get("colheader")
        data = group.get("data")
        if not isinstance(header, list) or not isinstance(data, list):
            raise HyztFormatError(
                f"第 {group_index} 个结果集缺少 colheader/data"
            )
        missing = REQUIRED_HEADERS - {str(item) for item in header}
        if missing:
            raise HyztFormatError(
                f"第 {group_index} 个结果集缺少字段：{', '.join(sorted(missing))}"
            )
        for row_index, row in enumerate(data):
            if not isinstance(row, list) or len(row) != len(header):
                raise HyztFormatError(
                    f"第 {group_index}/{row_index} 行列数不符"
                )
            rows.append(
                {
                    str(name): "" if cell is None else str(cell)
                    for name, cell in zip(header, row)
                }
            )
    return rows


def _industry_leaf_records(
    rows: Sequence[dict[str, str]],
) -> tuple[
    dict[str, dict[str, object]],
    dict[str, set[str]],
    list[dict[str, object]],
]:
    industries: dict[str, dict[str, object]] = {}
    direct_members: dict[str, set[str]] = defaultdict(set)
    stocks: list[dict[str, object]] = []
    seen_stocks: set[str] = set()
    for row in rows:
        market_id = parse_market(row["$SC"], "$SC")
        code = row["$ZQDM"].strip()
        industry_market = parse_market(row["$SC1"], "$SC1")
        industry_code = row["$ZQDM1"].strip()
        industry_name = row["TDXHY"].strip()
        if not code or not industry_code or not industry_name:
            raise HyztFormatError("股票代码、行业代码或行业名称为空")
        stock_id = security_id(market_id, code)
        if stock_id in seen_stocks:
            raise HyztFormatError(f"股票重复：{stock_id}")
        seen_stocks.add(stock_id)
        declared_members = tuple(
            security_id(member_market, member_code)
            for member_market, member_code in parse_member_list(
                row["$S_ZQDM"]
            )
        )
        industry = industries.get(industry_code)
        identity = {
            "code": industry_code,
            "market_id": industry_market,
            "name": industry_name,
            "pe_ttm": row["hyPE"],
            "pb_mrq": row["hyPB"],
            "declared_members": list(declared_members),
        }
        if industry is None:
            industries[industry_code] = identity
        elif (
            industry["market_id"] != industry_market
            or industry["name"] != industry_name
            or industry["declared_members"] != list(declared_members)
        ):
            raise HyztFormatError(f"行业记录不一致：{industry_code}")
        direct_members[industry_code].add(stock_id)
        stocks.append(
            {
                "security_id": stock_id,
                "market_id": market_id,
                "code": code,
                "industry_code": industry_code,
                "industry_name": industry_name,
                "themes": list(parse_themes(row["sszt"])),
            }
        )
    for industry_code, industry in industries.items():
        actual = direct_members[industry_code]
        declared = set(industry["declared_members"])
        if actual != declared:
            raise HyztFormatError(
                f"{industry_code} 行业成分不一致："
                f"逐股 {len(actual)}，声明 {len(declared)}"
            )
    return industries, direct_members, stocks


def _theme_records(
    stocks: Sequence[dict[str, object]],
) -> list[dict[str, object]]:
    members: dict[str, set[str]] = defaultdict(set)
    for stock in stocks:
        for theme in stock["themes"]:
            members[str(theme)].add(str(stock["security_id"]))
    return [
        {
            "name": name,
            "member_count": len(items),
            "members": sorted(items),
        }
        for name, items in sorted(
            members.items(),
            key=lambda pair: (-len(pair[1]), pair[0].casefold()),
        )
    ]


def build_model(
    rows: Sequence[dict[str, str]],
    *,
    tree_blocks: Sequence[blocks.Block] = (),
    security_names: dict[tuple[int, str], str] | None = None,
    include_themes: bool = True,
) -> dict[str, object]:
    industries, direct_members, stocks = _industry_leaf_records(rows)
    security_names = security_names or {}
    for stock in stocks:
        stock["name"] = security_names.get(
            (int(stock["market_id"]), str(stock["code"])),
            "",
        )
    stock_records = sorted(
        stocks,
        key=lambda item: (int(item["market_id"]), str(item["code"])),
    )

    block_records: list[dict[str, object]] = []
    if tree_blocks:
        industry_tree = [
            block for block in tree_blocks if block.family == "industry"
        ]
        by_id = {block.block_id: block for block in industry_tree}
        leaf_by_code = {
            block.block_code: block
            for block in industry_tree
            if block.is_leaf
        }
        missing = set(industries) - set(leaf_by_code)
        if missing:
            raise HyztFormatError(
                f"云端行业不在本地层级树中：{', '.join(sorted(missing))}"
            )
        expanded_members: dict[str, set[str]] = defaultdict(set)
        for industry_code, members in direct_members.items():
            current = leaf_by_code[industry_code]
            while True:
                expanded_members[current.block_id].update(members)
                if not current.parent_block_id:
                    break
                current = by_id[current.parent_block_id]
        for block in sorted(
            industry_tree,
            key=lambda item: (item.level, item.source_key),
        ):
            cloud_info = industries.get(block.block_code, {})
            member_ids = sorted(expanded_members[block.block_id])
            block_records.append(
                {
                    "block_id": block.block_id,
                    "code": block.block_code,
                    "name": block.name,
                    "source_key": block.source_key,
                    "parent_block_id": block.parent_block_id,
                    "level": block.level,
                    "is_leaf": block.is_leaf,
                    "member_count": len(member_ids),
                    "members": member_ids,
                    "pe_ttm": cloud_info.get("pe_ttm", ""),
                    "pb_mrq": cloud_info.get("pb_mrq", ""),
                }
            )
    else:
        for industry_code, industry in sorted(industries.items()):
            member_ids = sorted(direct_members[industry_code])
            block_records.append(
                {
                    "block_id": f"industry:{industry_code}",
                    "code": industry_code,
                    "name": industry["name"],
                    "source_key": "",
                    "parent_block_id": "",
                    "level": 1,
                    "is_leaf": True,
                    "member_count": len(member_ids),
                    "members": member_ids,
                    "pe_ttm": industry["pe_ttm"],
                    "pb_mrq": industry["pb_mrq"],
                }
            )

    themes = _theme_records(stock_records) if include_themes else []
    return {
        "schema": "tdx-hyzt-v1",
        "counts": {
            "stocks": len(stock_records),
            "industry_blocks": len(block_records),
            "industry_leaves": len(industries),
            "themes": len(themes),
            "theme_memberships": sum(
                int(theme["member_count"]) for theme in themes
            ),
        },
        "industry_blocks": block_records,
        "stocks": stock_records,
        "themes": themes,
    }


def load_local_tree(
    root: Path,
) -> tuple[list[blocks.Block], dict[tuple[int, str], str]]:
    securities, tree, _members = blocks.extract(root, {"industry"})
    names = {
        key: security.name
        for key, security in securities.items()
    }
    return tree, names


def write_json_atomic(path: Path, value: object, compact: bool) -> None:
    text = json.dumps(
        value,
        ensure_ascii=False,
        separators=(",", ":") if compact else None,
        indent=None if compact else 2,
    )
    updater.atomic_write_text(path, text + "\n", "utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "解析 func_gx_hyzt101_1.jsn，导出行业层级、行业成分股、"
            "股票所属行业以及主题反向索引。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录；默认自动寻找")
    parser.add_argument(
        "--input",
        type=Path,
        default=updater.PROJECT_ROOT
        / "output"
        / "tdx-jsn"
        / "list"
        / "func_gx_hyzt101_1.jsn",
        help="已下载的行业/主题 JSN",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-hyzt-model.json",
        help="输出 JSON",
    )
    parser.add_argument("--no-tree", action="store_true", help="不合并本地三级行业树")
    parser.add_argument("--no-themes", action="store_true", help="不生成主题反向索引")
    parser.add_argument("--compact", action="store_true", help="输出紧凑 JSON")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        input_path = args.input.expanduser().resolve()
        rows = load_rows(input_path)
        if args.no_tree:
            tree: Sequence[blocks.Block] = ()
            names: dict[tuple[int, str], str] = {}
        else:
            root = updater.find_tdx_root(args.root)
            tree, names = load_local_tree(root)
        model = build_model(
            rows,
            tree_blocks=tree,
            security_names=names,
            include_themes=not args.no_themes,
        )
        output = args.output.expanduser().resolve()
        write_json_atomic(output, model, args.compact)
        counts = model["counts"]
        print(
            f"已导出 {counts['industry_blocks']} 个行业节点、"
            f"{counts['stocks']} 只股票、{counts['themes']} 个主题：{output}"
        )
    except (
        OSError,
        UnicodeError,
        json.JSONDecodeError,
        blocks.BlockFormatError,
        updater.UpdateError,
        HyztFormatError,
    ) as error:
        print(f"导出失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

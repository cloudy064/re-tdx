#!/usr/bin/env python3
"""Query TDX server-side category rankings over 7709/TCP (0x054B)."""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
import zlib
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Callable, Sequence

import download_tdx_minute as transport
import extract_tdx_blocks as blocks
import tdx_market_depth as depth
import tdx_market_snapshot as snapshots
import update_tdx_blocks as updater


TYPE_CATEGORY_QUOTES = 0x054B
CATEGORY_A_SHARES = 6
CATEGORY_ALIASES = {
    "a-shares": CATEGORY_A_SHARES,
    "a股": CATEGORY_A_SHARES,
    "沪深a股": CATEGORY_A_SHARES,
}
SORT_TYPES = {
    "code": 0x0000,
    "代码": 0x0000,
    "price": 0x0006,
    "现价": 0x0006,
    "amount": 0x000A,
    "成交额": 0x000A,
    "change-pct": 0x000E,
    "涨幅": 0x000E,
    "seal-amount": 0x001C,
    "封单额": 0x001C,
    "opening-amount": 0x001D,
    "开盘金额": 0x001D,
    "rise-speed": 0x002E,
    "涨速": 0x002E,
    "short-turnover": 0x00CC,
    "短换手": 0x00CC,
    "volume-rise-speed": 0x00D0,
    "量涨速": 0x00D0,
    "opening-rush": 0x010A,
    "开盘抢筹": 0x010A,
    "two-minute-amount": 0x010C,
    "2分钟金额": 0x010C,
    "opening-change": 0x0119,
    "开盘涨幅": 0x0119,
    "highest-change": 0x011A,
    "最高涨幅": 0x011A,
    "lowest-change": 0x011B,
    "最低涨幅": 0x011B,
    "drawdown": 0x011E,
    "回撤": 0x011E,
    "attack": 0x011F,
    "攻击": 0x011F,
}


class CategoryQuoteError(RuntimeError):
    """Raised when a category-ranking request or response is invalid."""


@dataclass(frozen=True)
class CategoryQuote:
    market_id: int
    code: str
    active1: int
    active2: int
    last_price: float
    pre_close_price: float
    open_price: float
    high_price: float
    low_price: float
    server_time_raw: int
    neg_price_raw: int
    total_hand: int
    current_hand: int
    amount: float
    amount_raw: int
    inside_dish: int
    outer_disc: int
    after_outer_raw: int
    open_amount_yuan: float
    bid1_price: float
    ask1_price: float
    bid1_volume_hand: int
    ask1_volume_hand: int
    status_or_sort_raw: int
    rise_speed: float
    short_turnover: float
    two_minute_amount: float
    opening_rush: float
    volume_rise_speed: float
    depth: float
    extra_pair_hex: str
    extra_meta_hex: str

    @property
    def key(self) -> tuple[int, str]:
        return self.market_id, self.code

    @property
    def security_id(self) -> str:
        return snapshots.QuoteCode(self.market_id, self.code).display

    @property
    def change_pct(self) -> float | None:
        if self.pre_close_price <= 0:
            return None
        return (self.last_price / self.pre_close_price - 1.0) * 100.0

    @property
    def seal_amount_yuan(self) -> float:
        return self.bid1_price * self.bid1_volume_hand * 100.0

    @property
    def is_sealed(self) -> bool:
        return (
            abs(self.bid1_price - self.last_price) < 0.000001
            and (self.ask1_price <= 0 or self.ask1_volume_hand <= 0)
        )


@dataclass(frozen=True)
class CategoryQuotePage:
    category: int
    sort_type: int
    start: int
    request_count: int
    sort_reverse: int
    filter_raw: int
    header: int
    records: tuple[CategoryQuote, ...]


@dataclass(frozen=True)
class CategoryDownloadResult:
    page: CategoryQuotePage
    endpoint: transport.HostEndpoint
    server_name: str
    attempted_endpoints: tuple[transport.HostEndpoint, ...]
    failures: tuple[str, ...]


def normalize_category(value: str | int) -> int:
    if isinstance(value, int):
        result = value
    else:
        text = value.strip().lower()
        result = CATEGORY_ALIASES[text] if text in CATEGORY_ALIASES else int(text, 0)
    if not 0 <= result <= 0xFFFF:
        raise CategoryQuoteError(f"分类号超出 uint16：{result}")
    return result


def normalize_sort_type(value: str | int) -> int:
    if isinstance(value, int):
        result = value
    else:
        text = value.strip().lower()
        result = SORT_TYPES[text] if text in SORT_TYPES else int(text, 0)
    if not 0 <= result <= 0xFFFF:
        raise CategoryQuoteError(f"排序号超出 uint16：{result}")
    return result


def build_category_request_data(
    category: str | int = CATEGORY_A_SHARES,
    sort_type: str | int = 0,
    *,
    start: int = 0,
    count: int = 80,
    ascending: bool = False,
    filter_raw: int = 0,
) -> bytes:
    category_id = normalize_category(category)
    sort_id = normalize_sort_type(sort_type)
    fields = (start, count, filter_raw)
    if any(not 0 <= int(value) <= 0xFFFF for value in fields):
        raise CategoryQuoteError("start/count/filter_raw 必须位于 uint16 范围")
    sort_reverse = 0 if sort_id == 0 else (2 if ascending else 1)
    return struct.pack(
        "<9H",
        category_id,
        sort_id,
        int(start),
        int(count),
        sort_reverse,
        5,
        int(filter_raw),
        1,
        0,
    )


def _price(raw: int, code: str) -> float:
    return raw / 100.0 / snapshots._price_divisor(code)


def parse_category_record(payload: bytes, offset: int) -> tuple[CategoryQuote, int]:
    if offset + 9 > len(payload):
        raise CategoryQuoteError("分类行情记录头部不完整")
    market_id = payload[offset]
    try:
        code = payload[offset + 1 : offset + 7].decode("ascii")
    except UnicodeDecodeError as error:
        raise CategoryQuoteError("分类行情证券代码不是 ASCII") from error
    if market_id not in (0, 1, 2) or len(code) != 6 or not code.isdigit():
        raise CategoryQuoteError(f"分类行情证券标识无效：{market_id}/{code!r}")
    active1 = int.from_bytes(payload[offset + 7 : offset + 9], "little")
    offset += 9
    values: list[int] = []
    try:
        for _ in range(9):
            value, offset = snapshots.consume_varint(payload, offset)
            values.append(value)
    except snapshots.SnapshotError as error:
        raise CategoryQuoteError(f"{code} 分类行情基本字段不完整") from error
    (
        close_raw,
        pre_close_diff,
        open_diff,
        high_diff,
        low_diff,
        server_time_raw,
        neg_price_raw,
        total_hand,
        current_hand,
    ) = values
    if offset + 4 > len(payload):
        raise CategoryQuoteError(f"{code} 分类行情缺少成交额")
    amount_raw = int.from_bytes(payload[offset : offset + 4], "little")
    amount = transport.decode_wire_number(amount_raw)
    offset += 4
    try:
        inside_dish, offset = snapshots.consume_varint(payload, offset)
        outer_disc, offset = snapshots.consume_varint(payload, offset)
        after_outer_raw, offset = snapshots.consume_varint(payload, offset)
        open_amount_raw, offset = snapshots.consume_varint(payload, offset)
        bid1_diff, offset = snapshots.consume_varint(payload, offset)
        ask1_diff, offset = snapshots.consume_varint(payload, offset)
        bid_vol1, offset = snapshots.consume_varint(payload, offset)
        ask_vol1, offset = snapshots.consume_varint(payload, offset)
    except snapshots.SnapshotError as error:
        raise CategoryQuoteError(f"{code} 分类行情盘口字段不完整") from error
    if offset + 56 > len(payload):
        raise CategoryQuoteError(f"{code} 分类行情固定尾部不完整")
    tail = payload[offset : offset + 56]
    offset += 56
    (
        status_or_sort_raw,
        rise_speed_raw,
        short_turnover_raw,
        two_minute_amount,
        opening_rush_raw,
        extra_pair_raw,
        volume_rise_speed,
        depth_value,
        extra_meta_raw,
        active2,
    ) = struct.unpack("<Hhhfh10sff24sH", tail)
    numeric = (
        amount,
        two_minute_amount,
        volume_rise_speed,
        depth_value,
    )
    if not all(math.isfinite(value) for value in numeric):
        raise CategoryQuoteError(f"{code} 分类行情包含非有限数值")
    return CategoryQuote(
        market_id=market_id,
        code=code,
        active1=active1,
        active2=active2,
        last_price=_price(close_raw, code),
        pre_close_price=_price(close_raw + pre_close_diff, code),
        open_price=_price(close_raw + open_diff, code),
        high_price=_price(close_raw + high_diff, code),
        low_price=_price(close_raw + low_diff, code),
        server_time_raw=server_time_raw,
        neg_price_raw=neg_price_raw,
        total_hand=total_hand,
        current_hand=current_hand,
        amount=amount,
        amount_raw=amount_raw,
        inside_dish=inside_dish,
        outer_disc=outer_disc,
        after_outer_raw=after_outer_raw,
        open_amount_yuan=float(open_amount_raw * 100),
        bid1_price=_price(close_raw + bid1_diff, code),
        ask1_price=_price(close_raw + ask1_diff, code),
        bid1_volume_hand=bid_vol1,
        ask1_volume_hand=ask_vol1,
        status_or_sort_raw=status_or_sort_raw,
        rise_speed=rise_speed_raw / 100.0,
        short_turnover=short_turnover_raw / 100.0,
        two_minute_amount=float(two_minute_amount),
        opening_rush=opening_rush_raw / 100.0,
        volume_rise_speed=float(volume_rise_speed),
        depth=float(depth_value),
        extra_pair_hex=extra_pair_raw.hex(),
        extra_meta_hex=extra_meta_raw.hex(),
    ), offset


def parse_category_payload(
    payload: bytes,
    *,
    category: int,
    sort_type: int,
    start: int,
    request_count: int,
    ascending: bool,
    filter_raw: int,
) -> CategoryQuotePage:
    if len(payload) < 4:
        raise CategoryQuoteError("分类行情响应少于 4 字节")
    header, count = struct.unpack_from("<HH", payload)
    offset = 4
    records: list[CategoryQuote] = []
    for _ in range(count):
        item, offset = parse_category_record(payload, offset)
        records.append(item)
    if offset != len(payload):
        raise CategoryQuoteError(f"分类行情响应残留 {len(payload) - offset} 字节")
    return CategoryQuotePage(
        category=category,
        sort_type=sort_type,
        start=start,
        request_count=request_count,
        sort_reverse=0 if sort_type == 0 else (2 if ascending else 1),
        filter_raw=filter_raw,
        header=header,
        records=tuple(records),
    )


def download_category_quotes(
    endpoints: Sequence[transport.HostEndpoint],
    *,
    category: str | int = CATEGORY_A_SHARES,
    sort_type: str | int = "seal-amount",
    start: int = 0,
    count: int = 80,
    ascending: bool = False,
    filter_raw: int = 0,
    timeout: float = 5.0,
    progress: Callable[[str], None] | None = None,
) -> CategoryDownloadResult:
    category_id = normalize_category(category)
    sort_id = normalize_sort_type(sort_type)
    request = build_category_request_data(
        category_id,
        sort_id,
        start=start,
        count=count,
        ascending=ascending,
        filter_raw=filter_raw,
    )
    attempted: list[transport.HostEndpoint] = []
    failures: list[str] = []
    for endpoint in endpoints:
        attempted.append(endpoint)
        try:
            with transport.QuoteConnection(endpoint, timeout) as connection:
                response = connection.call(TYPE_CATEGORY_QUOTES, request)
                page = parse_category_payload(
                    response.data,
                    category=category_id,
                    sort_type=sort_id,
                    start=start,
                    request_count=count,
                    ascending=ascending,
                    filter_raw=filter_raw,
                )
                return CategoryDownloadResult(
                    page=page,
                    endpoint=endpoint,
                    server_name=connection.server_name,
                    attempted_endpoints=tuple(attempted),
                    failures=tuple(failures),
                )
        except (
            CategoryQuoteError,
            transport.DownloadError,
            OSError,
            TimeoutError,
            zlib.error,
            struct.error,
        ) as error:
            failures.append(f"{endpoint.address}: {error}")
            if progress:
                progress(f"分类排序主站失败：{endpoint.address}（{error}）")
    detail = "\n".join(f"  - {item}" for item in failures)
    raise CategoryQuoteError(f"所有分类排序主站均失败：\n{detail}")


def quote_to_dict(
    item: CategoryQuote,
    security_master: dict[tuple[int, str], blocks.Security],
) -> dict[str, object]:
    result = asdict(item)
    security = security_master.get(item.key)
    result.update({
        "security_id": item.security_id,
        "name": security.name if security else "",
        "name_resolved": security is not None,
        "change_pct": item.change_pct,
        "seal_amount_yuan": item.seal_amount_yuan,
        "is_sealed": item.is_sealed,
    })
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="直连通达信 7709 主站取得 0x054B 分类排序榜。"
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument("--category", default="a-shares")
    parser.add_argument("--sort", default="seal-amount")
    parser.add_argument("--start", type=int, default=0)
    parser.add_argument("--count", type=int, default=80)
    parser.add_argument("--ascending", action="store_true")
    parser.add_argument(
        "--all-sealed",
        action="store_true",
        help="按封单额连续翻页，并在第一条非封板记录处停止",
    )
    parser.add_argument("--filter-raw", type=lambda value: int(value, 0), default=0)
    parser.add_argument("--host", action="append", default=[])
    parser.add_argument("--max-hosts", type=int, default=5)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--download", action="store_true", help="明确允许联网")
    parser.add_argument(
        "--output",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-category-quotes.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        category_id = normalize_category(args.category)
        sort_id = normalize_sort_type(args.sort)
        output = args.output.expanduser().resolve()
        if not args.download:
            print(json.dumps({
                "action": "dry-run",
                "command": "0x054B",
                "category": category_id,
                "sort_type": f"0x{sort_id:04X}",
                "start": args.start,
                "count": args.count,
                "all_sealed": args.all_sealed,
                "output": str(output),
            }, ensure_ascii=False, indent=2))
            return 0
        endpoints = depth.endpoint_candidates(root, args.host, args.max_hosts)
        if args.all_sealed and (sort_id != SORT_TYPES["seal-amount"] or args.ascending):
            raise CategoryQuoteError("--all-sealed 只适用于封单额降序")
        selected: list[CategoryQuote] = []
        scanned = 0
        page_count = 0
        page_start = args.start
        result: CategoryDownloadResult | None = None
        while True:
            result = download_category_quotes(
                endpoints,
                category=category_id,
                sort_type=sort_id,
                start=page_start,
                count=args.count,
                ascending=args.ascending,
                filter_raw=args.filter_raw,
                timeout=args.timeout,
                progress=print if args.verbose else None,
            )
            page_count += 1
            page_records = result.page.records
            scanned += len(page_records)
            if not args.all_sealed:
                selected.extend(page_records)
                break
            sealed_prefix = []
            for item in page_records:
                if not item.is_sealed:
                    break
                sealed_prefix.append(item)
            selected.extend(sealed_prefix)
            if len(sealed_prefix) < len(page_records) or len(page_records) < args.count:
                break
            if page_count >= 10:
                raise CategoryQuoteError("--all-sealed 已达 10 页安全上限")
            page_start += args.count
        if result is None:
            raise CategoryQuoteError("分类排序没有执行任何请求")
        master = blocks.load_security_master(root / "T0002" / "hq_cache")
        model = {
            "schema": "tdx-category-quotes-v1",
            "generated_at": datetime.now().astimezone().isoformat(),
            "command": "0x054B",
            "endpoint": result.endpoint.address,
            "server_name": result.server_name,
            "category": result.page.category,
            "category_scope_note": (
                "分类 6 当前主站实际返回深圳、上海和北京市场证券"
                if result.page.category == CATEGORY_A_SHARES else ""
            ),
            "sort_type": f"0x{result.page.sort_type:04X}",
            "sort_reverse": result.page.sort_reverse,
            "start": args.start,
            "page_size": args.count,
            "pages": page_count,
            "scanned": scanned,
            "sealed_only": args.all_sealed,
            "received": len(selected),
            "records": [quote_to_dict(item, master) for item in selected],
        }
        rendered = json.dumps(
            model,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        updater.atomic_write_text(output, rendered + "\n", "utf-8")
        print(
            f"已取得 {len(selected)} 条分类排序，"
            f"sort=0x{sort_id:04X}：{output}"
        )
    except (
        OSError,
        UnicodeError,
        ValueError,
        blocks.BlockFormatError,
        updater.UpdateError,
        transport.DownloadError,
        snapshots.SnapshotError,
        CategoryQuoteError,
    ) as error:
        print(f"分类排序更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

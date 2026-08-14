#!/usr/bin/env python3
"""Download TDX call-auction point series over 7709/TCP (0x056A)."""

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


TYPE_AUCTION_SERIES = 0x056A
TYPE_HEARTBEAT = 0x0004
AUCTION_RECORD_SIZE = 16


class AuctionSeriesError(RuntimeError):
    """Raised when a 0x056A request or response is invalid."""


@dataclass(frozen=True)
class AuctionPoint:
    index: int
    minute_of_day_raw: int
    second_raw: int
    time_label: str
    time_seconds: int
    price: float
    matched_volume_hand: int
    unmatched_signed_hand: int
    unmatched_volume_hand: int
    unmatched_direction_raw: int
    unmatched_direction: str
    reserved_zero_0e: int

    @property
    def matched_amount_yuan(self) -> float:
        return self.price * self.matched_volume_hand * 100.0


@dataclass(frozen=True)
class AuctionSeries:
    market_id: int
    code: str
    selector: int
    start_raw: int
    limit: int
    points: tuple[AuctionPoint, ...]

    @property
    def key(self) -> tuple[int, str]:
        return self.market_id, self.code

    @property
    def security_id(self) -> str:
        return snapshots.QuoteCode(self.market_id, self.code).display


@dataclass(frozen=True)
class AuctionDownloadResult:
    series: tuple[AuctionSeries, ...]
    requested: int
    endpoint: transport.HostEndpoint
    server_name: str
    server_trade_date: str
    attempted_endpoints: tuple[transport.HostEndpoint, ...]
    failures: tuple[str, ...]


def _u32(value: int, name: str) -> int:
    result = int(value)
    if not 0 <= result <= 0xFFFFFFFF:
        raise AuctionSeriesError(f"{name} 超出 uint32：{result}")
    return result


def build_auction_request_data(
    code: snapshots.QuoteCode,
    *,
    selector: int = 3,
    start_raw: int = 0,
    limit: int = 500,
) -> bytes:
    selector = _u32(selector, "selector")
    start_raw = _u32(start_raw, "start_raw")
    limit = _u32(limit, "limit")
    if limit == 0:
        raise AuctionSeriesError("limit 必须大于 0")
    return (
        bytes((code.market_id, 0))
        + code.code.encode("ascii")
        + struct.pack("<IIIII", 0, selector, 0, start_raw, limit)
    )


def minute_label(minute_of_day: int, second: int) -> str:
    hour, minute = divmod(minute_of_day, 60)
    return f"{hour:02d}:{minute:02d}:{second:02d}"


def parse_auction_payload(
    payload: bytes,
    code: snapshots.QuoteCode,
    *,
    selector: int = 3,
    start_raw: int = 0,
    limit: int = 500,
) -> AuctionSeries:
    if len(payload) < 2:
        raise AuctionSeriesError("竞价序列响应少于 2 字节")
    count = int.from_bytes(payload[:2], "little")
    expected = 2 + count * AUCTION_RECORD_SIZE
    if len(payload) != expected:
        raise AuctionSeriesError(
            f"竞价序列长度不符：声明 {count} 点，应为 {expected} 字节，"
            f"实际 {len(payload)} 字节"
        )
    points: list[AuctionPoint] = []
    offset = 2
    for index in range(count):
        record = payload[offset : offset + AUCTION_RECORD_SIZE]
        offset += AUCTION_RECORD_SIZE
        minute_of_day = int.from_bytes(record[:2], "little")
        price = struct.unpack_from("<f", record, 2)[0]
        matched_volume = int.from_bytes(record[6:10], "little")
        unmatched_signed = int.from_bytes(record[10:14], "little", signed=True)
        reserved_zero = record[14]
        second = record[15]
        if minute_of_day >= 24 * 60 or second >= 60:
            raise AuctionSeriesError(
                f"{code.display} 第 {index + 1} 点时间无效："
                f"{minute_of_day}/{second}"
            )
        if not math.isfinite(price) or price < 0:
            raise AuctionSeriesError(
                f"{code.display} 第 {index + 1} 点价格无效：{price}"
            )
        points.append(AuctionPoint(
            index=index,
            minute_of_day_raw=minute_of_day,
            second_raw=second,
            time_label=minute_label(minute_of_day, second),
            time_seconds=minute_of_day * 60 + second,
            price=float(price),
            matched_volume_hand=matched_volume,
            unmatched_signed_hand=unmatched_signed,
            unmatched_volume_hand=abs(unmatched_signed),
            unmatched_direction_raw=(
                1 if unmatched_signed > 0 else -1 if unmatched_signed < 0 else 0
            ),
            unmatched_direction=(
                "buy" if unmatched_signed > 0
                else "sell" if unmatched_signed < 0
                else "balanced"
            ),
            reserved_zero_0e=reserved_zero,
        ))
    return AuctionSeries(
        market_id=code.market_id,
        code=code.code,
        selector=selector,
        start_raw=start_raw,
        limit=limit,
        points=tuple(points),
    )


def download_auction_series(
    endpoints: Sequence[transport.HostEndpoint],
    codes: Sequence[snapshots.QuoteCode],
    *,
    selector: int = 3,
    start_raw: int = 0,
    limit: int = 500,
    timeout: float = 8.0,
    progress: Callable[[str], None] | None = None,
) -> AuctionDownloadResult:
    unique_codes = tuple(dict.fromkeys(codes))
    if not unique_codes:
        raise AuctionSeriesError("竞价代码列表不能为空")
    completed: dict[tuple[int, str], AuctionSeries] = {}
    attempted: list[transport.HostEndpoint] = []
    failures: list[str] = []
    next_index = 0
    successful_endpoint: transport.HostEndpoint | None = None
    successful_server = ""
    successful_trade_date = ""
    for endpoint in endpoints:
        if next_index >= len(unique_codes):
            break
        attempted.append(endpoint)
        try:
            with transport.QuoteConnection(endpoint, timeout) as connection:
                heartbeat = connection.call(TYPE_HEARTBEAT, b"")
                if len(heartbeat.data) < 10:
                    raise AuctionSeriesError("心跳响应缺少服务器交易日")
                trade_date_raw = int.from_bytes(heartbeat.data[6:10], "little")
                try:
                    trade_date = datetime.strptime(
                        str(trade_date_raw), "%Y%m%d"
                    ).strftime("%Y%m%d")
                except ValueError as error:
                    raise AuctionSeriesError(
                        f"服务器交易日无效：{trade_date_raw}"
                    ) from error
                if progress:
                    progress(
                        f"已连接 {endpoint.address}（"
                        f"{connection.server_name or endpoint.name or '未命名主站'}）"
                    )
                while next_index < len(unique_codes):
                    code = unique_codes[next_index]
                    response = connection.call(
                        TYPE_AUCTION_SERIES,
                        build_auction_request_data(
                            code,
                            selector=selector,
                            start_raw=start_raw,
                            limit=limit,
                        ),
                    )
                    item = parse_auction_payload(
                        response.data,
                        code,
                        selector=selector,
                        start_raw=start_raw,
                        limit=limit,
                    )
                    completed[item.key] = item
                    next_index += 1
                    successful_endpoint = endpoint
                    successful_server = connection.server_name
                    successful_trade_date = trade_date
                    if progress:
                        progress(
                            f"竞价 {next_index}/{len(unique_codes)}："
                            f"{item.security_id} {len(item.points)} 点"
                        )
        except (
            AuctionSeriesError,
            transport.DownloadError,
            OSError,
            TimeoutError,
            zlib.error,
            struct.error,
        ) as error:
            failures.append(f"{endpoint.address}: {error}")
            if progress:
                progress(f"竞价主站失败，继续未完成证券：{endpoint.address}（{error}）")
    if next_index < len(unique_codes) or successful_endpoint is None:
        detail = "\n".join(f"  - {item}" for item in failures)
        raise AuctionSeriesError(
            f"竞价只完成 {next_index}/{len(unique_codes)} 只：\n{detail}"
        )
    return AuctionDownloadResult(
        series=tuple(completed[item.key] for item in unique_codes),
        requested=len(unique_codes),
        endpoint=successful_endpoint,
        server_name=successful_server,
        server_trade_date=successful_trade_date,
        attempted_endpoints=tuple(attempted),
        failures=tuple(failures),
    )


def _summarize_segment(points: Sequence[AuctionPoint]) -> dict[str, object]:
    final = points[-1] if points else None
    nonzero_directions = [
        point.unmatched_direction_raw
        for point in points
        if point.unmatched_direction_raw
    ]
    direction_flips = sum(
        right != left
        for left, right in zip(nonzero_directions, nonzero_directions[1:])
    )
    matched_violations = sum(
        right.matched_volume_hand < left.matched_volume_hand
        for left, right in zip(points, points[1:])
    )
    max_unmatched = max(points, key=lambda point: point.unmatched_volume_hand, default=None)
    return {
        "point_count": len(points),
        "start_time": points[0].time_label if points else None,
        "end_time": final.time_label if final else None,
        "first_price": points[0].price if points else None,
        "last_sample_price": final.price if final else None,
        "min_price": min((point.price for point in points), default=None),
        "max_price": max((point.price for point in points), default=None),
        "last_sample_matched_volume_hand": (
            final.matched_volume_hand if final else None
        ),
        "last_sample_matched_amount_yuan": (
            final.matched_amount_yuan if final else None
        ),
        "last_sample_unmatched_signed_hand": (
            final.unmatched_signed_hand if final else None
        ),
        "last_sample_unmatched_volume_hand": (
            final.unmatched_volume_hand if final else None
        ),
        "last_sample_unmatched_direction_raw": (
            final.unmatched_direction_raw if final else None
        ),
        "unmatched_direction_flips": direction_flips,
        "max_unmatched_volume_hand": (
            max_unmatched.unmatched_volume_hand if max_unmatched else None
        ),
        "max_unmatched_time": max_unmatched.time_label if max_unmatched else None,
        "matched_volume_monotonic_violations": matched_violations,
    }


def summarize_series(item: AuctionSeries) -> dict[str, object]:
    points = item.points
    opening = tuple(point for point in points if point.minute_of_day_raw < 12 * 60)
    closing = tuple(point for point in points if point.minute_of_day_raw >= 12 * 60)
    largest_gap = max(
        (
            (right.time_seconds - left.time_seconds, left.time_label, right.time_label)
            for left, right in zip(points, points[1:])
        ),
        default=(0, None, None),
    )
    return {
        "point_count": len(points),
        "opening": _summarize_segment(opening),
        "closing": _summarize_segment(closing),
        "largest_gap_seconds": largest_gap[0],
        "largest_gap_after": largest_gap[1],
        "largest_gap_before": largest_gap[2],
        "reserved_nonzero_points": sum(
            point.reserved_zero_0e != 0 for point in points
        ),
    }


def series_to_dict(
    item: AuctionSeries,
    security_master: dict[tuple[int, str], blocks.Security],
) -> dict[str, object]:
    security = security_master.get(item.key)
    return {
        "market_id": item.market_id,
        "code": item.code,
        "security_id": item.security_id,
        "name": security.name if security else "",
        "name_resolved": security is not None,
        "selector": item.selector,
        "start_raw": item.start_raw,
        "limit": item.limit,
        "summary": summarize_series(item),
        "points": [asdict(point) | {
            "matched_amount_yuan": point.matched_amount_yuan,
        } for point in item.points],
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="直连通达信 7709 主站取得 0x056A 集合竞价逐点序列。"
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument(
        "--security", action="append", default=[], metavar="[MARKET:]CODE",
        help="证券，如 sz000001、0:000001；可重复",
    )
    parser.add_argument("--selector", type=int, default=3)
    parser.add_argument("--start-raw", type=int, default=0)
    parser.add_argument("--limit", type=int, default=500)
    parser.add_argument("--host", action="append", default=[])
    parser.add_argument("--max-hosts", type=int, default=5)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--download", action="store_true", help="明确允许联网")
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-auction-series.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        if not args.security:
            raise AuctionSeriesError("请至少提供一个 --security")
        codes = tuple(depth.parse_code(root, value) for value in args.security)
        output = args.output.expanduser().resolve()
        if not args.download:
            print(json.dumps({
                "action": "dry-run",
                "command": "0x056A",
                "securities": [item.display for item in codes],
                "selector": args.selector,
                "start_raw": args.start_raw,
                "limit": args.limit,
                "output": str(output),
            }, ensure_ascii=False, indent=2))
            return 0
        endpoints = depth.endpoint_candidates(root, args.host, args.max_hosts)
        result = download_auction_series(
            endpoints,
            codes,
            selector=args.selector,
            start_raw=args.start_raw,
            limit=args.limit,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        master = blocks.load_security_master(root / "T0002" / "hq_cache")
        model = {
            "schema": "tdx-auction-series-v1",
            "generated_at": datetime.now().astimezone().isoformat(),
            "command": "0x056A",
            "endpoint": result.endpoint.address,
            "server_name": result.server_name,
            "server_trade_date": result.server_trade_date,
            "requested": result.requested,
            "received": len(result.series),
            "selector_note": (
                "实测 selector=0 仅返回开盘竞价；任意非零值返回开盘+收盘竞价"
            ),
            "paging_note": "start_raw/limit 按合并后的逐点序列分页",
            "unmatched_direction_note": (
                "int32 正数=买方未匹配、负数=卖方未匹配、零=平衡；"
                "已用涨停买单和跌停卖单样本交叉验证"
            ),
            "records": [series_to_dict(item, master) for item in result.series],
        }
        rendered = json.dumps(
            model,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        updater.atomic_write_text(output, rendered + "\n", "utf-8")
        print(f"已取得 {len(result.series)} 只证券竞价序列：{output}")
    except (
        OSError,
        UnicodeError,
        ValueError,
        blocks.BlockFormatError,
        updater.UpdateError,
        transport.DownloadError,
        snapshots.SnapshotError,
        AuctionSeriesError,
    ) as error:
        print(f"竞价序列更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

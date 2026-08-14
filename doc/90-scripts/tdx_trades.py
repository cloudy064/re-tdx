#!/usr/bin/env python3
"""Download TDX current/historical L1 trade details (0x0FC5/0x0FC6)."""

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


TYPE_HEARTBEAT = 0x0004
TYPE_TODAY_TICKS = 0x0FC5
TYPE_HISTORICAL_TICKS = 0x0FC6
THREE_DECIMAL_PRICE_PREFIXES = (
    "10", "11", "12",
    *snapshots.ETF_PRICE_PREFIXES,
)


class TradeError(RuntimeError):
    """Raised when trade-detail requests or responses are invalid."""


@dataclass(frozen=True)
class TradeTick:
    index: int
    absolute_index: int
    time_minutes: int
    time_label: str
    price: float
    volume_hand: int
    order_count: int
    status_raw: int
    side: str
    price_delta_raw: int
    price_acc_raw: int
    tail_raw: int

    @property
    def amount_yuan(self) -> float:
        return round(self.price * self.volume_hand * 100.0, 6)


@dataclass(frozen=True)
class TradePage:
    market_id: int
    code: str
    trading_date: str | None
    start: int
    request_count: int
    price_divisor: int
    price_base_raw: float | None
    ticks: tuple[TradeTick, ...]


@dataclass(frozen=True)
class TradeSeries:
    market_id: int
    code: str
    trading_date: str
    source_mode: str
    pages: int
    page_size: int
    price_divisor: int
    price_base_raw: float | None
    ticks: tuple[TradeTick, ...]

    @property
    def key(self) -> tuple[int, str]:
        return self.market_id, self.code

    @property
    def security_id(self) -> str:
        return snapshots.QuoteCode(self.market_id, self.code).display


@dataclass(frozen=True)
class TradeDownloadResult:
    series: tuple[TradeSeries, ...]
    requested: int
    endpoint: transport.HostEndpoint
    server_name: str
    server_trade_date: str
    attempted_endpoints: tuple[transport.HostEndpoint, ...]
    failures: tuple[str, ...]


def _u16(value: int, name: str, *, positive: bool = False) -> int:
    result = int(value)
    minimum = 1 if positive else 0
    if not minimum <= result <= 0xFFFF:
        raise TradeError(f"{name} 必须为 {minimum}..65535")
    return result


def normalize_date(value: str | int) -> str:
    text = str(value).strip().replace("-", "")
    try:
        return datetime.strptime(text, "%Y%m%d").strftime("%Y%m%d")
    except ValueError as error:
        raise TradeError(f"交易日无效：{value!r}") from error


def build_today_request_data(
    code: snapshots.QuoteCode,
    *,
    start: int = 0,
    count: int = 1800,
) -> bytes:
    start = _u16(start, "start")
    count = _u16(count, "count", positive=True)
    return (
        bytes((code.market_id, 0))
        + code.code.encode("ascii")
        + struct.pack("<HH", start, count)
    )


def build_history_request_data(
    code: snapshots.QuoteCode,
    trading_date: str | int,
    *,
    start: int = 0,
    count: int = 2000,
) -> bytes:
    date_text = normalize_date(trading_date)
    start = _u16(start, "start")
    count = _u16(count, "count", positive=True)
    return (
        struct.pack("<IH", int(date_text), code.market_id)
        + code.code.encode("ascii")
        + struct.pack("<HH", start, count)
    )


def _time_label(value: int) -> str:
    hour, minute = divmod(value, 60)
    return f"{hour:02d}:{minute:02d}"


def _side(status_raw: int) -> str:
    return {0: "buy", 1: "sell", 2: "neutral"}.get(
        status_raw, f"status_{status_raw}"
    )


def _trade_price_divisor(code: str) -> int:
    return 1000 if code.startswith(THREE_DECIMAL_PRICE_PREFIXES) else 100


def _parse_records(
    payload: bytes,
    *,
    offset: int,
    count: int,
    start: int,
    price_divisor: int,
) -> tuple[tuple[TradeTick, ...], int]:
    ticks: list[TradeTick] = []
    price_acc_raw = 0
    for index in range(count):
        if offset + 2 > len(payload):
            raise TradeError(f"第 {index + 1} 条成交缺少时间")
        time_minutes = int.from_bytes(payload[offset : offset + 2], "little")
        offset += 2
        if time_minutes >= 24 * 60:
            raise TradeError(f"第 {index + 1} 条成交时间无效：{time_minutes}")
        try:
            price_delta, offset = snapshots.consume_varint(payload, offset)
            volume, offset = snapshots.consume_varint(payload, offset)
            order_count, offset = snapshots.consume_varint(payload, offset)
            status_raw, offset = snapshots.consume_varint(payload, offset)
            tail_raw, offset = snapshots.consume_varint(payload, offset)
        except snapshots.SnapshotError as error:
            raise TradeError(f"第 {index + 1} 条成交变长字段不完整") from error
        price_acc_raw += price_delta
        price = price_acc_raw / price_divisor
        if not math.isfinite(price) or price < 0:
            raise TradeError(f"第 {index + 1} 条成交价格无效：{price}")
        if volume < 0 or order_count < 0:
            raise TradeError(f"第 {index + 1} 条成交量或笔数为负")
        ticks.append(TradeTick(
            index=index,
            absolute_index=start + index,
            time_minutes=time_minutes,
            time_label=_time_label(time_minutes),
            price=price,
            volume_hand=volume,
            order_count=order_count,
            status_raw=status_raw,
            side=_side(status_raw),
            price_delta_raw=price_delta,
            price_acc_raw=price_acc_raw,
            tail_raw=tail_raw,
        ))
    return tuple(ticks), offset


def parse_today_payload(
    payload: bytes,
    code: snapshots.QuoteCode,
    *,
    start: int,
    request_count: int,
    trading_date: str | None = None,
) -> TradePage:
    if len(payload) < 2:
        raise TradeError("当日成交响应少于 2 字节")
    count = int.from_bytes(payload[:2], "little")
    price_divisor = _trade_price_divisor(code.code)
    ticks, offset = _parse_records(
        payload,
        offset=2,
        count=count,
        start=start,
        price_divisor=price_divisor,
    )
    if offset != len(payload):
        raise TradeError(f"当日成交响应残留 {len(payload) - offset} 字节")
    return TradePage(
        market_id=code.market_id,
        code=code.code,
        trading_date=trading_date,
        start=start,
        request_count=request_count,
        price_divisor=price_divisor,
        price_base_raw=None,
        ticks=ticks,
    )


def parse_history_payload(
    payload: bytes,
    code: snapshots.QuoteCode,
    trading_date: str | int,
    *,
    start: int,
    request_count: int,
) -> TradePage:
    if len(payload) < 6:
        raise TradeError("历史成交响应少于 6 字节")
    count = int.from_bytes(payload[:2], "little")
    price_base = struct.unpack_from("<f", payload, 2)[0]
    if not math.isfinite(price_base):
        raise TradeError("历史成交价格基数不是有限数值")
    price_divisor = _trade_price_divisor(code.code)
    ticks, offset = _parse_records(
        payload,
        offset=6,
        count=count,
        start=start,
        price_divisor=price_divisor,
    )
    if offset != len(payload):
        raise TradeError(f"历史成交响应残留 {len(payload) - offset} 字节")
    return TradePage(
        market_id=code.market_id,
        code=code.code,
        trading_date=normalize_date(trading_date),
        start=start,
        request_count=request_count,
        price_divisor=price_divisor,
        price_base_raw=float(price_base),
        ticks=ticks,
    )


def _server_date(connection: transport.QuoteConnection) -> str:
    response = connection.call(TYPE_HEARTBEAT, b"")
    if len(response.data) < 10:
        raise TradeError("心跳响应缺少服务器交易日")
    return normalize_date(int.from_bytes(response.data[6:10], "little"))


def _download_one(
    connection: transport.QuoteConnection,
    code: snapshots.QuoteCode,
    *,
    trading_date: str | None,
    server_trade_date: str,
    page_size: int,
    max_pages: int,
    progress: Callable[[str], None] | None,
) -> TradeSeries:
    page_ticks: list[tuple[TradeTick, ...]] = []
    start = 0
    pages = 0
    price_base: float | None = None
    while True:
        if trading_date is None:
            command = TYPE_TODAY_TICKS
            request = build_today_request_data(code, start=start, count=page_size)
        else:
            command = TYPE_HISTORICAL_TICKS
            request = build_history_request_data(
                code, trading_date, start=start, count=page_size
            )
        response = connection.call(command, request)
        page = (
            parse_today_payload(
                response.data,
                code,
                start=start,
                request_count=page_size,
                trading_date=server_trade_date,
            )
            if trading_date is None
            else parse_history_payload(
                response.data,
                code,
                trading_date,
                start=start,
                request_count=page_size,
            )
        )
        if price_base is None:
            price_base = page.price_base_raw
        if not page.ticks:
            break
        if pages >= max_pages:
            raise TradeError(f"{code.display} 成交超过 {max_pages} 页安全上限")
        page_ticks.append(page.ticks)
        pages += 1
        if progress:
            progress(
                f"成交 {code.display} 第 {pages} 页："
                f"{len(page.ticks)} 条，累计 "
                f"{sum(len(items) for items in page_ticks)} 条"
            )
        # The server currently caps a response at 1,800 records even when a
        # larger count was requested.  A short page therefore does not mean
        # EOF: advance by the number actually returned and stop on an empty
        # page.  start=0 addresses the newest block; pages themselves are in
        # chronological order, so reverse page order before aggregation.
        if start + len(page.ticks) > 0xFFFF:
            raise TradeError(f"{code.display} 成交分页超过 uint16 游标")
        start += len(page.ticks)
    ticks = tuple(
        tick
        for items in reversed(page_ticks)
        for tick in items
    )
    return TradeSeries(
        market_id=code.market_id,
        code=code.code,
        trading_date=trading_date or server_trade_date,
        source_mode="history" if trading_date else "today",
        pages=pages,
        page_size=page_size,
        price_divisor=_trade_price_divisor(code.code),
        price_base_raw=price_base,
        ticks=ticks,
    )


def download_trades(
    endpoints: Sequence[transport.HostEndpoint],
    codes: Sequence[snapshots.QuoteCode],
    *,
    trading_date: str | int | None = None,
    page_size: int | None = None,
    max_pages: int = 100,
    timeout: float = 8.0,
    progress: Callable[[str], None] | None = None,
) -> TradeDownloadResult:
    unique_codes = tuple(dict.fromkeys(codes))
    if not unique_codes:
        raise TradeError("成交代码列表不能为空")
    date_text = normalize_date(trading_date) if trading_date is not None else None
    page_size = page_size or (2000 if date_text else 1800)
    _u16(page_size, "page_size", positive=True)
    if max_pages <= 0:
        raise TradeError("max_pages 必须大于 0")
    completed: dict[tuple[int, str], TradeSeries] = {}
    next_index = 0
    attempted: list[transport.HostEndpoint] = []
    failures: list[str] = []
    successful_endpoint: transport.HostEndpoint | None = None
    successful_server = ""
    successful_date = ""
    for endpoint in endpoints:
        if next_index >= len(unique_codes):
            break
        attempted.append(endpoint)
        try:
            with transport.QuoteConnection(endpoint, timeout) as connection:
                server_date = _server_date(connection)
                if progress:
                    progress(
                        f"已连接 {endpoint.address}（"
                        f"{connection.server_name or endpoint.name or '未命名主站'}），"
                        f"交易日 {server_date}"
                    )
                while next_index < len(unique_codes):
                    code = unique_codes[next_index]
                    item = _download_one(
                        connection,
                        code,
                        trading_date=date_text,
                        server_trade_date=server_date,
                        page_size=page_size,
                        max_pages=max_pages,
                        progress=progress,
                    )
                    completed[item.key] = item
                    next_index += 1
                    successful_endpoint = endpoint
                    successful_server = connection.server_name
                    successful_date = server_date
        except (
            TradeError,
            transport.DownloadError,
            OSError,
            TimeoutError,
            zlib.error,
            struct.error,
        ) as error:
            failures.append(f"{endpoint.address}: {error}")
            if progress:
                progress(f"成交主站失败，重试当前证券：{endpoint.address}（{error}）")
    if next_index < len(unique_codes) or successful_endpoint is None:
        detail = "\n".join(f"  - {item}" for item in failures)
        raise TradeError(
            f"成交只完成 {next_index}/{len(unique_codes)} 只：\n{detail}"
        )
    return TradeDownloadResult(
        series=tuple(completed[item.key] for item in unique_codes),
        requested=len(unique_codes),
        endpoint=successful_endpoint,
        server_name=successful_server,
        server_trade_date=successful_date,
        attempted_endpoints=tuple(attempted),
        failures=tuple(failures),
    )


def aggregate_ticks(ticks: Sequence[TradeTick]) -> dict[str, object]:
    groups: dict[int, list[TradeTick]] = {}
    for item in ticks:
        groups.setdefault(item.time_minutes, []).append(item)

    def aggregate(values: Sequence[TradeTick]) -> dict[str, object]:
        total_volume = sum(item.volume_hand for item in values)
        total_amount = sum(item.amount_yuan for item in values)
        return {
            "tick_count": len(values),
            "volume_hand": total_volume,
            "amount_yuan": total_amount,
            "order_count": sum(item.order_count for item in values),
            "vwap": total_amount / total_volume / 100.0 if total_volume else None,
            "first_price": values[0].price if values else None,
            "last_price": values[-1].price if values else None,
            "high_price": max((item.price for item in values), default=None),
            "low_price": min((item.price for item in values), default=None),
            "buy_volume_hand": sum(
                item.volume_hand for item in values if item.side == "buy"
            ),
            "sell_volume_hand": sum(
                item.volume_hand for item in values if item.side == "sell"
            ),
            "neutral_volume_hand": sum(
                item.volume_hand for item in values if item.side == "neutral"
            ),
            "buy_amount_yuan": sum(
                item.amount_yuan for item in values if item.side == "buy"
            ),
            "sell_amount_yuan": sum(
                item.amount_yuan for item in values if item.side == "sell"
            ),
            "neutral_amount_yuan": sum(
                item.amount_yuan for item in values if item.side == "neutral"
            ),
            "status_counts": {
                str(status): sum(item.status_raw == status for item in values)
                for status in sorted({item.status_raw for item in values})
            },
        }

    minutes = [
        {
            "time": _time_label(minute),
            "time_minutes": minute,
            **aggregate(values),
        }
        for minute, values in sorted(groups.items())
    ]
    post_close_status_5 = tuple(
        item for item in ticks
        if item.status_raw == 5 and item.time_minutes > 15 * 60
    )
    opening_0925 = aggregate(groups.get(9 * 60 + 25, ()))
    opening_0925["execution_status"] = (
        "executed-0925" if opening_0925["tick_count"] else "no-0925-trade"
    )
    closing_1500 = aggregate(groups.get(15 * 60, ()))
    closing_1500["execution_status"] = (
        "executed-1500" if closing_1500["tick_count"] else "no-1500-trade"
    )
    return {
        **aggregate(ticks),
        "first_time": ticks[0].time_label if ticks else None,
        "last_time": ticks[-1].time_label if ticks else None,
        "minute_count": len(minutes),
        "auction_0925": opening_0925,
        "closing_1500": closing_1500,
        "post_close_status_5": {
            **aggregate(post_close_status_5),
            "first_time": (
                post_close_status_5[0].time_label
                if post_close_status_5 else None
            ),
            "last_time": (
                post_close_status_5[-1].time_label
                if post_close_status_5 else None
            ),
        },
        "minutes": minutes,
    }


def series_to_dict(
    item: TradeSeries,
    security_master: dict[tuple[int, str], blocks.Security],
) -> dict[str, object]:
    security = security_master.get(item.key)
    return {
        "market_id": item.market_id,
        "code": item.code,
        "security_id": item.security_id,
        "name": security.name if security else "",
        "name_resolved": security is not None,
        "trading_date": item.trading_date,
        "source_mode": item.source_mode,
        "pages": item.pages,
        "page_size": item.page_size,
        "price_divisor": item.price_divisor,
        "price_base_raw": item.price_base_raw,
        "summary": aggregate_ticks(item.ticks),
        "ticks": [asdict(tick) | {"amount_yuan": tick.amount_yuan} for tick in item.ticks],
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="下载通达信 0x0FC5/0x0FC6 当日或历史 L1 成交明细。"
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument(
        "--security", action="append", default=[], metavar="[MARKET:]CODE",
        help="证券，如 sz000001、0:000001；可重复",
    )
    parser.add_argument("--date", help="历史交易日 YYYYMMDD；省略取当日")
    parser.add_argument("--page-size", type=int)
    parser.add_argument("--max-pages", type=int, default=100)
    parser.add_argument("--host", action="append", default=[])
    parser.add_argument("--max-hosts", type=int, default=5)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--download", action="store_true", help="明确允许联网")
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-trades.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        if not args.security:
            raise TradeError("请至少提供一个 --security")
        codes = tuple(depth.parse_code(root, value) for value in args.security)
        output = args.output.expanduser().resolve()
        if not args.download:
            print(json.dumps({
                "action": "dry-run",
                "command": "0x0FC6" if args.date else "0x0FC5",
                "date": normalize_date(args.date) if args.date else None,
                "securities": [item.display for item in codes],
                "page_size": args.page_size,
                "output": str(output),
            }, ensure_ascii=False, indent=2))
            return 0
        endpoints = depth.endpoint_candidates(root, args.host, args.max_hosts)
        result = download_trades(
            endpoints,
            codes,
            trading_date=args.date,
            page_size=args.page_size,
            max_pages=args.max_pages,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        master = blocks.load_security_master(root / "T0002" / "hq_cache")
        model = {
            "schema": "tdx-trades-v1",
            "generated_at": datetime.now().astimezone().isoformat(),
            "command": "0x0FC6" if args.date else "0x0FC5",
            "endpoint": result.endpoint.address,
            "server_name": result.server_name,
            "server_trade_date": result.server_trade_date,
            "requested": result.requested,
            "received": len(result.series),
            "data_scope_note": (
                "公开 L1 成交明细，时间精度为分钟；不是 Level2 秒级逐笔委托"
            ),
            "volume_note": "个股成交量单位为手，金额=价格×成交量×100",
            "ordering_note": (
                "ticks 已按时间正序排列；absolute_index 是服务端从最新纪录"
                "向历史回溯的偏移，数值越大越早"
            ),
            "status_note": (
                "status=0/1/2 分别为买/卖/中性；已验证的深市样本中 "
                "status=5 为 15:05—15:30 盘后定价成交，已单独汇总"
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
        print(
            f"已取得 {len(result.series)} 只证券成交明细、"
            f"共 {sum(len(item.ticks) for item in result.series):,} 条：{output}"
        )
    except (
        OSError,
        UnicodeError,
        ValueError,
        blocks.BlockFormatError,
        updater.UpdateError,
        transport.DownloadError,
        snapshots.SnapshotError,
        TradeError,
    ) as error:
        print(f"成交明细更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

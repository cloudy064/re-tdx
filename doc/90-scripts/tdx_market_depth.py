#!/usr/bin/env python3
"""Read TDX five-level order books over 7709/TCP (command 0x0547)."""

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
from typing import Callable, Iterable, Sequence

import download_tdx_minute as transport
import extract_tdx_blocks as blocks
import tdx_market_snapshot as snapshots
import update_tdx_blocks as updater


TYPE_MARKET_DEPTH = 0x0547
RESPONSE_XOR = 0x93


class MarketDepthError(RuntimeError):
    """Raised when a depth request or response is invalid."""


@dataclass(frozen=True)
class QuoteLevel:
    price: float
    volume_hand: int

    @property
    def amount_yuan(self) -> float:
        return self.price * self.volume_hand * 100.0


@dataclass(frozen=True)
class MarketDepth:
    market_id: int
    code: str
    active: int
    last_price: float
    pre_close_price: float
    open_price: float
    high_price: float
    low_price: float
    update_time_raw: int
    status_raw: int
    total_hand: int
    current_hand: int
    amount: float
    inside_dish: int
    outer_disc: int
    unknown_after_outer_raw: int
    open_amount_yuan: float
    buy_levels: tuple[QuoteLevel, ...]
    sell_levels: tuple[QuoteLevel, ...]
    tail_hex: str

    @property
    def key(self) -> tuple[int, str]:
        return self.market_id, self.code

    @property
    def change_pct(self) -> float | None:
        if self.last_price <= 0 or self.pre_close_price <= 0:
            return None
        return (self.last_price / self.pre_close_price - 1.0) * 100.0

    @property
    def bid1_amount_yuan(self) -> float | None:
        return self.buy_levels[0].amount_yuan if self.buy_levels else None

    @property
    def ask1_amount_yuan(self) -> float | None:
        return self.sell_levels[0].amount_yuan if self.sell_levels else None


@dataclass(frozen=True)
class DepthDownloadResult:
    depths: tuple[MarketDepth, ...]
    requested: int
    completed_batches: int
    endpoint: transport.HostEndpoint
    server_name: str
    attempted_endpoints: tuple[transport.HostEndpoint, ...]
    failures: tuple[str, ...]


def build_depth_request_data(
    codes: Sequence[snapshots.QuoteCode],
    cursors: dict[tuple[int, str], int] | None = None,
) -> bytes:
    if len(codes) > 0xFFFF:
        raise MarketDepthError("单次五档请求代码数超过 uint16")
    cursors = cursors or {}
    output = bytearray(len(codes).to_bytes(2, "little", signed=False))
    for item in codes:
        cursor = int(cursors.get(item.key, 0))
        if not (0 <= cursor <= 0xFFFFFFFF):
            raise MarketDepthError(f"{item.display} 游标超出 uint32：{cursor}")
        output.append(item.market_id)
        output.extend(item.code.encode("ascii"))
        output.extend(cursor.to_bytes(4, "little", signed=False))
    return bytes(output)


def _split_depth_records(
    data: bytes,
    requested_codes: Sequence[snapshots.QuoteCode],
    count: int,
) -> list[bytes]:
    if count == 0:
        return []
    markers = [
        bytes((item.market_id,)) + item.code.encode("ascii")
        for item in requested_codes
    ]
    if not markers:
        raise MarketDepthError("多记录五档响应缺少请求代码，无法识别边界")
    starts = [0]
    search_from = 7
    while len(starts) < count:
        positions = [
            position
            for marker in markers
            if (position := data.find(marker, search_from)) >= 0
        ]
        if not positions:
            raise MarketDepthError(
                f"五档声明 {count} 条，只识别到 {len(starts)} 个记录边界"
            )
        start = min(positions)
        starts.append(start)
        search_from = start + 7
    return [
        data[start : starts[index + 1] if index + 1 < len(starts) else len(data)]
        for index, start in enumerate(starts)
    ]


def parse_depth_record(record: bytes) -> MarketDepth:
    if len(record) < 9:
        raise MarketDepthError("五档记录头部不完整")
    market_id = record[0]
    try:
        code = record[1:7].decode("ascii")
    except UnicodeDecodeError as error:
        raise MarketDepthError("五档证券代码不是 ASCII") from error
    if market_id not in (0, 1, 2) or len(code) != 6 or not code.isdigit():
        raise MarketDepthError(f"五档证券标识无效：{market_id}/{code!r}")
    active = int.from_bytes(record[7:9], "little", signed=False)
    prices, offset = snapshots._decode_prices(record, 9)
    if offset + 4 > len(record):
        raise MarketDepthError(f"{code} 五档记录缺少更新时间")
    update_time_raw = int.from_bytes(record[offset : offset + 4], "little")
    offset += 4
    status_raw, offset = snapshots.consume_varint(record, offset)
    total_hand, offset = snapshots.consume_varint(record, offset)
    current_hand, offset = snapshots.consume_varint(record, offset)
    if offset + 4 > len(record):
        raise MarketDepthError(f"{code} 五档记录缺少成交额")
    amount_raw = int.from_bytes(record[offset : offset + 4], "little")
    offset += 4
    inside_dish, offset = snapshots.consume_varint(record, offset)
    outer_disc, offset = snapshots.consume_varint(record, offset)
    unknown_after_outer_raw, offset = snapshots.consume_varint(record, offset)
    open_amount_raw, offset = snapshots.consume_varint(record, offset)

    divisor = snapshots._price_divisor(code)
    quote_prices = {
        key: raw / divisor / 1000.0
        for key, raw in prices.items()
    }
    buy_levels: list[QuoteLevel] = []
    sell_levels: list[QuoteLevel] = []
    for level in range(1, 6):
        try:
            buy_delta, offset = snapshots.consume_varint(record, offset)
            sell_delta, offset = snapshots.consume_varint(record, offset)
            buy_volume, offset = snapshots.consume_varint(record, offset)
            sell_volume, offset = snapshots.consume_varint(record, offset)
        except snapshots.SnapshotError as error:
            raise MarketDepthError(f"{code} 五档第 {level} 档不完整") from error
        buy_price = (prices["current"] + buy_delta * 10) / divisor / 1000.0
        sell_price = (prices["current"] + sell_delta * 10) / divisor / 1000.0
        buy_levels.append(QuoteLevel(buy_price, buy_volume))
        sell_levels.append(QuoteLevel(sell_price, sell_volume))
    amount = transport.decode_wire_number(amount_raw)
    finite = (*quote_prices.values(), amount)
    if not all(math.isfinite(value) for value in finite):
        raise MarketDepthError(f"{code} 五档记录包含非有限数值")
    return MarketDepth(
        market_id=market_id,
        code=code,
        active=active,
        last_price=quote_prices["current"],
        pre_close_price=quote_prices["pre_close"],
        open_price=quote_prices["open"],
        high_price=quote_prices["high"],
        low_price=quote_prices["low"],
        update_time_raw=update_time_raw,
        status_raw=status_raw,
        total_hand=total_hand,
        current_hand=current_hand,
        amount=amount,
        inside_dish=inside_dish,
        outer_disc=outer_disc,
        unknown_after_outer_raw=unknown_after_outer_raw,
        open_amount_yuan=float(open_amount_raw * 10),
        buy_levels=tuple(buy_levels),
        sell_levels=tuple(sell_levels),
        tail_hex=record[offset:].hex(),
    )


def parse_depth_payload(
    payload: bytes,
    requested_codes: Sequence[snapshots.QuoteCode],
) -> tuple[MarketDepth, ...]:
    decoded = bytes(byte ^ RESPONSE_XOR for byte in payload)
    if len(decoded) < 2:
        raise MarketDepthError("五档响应少于 2 字节")
    count = int.from_bytes(decoded[:2], "little", signed=False)
    if count > len(requested_codes):
        raise MarketDepthError(
            f"五档响应记录数 {count} 超过请求数 {len(requested_codes)}"
        )
    records = _split_depth_records(decoded[2:], requested_codes, count)
    parsed = tuple(parse_depth_record(record) for record in records)
    requested_keys = {item.key for item in requested_codes}
    if any(item.key not in requested_keys for item in parsed):
        raise MarketDepthError("五档响应包含未请求的证券")
    return parsed


def _batches(
    values: Sequence[snapshots.QuoteCode],
    size: int,
) -> tuple[tuple[snapshots.QuoteCode, ...], ...]:
    if size <= 0:
        raise MarketDepthError("batch_size 必须是正整数")
    return tuple(
        tuple(values[offset : offset + size])
        for offset in range(0, len(values), size)
    )


def download_depths(
    endpoints: Sequence[transport.HostEndpoint],
    codes: Sequence[snapshots.QuoteCode],
    *,
    batch_size: int = 80,
    timeout: float = 5.0,
    progress: Callable[[str], None] | None = None,
) -> DepthDownloadResult:
    if not endpoints:
        raise MarketDepthError("没有可尝试的行情主站")
    if not codes:
        raise MarketDepthError("五档代码列表不能为空")
    if timeout <= 0:
        raise MarketDepthError("timeout 必须大于 0")
    unique_codes = tuple(dict.fromkeys(codes))
    batches = _batches(unique_codes, batch_size)
    depths: dict[tuple[int, str], MarketDepth] = {}
    batch_index = 0
    failures: list[str] = []
    attempted: list[transport.HostEndpoint] = []
    successful_endpoint: transport.HostEndpoint | None = None
    successful_server = ""
    for endpoint in endpoints:
        if batch_index >= len(batches):
            break
        attempted.append(endpoint)
        try:
            with transport.QuoteConnection(endpoint, timeout) as connection:
                if progress:
                    server = connection.server_name or endpoint.name or "未命名主站"
                    progress(f"已连接 {endpoint.address}（{server}）")
                while batch_index < len(batches):
                    batch = batches[batch_index]
                    response = connection.call(
                        TYPE_MARKET_DEPTH,
                        build_depth_request_data(batch),
                    )
                    parsed = parse_depth_payload(response.data, batch)
                    depths.update((item.key, item) for item in parsed)
                    batch_index += 1
                    successful_endpoint = endpoint
                    successful_server = connection.server_name
                    if progress:
                        progress(
                            f"五档批次 {batch_index}/{len(batches)}："
                            f"已收到 {len(depths)}/{len(unique_codes)} 条"
                        )
        except (
            MarketDepthError,
            transport.DownloadError,
            OSError,
            TimeoutError,
            zlib.error,
            struct.error,
        ) as error:
            failures.append(f"{endpoint.address}: {error}")
            if progress:
                progress(f"主站失败，继续未完成批次：{endpoint.address}（{error}）")
    if batch_index < len(batches) or successful_endpoint is None:
        detail = "\n".join(f"  - {failure}" for failure in failures)
        raise MarketDepthError(
            f"五档只完成 {batch_index}/{len(batches)} 批；请重试或增加 --max-hosts。\n"
            f"{detail}"
        )
    return DepthDownloadResult(
        depths=tuple(depths[key] for key in sorted(depths)),
        requested=len(unique_codes),
        completed_batches=batch_index,
        endpoint=successful_endpoint,
        server_name=successful_server,
        attempted_endpoints=tuple(attempted),
        failures=tuple(failures),
    )


def parse_code(root: Path, value: str) -> snapshots.QuoteCode:
    text = value.strip().lower()
    if ":" in text:
        market, code = text.split(":", 1)
        aliases = {"sz": 0, "sh": 1, "bj": 2}
        market_id = aliases[market] if market in aliases else int(market)
    elif len(text) == 8 and text[:2] in {"sz", "sh", "bj"}:
        market_id = {"sz": 0, "sh": 1, "bj": 2}[text[:2]]
        code = text[2:]
    else:
        code = text
        market = transport.infer_market(root, code, None)
        market_id = transport.MARKET_IDS[market]
    return snapshots.QuoteCode(market_id, code)


def endpoint_candidates(
    root: Path,
    hosts: Sequence[str],
    max_hosts: int,
) -> tuple[transport.HostEndpoint, ...]:
    explicit = [transport.parse_host(value) for value in hosts]
    configured = transport.load_hq_hosts(root / "T0002" / "newhost.lst")
    return transport.unique_endpoints((*explicit, *configured))[:max_hosts]


def depth_to_dict(
    depth: MarketDepth,
    security_master: dict[tuple[int, str], blocks.Security],
) -> dict[str, object]:
    item = asdict(depth)
    security = security_master.get(depth.key)
    item.update({
        "security_id": snapshots.QuoteCode(*depth.key).display,
        "name": security.name if security else "",
        "name_resolved": security is not None,
        "change_pct": depth.change_pct,
        "bid1_amount_yuan": depth.bid1_amount_yuan,
        "ask1_amount_yuan": depth.ask1_amount_yuan,
    })
    return item


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="直连通达信 7709 主站取得个股买卖五档，并计算买一/卖一金额。"
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument(
        "--security", action="append", default=[], metavar="[MARKET:]CODE",
        help="证券，如 0:000001、sz000001；可重复",
    )
    parser.add_argument("--host", action="append", default=[])
    parser.add_argument("--max-hosts", type=int, default=5)
    parser.add_argument("--batch-size", type=int, default=80)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument(
        "--download", action="store_true", help="明确允许联网；省略时只显示计划",
    )
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-market-depth.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        if not args.security:
            raise MarketDepthError("请至少提供一个 --security")
        codes = tuple(parse_code(root, value) for value in args.security)
        output = args.output.expanduser().resolve()
        if not args.download:
            print(json.dumps({
                "action": "dry-run",
                "command": "0x0547",
                "securities": [item.display for item in codes],
                "output": str(output),
            }, ensure_ascii=False, indent=2))
            return 0
        endpoints = endpoint_candidates(root, args.host, args.max_hosts)
        result = download_depths(
            endpoints,
            codes,
            batch_size=args.batch_size,
            timeout=args.timeout,
            progress=print if args.verbose else None,
        )
        security_master = blocks.load_security_master(root / "T0002" / "hq_cache")
        model = {
            "schema": "tdx-market-depth-v1",
            "generated_at": datetime.now().astimezone().isoformat(),
            "command": "0x0547",
            "endpoint": result.endpoint.address,
            "server_name": result.server_name,
            "requested": result.requested,
            "received": len(result.depths),
            "records": [
                depth_to_dict(depth, security_master)
                for depth in result.depths
            ],
        }
        rendered = json.dumps(
            model,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        updater.atomic_write_text(output, rendered + "\n", "utf-8")
        print(f"已取得 {len(result.depths)}/{result.requested} 条五档：{output}")
    except (
        OSError,
        UnicodeError,
        ValueError,
        blocks.BlockFormatError,
        updater.UpdateError,
        snapshots.SnapshotError,
        transport.DownloadError,
        MarketDepthError,
    ) as error:
        print(f"五档更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

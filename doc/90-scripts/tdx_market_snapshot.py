#!/usr/bin/env python3
"""Batch quote snapshots from TDX 7709/TCP (command 0x054C)."""

from __future__ import annotations

import math
import struct
import zlib
from dataclasses import dataclass
from typing import Callable, Sequence

import download_tdx_minute as transport


TYPE_SNAPSHOTS = 0x054C
REQUEST_HEADER = bytes.fromhex("0500000000000000")
ETF_PRICE_PREFIXES = ("15", "16", "50", "51", "52", "53", "56", "58")
BOND_PRICE_PREFIXES = ("10", "11", "12")


class SnapshotError(RuntimeError):
    """Raised when a snapshot request or response is invalid."""


@dataclass(frozen=True)
class QuoteCode:
    market_id: int
    code: str

    def __post_init__(self) -> None:
        if self.market_id not in (0, 1, 2):
            raise SnapshotError(f"市场号无效：{self.market_id}")
        if len(self.code) != 6 or not self.code.isdigit():
            raise SnapshotError(f"代码必须是 6 位数字：{self.code!r}")

    @property
    def key(self) -> tuple[int, str]:
        return self.market_id, self.code

    @property
    def display(self) -> str:
        prefix = ("SZ", "SH", "BJ")[self.market_id]
        return f"{prefix}{self.code}"


@dataclass(frozen=True)
class QuoteSnapshot:
    market_id: int
    code: str
    active: int
    last_price: float
    pre_close_price: float
    open_price: float
    high_price: float
    low_price: float
    time_raw: int
    total_hand: int
    current_hand: int
    amount: float
    inside_dish: int
    outer_disc: int
    open_amount_yuan: float

    @property
    def key(self) -> tuple[int, str]:
        return self.market_id, self.code

    @property
    def change_pct(self) -> float | None:
        if self.last_price <= 0 or self.pre_close_price <= 0:
            return None
        return (self.last_price / self.pre_close_price - 1.0) * 100.0


@dataclass(frozen=True)
class SnapshotDownloadResult:
    quotes: tuple[QuoteSnapshot, ...]
    requested: int
    completed_batches: int
    endpoint: transport.HostEndpoint
    server_name: str
    attempted_endpoints: tuple[transport.HostEndpoint, ...]
    failures: tuple[str, ...]


def build_snapshot_request_data(codes: Sequence[QuoteCode]) -> bytes:
    if len(codes) > 0xFFFF:
        raise SnapshotError("单次快照请求代码数超过 uint16")
    data = bytearray(REQUEST_HEADER)
    data.extend(len(codes).to_bytes(2, "little", signed=False))
    for item in codes:
        data.append(item.market_id)
        data.extend(item.code.encode("ascii"))
    return bytes(data)


def consume_varint(payload: bytes, offset: int) -> tuple[int, int]:
    if offset >= len(payload):
        raise SnapshotError("变长整数越过快照记录末尾")
    start = offset
    value = 0
    shift = 0
    while True:
        if offset >= len(payload):
            raise SnapshotError("快照变长整数没有终止字节")
        byte = payload[offset]
        if offset == start:
            value = byte & 0x3F
            shift = 6
        else:
            value += (byte & 0x7F) << shift
            shift += 7
        offset += 1
        if byte & 0x80 == 0:
            break
        if shift > 48:
            raise SnapshotError("快照变长整数过长")
    if payload[start] & 0x40:
        value = -value
    return value, offset


def _decode_prices(
    record: bytes,
    offset: int,
) -> tuple[dict[str, int], int]:
    current_delta, offset = consume_varint(record, offset)
    pre_close_delta, offset = consume_varint(record, offset)
    open_delta, offset = consume_varint(record, offset)
    high_delta, offset = consume_varint(record, offset)
    low_delta, offset = consume_varint(record, offset)
    current_milli = current_delta * 10
    return {
        "current": current_milli,
        "pre_close": (pre_close_delta + current_delta) * 10,
        "open": (open_delta + current_delta) * 10,
        "high": (high_delta + current_delta) * 10,
        "low": (low_delta + current_delta) * 10,
    }, offset


def _price_divisor(code: str) -> int:
    if code.startswith(BOND_PRICE_PREFIXES):
        return 100
    return 10 if code.startswith(ETF_PRICE_PREFIXES) else 1


def _split_records(
    data: bytes,
    count: int,
) -> list[bytes]:
    if count == 0:
        return []
    starts: list[int] = []
    for position in range(max(0, len(data) - 6)):
        if data[position] not in (0, 1, 2):
            continue
        code_bytes = data[position + 1 : position + 7]
        if len(code_bytes) == 6 and all(48 <= byte <= 57 for byte in code_bytes):
            starts.append(position)
    if len(starts) != count:
        raise SnapshotError(
            f"快照声明 {count} 条，但识别到 {len(starts)} 个记录边界"
        )
    if starts and starts[0] != 0:
        raise SnapshotError(f"第一条快照记录从异常偏移 {starts[0]} 开始")
    return [
        data[start : starts[index + 1] if index + 1 < len(starts) else len(data)]
        for index, start in enumerate(starts)
    ]


def parse_snapshot_record(
    record: bytes,
    expected: QuoteCode | None = None,
) -> QuoteSnapshot:
    if len(record) < 9:
        raise SnapshotError("快照记录头部不完整")
    market_id = record[0]
    try:
        code = record[1:7].decode("ascii")
    except UnicodeDecodeError as error:
        raise SnapshotError("快照证券代码不是 ASCII") from error
    if market_id not in (0, 1, 2) or len(code) != 6 or not code.isdigit():
        raise SnapshotError(f"快照证券标识无效：{market_id}/{code!r}")
    if expected is not None and (market_id, code) != expected.key:
        raise SnapshotError(
            f"快照记录错位：期望 {expected.display}，收到 {market_id}/{code}"
        )

    active = int.from_bytes(record[7:9], "little", signed=False)
    prices, offset = _decode_prices(record, 9)
    time_raw, offset = consume_varint(record, offset)
    _, offset = consume_varint(record, offset)
    total_hand, offset = consume_varint(record, offset)
    current_hand, offset = consume_varint(record, offset)
    if offset + 4 > len(record):
        raise SnapshotError(f"{code} 快照缺少成交额字段")
    amount_raw = int.from_bytes(
        record[offset : offset + 4],
        "little",
        signed=False,
    )
    offset += 4
    inside_dish, offset = consume_varint(record, offset)
    outer_disc, offset = consume_varint(record, offset)
    _, offset = consume_varint(record, offset)
    open_amount_raw, _ = consume_varint(record, offset)

    divisor = _price_divisor(code)
    values = {
        key: raw / divisor / 1000.0
        for key, raw in prices.items()
    }
    amount = transport.decode_wire_number(amount_raw)
    finite_values = (*values.values(), amount)
    if not all(math.isfinite(value) for value in finite_values):
        raise SnapshotError(f"{code} 快照包含非有限数值")
    return QuoteSnapshot(
        market_id=market_id,
        code=code,
        active=active,
        last_price=values["current"],
        pre_close_price=values["pre_close"],
        open_price=values["open"],
        high_price=values["high"],
        low_price=values["low"],
        time_raw=time_raw,
        total_hand=total_hand,
        current_hand=current_hand,
        amount=amount,
        inside_dish=inside_dish,
        outer_disc=outer_disc,
        open_amount_yuan=float(open_amount_raw * 100),
    )


def parse_snapshots_payload(
    payload: bytes,
    requested_codes: Sequence[QuoteCode],
) -> tuple[QuoteSnapshot, ...]:
    if len(payload) < 4:
        raise SnapshotError("快照响应少于 4 字节")
    count = int.from_bytes(payload[2:4], "little", signed=False)
    if count > len(requested_codes):
        raise SnapshotError(
            f"快照响应记录数 {count} 超过请求数 {len(requested_codes)}"
        )
    records = _split_records(payload[4:], count)
    return tuple(
        parse_snapshot_record(record)
        for record in records
    )


def _batches(
    values: Sequence[QuoteCode],
    size: int,
) -> tuple[tuple[QuoteCode, ...], ...]:
    if size <= 0:
        raise SnapshotError("batch_size 必须是正整数")
    return tuple(
        tuple(values[offset : offset + size])
        for offset in range(0, len(values), size)
    )


def download_snapshots(
    endpoints: Sequence[transport.HostEndpoint],
    codes: Sequence[QuoteCode],
    *,
    batch_size: int = 80,
    timeout: float = 5.0,
    progress: Callable[[str], None] | None = None,
) -> SnapshotDownloadResult:
    if not endpoints:
        raise SnapshotError("没有可尝试的行情主站")
    if not codes:
        raise SnapshotError("快照代码列表不能为空")
    if timeout <= 0:
        raise SnapshotError("timeout 必须大于 0")
    unique_codes = tuple(dict.fromkeys(codes))
    batches = _batches(unique_codes, batch_size)
    quotes: dict[tuple[int, str], QuoteSnapshot] = {}
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
                        TYPE_SNAPSHOTS,
                        build_snapshot_request_data(batch),
                    )
                    parsed = parse_snapshots_payload(response.data, batch)
                    requested_keys = {item.key for item in batch}
                    quotes.update(
                        (quote.key, quote)
                        for quote in parsed
                        if quote.key in requested_keys
                    )
                    batch_index += 1
                    successful_endpoint = endpoint
                    successful_server = connection.server_name
                    if progress and (
                        batch_index == 1
                        or batch_index == len(batches)
                        or batch_index % 10 == 0
                    ):
                        progress(
                            f"行情批次 {batch_index}/{len(batches)}："
                            f"已收到 {len(quotes)}/{len(unique_codes)} 个快照"
                        )
        except (
            SnapshotError,
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
        raise SnapshotError(
            f"行情快照只完成 {batch_index}/{len(batches)} 批；"
            f"请重试或增加 --max-hosts。\n{detail}"
        )
    return SnapshotDownloadResult(
        quotes=tuple(quotes.values()),
        requested=len(unique_codes),
        completed_batches=batch_index,
        endpoint=successful_endpoint,
        server_name=successful_server,
        attempted_endpoints=tuple(attempted),
        failures=tuple(failures),
    )

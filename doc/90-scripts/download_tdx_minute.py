#!/usr/bin/env python3
"""Download TDX 1-minute bars over 7709/TCP and build an offline chart."""

from __future__ import annotations

import argparse
import math
import os
import re
import socket
import struct
import sys
import tempfile
import webbrowser
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Sequence

import extract_tdx_minute as local


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "output"
MARKET_IDS = {"sz": 0, "sh": 1, "bj": 2}
REQUEST_PREFIX = 0x0C
RESPONSE_PREFIX = b"\xB1\xCB\x74\x00"
CONTROL_DEFAULT = 1
TYPE_HANDSHAKE = 0x000D
TYPE_KLINES = 0x052D
PERIOD_1M = 7
PERIOD_PARAMETER = 1
MAX_PAGE_SIZE = 800
RESPONSE_HEADER = struct.Struct("<4sBIBHHH")
KLINE_REQUEST_DATA = struct.Struct("<H6sHHHHHI20s")


class DownloadError(RuntimeError):
    """Raised when a 7709 request or downloaded payload is invalid."""


@dataclass(frozen=True)
class HostEndpoint:
    host: str
    port: int = 7709
    name: str = ""

    @property
    def address(self) -> str:
        return f"{self.host}:{self.port}"


@dataclass(frozen=True)
class ResponseFrame:
    control: int
    message_id: int
    message_type: int
    data: bytes


@dataclass(frozen=True)
class DownloadResult:
    bars: tuple[local.MinuteBar, ...]
    pages: int
    endpoints: tuple[HostEndpoint, ...]
    reached_history_end: bool


def parse_endpoint(value: str) -> HostEndpoint:
    text = value.strip()
    if not text:
        raise DownloadError("主站地址不能为空")
    if ":" in text:
        host, separator, port_text = text.rpartition(":")
        if not separator or not host or not port_text.isdigit():
            raise DownloadError(f"主站地址格式无效：{value!r}")
        port = int(port_text)
    else:
        host = text
        port = 7709
    if not (1 <= port <= 65535):
        raise DownloadError(f"主站端口无效：{port}")
    return HostEndpoint(host=host, port=port, name="命令行指定")


def load_hq_hosts(path: Path) -> tuple[HostEndpoint, ...]:
    if not path.is_file():
        raise DownloadError(f"未找到行情主站配置：{path}")
    section = ""
    values: dict[int, dict[str, str]] = {}
    key_pattern = re.compile(r"(HostName|IPAddress|Port)(\d+)$", re.I)
    for raw_line in local.read_text_guess(path).splitlines():
        line = raw_line.strip()
        if not line or line.startswith((";", "#")):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip().upper()
            continue
        if section != "HQHOST" or "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        match = key_pattern.fullmatch(key)
        if not match:
            continue
        field, number_text = match.groups()
        normalized = {
            "hostname": "name",
            "ipaddress": "host",
            "port": "port",
        }[field.lower()]
        values.setdefault(int(number_text), {})[normalized] = value

    endpoints: list[HostEndpoint] = []
    for number in sorted(values):
        item = values[number]
        host = item.get("host", "").strip()
        port_text = item.get("port", "7709").strip()
        if not host or not port_text.isdigit():
            continue
        port = int(port_text)
        if 1 <= port <= 65535:
            endpoints.append(
                HostEndpoint(
                    host=host,
                    port=port,
                    name=item.get("name", "").strip(),
                )
            )
    if not endpoints:
        raise DownloadError(f"{path} 的 [HQHOST] 中没有可用主站")
    return tuple(endpoints)


def unique_endpoints(endpoints: Iterable[HostEndpoint]) -> tuple[HostEndpoint, ...]:
    result: list[HostEndpoint] = []
    seen: set[tuple[str, int]] = set()
    for endpoint in endpoints:
        key = (endpoint.host.casefold(), endpoint.port)
        if key in seen:
            continue
        seen.add(key)
        result.append(endpoint)
    return tuple(result)


def build_request_frame(
    message_id: int,
    message_type: int,
    data: bytes = b"",
) -> bytes:
    length = len(data) + 2
    if not (0 <= message_id <= 0xFFFFFFFF):
        raise DownloadError(f"消息 ID 超出 uint32：{message_id}")
    if not (0 <= length <= 0xFFFF):
        raise DownloadError(f"请求数据过长：{length}")
    return struct.pack(
        "<BIBHHH",
        REQUEST_PREFIX,
        message_id,
        CONTROL_DEFAULT,
        length,
        length,
        message_type,
    ) + data


def build_kline_request_data(
    market_id: int,
    code: str,
    start: int,
    count: int,
) -> bytes:
    if market_id not in MARKET_IDS.values():
        raise DownloadError(f"市场号无效：{market_id}")
    if not (len(code) == 6 and code.isdigit()):
        raise DownloadError(f"当前 7709 K 线接口要求 6 位代码：{code!r}")
    if not (0 <= start <= 0xFFFF):
        raise DownloadError(f"start 必须在 0—65535：{start}")
    if not (1 <= count <= MAX_PAGE_SIZE):
        raise DownloadError(f"count 必须在 1—{MAX_PAGE_SIZE}：{count}")
    return KLINE_REQUEST_DATA.pack(
        market_id,
        code.encode("ascii"),
        PERIOD_1M,
        PERIOD_PARAMETER,
        start,
        count,
        0,
        0,
        b"\x00" * 20,
    )


def receive_exact(stream: socket.socket, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        chunk = stream.recv(size - len(chunks))
        if not chunk:
            raise DownloadError(
                f"主站提前断开连接，期望 {size} 字节，只收到 {len(chunks)} 字节"
            )
        chunks.extend(chunk)
    return bytes(chunks)


def read_response(stream: socket.socket) -> ResponseFrame:
    header = receive_exact(stream, RESPONSE_HEADER.size)
    (
        prefix,
        control,
        message_id,
        _reserved,
        message_type,
        compressed_size,
        decoded_size,
    ) = RESPONSE_HEADER.unpack(header)
    if prefix != RESPONSE_PREFIX:
        raise DownloadError(f"响应帧前缀无效：{prefix.hex(' ')}")
    wire_data = receive_exact(stream, compressed_size)
    if compressed_size == decoded_size:
        data = wire_data
    else:
        try:
            data = zlib.decompress(wire_data)
        except zlib.error as error:
            raise DownloadError(f"响应 zlib 解压失败：{error}") from error
    if len(data) != decoded_size:
        raise DownloadError(
            f"响应解压长度不符：声明 {decoded_size}，实际 {len(data)}"
        )
    return ResponseFrame(
        control=control,
        message_id=message_id,
        message_type=message_type,
        data=data,
    )


class QuoteConnection:
    def __init__(self, endpoint: HostEndpoint, timeout: float) -> None:
        self.endpoint = endpoint
        self.timeout = timeout
        self.stream: socket.socket | None = None
        self.message_id = 0x01640801
        self.server_name = ""

    def __enter__(self) -> "QuoteConnection":
        try:
            self.stream = socket.create_connection(
                (self.endpoint.host, self.endpoint.port),
                timeout=self.timeout,
            )
            self.stream.settimeout(self.timeout)
            response = self.call(TYPE_HANDSHAKE, b"\x01")
        except BaseException:
            self.close()
            raise
        if len(response.data) < 189:
            self.close()
            raise DownloadError(
                f"{self.endpoint.address} 握手响应过短：{len(response.data)}"
            )
        self.server_name = (
            response.data[68:152]
            .decode("gb18030", errors="ignore")
            .replace("\x00", "")
            .strip()
        )
        return self

    def __exit__(self, *_exc_info: object) -> None:
        self.close()

    def close(self) -> None:
        if self.stream is not None:
            try:
                self.stream.close()
            finally:
                self.stream = None

    def call(self, message_type: int, data: bytes = b"") -> ResponseFrame:
        if self.stream is None:
            raise DownloadError("行情连接尚未建立")
        message_id = self.message_id
        self.message_id = (self.message_id + 1) & 0xFFFFFFFF
        self.stream.sendall(build_request_frame(message_id, message_type, data))
        response = read_response(self.stream)
        if response.message_id != message_id:
            raise DownloadError(
                f"响应消息 ID 不匹配：请求 0x{message_id:08X}，"
                f"响应 0x{response.message_id:08X}"
            )
        if response.message_type != message_type:
            raise DownloadError(
                f"响应命令不匹配：请求 0x{message_type:04X}，"
                f"响应 0x{response.message_type:04X}"
            )
        return response

    def get_minute_page(
        self,
        market_id: int,
        code: str,
        start: int,
        count: int,
        index_mode: bool,
    ) -> tuple[local.MinuteBar, ...]:
        data = build_kline_request_data(market_id, code, start, count)
        response = self.call(TYPE_KLINES, data)
        return parse_kline_payload(response.data, index_mode=index_mode)


def consume_varint(payload: bytes, offset: int) -> tuple[int, int]:
    if offset >= len(payload):
        raise DownloadError("K 线变长整数越过响应末尾")
    start = offset
    value = 0
    shift = 0
    while True:
        if offset >= len(payload):
            raise DownloadError("K 线变长整数没有终止字节")
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
        if shift > 34:
            raise DownloadError("K 线变长整数过长")
    if payload[start] & 0x40:
        value = -value
    return value, offset


def decode_wire_number(value: int) -> float:
    if value == 0:
        return 0.0
    signed = int.from_bytes(value.to_bytes(4, "big"), "big", signed=True)
    exponent = signed >> 24
    high_byte = (signed >> 16) & 0xFF
    middle_byte = (signed >> 8) & 0xFF
    low_byte = signed & 0xFF
    base = math.pow(2.0, float(exponent * 2 - 0x7F))
    if high_byte > 0x80:
        high = base * (64.0 + float(high_byte & 0x7F)) / 64.0
    else:
        high = base * float(high_byte) / 128.0
    scale = 2.0 if high_byte & 0x80 else 1.0
    middle = base * float(middle_byte) / 32768.0 * scale
    low = base * float(low_byte) / 8388608.0 * scale
    return base + high + middle + low


def parse_kline_payload(
    payload: bytes,
    *,
    index_mode: bool,
) -> tuple[local.MinuteBar, ...]:
    if len(payload) < 2:
        raise DownloadError("K 线响应少于 2 字节")
    count = int.from_bytes(payload[:2], "little")
    if len(payload) == 2 and count:
        raise DownloadError(
            f"K 线响应只有记录数 {count} 而没有记录；"
            "可能使用了不兼容的短请求格式"
        )
    if count > MAX_PAGE_SIZE:
        raise DownloadError(
            f"K 线响应记录数异常：{count}；可能使用了不兼容的短请求格式"
        )
    offset = 2
    previous_close_milli = 0
    bars: list[local.MinuteBar] = []
    for record_number in range(1, count + 1):
        if offset + 4 > len(payload):
            raise DownloadError(f"第 {record_number} 根 K 线缺少时间字段")
        encoded_date, minute_of_day = struct.unpack_from("<HH", payload, offset)
        offset += 4
        date_value = local.decode_date(encoded_date)
        time_value = local.decode_time(minute_of_day)

        open_delta, offset = consume_varint(payload, offset)
        close_delta, offset = consume_varint(payload, offset)
        high_delta, offset = consume_varint(payload, offset)
        low_delta, offset = consume_varint(payload, offset)
        open_milli = previous_close_milli + open_delta
        close_milli = open_milli + close_delta
        high_milli = open_milli + high_delta
        low_milli = open_milli + low_delta
        previous_close_milli = close_milli

        if offset + 8 > len(payload):
            raise DownloadError(f"第 {record_number} 根 K 线缺少量额字段")
        volume_raw, amount_raw = struct.unpack_from("<II", payload, offset)
        offset += 8
        up_count = 0
        down_count = 0
        if index_mode:
            if offset + 4 > len(payload):
                raise DownloadError(
                    f"第 {record_number} 根指数 K 线缺少上涨/下跌家数字段"
                )
            up_count, down_count = struct.unpack_from("<HH", payload, offset)
            offset += 4

        volume_value = decode_wire_number(volume_raw)
        amount_value = decode_wire_number(amount_raw)
        if not all(
            math.isfinite(value)
            for value in (
                volume_value,
                amount_value,
                open_milli,
                close_milli,
                high_milli,
                low_milli,
            )
        ):
            raise DownloadError(f"第 {record_number} 根 K 线包含非有限数值")
        volume = int(round(volume_value))
        if not (-0x80000000 <= volume <= 0x7FFFFFFF):
            raise DownloadError(f"第 {record_number} 根 K 线成交量超出 int32")

        bars.append(
            local.MinuteBar(
                date=date_value,
                time=time_value,
                minute=minute_of_day,
                open=open_milli / 1000.0,
                high=high_milli / 1000.0,
                low=low_milli / 1000.0,
                close=close_milli / 1000.0,
                amount=amount_value,
                volume=volume,
                extra_1=up_count,
                extra_2=down_count,
            )
        )
    if offset != len(payload):
        raise DownloadError(f"K 线响应末尾有 {len(payload) - offset} 个未解析字节")
    return tuple(bars)


def download_pages(
    endpoints: Sequence[HostEndpoint],
    *,
    market_id: int,
    code: str,
    index_mode: bool,
    start: int,
    page_size: int,
    page_count: int,
    timeout: float,
    progress: Callable[[str], None] | None = None,
) -> DownloadResult:
    if not endpoints:
        raise DownloadError("没有可尝试的行情主站")
    if page_count <= 0:
        raise DownloadError("pages 必须是正整数")
    last_start = start + (page_count - 1) * page_size
    if last_start > 0xFFFF:
        raise DownloadError(
            f"最后一页 start={last_start} 超出协议上限 65535"
        )

    page_index = 0
    bars: list[local.MinuteBar] = []
    failures: list[str] = []
    used: list[HostEndpoint] = []
    reached_history_end = False

    for endpoint in endpoints:
        if page_index >= page_count or reached_history_end:
            break
        try:
            with QuoteConnection(endpoint, timeout) as connection:
                used.append(endpoint)
                if progress:
                    server = connection.server_name or endpoint.name or "未命名主站"
                    progress(f"已连接 {endpoint.address}（{server}）")
                while page_index < page_count:
                    current_start = start + page_index * page_size
                    page = connection.get_minute_page(
                        market_id,
                        code,
                        current_start,
                        page_size,
                        index_mode,
                    )
                    bars.extend(page)
                    page_index += 1
                    if progress:
                        if page:
                            progress(
                                f"第 {page_index}/{page_count} 页："
                                f"start={current_start}，{len(page)} 根，"
                                f"{page[0].date} {page[0].time}—"
                                f"{page[-1].date} {page[-1].time}"
                            )
                        else:
                            progress(
                                f"第 {page_index}/{page_count} 页："
                                f"start={current_start}，已到服务端历史末尾"
                            )
                    if len(page) < page_size:
                        reached_history_end = True
                        break
        except (DownloadError, OSError, TimeoutError, zlib.error) as error:
            failures.append(f"{endpoint.address}: {error}")
            if progress:
                progress(f"主站失败，准备切换：{endpoint.address}（{error}）")

    if page_index == 0:
        detail = "\n".join(f"  - {item}" for item in failures)
        raise DownloadError(f"所有行情主站均下载失败：\n{detail}")
    if page_index < page_count and not reached_history_end:
        detail = "\n".join(f"  - {item}" for item in failures)
        raise DownloadError(
            f"只完成 {page_index}/{page_count} 页，剩余主站均失败：\n{detail}"
        )
    return DownloadResult(
        bars=tuple(bars),
        pages=page_index,
        endpoints=tuple(used),
        reached_history_end=reached_history_end,
    )


def infer_market(root: Path, code: str, explicit: str | None) -> str:
    if explicit:
        return explicit
    configured = local.configured_market(root, code)
    if configured:
        return configured
    if code.startswith(("6", "9")):
        return "sh"
    if code.startswith(("8", "92")):
        return "bj"
    return "sz"


def infer_index_mode(root: Path, code: str, requested_kind: str) -> bool:
    if requested_kind != "auto":
        return requested_kind == "index"
    return code in local.load_board_names(root) or code.startswith("880")


def existing_cache_path(root: Path, code: str, market: str) -> Path:
    return (
        root
        / "vipdoc"
        / market
        / "minline"
        / f"{market}{code}.lc1"
    )


def load_existing_bars(paths: Iterable[Path]) -> tuple[local.MinuteBar, ...]:
    bars: list[local.MinuteBar] = []
    seen_paths: set[str] = set()
    for path in paths:
        try:
            resolved = path.expanduser().resolve()
        except OSError:
            resolved = path
        key = str(resolved).casefold()
        if key in seen_paths or not resolved.is_file():
            continue
        seen_paths.add(key)
        bars.extend(local.parse_lc1(local.read_stable(resolved)))
    return merge_bars((), bars)


def merge_bars(
    existing: Iterable[local.MinuteBar],
    downloaded: Iterable[local.MinuteBar],
) -> tuple[local.MinuteBar, ...]:
    by_key = {(bar.date, bar.minute): bar for bar in existing}
    by_key.update(
        ((bar.date, bar.minute), bar)
        for bar in downloaded
    )
    return tuple(by_key[key] for key in sorted(by_key))


def encode_lc1_date(value: int) -> int:
    year = value // 10000
    month = value // 100 % 100
    day = value % 100
    try:
        decoded = local.decode_date((year - 2004) * 2048 + month * 100 + day)
    except local.MinuteDataError as error:
        raise DownloadError(f"日期无法编码为 lc1：{value}") from error
    if decoded != value:
        raise DownloadError(f"日期无法编码为 lc1：{value}")
    encoded = (year - 2004) * 2048 + month * 100 + day
    if not (0 <= encoded <= 0xFFFF):
        raise DownloadError(f"日期超出 lc1 范围：{value}")
    return encoded


def pack_lc1(bars: Iterable[local.MinuteBar]) -> bytes:
    output = bytearray()
    for bar in bars:
        output.extend(
            local.RECORD.pack(
                encode_lc1_date(bar.date),
                bar.minute,
                bar.open,
                bar.high,
                bar.low,
                bar.close,
                bar.amount,
                bar.volume,
                bar.extra_1,
                bar.extra_2,
            )
        )
    return bytes(output)


def atomic_write_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=str(path.parent),
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(str(temporary), str(path))
    except BaseException:
        try:
            os.close(descriptor)
        except OSError:
            pass
        try:
            temporary.unlink()
        except OSError:
            pass
        raise


def default_render_output(code: str, output_format: str) -> Path | None:
    if output_format == "none":
        return None
    suffix = {"html": "html", "csv": "csv", "json": "json"}[output_format]
    return DEFAULT_OUTPUT_DIR / f"tdx-{code}-1m.{suffix}"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "通过通达信 7709/TCP 主站下载证券或板块指数的 1 分钟 K 线，"
            "增量生成 lc1、CSV、JSON 或单文件 HTML。"
        )
    )
    parser.add_argument(
        "--download",
        action="store_true",
        help="显式允许联网；不加时只显示执行计划",
    )
    parser.add_argument("--root", type=Path, help="通达信安装根目录")
    parser.add_argument("--code", required=True, help="6 位证券或板块代码")
    parser.add_argument(
        "--market",
        choices=("sh", "sz", "bj"),
        help="市场前缀；默认优先从通达信板块配置判断",
    )
    parser.add_argument(
        "--kind",
        choices=("auto", "index", "stock"),
        default="auto",
        help="响应记录类型；880xxx 默认自动识别为 index",
    )
    parser.add_argument(
        "--host",
        action="append",
        default=[],
        metavar="IP[:PORT]",
        help="优先尝试的主站，可重复；默认读取 T0002/newhost.lst",
    )
    parser.add_argument(
        "--max-hosts",
        type=int,
        default=6,
        help="最多尝试的主站数（默认 6）",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="每次连接和读取超时秒数（默认 5）",
    )
    parser.add_argument(
        "--pages",
        type=int,
        default=1,
        help="下载页数，每页最多 800 根（默认 1）",
    )
    parser.add_argument(
        "--page-size",
        type=int,
        default=800,
        help="每页根数，范围 1—800（默认 800）",
    )
    parser.add_argument(
        "--start",
        type=int,
        default=0,
        help="从服务端最新数据向历史偏移的起点（默认 0）",
    )
    parser.add_argument(
        "--no-merge-existing",
        action="store_true",
        help="不合并通达信原缓存和上次工具快照",
    )
    parser.add_argument(
        "--lc1-output",
        type=Path,
        help="lc1 快照路径；默认 output/minute/<市场><代码>.lc1",
    )
    parser.add_argument(
        "--format",
        choices=("html", "csv", "json", "none"),
        default="html",
        help="附加导出格式；none 表示只生成 lc1（默认 html）",
    )
    parser.add_argument(
        "--date",
        default="latest",
        help="导出日期 YYYYMMDD、latest 或 all；HTML 不支持 all",
    )
    parser.add_argument("--output", type=Path, help="HTML/CSV/JSON 输出路径")
    parser.add_argument(
        "--open",
        action="store_true",
        help="HTML 生成后用默认浏览器打开",
    )
    return parser


def validate_arguments(args: argparse.Namespace) -> None:
    if not (len(args.code) == 6 and args.code.isdigit()):
        raise DownloadError("--code 必须是 6 位数字")
    if args.pages <= 0:
        raise DownloadError("--pages 必须是正整数")
    if not (1 <= args.page_size <= MAX_PAGE_SIZE):
        raise DownloadError(f"--page-size 必须在 1—{MAX_PAGE_SIZE}")
    if args.start < 0:
        raise DownloadError("--start 不能为负数")
    if args.timeout <= 0:
        raise DownloadError("--timeout 必须大于 0")
    if args.max_hosts <= 0:
        raise DownloadError("--max-hosts 必须是正整数")
    if args.open and args.format != "html":
        raise DownloadError("--open 只能与 --format html 一起使用")
    local.parse_date_argument(
        args.date,
        "html" if args.format == "html" else "json",
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        args.code = args.code.strip()
        validate_arguments(args)
        root = local.find_tdx_root(args.root)
        market = infer_market(root, args.code, args.market)
        market_id = MARKET_IDS[market]
        index_mode = infer_index_mode(root, args.code, args.kind)
        configured_hosts = load_hq_hosts(root / "T0002" / "newhost.lst")
        preferred_hosts = tuple(parse_endpoint(value) for value in args.host)
        endpoints = unique_endpoints((*preferred_hosts, *configured_hosts))[
            : args.max_hosts
        ]
        if not endpoints:
            raise DownloadError("没有可尝试的行情主站")

        lc1_output = (
            args.lc1_output
            or DEFAULT_OUTPUT_DIR / "minute" / f"{market}{args.code}.lc1"
        ).expanduser().resolve()
        render_output = args.output or default_render_output(
            args.code,
            args.format,
        )
        if render_output is not None:
            render_output = render_output.expanduser().resolve()
        official_cache = existing_cache_path(root, args.code, market)
        kind_label = "指数/板块" if index_mode else "普通证券"

        print(
            f"目标：{market.upper()}{args.code}，{kind_label}，"
            f"1 分钟线，{args.pages} 页 × {args.page_size} 根"
        )
        print(f"主站配置：{root / 'T0002' / 'newhost.lst'}")
        for endpoint in endpoints:
            label = f"（{endpoint.name}）" if endpoint.name else ""
            print(f"  - {endpoint.address}{label}")
        print(f"lc1 快照：{lc1_output}")
        if render_output is not None:
            print(f"{args.format.upper()} 输出：{render_output}")

        if not args.download:
            print("当前是计划模式；确认后加 --download 才会联网和写入输出。")
            return 0

        result = download_pages(
            endpoints,
            market_id=market_id,
            code=args.code,
            index_mode=index_mode,
            start=args.start,
            page_size=args.page_size,
            page_count=args.pages,
            timeout=args.timeout,
            progress=print,
        )
        if not result.bars:
            raise DownloadError("服务端没有返回任何分钟记录")

        existing: tuple[local.MinuteBar, ...] = ()
        if not args.no_merge_existing:
            existing = load_existing_bars((official_cache, lc1_output))
        merged = merge_bars(existing, result.bars)
        encoded = pack_lc1(merged)
        normalized = local.parse_lc1(encoded)
        if (
            len(normalized) != len(merged)
            or [(bar.date, bar.minute) for bar in normalized]
            != [(bar.date, bar.minute) for bar in merged]
        ):
            raise DownloadError("写入前 lc1 往返校验失败")
        merged = normalized
        atomic_write_bytes(lc1_output, encoded)

        rendered_bars: tuple[local.MinuteBar, ...] = ()
        if args.format != "none":
            requested_date = local.parse_date_argument(
                args.date,
                args.format,
            )
            series = local.MinuteSeries(
                code=args.code,
                name=local.load_board_names(root).get(args.code, args.code),
                market=market,
                source=lc1_output,
                bars=merged,
            )
            rendered_bars = local.select_bars(merged, requested_date)
            if args.format == "html":
                content = local.render_html(series, rendered_bars)
            elif args.format == "csv":
                content = local.render_csv(rendered_bars)
            else:
                content = local.render_json(series, rendered_bars)
            if render_output is None:
                raise DownloadError("缺少附加导出路径")
            local.atomic_write(render_output, content)

        downloaded_dates = sorted({bar.date for bar in result.bars})
        print(
            f"完成：下载 {len(result.bars)} 根，"
            f"服务端日期 {downloaded_dates[0]}—{downloaded_dates[-1]}；"
            f"合并后 {len(merged)} 根。"
        )
        print(f"已原子写入 lc1 快照：{lc1_output}")
        if render_output is not None:
            print(
                f"已导出 {len(rendered_bars)} 根到：{render_output}"
            )
        if args.open and render_output is not None:
            webbrowser.open(render_output.as_uri())
        return 0
    except (
        DownloadError,
        local.MinuteDataError,
        OSError,
        ValueError,
    ) as error:
        print(f"错误：{error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

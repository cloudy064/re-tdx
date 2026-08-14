#!/usr/bin/env python3
"""Download and parse TDX tdxstat.cfg/tdxstat2.cfg statistics (0x06B9)."""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
import zlib
from collections import Counter
from dataclasses import asdict, dataclass
from datetime import datetime
from io import BytesIO
from pathlib import Path
from typing import Callable, Iterable, Sequence, TypeVar
from zipfile import BadZipFile, ZipFile

import download_tdx_minute as transport
import tdx_market_depth as depth
import update_tdx_blocks as updater


TYPE_FILE_CONTENT = 0x06B9
FILE_PATH_SIZE = 300
DEFAULT_CHUNK_SIZE = 30_000
MAX_CHUNK_SIZE = 60_000
MAX_ARCHIVE_BYTES = 32 * 1024 * 1024
MAX_ENTRY_BYTES = 32 * 1024 * 1024
MAX_UNCOMPRESSED_BYTES = 64 * 1024 * 1024
MAX_ARCHIVE_ENTRIES = 128
REQUIRED_MEMBERS = ("tdxstat.cfg", "tdxstat2.cfg")


class StatsError(RuntimeError):
    """Raised when a TDX statistics resource is invalid."""


@dataclass(frozen=True)
class TdxStatRow:
    market_id: int
    code: str
    stats_date: str | None
    beta_60d: float | None
    pe_ttm: float | None
    free_float_shares_10k: float | None
    year_limit_up_days: int | None
    limit_stat_days: int | None
    limit_up_count_in_stat_days: int | None
    limit_up_streak_days: int | None

    @property
    def key(self) -> tuple[int, str]:
        return self.market_id, self.code


@dataclass(frozen=True)
class TdxStat2Row:
    market_id: int
    code: str
    stats_date: str | None
    amount_10k: float | None
    seal_amount_10k: float | None
    prev_amount_10k: float | None
    prev_seal_amount_10k: float | None
    prev2_amount_10k: float | None
    prev2_seal_amount_10k: float | None
    open_volume_hand: float | None
    prev_open_volume_hand: float | None
    open_amount_10k: float | None
    prev_open_amount_10k: float | None

    @property
    def key(self) -> tuple[int, str]:
        return self.market_id, self.code


@dataclass(frozen=True)
class TdxStatsResource:
    stat: dict[tuple[int, str], TdxStatRow]
    stat2: dict[tuple[int, str], TdxStat2Row]
    source_path: str

    @property
    def stats_date(self) -> str | None:
        left, left_coverage = dominant_date_and_coverage(self.stat.values())
        right, right_coverage = dominant_date_and_coverage(self.stat2.values())
        if left == right and min(left_coverage, right_coverage) >= 0.95:
            return left
        return None

    @property
    def stats_date_coverage(self) -> float:
        left, left_coverage = dominant_date_and_coverage(self.stat.values())
        right, right_coverage = dominant_date_and_coverage(self.stat2.values())
        return min(left_coverage, right_coverage) if left == right else 0.0


@dataclass(frozen=True)
class StatsDownloadResult:
    archive: bytes
    resource: TdxStatsResource
    endpoint: transport.HostEndpoint
    server_name: str
    attempted_endpoints: tuple[transport.HostEndpoint, ...]
    failures: tuple[str, ...]


def build_file_request_data(
    path: str,
    *,
    offset: int = 0,
    size: int = DEFAULT_CHUNK_SIZE,
) -> bytes:
    normalized = path.strip().replace("\\", "/")
    if not normalized or "\x00" in normalized:
        raise StatsError("资源路径不能为空或包含 NUL")
    try:
        path_raw = normalized.encode("ascii")
    except UnicodeEncodeError as error:
        raise StatsError("资源路径必须是 ASCII") from error
    if len(path_raw) > FILE_PATH_SIZE:
        raise StatsError("资源路径超过 300 字节")
    if not 0 <= int(offset) <= 0xFFFFFFFF:
        raise StatsError("资源偏移超出 uint32")
    if not 1 <= int(size) <= MAX_CHUNK_SIZE:
        raise StatsError(f"资源分块必须为 1..{MAX_CHUNK_SIZE}")
    return (
        struct.pack("<II", int(offset), int(size))
        + path_raw.ljust(FILE_PATH_SIZE, b"\x00")
    )


def parse_file_chunk(payload: bytes, *, request_size: int) -> bytes:
    if len(payload) < 4:
        raise StatsError("0x06B9 响应少于 4 字节")
    chunk_length = int.from_bytes(payload[:4], "little")
    if chunk_length > request_size:
        raise StatsError(
            f"0x06B9 分块 {chunk_length} 超过请求大小 {request_size}"
        )
    expected = 4 + chunk_length
    if len(payload) != expected:
        raise StatsError(
            f"0x06B9 响应长度不符：声明 {chunk_length}，实际 {len(payload) - 4}"
        )
    return payload[4:]


def _text(value: str) -> str | None:
    result = value.strip()
    return result or None


def _float(value: str) -> float | None:
    text = value.strip()
    if not text:
        return None
    try:
        result = float(text)
    except ValueError:
        return None
    return result if math.isfinite(result) else None


def _int(value: str) -> int | None:
    result = _float(value)
    return None if result is None else int(result)


def _lines(payload: bytes) -> list[str]:
    return payload.decode("gbk", errors="ignore").splitlines()


def parse_stat_rows(payload: bytes) -> tuple[TdxStatRow, ...]:
    rows: list[TdxStatRow] = []
    for line in _lines(payload):
        parts = line.rstrip("\r\n").split("|")
        if len(parts) < 35:
            continue
        market_id = _int(parts[0])
        code = parts[1].strip()
        if market_id is None or market_id not in (0, 1, 2) or not code:
            continue
        rows.append(TdxStatRow(
            market_id=market_id,
            code=code.zfill(6),
            stats_date=_text(parts[4]),
            beta_60d=_float(parts[2]),
            pe_ttm=_float(parts[3]),
            free_float_shares_10k=_float(parts[11]),
            year_limit_up_days=_int(parts[26]),
            limit_stat_days=_int(parts[31]),
            limit_up_count_in_stat_days=_int(parts[32]),
            limit_up_streak_days=_int(parts[33]),
        ))
    return tuple(rows)


def parse_stat2_rows(payload: bytes) -> tuple[TdxStat2Row, ...]:
    rows: list[TdxStat2Row] = []
    for line in _lines(payload):
        parts = line.rstrip("\r\n").split("|")
        if len(parts) < 21:
            continue
        market_id = _int(parts[0])
        code = parts[1].strip()
        if market_id is None or market_id not in (0, 1, 2) or not code:
            continue
        rows.append(TdxStat2Row(
            market_id=market_id,
            code=code.zfill(6),
            stats_date=_text(parts[2]),
            amount_10k=_float(parts[3]),
            seal_amount_10k=_float(parts[4]),
            prev_amount_10k=_float(parts[5]),
            prev_seal_amount_10k=_float(parts[6]),
            prev2_amount_10k=_float(parts[7]),
            prev2_seal_amount_10k=_float(parts[8]),
            open_volume_hand=_float(parts[9]),
            prev_open_volume_hand=_float(parts[10]),
            open_amount_10k=_float(parts[14]),
            prev_open_amount_10k=_float(parts[15]),
        ))
    return tuple(rows)


Row = TypeVar("Row", TdxStatRow, TdxStat2Row)


def _unique_map(rows: Sequence[Row], *, member: str) -> dict[tuple[int, str], Row]:
    result: dict[tuple[int, str], Row] = {}
    duplicates: list[tuple[int, str]] = []
    for row in rows:
        if row.key in result:
            duplicates.append(row.key)
        result[row.key] = row
    if duplicates:
        sample = ", ".join(f"{market}:{code}" for market, code in duplicates[:5])
        raise StatsError(f"{member} 存在重复证券键：{sample}")
    return result


def parse_stats_files(
    stat_payload: bytes,
    stat2_payload: bytes,
    *,
    source_path: str,
) -> TdxStatsResource:
    stat_rows = parse_stat_rows(stat_payload)
    stat2_rows = parse_stat2_rows(stat2_payload)
    if not stat_rows or not stat2_rows:
        raise StatsError("tdxstat.cfg/tdxstat2.cfg 没有可用记录")
    return TdxStatsResource(
        stat=_unique_map(stat_rows, member="tdxstat.cfg"),
        stat2=_unique_map(stat2_rows, member="tdxstat2.cfg"),
        source_path=source_path,
    )


def parse_stats_archive(payload: bytes, *, source_path: str = "tdx://zhb.zip") -> TdxStatsResource:
    if not payload:
        raise StatsError("统计 ZIP 为空")
    if len(payload) > MAX_ARCHIVE_BYTES:
        raise StatsError(f"统计 ZIP 超过 {MAX_ARCHIVE_BYTES} 字节")
    try:
        with ZipFile(BytesIO(payload)) as archive:
            infos = archive.infolist()
            if len(infos) > MAX_ARCHIVE_ENTRIES:
                raise StatsError(f"统计 ZIP 条目过多：{len(infos)}")
            names = [info.filename for info in infos]
            missing = [name for name in REQUIRED_MEMBERS if name not in names]
            if missing:
                raise StatsError("统计 ZIP 缺少：" + ", ".join(missing))
            duplicates = [name for name in REQUIRED_MEMBERS if names.count(name) != 1]
            if duplicates:
                raise StatsError("统计 ZIP 重复包含：" + ", ".join(duplicates))
            total = 0
            for info in infos:
                if info.flag_bits & 0x1:
                    raise StatsError(f"统计 ZIP 含加密条目：{info.filename}")
                if info.file_size > MAX_ENTRY_BYTES:
                    raise StatsError(f"统计 ZIP 条目过大：{info.filename}")
                total += info.file_size
            if total > MAX_UNCOMPRESSED_BYTES:
                raise StatsError("统计 ZIP 解压后总大小过大")
            return parse_stats_files(
                archive.read(REQUIRED_MEMBERS[0]),
                archive.read(REQUIRED_MEMBERS[1]),
                source_path=source_path,
            )
    except StatsError:
        raise
    except (BadZipFile, OSError, RuntimeError, ValueError) as error:
        raise StatsError(f"统计资源不是有效 ZIP：{error}") from error


def load_local_stats(directory: Path) -> TdxStatsResource:
    resolved = directory.expanduser().resolve()
    return parse_stats_files(
        (resolved / REQUIRED_MEMBERS[0]).read_bytes(),
        (resolved / REQUIRED_MEMBERS[1]).read_bytes(),
        source_path=str(resolved),
    )


def dominant_date_and_coverage(rows: Iterable[object]) -> tuple[str | None, float]:
    materialized = list(rows)
    counts = Counter(
        str(value)
        for row in materialized
        if (value := getattr(row, "stats_date", None))
    )
    if not counts:
        return None, 0.0
    dominant = max(counts, key=lambda value: (counts[value], value))
    return dominant, counts[dominant] / max(1, len(materialized))


def download_stats(
    endpoints: Sequence[transport.HostEndpoint],
    *,
    path: str = "zhb.zip",
    chunk_size: int = DEFAULT_CHUNK_SIZE,
    timeout: float = 8.0,
    progress: Callable[[str], None] | None = None,
) -> StatsDownloadResult:
    if not 1 <= chunk_size <= MAX_CHUNK_SIZE:
        raise StatsError(f"chunk_size 必须为 1..{MAX_CHUNK_SIZE}")
    attempted: list[transport.HostEndpoint] = []
    failures: list[str] = []
    for endpoint in endpoints:
        attempted.append(endpoint)
        try:
            chunks: list[bytes] = []
            offset = 0
            with transport.QuoteConnection(endpoint, timeout) as connection:
                while True:
                    size = min(chunk_size, MAX_ARCHIVE_BYTES + 1 - offset)
                    if size <= 0:
                        raise StatsError(f"统计 ZIP 超过 {MAX_ARCHIVE_BYTES} 字节")
                    response = connection.call(
                        TYPE_FILE_CONTENT,
                        build_file_request_data(path, offset=offset, size=size),
                    )
                    chunk = parse_file_chunk(response.data, request_size=size)
                    chunks.append(chunk)
                    offset += len(chunk)
                    if progress:
                        progress(f"0x06B9 {path}：已收到 {offset:,} 字节")
                    if len(chunk) < size:
                        break
                archive = b"".join(chunks)
                resource = parse_stats_archive(
                    archive,
                    source_path=f"tdx://{endpoint.address}/{path}",
                )
                return StatsDownloadResult(
                    archive=archive,
                    resource=resource,
                    endpoint=endpoint,
                    server_name=connection.server_name,
                    attempted_endpoints=tuple(attempted),
                    failures=tuple(failures),
                )
        except (
            StatsError,
            transport.DownloadError,
            OSError,
            TimeoutError,
            zlib.error,
            struct.error,
        ) as error:
            failures.append(f"{endpoint.address}: {error}")
            if progress:
                progress(f"统计资源主站失败：{endpoint.address}（{error}）")
    detail = "\n".join(f"  - {item}" for item in failures)
    raise StatsError(f"所有统计资源主站均失败：\n{detail}")


def resource_to_model(
    resource: TdxStatsResource,
    *,
    endpoint: str = "",
    server_name: str = "",
) -> dict[str, object]:
    keys = sorted(set(resource.stat) | set(resource.stat2))
    records: list[dict[str, object]] = []
    for market_id, code in keys:
        stat = resource.stat.get((market_id, code))
        stat2 = resource.stat2.get((market_id, code))
        records.append({
            "market_id": market_id,
            "code": code,
            "stat": asdict(stat) if stat else None,
            "stat2": asdict(stat2) if stat2 else None,
        })
    return {
        "schema": "tdx-stats-v1",
        "generated_at": datetime.now().astimezone().isoformat(),
        "command": "0x06B9" if endpoint else None,
        "source_path": resource.source_path,
        "endpoint": endpoint,
        "server_name": server_name,
        "stats_date": resource.stats_date,
        "stats_date_coverage": resource.stats_date_coverage,
        "stat_count": len(resource.stat),
        "stat2_count": len(resource.stat2),
        "records": records,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="通过 0x06B9 更新并解析 zhb.zip 中的通达信统计表。"
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--download", action="store_true", help="从行情主站更新 zhb.zip")
    mode.add_argument("--local", action="store_true", help="解析本地 hq_cache 统计文件")
    parser.add_argument("--stats-dir", type=Path, help="本地统计文件目录")
    parser.add_argument("--host", action="append", default=[])
    parser.add_argument("--max-hosts", type=int, default=5)
    parser.add_argument("--chunk-size", type=int, default=DEFAULT_CHUNK_SIZE)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument(
        "--output",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-stats.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        output = args.output.expanduser().resolve()
        if not args.download and not args.local:
            print(json.dumps({
                "action": "dry-run",
                "command": "0x06B9",
                "remote_path": "zhb.zip",
                "local_dir": str(args.stats_dir or root / "T0002" / "hq_cache"),
                "output": str(output),
            }, ensure_ascii=False, indent=2))
            return 0
        endpoint = ""
        server_name = ""
        if args.download:
            endpoints = depth.endpoint_candidates(root, args.host, args.max_hosts)
            result = download_stats(
                endpoints,
                chunk_size=args.chunk_size,
                timeout=args.timeout,
                progress=print if args.verbose else None,
            )
            resource = result.resource
            endpoint = result.endpoint.address
            server_name = result.server_name
        else:
            resource = load_local_stats(
                args.stats_dir or root / "T0002" / "hq_cache"
            )
        model = resource_to_model(
            resource,
            endpoint=endpoint,
            server_name=server_name,
        )
        rendered = json.dumps(
            model,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        updater.atomic_write_text(output, rendered + "\n", "utf-8")
        print(
            f"已解析 tdxstat={len(resource.stat):,}、"
            f"tdxstat2={len(resource.stat2):,}，"
            f"统计日 {resource.stats_date}：{output}"
        )
    except (
        OSError,
        UnicodeError,
        ValueError,
        updater.UpdateError,
        transport.DownloadError,
        StatsError,
    ) as error:
        print(f"统计资源更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

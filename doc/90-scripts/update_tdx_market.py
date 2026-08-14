#!/usr/bin/env python3
"""Build a one-file TDX block radar with a current 7709 market snapshot."""

from __future__ import annotations

import argparse
import math
import statistics
import sys
import webbrowser
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Mapping, Sequence

import build_tdx_blocks_html as html_builder
import download_tdx_minute as transport
import extract_tdx_blocks as extractor
import extract_tdx_minute as minute_local
import tdx_market_snapshot as snapshot
import update_tdx_blocks as block_updater


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = PROJECT_ROOT / "output" / "tdx-market.html"


@dataclass(frozen=True)
class MarketUpdateResult:
    root: Path
    output: Path
    blocks: int
    memberships: int
    securities: int
    requested_quotes: int
    received_quotes: int
    quoted_securities: int
    quoted_blocks: int
    endpoint: transport.HostEndpoint
    server_name: str
    html_size: int
    packed_size: int


def load_board_markets(root: Path) -> dict[str, int]:
    """Load the market assigned to every board/index code by TDX."""
    path = root / "T0002" / "hq_cache" / "tdxzsbase.cfg"
    if not path.is_file():
        return {}
    markets: dict[str, int] = {}
    for line in minute_local.read_text_guess(path).splitlines():
        fields = line.strip().split("|", 2)
        if (
            len(fields) >= 2
            and fields[0] in {"0", "1", "2"}
            and len(fields[1]) == 6
            and fields[1].isdigit()
        ):
            markets.setdefault(fields[1], int(fields[0]))
    return markets


def collect_quote_codes(
    document: Mapping[str, object],
    board_markets: Mapping[str, int],
) -> tuple[snapshot.QuoteCode, ...]:
    result: list[snapshot.QuoteCode] = []
    for security in document["securities"]:  # type: ignore[index]
        market_id, code = int(security[0]), str(security[1])
        if market_id in (0, 1, 2) and len(code) == 6 and code.isdigit():
            result.append(snapshot.QuoteCode(market_id, code))
    for block in document["blocks"]:  # type: ignore[index]
        code = str(block[2])
        market_id = board_markets.get(code)
        if market_id is not None:
            result.append(snapshot.QuoteCode(market_id, code))
    return tuple(dict.fromkeys(result))


def compact_quote(quote: snapshot.QuoteSnapshot) -> list[float | int]:
    change = quote.change_pct
    return [
        round(quote.last_price, 4),
        round(quote.pre_close_price, 4),
        round(quote.open_price, 4),
        round(quote.high_price, 4),
        round(quote.low_price, 4),
        quote.total_hand,
        round(quote.amount, 2),
        quote.inside_dish,
        quote.outer_disc,
        round(quote.open_amount_yuan, 2),
        round(change, 4) if change is not None else 0.0,
    ]


def usable_quote(quote: snapshot.QuoteSnapshot | None) -> bool:
    return bool(
        quote
        and quote.last_price > 0
        and quote.pre_close_price > 0
        and quote.change_pct is not None
    )


def compute_block_metric(
    packed_members: Sequence[int],
    security_quotes: Sequence[list[float | int] | None],
) -> list[float | int]:
    changes: list[tuple[float, int]] = []
    amount = 0.0
    up = down = flat = 0
    for packed in packed_members:
        security_index = packed >> 1
        quote = security_quotes[security_index]
        if quote is None:
            continue
        change = float(quote[10])
        changes.append((change, security_index))
        amount += float(quote[6])
        if change > 1e-9:
            up += 1
        elif change < -1e-9:
            down += 1
        else:
            flat += 1
    if not changes:
        return [0, 0, 0, 0, 0.0, -1, 0.0, -1, 0.0, 0.0]
    leader_change, leader_index = max(changes)
    laggard_change, laggard_index = min(changes)
    return [
        len(changes),
        up,
        down,
        flat,
        round(statistics.fmean(change for change, _ in changes), 4),
        leader_index,
        round(leader_change, 4),
        laggard_index,
        round(laggard_change, 4),
        round(amount, 2),
    ]


def attach_market_data(
    document: dict[str, object],
    quotes: Sequence[snapshot.QuoteSnapshot],
    board_markets: Mapping[str, int],
    *,
    captured_at: str,
    endpoint: str,
    server_name: str,
    requested: int,
) -> dict[str, object]:
    quote_lookup = {quote.key: quote for quote in quotes}
    securities = document["securities"]
    blocks = document["blocks"]
    members = document["members"]
    if not isinstance(securities, list) or not isinstance(blocks, list):
        raise block_updater.UpdateError("压缩图谱的数据结构无效")
    if not isinstance(members, list):
        raise block_updater.UpdateError("压缩图谱缺少板块成员")

    security_quotes: list[list[float | int] | None] = []
    for security in securities:
        quote = quote_lookup.get((int(security[0]), str(security[1])))
        security_quotes.append(
            compact_quote(quote) if usable_quote(quote) else None
        )

    block_quotes: list[list[float | int] | None] = []
    for block in blocks:
        code = str(block[2])
        market_id = board_markets.get(code)
        quote = quote_lookup.get((market_id, code)) if market_id is not None else None
        block_quotes.append(compact_quote(quote) if usable_quote(quote) else None)

    block_metrics = [
        compute_block_metric(packed_members, security_quotes)
        for packed_members in members
    ]
    quoted_security_rows = [quote for quote in security_quotes if quote]
    up = sum(float(quote[10]) > 1e-9 for quote in quoted_security_rows)
    down = sum(float(quote[10]) < -1e-9 for quote in quoted_security_rows)
    flat = len(quoted_security_rows) - up - down
    document["market"] = {
        "captured_at": captured_at,
        "endpoint": endpoint,
        "server_name": server_name,
        "requested": requested,
        "received": len(quotes),
        "security_quotes": security_quotes,
        "block_quotes": block_quotes,
        "block_metrics": block_metrics,
        "stats": {
            "quoted_securities": len(quoted_security_rows),
            "quoted_blocks": sum(quote is not None for quote in block_quotes),
            "up": up,
            "down": down,
            "flat": flat,
        },
    }
    return document


def update(
    root: Path,
    output: Path,
    families: set[str],
    endpoints: Sequence[transport.HostEndpoint],
    *,
    retries: int = 3,
    batch_size: int = 80,
    timeout: float = 5.0,
) -> MarketUpdateResult:
    _, blocks, members, _ = block_updater.extract_stable_snapshot(
        root,
        families,
        retries,
    )
    document = html_builder.compact_payload(blocks, members)
    board_markets = load_board_markets(root)
    codes = collect_quote_codes(document, board_markets)
    downloaded = snapshot.download_snapshots(
        endpoints,
        codes,
        batch_size=batch_size,
        timeout=timeout,
        progress=lambda message: print(message, file=sys.stderr),
    )
    captured_at = datetime.now().astimezone().isoformat(timespec="seconds")
    attach_market_data(
        document,
        downloaded.quotes,
        board_markets,
        captured_at=captured_at,
        endpoint=downloaded.endpoint.address,
        server_name=downloaded.server_name,
        requested=downloaded.requested,
    )
    html, _, packed_size = html_builder.render_html(document)
    resolved_output = output.expanduser().resolve()
    block_updater.atomic_write_text(resolved_output, html, "utf-8")
    market = document["market"]
    assert isinstance(market, dict)
    stats = market["stats"]
    assert isinstance(stats, dict)
    return MarketUpdateResult(
        root=root,
        output=resolved_output,
        blocks=len(blocks),
        memberships=len(members),
        securities=len(document["securities"]),  # type: ignore[arg-type]
        requested_quotes=downloaded.requested,
        received_quotes=len(downloaded.quotes),
        quoted_securities=int(stats["quoted_securities"]),
        quoted_blocks=int(stats["quoted_blocks"]),
        endpoint=downloaded.endpoint,
        server_name=downloaded.server_name,
        html_size=resolved_output.stat().st_size,
        packed_size=packed_size,
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "读取通达信本地板块树并通过 7709/TCP 批量获取最新行情，"
            "生成可双击打开的单文件板块雷达。"
        )
    )
    parser.add_argument(
        "--download",
        action="store_true",
        help="显式允许联网；不加时只显示执行计划",
    )
    parser.add_argument("--root", type=Path, help="通达信安装根目录；默认自动寻找")
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"HTML 输出路径（默认：{DEFAULT_OUTPUT}）",
    )
    parser.add_argument(
        "--family",
        action="append",
        choices=extractor.FAMILY_CHOICES,
        help="只包含指定类别；可重复，默认全部",
    )
    parser.add_argument(
        "--host",
        action="append",
        default=[],
        metavar="IP[:PORT]",
        help="优先主站，可重复指定",
    )
    parser.add_argument(
        "--max-hosts",
        type=int,
        default=6,
        help="最多尝试的主站数（默认 6）",
    )
    parser.add_argument(
        "--batch-size",
        type=int,
        default=80,
        help="每个 0x054C 请求包含的代码数（默认 80）",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="连接和读取超时秒数（默认 5）",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=3,
        help="本地源文件变动时重试次数（默认 3）",
    )
    parser.add_argument(
        "--open",
        action="store_true",
        dest="open_after",
        help="生成后用默认浏览器打开",
    )
    return parser


def validate_arguments(args: argparse.Namespace) -> None:
    if args.max_hosts <= 0:
        raise block_updater.UpdateError("--max-hosts 必须是正整数")
    if not (1 <= args.batch_size <= 500):
        raise block_updater.UpdateError("--batch-size 必须在 1—500")
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        raise block_updater.UpdateError("--timeout 必须大于 0")
    if args.retries <= 0:
        raise block_updater.UpdateError("--retries 必须是正整数")


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        validate_arguments(args)
        root = block_updater.find_tdx_root(args.root)
        families = set(args.family or extractor.FAMILY_CHOICES)
        _, blocks, members, _ = block_updater.extract_stable_snapshot(
            root,
            families,
            args.retries,
        )
        plan_document = html_builder.compact_payload(blocks, members)
        board_markets = load_board_markets(root)
        codes = collect_quote_codes(plan_document, board_markets)
        stock_count = len(plan_document["securities"])
        block_count = len(codes) - stock_count
        output = args.output.expanduser().resolve()

        configured = transport.load_hq_hosts(root / "T0002" / "newhost.lst")
        preferred = tuple(transport.parse_endpoint(value) for value in args.host)
        endpoints = transport.unique_endpoints((*preferred, *configured))[
            : args.max_hosts
        ]
        print(f"通达信目录：{root}")
        print(
            f"图谱：{len(blocks)} 个板块，{len(members)} 条关系，"
            f"{stock_count} 只唯一证券"
        )
        print(
            f"行情：{len(codes)} 个唯一代码"
            f"（证券 {stock_count}，可定位板块/指数 {block_count}），"
            f"约 {math.ceil(len(codes) / args.batch_size)} 个批次"
        )
        print(f"输出：{output}")
        for endpoint in endpoints:
            suffix = f"（{endpoint.name}）" if endpoint.name else ""
            print(f"  - {endpoint.address}{suffix}")
        if not args.download:
            print("当前是计划模式；加 --download 后才会联网并写入 HTML。")
            return 0

        result = update(
            root,
            output,
            families,
            endpoints,
            retries=args.retries,
            batch_size=args.batch_size,
            timeout=args.timeout,
        )
    except (
        OSError,
        UnicodeError,
        extractor.BlockFormatError,
        block_updater.UpdateError,
        transport.DownloadError,
        snapshot.SnapshotError,
    ) as error:
        print(f"生成失败：{error}", file=sys.stderr)
        return 1

    print(
        f"生成完成：收到 {result.received_quotes}/{result.requested_quotes} 个快照，"
        f"覆盖 {result.quoted_securities} 只证券、{result.quoted_blocks} 个板块",
        file=sys.stderr,
    )
    server = result.server_name or result.endpoint.name or "未命名主站"
    print(
        f"行情主站：{result.endpoint.address}（{server}）",
        file=sys.stderr,
    )
    print(
        f"HTML：{result.output}（{html_builder.human_size(result.html_size)}；"
        f"内嵌 gzip {html_builder.human_size(result.packed_size)}）",
        file=sys.stderr,
    )
    if args.open_after:
        webbrowser.open(result.output.as_uri())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

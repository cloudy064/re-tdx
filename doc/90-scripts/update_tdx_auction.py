#!/usr/bin/env python3
"""Download and merge the seven TDX call-auction signal screens."""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Mapping, Sequence

import tdx_pbrpc as pbrpc
import update_tdx_blocks as updater


class AuctionUpdateError(RuntimeError):
    """Raised when an auction signal response cannot be normalized."""


@dataclass(frozen=True)
class AuctionSignal:
    key: str
    name: str
    request_id: str
    description: str


SIGNALS = (
    AuctionSignal(
        "auction-volume", "竞价爆量", "200404",
        "集合竞价成交额相对昨日成交额显著放大",
    ),
    AuctionSignal(
        "bad-board-strength", "烂板转强", "200400",
        "昨日多次开板但竞价重新走强",
    ),
    AuctionSignal(
        "broken-board-strength", "炸板转强", "200401",
        "昨日炸板股票在集合竞价阶段重新走强",
    ),
    AuctionSignal(
        "auction-bottom", "竞价止跌", "200402",
        "昨日大跌后在集合竞价阶段明显修复",
    ),
    AuctionSignal(
        "swallow-upper-shadow", "预吞上影", "200403",
        "集合竞价涨幅覆盖昨日上影线",
    ),
    AuctionSignal(
        "limit-up-high-open", "涨停高开", "200405",
        "涨停相关股票次日竞价强弱和成交占比",
    ),
    AuctionSignal(
        "five-minute-surge", "5分钟陡增", "200406",
        "9:20 后五分钟竞价量显著增加",
    ),
)
SIGNAL_BY_KEY = {item.key: item for item in SIGNALS}
Query = Callable[[AuctionSignal], object]


def response_rows(value: object) -> list[dict[str, object]]:
    if not isinstance(value, dict):
        raise AuctionUpdateError("PBRPC result is not an object")
    error_code = value.get("ErrorCode", 0)
    if error_code not in (0, "0", None):
        raise AuctionUpdateError(
            f"server returned {error_code}: {value.get('ErrorInfo', '')}"
        )
    result_sets = value.get("ResultSets", [])
    if not isinstance(result_sets, list):
        raise AuctionUpdateError("ResultSets is not a list")
    records: list[dict[str, object]] = []
    for result_set in result_sets:
        if not isinstance(result_set, dict):
            continue
        columns = result_set.get("ColDes", [])
        rows = result_set.get("Content", [])
        if not isinstance(columns, list) or not isinstance(rows, list):
            raise AuctionUpdateError("invalid result-set columns or rows")
        names = [
            str(item.get("Name", "")) if isinstance(item, dict) else str(item)
            for item in columns
        ]
        if not names or any(not name for name in names):
            raise AuctionUpdateError("result set has an unnamed column")
        for row in rows:
            if not isinstance(row, list) or len(row) != len(names):
                raise AuctionUpdateError("result row width does not match columns")
            records.append(dict(zip(names, row)))
    return records


def build_model(
    responses: Mapping[str, object],
    *,
    market_filter: str,
) -> dict[str, object]:
    signal_results: list[dict[str, object]] = []
    securities: dict[tuple[str, str], list[dict[str, object]]] = {}
    total_records = 0
    for signal in SIGNALS:
        if signal.key not in responses:
            continue
        records = response_rows(responses[signal.key])
        total_records += len(records)
        signal_results.append({
            "key": signal.key,
            "name": signal.name,
            "request_id": signal.request_id,
            "description": signal.description,
            "records": records,
        })
        for record in records:
            market = str(record.get("market", "")).strip()
            code = str(record.get("code", "")).strip()
            if not market or not code:
                continue
            metrics = {
                name: value
                for name, value in record.items()
                if name not in ("market", "code")
            }
            securities.setdefault((market, code), []).append({
                "key": signal.key,
                "name": signal.name,
                "request_id": signal.request_id,
                "metrics": metrics,
            })
    security_rows = [
        {"market": market, "code": code, "signals": matches}
        for (market, code), matches in sorted(securities.items())
    ]
    return {
        "schema": "tdx-auction-signals-v1",
        "market_filter": market_filter,
        "counts": {
            "signals": len(signal_results),
            "records": total_records,
            "securities": len(security_rows),
            "multi_signal_securities": sum(
                len(item["signals"]) > 1 for item in security_rows
            ),
        },
        "signals": signal_results,
        "securities": security_rows,
    }


def fetch_signals(
    root: Path,
    selected: Sequence[AuctionSignal],
    *,
    market: str,
    base_url: str,
    timeout: float,
    max_rounds: int,
    retry_delay: float,
    verbose: bool,
) -> dict[str, object]:
    responses: dict[str, object] = {}
    for signal in selected:
        spec = pbrpc.find_config_spec(
            root,
            signal.request_id,
            source_file="sc_jjcl.xml",
        )
        request = dict(spec.request)
        request.update({"ReqId": signal.request_id, "market": market})
        raw = pbrpc.query_pbrpc(
            spec.entry,
            spec.module,
            request,
            base_url=base_url,
            timeout=timeout,
            max_rounds=max_rounds,
            retry_delay=retry_delay,
            verbose=verbose,
        )
        responses[signal.key] = pbrpc.decode_result(raw)
    return responses


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "下载并合并通达信竞价爆量、烂板/炸板转强、竞价止跌、"
            "预吞上影、涨停高开和 5 分钟陡增七类信号。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument(
        "--signal", action="append", choices=tuple(SIGNAL_BY_KEY), default=[],
        help="只更新指定信号；可重复，默认全部",
    )
    parser.add_argument("--market", default="0", help="服务市场过滤值，默认 0")
    parser.add_argument(
        "--download", action="store_true", help="明确允许联网；省略时只显示计划",
    )
    parser.add_argument("--base-url", default=pbrpc.DEFAULT_BASE_URL)
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--max-rounds", type=int, default=32)
    parser.add_argument("--retry-delay", type=float, default=0.15)
    parser.add_argument(
        "--output", type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-auction-signals.json",
    )
    parser.add_argument("--compact", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        keys = args.signal or [item.key for item in SIGNALS]
        selected = [SIGNAL_BY_KEY[key] for key in dict.fromkeys(keys)]
        output = args.output.expanduser().resolve()
        if not args.download:
            print(json.dumps({
                "action": "dry-run",
                "market": str(args.market),
                "output": str(output),
                "signals": [
                    {"key": item.key, "name": item.name, "request_id": item.request_id}
                    for item in selected
                ],
            }, ensure_ascii=False, indent=2))
            return 0
        responses = fetch_signals(
            root,
            selected,
            market=str(args.market),
            base_url=args.base_url,
            timeout=args.timeout,
            max_rounds=args.max_rounds,
            retry_delay=args.retry_delay,
            verbose=args.verbose,
        )
        model = build_model(responses, market_filter=str(args.market))
        rendered = json.dumps(
            model,
            ensure_ascii=False,
            indent=None if args.compact else 2,
            separators=(",", ":") if args.compact else None,
        )
        updater.atomic_write_text(output, rendered + "\n", "utf-8")
        counts = model["counts"]
        print(
            f"已更新 {counts['signals']} 类竞价信号、{counts['records']} 条记录、"
            f"{counts['securities']} 只证券：{output}"
        )
    except (
        OSError,
        UnicodeError,
        ValueError,
        json.JSONDecodeError,
        urllib.error.URLError,
        updater.UpdateError,
        pbrpc.PBRPCError,
        AuctionUpdateError,
    ) as error:
        print(f"竞价信号更新失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

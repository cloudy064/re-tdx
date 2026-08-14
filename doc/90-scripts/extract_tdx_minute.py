#!/usr/bin/env python3
"""Read TDX 1-minute cache files and export CSV, JSON, or an offline chart."""

from __future__ import annotations

import argparse
import csv
import html
import io
import json
import math
import os
import struct
import sys
import tempfile
from dataclasses import asdict, dataclass
from datetime import date, datetime
from pathlib import Path
from typing import Sequence


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_HTML_DIR = PROJECT_ROOT / "output"
RECORD = struct.Struct("<HHfffffiHH")
MARKET_PREFIXES = {"0": "sz", "1": "sh", "2": "bj"}


class MinuteDataError(RuntimeError):
    """Raised when a minute cache cannot be located or parsed."""


@dataclass(frozen=True)
class MinuteBar:
    date: int
    time: str
    minute: int
    open: float
    high: float
    low: float
    close: float
    amount: float
    volume: int
    extra_1: int
    extra_2: int


@dataclass(frozen=True)
class MinuteSeries:
    code: str
    name: str
    market: str
    source: Path
    bars: tuple[MinuteBar, ...]


def decode_date(value: int) -> int:
    year = value // 2048 + 2004
    remainder = value % 2048
    month = remainder // 100
    day = remainder % 100
    try:
        parsed = date(year, month, day)
    except ValueError as error:
        raise MinuteDataError(
            f"无效的 lc1 日期编码 0x{value:04X}："
            f"{year:04d}-{month:02d}-{day:02d}"
        ) from error
    return parsed.year * 10000 + parsed.month * 100 + parsed.day


def decode_time(value: int) -> str:
    hour, minute = divmod(value, 60)
    if not (0 <= hour <= 23 and 0 <= minute <= 59):
        raise MinuteDataError(f"无效的分钟编码：{value}")
    return f"{hour:02d}:{minute:02d}"


def parse_lc1(data: bytes) -> tuple[MinuteBar, ...]:
    if len(data) % RECORD.size:
        raise MinuteDataError(
            f"lc1 文件长度 {len(data)} 不是 {RECORD.size} 字节记录的整数倍"
        )

    bars: list[MinuteBar] = []
    for offset in range(0, len(data), RECORD.size):
        (
            encoded_date,
            minute,
            open_price,
            high_price,
            low_price,
            close_price,
            amount,
            volume,
            extra_1,
            extra_2,
        ) = RECORD.unpack_from(data, offset)
        prices = (open_price, high_price, low_price, close_price, amount)
        if not all(math.isfinite(value) for value in prices):
            record_number = offset // RECORD.size + 1
            raise MinuteDataError(
                f"第 {record_number} 条记录包含非有限浮点数"
            )
        if (
            high_price + 1e-5 < max(open_price, low_price, close_price)
            or low_price - 1e-5 > min(open_price, high_price, close_price)
        ):
            record_number = offset // RECORD.size + 1
            raise MinuteDataError(
                f"第 {record_number} 条记录的 OHLC 范围无效"
            )
        bars.append(
            MinuteBar(
                date=decode_date(encoded_date),
                time=decode_time(minute),
                minute=minute,
                open=open_price,
                high=high_price,
                low=low_price,
                close=close_price,
                amount=amount,
                volume=volume,
                extra_1=extra_1,
                extra_2=extra_2,
            )
        )
    return tuple(bars)


def read_stable(path: Path, retries: int = 3) -> bytes:
    for _ in range(retries):
        before = path.stat()
        data = path.read_bytes()
        after = path.stat()
        if (
            before.st_size == after.st_size
            and before.st_mtime_ns == after.st_mtime_ns
            and len(data) == after.st_size
        ):
            return data
    raise MinuteDataError(f"文件读取期间持续变化，请稍后重试：{path}")


def find_tdx_root(explicit: Path | None) -> Path:
    if explicit is not None:
        resolved = explicit.expanduser().resolve()
        if (
            (resolved / "vipdoc").is_dir()
            and (resolved / "T0002" / "hq_cache").is_dir()
        ):
            return resolved
        raise MinuteDataError(f"指定目录不是通达信安装根目录：{resolved}")

    candidates: list[Path] = []
    for variable in ("TDX_ROOT", "TDX_HOME"):
        value = os.environ.get(variable)
        if value:
            candidates.append(Path(value))
    current = Path.cwd().resolve()
    candidates.extend((current, current / "new_tdx"))
    candidates.extend(parent / "new_tdx" for parent in current.parents)
    if os.name == "nt":
        candidates.extend(Path(f"{letter}:\\new_tdx") for letter in "CDEFG")

    seen: set[str] = set()
    for candidate in candidates:
        try:
            resolved = candidate.expanduser().resolve()
        except OSError:
            continue
        key = str(resolved).casefold()
        if key in seen:
            continue
        seen.add(key)
        if (
            (resolved / "vipdoc").is_dir()
            and (resolved / "T0002" / "hq_cache").is_dir()
        ):
            return resolved
    if explicit is not None:
        raise MinuteDataError(f"指定目录不是通达信安装根目录：{explicit}")
    raise MinuteDataError(
        "未自动找到通达信安装目录；请使用 --root C:\\new_tdx 指定。"
    )


def read_text_guess(path: Path) -> str:
    data = path.read_bytes()
    for encoding in ("gb18030", "utf-8-sig", "utf-16"):
        try:
            return data.decode(encoding)
        except UnicodeError:
            continue
    return data.decode("gb18030", errors="replace")


def load_board_names(root: Path) -> dict[str, str]:
    path = root / "T0002" / "hq_cache" / "tdxzs3.cfg"
    if not path.is_file():
        return {}
    names: dict[str, str] = {}
    for line in read_text_guess(path).splitlines():
        fields = line.strip().split("|")
        if len(fields) >= 2 and fields[1].isdigit():
            names[fields[1]] = fields[0].strip()
    return names


def configured_market(root: Path, code: str) -> str | None:
    path = root / "T0002" / "hq_cache" / "tdxzsbase.cfg"
    if not path.is_file():
        return None
    for line in read_text_guess(path).splitlines():
        fields = line.strip().split("|", 2)
        if len(fields) >= 2 and fields[1] == code:
            return MARKET_PREFIXES.get(fields[0])
    return None


def minute_candidates(
    root: Path,
    code: str,
    market: str | None,
) -> list[tuple[str, Path]]:
    prefixes = [market] if market else []
    if market is None:
        configured = configured_market(root, code)
        if configured:
            prefixes.append(configured)
        for prefix in ("sh", "sz", "bj"):
            if prefix not in prefixes:
                prefixes.append(prefix)

    candidates = [
        (
            prefix,
            root / "vipdoc" / prefix / "minline" / f"{prefix}{code}.lc1",
        )
        for prefix in prefixes
    ]
    for market_id, prefix in MARKET_PREFIXES.items():
        if market is not None and prefix != market:
            continue
        candidates.append(
            (
                prefix,
                root / "vipdoc" / "ds" / "minline" / f"{market_id}#{code}.lc1",
            )
        )
    return candidates


def load_series(
    root: Path,
    code: str,
    market: str | None = None,
) -> MinuteSeries:
    if not (code.isdigit() and 5 <= len(code) <= 8):
        raise MinuteDataError(f"证券或板块代码格式无效：{code!r}")
    for prefix, path in minute_candidates(root, code, market):
        if path.is_file():
            bars = parse_lc1(read_stable(path))
            if not bars:
                raise MinuteDataError(f"分钟线文件为空：{path}")
            return MinuteSeries(
                code=code,
                name=load_board_names(root).get(code, code),
                market=prefix,
                source=path,
                bars=bars,
            )
    expected = minute_candidates(root, code, market)[0][1]
    raise MinuteDataError(
        f"未找到 {code} 的 1 分钟缓存，预期位置类似：{expected}\n"
        "请先让通达信下载/加载该代码的 1 分钟数据，再重试。"
    )


def select_bars(
    bars: tuple[MinuteBar, ...],
    requested_date: str,
) -> tuple[MinuteBar, ...]:
    available = sorted({bar.date for bar in bars})
    if not available:
        raise MinuteDataError("分钟线文件没有记录")
    if requested_date == "all":
        return bars
    target = available[-1] if requested_date == "latest" else int(requested_date)
    selected = tuple(bar for bar in bars if bar.date == target)
    if not selected:
        first, last = available[0], available[-1]
        raise MinuteDataError(
            f"没有 {target} 的分钟线；当前文件日期范围为 {first}—{last}"
        )
    return selected


def series_metadata(series: MinuteSeries, bars: tuple[MinuteBar, ...]) -> dict:
    all_dates = sorted({bar.date for bar in series.bars})
    selected_dates = sorted({bar.date for bar in bars})
    return {
        "code": series.code,
        "name": series.name,
        "market": series.market,
        "source": str(series.source),
        "source_modified": datetime.fromtimestamp(
            series.source.stat().st_mtime
        ).astimezone().isoformat(timespec="seconds"),
        "record_size": RECORD.size,
        "total_records": len(series.bars),
        "available_first_date": all_dates[0],
        "available_last_date": all_dates[-1],
        "selected_first_date": selected_dates[0],
        "selected_last_date": selected_dates[-1],
        "selected_records": len(bars),
    }


def render_csv(bars: tuple[MinuteBar, ...]) -> str:
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=list(asdict(bars[0]).keys()))
    writer.writeheader()
    writer.writerows(asdict(bar) for bar in bars)
    return stream.getvalue()


def render_json(series: MinuteSeries, bars: tuple[MinuteBar, ...]) -> str:
    payload = {
        "metadata": series_metadata(series, bars),
        "bars": [asdict(bar) for bar in bars],
    }
    return json.dumps(payload, ensure_ascii=False, indent=2)


HTML_TEMPLATE = r"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>__TITLE__</title>
<style>
:root{color-scheme:dark;--bg:#071019;--panel:#0c1824;--grid:#203344;
--text:#dce8f2;--muted:#8297aa;--up:#ff4d5f;--down:#20c985}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 20% 0,
#12273b 0,#071019 45%);color:var(--text);font:14px/1.5 system-ui,
"Microsoft YaHei",sans-serif}.shell{max-width:1440px;margin:auto;padding:26px}
h1{font-size:22px;margin:0}.sub{color:var(--muted);margin-top:4px}
.cards{display:grid;grid-template-columns:repeat(5,minmax(120px,1fr));gap:10px;
margin:20px 0}.card,.chart{background:color-mix(in srgb,var(--panel) 92%,
transparent);border:1px solid #1c3041;border-radius:12px;box-shadow:
0 18px 50px #0005}.card{padding:12px 14px}.label{color:var(--muted);
font-size:12px}.value{font:600 18px/1.5 ui-monospace,Consolas,monospace}
.chart{position:relative;height:650px;padding:12px}.chart canvas{width:100%;
height:100%;display:block}.tip{position:absolute;display:none;pointer-events:none;
background:#071019ee;border:1px solid #35516a;border-radius:8px;padding:8px 10px;
white-space:nowrap;font:12px/1.65 ui-monospace,Consolas,monospace;
box-shadow:0 8px 24px #0008}.foot{color:var(--muted);font-size:12px;
margin-top:12px;word-break:break-all}
@media(max-width:760px){.shell{padding:12px}.cards{grid-template-columns:
repeat(2,1fr)}.chart{height:540px}}
</style>
</head>
<body>
<main class="shell">
  <h1 id="title"></h1>
  <div class="sub" id="subtitle"></div>
  <section class="cards" id="cards"></section>
  <section class="chart"><canvas id="canvas"></canvas><div class="tip" id="tip"></div></section>
  <div class="foot" id="source"></div>
</main>
<script>
const DATA=__DATA__;
const bars=DATA.bars,meta=DATA.metadata;
const fmt=(n,d=2)=>Number(n).toLocaleString("zh-CN",{minimumFractionDigits:d,
maximumFractionDigits:d});
document.querySelector("#title").textContent=`${meta.name} ${meta.code} · 1分钟K线`;
document.querySelector("#subtitle").textContent=
 `${meta.selected_first_date} · ${bars.length} 根 · ${meta.market.toUpperCase()}`;
const first=bars[0],last=bars[bars.length-1],change=last.close-first.open;
const changePct=change/first.open*100;
const cards=[
 ["开盘",fmt(first.open)],["收盘",fmt(last.close)],
 ["最高",fmt(Math.max(...bars.map(x=>x.high)))],
 ["最低",fmt(Math.min(...bars.map(x=>x.low)))],
 ["涨跌",`${change>=0?"+":""}${fmt(change)} (${changePct>=0?"+":""}${fmt(changePct)}%)`]
];
document.querySelector("#cards").innerHTML=cards.map(([a,b])=>
 `<div class="card"><div class="label">${a}</div><div class="value">${b}</div></div>`
).join("");
document.querySelector("#source").textContent=
 `缓存：${meta.source} · 可用日期 ${meta.available_first_date}—${meta.available_last_date} · 文件修改 ${meta.source_modified}`;
const canvas=document.querySelector("#canvas"),tip=document.querySelector("#tip");
const ctx=canvas.getContext("2d");let active=-1;
function draw(){
 const dpr=devicePixelRatio||1,box=canvas.getBoundingClientRect();
 canvas.width=Math.max(1,Math.round(box.width*dpr));
 canvas.height=Math.max(1,Math.round(box.height*dpr));
 ctx.setTransform(dpr,0,0,dpr,0,0);
 const W=box.width,H=box.height,L=66,R=18,T=18,B=30,gap=26,volH=105;
 const priceH=H-T-B-gap-volH,plotW=W-L-R;
 const hi=Math.max(...bars.map(x=>x.high)),lo=Math.min(...bars.map(x=>x.low));
 const pad=Math.max((hi-lo)*.06,hi*.0005),yMax=hi+pad,yMin=lo-pad;
 const x=i=>L+(i+.5)*plotW/bars.length;
 const y=v=>T+(yMax-v)/(yMax-yMin)*priceH;
 const volTop=T+priceH+gap,maxVol=Math.max(...bars.map(x=>x.volume),1);
 ctx.clearRect(0,0,W,H);ctx.font="11px system-ui";ctx.textBaseline="middle";
 ctx.strokeStyle="#203344";ctx.fillStyle="#8297aa";ctx.lineWidth=1;
 for(let i=0;i<=5;i++){const yy=T+i*priceH/5,val=yMax-i*(yMax-yMin)/5;
  ctx.beginPath();ctx.moveTo(L,yy+.5);ctx.lineTo(W-R,yy+.5);ctx.stroke();
  ctx.textAlign="right";ctx.fillText(fmt(val),L-8,yy)}
 const marks=[0,59,119,179,bars.length-1].filter((v,i,a)=>
  v>=0&&v<bars.length&&a.indexOf(v)===i);
 ctx.textAlign="center";
 for(const i of marks){const xx=x(i);ctx.beginPath();ctx.moveTo(xx,T);
  ctx.lineTo(xx,volTop+volH);ctx.stroke();
  ctx.fillText(i===119?"午间":bars[i].time,xx,H-12)}
 const step=plotW/bars.length,body=Math.max(1,Math.min(6,step*.68));
 for(let i=0;i<bars.length;i++){const b=bars[i],xx=x(i),up=b.close>=b.open;
  ctx.strokeStyle=ctx.fillStyle=up?"#ff4d5f":"#20c985";
  ctx.beginPath();ctx.moveTo(xx,y(b.high));ctx.lineTo(xx,y(b.low));ctx.stroke();
  const top=y(Math.max(b.open,b.close)),bottom=y(Math.min(b.open,b.close));
  if(Math.abs(bottom-top)<1){ctx.fillRect(xx-body/2,top,body,1)}
  else if(up){ctx.strokeRect(xx-body/2,top,body,bottom-top)}
  else{ctx.fillRect(xx-body/2,top,body,bottom-top)}
  const vh=b.volume/maxVol*volH;
  ctx.globalAlpha=.6;ctx.fillRect(xx-body/2,volTop+volH-vh,body,vh);ctx.globalAlpha=1}
 ctx.fillStyle="#8297aa";ctx.textAlign="right";
 ctx.fillText(maxVol.toLocaleString("zh-CN"),L-8,volTop);
 ctx.fillText("0",L-8,volTop+volH);
 if(active>=0){const b=bars[active],xx=x(active),yy=y(b.close);
  ctx.strokeStyle="#b5c8d955";ctx.setLineDash([4,4]);ctx.beginPath();
  ctx.moveTo(xx,T);ctx.lineTo(xx,volTop+volH);ctx.moveTo(L,yy);
  ctx.lineTo(W-R,yy);ctx.stroke();ctx.setLineDash([])}
}
function point(ev){
 const r=canvas.getBoundingClientRect(),L=66,R=18;
 const i=Math.max(0,Math.min(bars.length-1,
  Math.floor((ev.clientX-r.left-L)/(r.width-L-R)*bars.length)));
 active=i;draw();const b=bars[i],up=b.close>=b.open;
 tip.style.display="block";tip.innerHTML=`<b>${b.time}</b><br>
 开 ${fmt(b.open)}　高 ${fmt(b.high)}<br>低 ${fmt(b.low)}　收 ${fmt(b.close)}
 <br>量 ${b.volume.toLocaleString("zh-CN")}　额 ${fmt(b.amount,0)}`;
 const x=ev.clientX-r.left+14,y=ev.clientY-r.top+14;
 tip.style.left=Math.min(x,r.width-tip.offsetWidth-10)+"px";
 tip.style.top=Math.min(y,r.height-tip.offsetHeight-10)+"px";
 tip.style.borderColor=up?"#ff4d5f88":"#20c98588";
}
canvas.addEventListener("mousemove",point);
canvas.addEventListener("mouseleave",()=>{active=-1;tip.style.display="none";draw()});
new ResizeObserver(draw).observe(canvas);draw();
</script>
</body></html>
"""


def render_html(series: MinuteSeries, bars: tuple[MinuteBar, ...]) -> str:
    dates = {bar.date for bar in bars}
    if len(dates) != 1:
        raise MinuteDataError("HTML K 线图一次只支持一个交易日，请指定 --date")
    payload = {
        "metadata": series_metadata(series, bars),
        "bars": [asdict(bar) for bar in bars],
    }
    embedded = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
    embedded = embedded.replace("<", "\\u003c")
    title = html.escape(f"{series.name} {series.code} 1分钟K线", quote=True)
    return HTML_TEMPLATE.replace("__TITLE__", title).replace("__DATA__", embedded)


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
        text=True,
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="") as stream:
            stream.write(content)
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="读取通达信 .lc1 一分钟缓存并导出 CSV、JSON 或离线 K 线图。"
    )
    parser.add_argument("--root", type=Path, help="通达信安装根目录")
    parser.add_argument("--code", required=True, help="证券或板块代码，如 880471")
    parser.add_argument(
        "--market",
        choices=("sh", "sz", "bj"),
        help="市场前缀；默认按配置和现有文件自动判断",
    )
    parser.add_argument(
        "--date",
        default="latest",
        help="交易日 YYYYMMDD；默认 latest，CSV/JSON 可指定 all",
    )
    parser.add_argument(
        "--format",
        choices=("csv", "json", "html"),
        default="html",
        help="输出格式（默认 html）",
    )
    parser.add_argument("--output", type=Path, help="输出路径")
    return parser


def parse_date_argument(value: str, output_format: str) -> str:
    normalized = value.strip().lower()
    if normalized in {"latest", "all"}:
        if normalized == "all" and output_format == "html":
            raise MinuteDataError("HTML 输出不支持 --date all")
        return normalized
    try:
        datetime.strptime(normalized, "%Y%m%d")
    except ValueError as error:
        raise MinuteDataError(
            "--date 必须是 YYYYMMDD、latest 或 all"
        ) from error
    return normalized


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        root = find_tdx_root(args.root)
        requested_date = parse_date_argument(args.date, args.format)
        series = load_series(root, args.code.strip(), args.market)
        bars = select_bars(series.bars, requested_date)
        if args.format == "csv":
            content = render_csv(bars)
        elif args.format == "json":
            content = render_json(series, bars)
        else:
            content = render_html(series, bars)

        output = args.output
        if output is None and args.format == "html":
            output = DEFAULT_HTML_DIR / f"tdx-{series.code}-1m.html"
        if output is None:
            sys.stdout.write(content)
        else:
            resolved = output.expanduser().resolve()
            atomic_write(resolved, content)
            metadata = series_metadata(series, bars)
            selected_range = str(metadata["selected_first_date"])
            if metadata["selected_last_date"] != metadata["selected_first_date"]:
                selected_range += f"—{metadata['selected_last_date']}"
            print(
                f"已导出 {series.name}({series.code}) "
                f"{selected_range} 的 {len(bars)} 根 1 分钟线："
                f"{resolved}"
            )
            print(
                f"本地缓存范围：{metadata['available_first_date']}—"
                f"{metadata['available_last_date']}；源文件：{series.source}"
            )
        return 0
    except (MinuteDataError, OSError) as error:
        print(f"错误：{error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

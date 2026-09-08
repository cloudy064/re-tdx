#!/usr/bin/env python3
"""通达信上游 API 命令行工具。Python 3.10+，仅标准库，可单文件复制运行。"""

from __future__ import annotations

import argparse
import ast
import configparser
import datetime as dt
import hashlib
import html
import json
import math
import os
from pathlib import Path
import re
import socket
import struct
import sys
import tempfile
import urllib.parse
import urllib.request
import zlib


VERSION = "1.0.0"
GATEWAY = "http://static.tdx.com.cn:7615/TQLEX"
DATA_URL = "https://data.tdx.com.cn/"
DEFAULT_HOST = "123.60.84.66:7709"
EXPANSION_HOST = "116.205.143.214:7727"
PERIODS = {"1m": 7, "5m": 0, "15m": 1, "30m": 2, "60m": 3,
           "day": 4, "week": 5, "month": 6}
FUNDS = ("15", "16", "50", "51", "52", "53", "56", "58")
BONDS = ("10", "11", "12")
EXPANSION_SETUP = bytes.fromhex("1f32c6e5d53dfb41" * 8 + "cce16dffd5ba3fb8cbc57a054f7748ea")

# Catalog records are descriptive metadata, never guessed request templates.
CLOUD_CATALOG_TEXT = """
tqlex|200626,200650|因子目录|gp_gz_fsld.xml
tqlex|200636,200651|因子成分（ID）|gp_gz_fsld.xml
tqlex|200646|股票因子看板|gp_gz_fsld.xml
tqlex|200660|分时雷达|gp_gz_fsld.xml
tqlex|200661,200662|盘中机会、盘面信号（flag）|gp_gz_dxjh.xml
tqlex|200000|指数相对估值概览|zs_zsgzb.xml
tqlex|200003,200004|指数波动率历史、汇总|zs_zsbdl.xml
tqlex|200009,200010|两融分档、汇总|rzrq_zsyc.xml
tqlex|500501,500502|北向资金分档、汇总|bxzj_zsyc3.xml
tqlex|200770|全收益与价格指数比较|sc_qsyzs.xml
tqlex|2044|异常波动统计|sc_ydjj.xml
tqlex|200720|异动停牌风险|sc_gjyd.xml
tqlex|500109,500108|异常刷新摘要、单票原因|hq_lhb_dryc.xml
tqlex|500601|利润断层|gp_gz_lrdc.xml
tqlex|500030,500031|基金风险、日收益|jj_jzfx_fxsy.xml
tqlex|500032,500033|基金月度风险、收益|jj_jzfx_ydfx.xml
tqlex|500035|基金择时选股能力|jj_jzfx_zsxg.xml
tqlex|500050,500051,500052|基金持仓、行业、股票明细|jj_ccfx_dqcc.xml
tqlex|500055,500056,500057|基金稳定性、行业、仓位序列|jj_ccfx_qjcc.xml
tqlex|500060|基金仓位估算|jj_cwgs.xml
tqlex|500062|全市场基金仓位|jj_cwgs_qsc.xml
pbrpc|200340,200341|行业资金、成分资金|sszjtj.xml
pbrpc|200300,200301,200302|个股PE、行业PE成分、行业目录|gp_gz_peg.xml
pbrpc|200303,200305|PB-ROE成分、行业目录|gp_gz_PB-ROE.xml
pbrpc|200001|指数相对估值历史|zs_zsgzb.xml
pbrpc|200199|板块及成分历史表现|BK_BKLSHC.xml
pbrpc|200011|两融历史|rzrq_zsyc1.xml
pbrpc|500503|北向资金历史|bxzj_zsyc4.xml
pbrpc|500107|异常交易证券列表|hq_lhb_dryc.xml
pbrpc|200451|神奇九转|func_sqjz101.xml
pbrpc|200452,200453|股票、板块RPS|gp_gz_rpsxg.xml
pbrpc|200327,200329,200316|新高新低、横盘突破、强势启动|gp_gz_xgxd.xml
pbrpc|200320|上涨通道|gp_gz_SSTD.xml
pbrpc|200325|下跌通道|gp_gz_cdgg.xml
pbrpc|200225,200250|事件驱动、固定模型选股|tdxxgcl6.xml
pbrpc|200400,200401,200402,200403,200404,200405,200406|竞价策略|sc_jjcl.xml
"""
TCP_CATALOG = {
    0x000D: "普通行情握手", 0x0004: "心跳及服务端时间",
    0x044E: "证券数量", 0x044D: "证券目录", 0x054C: "批量最新行情",
    0x053E: "涨速及五档", 0x0547: "五档及增量行情", 0x054B: "行情排序",
    0x052D: "多周期K线", 0x0FC5: "当日成交", 0x0FC6: "历史成交",
    0x056A: "集合竞价", 0x0010: "基础财务", 0x000F: "股本及除权除息",
    0x0452: "特殊品种涨跌停", 0x02C5: "文件信息", 0x06B9: "文件分块",
    0x2454: "扩展握手", 0x23F0: "合约数量", 0x23F5: "合约目录",
    0x23FA: "扩展最新行情", 0x23FF: "扩展K线", 0x240B: "扩展当日分时",
    0x240C: "扩展历史分时", 0x23FC: "扩展当日逐笔", 0x2406: "扩展历史逐笔",
}
JSN_CATALOG = {
    "themes": ("list/func_zttz105_1.jsn", "统一主题主表"),
    "industries": ("list/func_gx_hyzt101_1.jsn", "行业、主题与证券关系"),
    "ai": ("list/func_rgzn101_1.jsn", "人工智能战略主题"),
    "etf-stocks": ("list/func_etfsg101_1.jsn", "股票ETF资金"),
    "etf-industries": ("list/func_etfsg102_1.jsn", "行业ETF资金"),
    "disclosures": ("list/func_cbpl103_1.jsn", "财报披露预约"),
    "earnings": ("list/func_cbpl102_1.jsn", "业绩快报"),
    "research": ("list/func_tzzhd109_1.jsn", "机构调研"),
    "qa": ("list/func_tzzhd103_1.jsn", "互动问答"),
    "bonds": ("list/kzz_kzzsy201_1.jsn", "可转债基础资料"),
    "bond-terms": ("list/func_kzz_tkjd201.jsn", "可转债条款进度"),
}


class APIError(Exception):
    """User-facing request, transport, or decoding failure."""


def require(condition, message):
    if not condition:
        raise APIError(message)


def bounded(value, minimum, maximum, name):
    require(minimum <= value <= maximum, f"{name} 必须在 {minimum}..{maximum} 之间")
    return value


def text_decode(data):
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16")
    for encoding in ("utf-8-sig", "gb18030"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    raise APIError("响应文本既不是 UTF-8，也不是 GB18030")


def json_bytes(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode("utf-8")


def json_response(data):
    value = json.loads(text_decode(data).strip("\x00 \t\r\n"))
    roots = value if isinstance(value, list) else [value]
    for root in roots:
        if isinstance(root, dict):
            code = root.get("ErrorCode", root.get("errorCode", 0))
            require(code in (0, "0", None, ""),
                    f"上游业务错误 {code}: {root.get('ErrorInfo', root.get('errorInfo', ''))}")
    return value


def atomic_write(path, data):
    destination = Path(path).expanduser()
    with tempfile.NamedTemporaryFile(dir=destination.parent, prefix=".tdx-", delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(data)
            stream.flush()
            stream.close()
            os.replace(temporary, destination)
        finally:
            temporary.unlink(missing_ok=True)


def emit(value, args):
    data = (json.dumps(value, ensure_ascii=False, indent=None if args.compact else 2,
                       allow_nan=False) + "\n").encode("utf-8")
    if args.output:
        atomic_write(args.output, data)
    else:
        sys.stdout.write(data.decode("utf-8"))


def http_fetch(url, args, body=None, content_type="application/json"):
    require(urllib.parse.urlsplit(url).scheme in ("http", "https"), "URL 必须为 HTTP 或 HTTPS")
    headers = {"User-Agent": "tdx-api-python/" + VERSION, "Accept-Encoding": "identity"}
    if body is not None:
        headers["Content-Type"] = content_type
    request = urllib.request.Request(url, data=body, headers=headers)
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({})) if args.no_proxy else urllib.request.build_opener()
    with opener.open(request, timeout=args.timeout) as response:
        result = response.read(args.max_bytes + 1)
    require(len(result) <= args.max_bytes, "HTTP 响应超过 --max-bytes")
    return result


def endpoint(value, expansion=False):
    parsed = urllib.parse.urlsplit("//" + value)
    require(bool(parsed.hostname) and not parsed.path and not parsed.username and not parsed.query,
            "--host 应为 HOST[:PORT]；IPv6 使用 [地址]:端口")
    return parsed.hostname, parsed.port or (7727 if expansion else 7709)


def request_frame(message_id, command, body, expansion=False):
    bounded(command, 0, 65535, "TCP command")
    require(len(body) <= 65533, "TCP 请求业务体过长")
    return struct.pack("<BIBHHH", 1 if expansion else 12, message_id, 1,
                       len(body) + 2, len(body) + 2, command) + body


class QuoteConnection:
    def __init__(self, host, args):
        self.expansion = getattr(args, "expansion", False)
        self.sock = socket.create_connection(endpoint(host, self.expansion), args.timeout)
        self.message_id = 0 if self.expansion else 0x01640801
        try:
            if self.expansion:
                self.call(0x2454, EXPANSION_SETUP)
            else:
                response = self.call(0x000D, b"\x01")
                require(len(response) >= 189, "普通行情握手响应不足 189 字节")
        except BaseException:
            self.sock.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.sock.close()

    def read_exact(self, count):
        result = bytearray()
        while len(result) < count:
            part = self.sock.recv(count - len(result))
            require(bool(part), "TCP 响应未读完，连接已关闭")
            result.extend(part)
        return bytes(result)

    def call(self, command, body=b""):
        message_id = self.message_id
        self.message_id = (message_id + 1) & 0xFFFFFFFF
        self.sock.sendall(request_frame(message_id, command, body, self.expansion))
        prefix, _, answer_id, _, answer_command, wire_size, plain_size = struct.unpack(
            "<4sBIBHHH", self.read_exact(16))
        require(prefix == bytes.fromhex("b1cb7400"), "TCP 响应前缀错误")
        require(answer_id == message_id and answer_command == command, "TCP 响应 message_id/command 不匹配")
        data = self.read_exact(wire_size)
        if wire_size != plain_size:
            decoder = zlib.decompressobj()
            data = decoder.decompress(data, plain_size + 1)
            require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,
                    "TCP zlib 响应截断、超长或存在尾随数据")
        require(len(data) == plain_size, "TCP 响应解码长度不匹配")
        return data


def hosts_for(args):
    if args.host:
        return args.host
    if args.connect_cfg:
        cfg = configparser.ConfigParser(interpolation=None, strict=False)
        cfg.read_string(text_decode(Path(args.connect_cfg).read_bytes()))
        require("HQHOST" in cfg, "connect.cfg 缺少 [HQHOST]")
        section = cfg["HQHOST"]
        hosts = [f"{value}:{section.get('port' + key[2:], '7709')}"
                 for key, value in section.items() if re.fullmatch(r"ip\d+", key) and value.strip()]
        require(bool(hosts), "connect.cfg 没有 HQHOST IP 节点")
        return list(dict.fromkeys(hosts))[:8]
    return [EXPANSION_HOST if getattr(args, "expansion", False) else DEFAULT_HOST]


def tcp_run(args, action):
    failures = []
    for host in hosts_for(args):
        try:
            with QuoteConnection(host, args) as connection:
                return action(connection)
        except (OSError, APIError, zlib.error) as error:
            failures.append(f"{host}: {error}")
    raise APIError("行情节点请求失败；可用 --host 指定其他节点。" + "；".join(failures))


class Reader:
    def __init__(self, data, offset=0):
        self.data, self.offset = data, offset

    def take(self, count):
        require(self.offset + count <= len(self.data), f"二进制响应截断，偏移 {self.offset} 需要 {count} 字节")
        result = self.data[self.offset:self.offset + count]
        self.offset += count
        return result

    def unpack(self, fmt):
        values = struct.unpack("<" + fmt, self.take(struct.calcsize("<" + fmt)))
        return values[0] if len(values) == 1 else values

    def varint(self):
        first = self.unpack("B")
        value, shift, byte = first & 63, 6, first
        while byte & 128:
            require(shift < 55, "TDX 变长整数过长")
            byte = self.unpack("B")
            value |= (byte & 127) << shift
            shift += 7
        return -value if first & 64 else value

    def end(self):
        require(self.offset == len(self.data), f"响应残留 {len(self.data) - self.offset} 字节")


def wire_number(bits):
    if not bits:
        return 0.0
    exponent = struct.unpack("<i", struct.pack("<I", bits))[0] >> 24
    high, middle, low = (bits >> 16) & 255, (bits >> 8) & 255, bits & 255
    base = 2.0 ** (exponent * 2 - 127)
    top = base * (64 + (high & 127)) / 64 if high > 128 else base * high / 128
    scale = 2 if high & 128 else 1
    return base + top + base * middle / 32768 * scale + base * low / 8388608 * scale


def security(value, expansion=False):
    match = re.fullmatch(r"(?:(sz|sh|bj):?|(\d+):)([A-Za-z0-9 ]+)", value, re.I)
    require(bool(match), "证券须显式带市场，如 sz000001、sh:600000、2:920001；扩展市场如 31:00700")
    prefix, market, code = match.groups()
    market = {"sz": 0, "sh": 1, "bj": 2}[prefix.lower()] if prefix else int(market)
    bounded(market, 0, 255 if expansion else 2, "market")
    require(bool(re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9 ]{0,7}[A-Za-z0-9])?", code)) if expansion
            else bool(re.fullmatch(r"[0-9]{6}", code)), "普通代码须为 6 位数字；扩展代码须为 1..9 位 ASCII")
    return market, code


def date_value(value):
    normalized = value.replace("-", "")
    if not re.fullmatch(r"\d{8}", normalized):
        raise ValueError("日期应为 YYYYMMDD 或 YYYY-MM-DD")
    dt.datetime.strptime(normalized, "%Y%m%d")
    return int(normalized)


def minute_label(minute):
    bounded(minute, 0, 1439, "minute_of_day")
    return f"{minute // 60:02d}:{minute % 60:02d}"


def decode_bar_time(reader, period):
    if period < 4 or period == 7:
        encoded, minute = reader.unpack("HH")
        year, md = encoded // 2048 + 2004, encoded % 2048
        date = date_value(f"{year:04d}{md // 100:02d}{md % 100:02d}")
    else:
        date, minute = date_value(str(reader.unpack("I"))), 900
    return {"date": date, "time": minute_label(minute)}


def parse_quotes(data, requested, depth=False):
    if depth:
        data = bytes(byte ^ 0x93 for byte in data)
    reader = Reader(data, 0 if depth else 2)
    count = reader.unpack("H")
    require(count <= len(requested), "行情条数超过请求条数")
    body = data[reader.offset:]
    if not count:
        require(not body, "零条行情响应存在多余数据")
        return []
    starts = [match.start() for match in re.finditer(rb"[\x00-\x02][0-9]{6}", body)]
    require(len(starts) == count and starts[0] == 0, "无法确定行情记录边界")
    rows, seen = [], set()
    for start, end in zip(starts, starts[1:] + [len(body)]):
        r = Reader(body[start:end])
        market, code, active = r.unpack("B6sH")
        code = code.decode("ascii")
        require((market, code) in requested and (market, code) not in seen, "行情证券错位或重复")
        seen.add((market, code))
        current = r.varint()
        prices = [current] + [current + r.varint() for _ in range(4)]
        divisor = 100 if code.startswith(BONDS) else 10 if code.startswith(FUNDS) else 1
        row = dict(zip(("price", "pre_close", "open", "high", "low"),
                       (value / (100 * divisor) for value in prices)))
        row.update(market=market, code=code, active=active)
        if depth:
            row.update(update_time_raw=r.unpack("I"), status_raw=r.varint())
        else:
            row.update(time_raw=r.varint(), auxiliary_price_delta_raw=r.varint())
        row.update(volume_hand=r.varint(), last_volume_hand=r.varint(), amount=wire_number(r.unpack("I")),
                   inside_hand=r.varint(), outside_hand=r.varint(), auction_imbalance_hand=r.varint(),
                   open_amount=r.varint() * (10 if depth else 100))
        if depth:
            bids, asks = [], []
            for _ in range(5):
                bid_delta, ask_delta, bid_volume, ask_volume = [r.varint() for _ in range(4)]
                bids.append({"price": (current + bid_delta) / (100 * divisor), "volume_hand": bid_volume})
                asks.append({"price": (current + ask_delta) / (100 * divisor), "volume_hand": ask_volume})
            row.update(bids=bids, asks=asks)
        row["tail_hex"] = r.take(len(r.data) - r.offset).hex()
        rows.append(row)
    return rows


def parse_expansion_quote(data, expected):
    require(len(data) >= 150, "扩展行情响应不足 150 字节")
    market, code = data[0], data[1:10].rstrip(b"\0").decode("ascii")
    require((market, code) == expected, "扩展行情证券不匹配")
    row = {"market": market, "code": code}
    for name, offset in (("pre_close_or_settlement", 14), ("open", 18), ("high", 22), ("low", 26), ("price", 30)):
        row[name] = struct.unpack_from("<f", data, offset)[0]
    for name, offset in (("opening_volume", 34), ("volume", 42), ("last_volume", 46),
                         ("inside_volume", 54), ("outside_volume", 58), ("open_interest", 66)):
        row[name] = struct.unpack_from("<I", data, offset)[0]
    for name, price_offset, volume_offset in (("bids", 70, 90), ("asks", 110, 130)):
        row[name] = [{"price": struct.unpack_from("<f", data, price_offset + i * 4)[0],
                      "volume": struct.unpack_from("<I", data, volume_offset + i * 4)[0]} for i in range(5)]
    return [row]


def parse_klines(data, args):
    r = Reader(data, 18 if args.expansion else 0)
    count = r.unpack("H")
    require(count <= args.count, "K线条数超过请求条数")
    rows, previous = [], 0
    for _ in range(count):
        row = decode_bar_time(r, PERIODS[args.period])
        if args.expansion:
            opening, high, low, closing, interest, volume, auxiliary = r.unpack("ffffIIf")
            row.update(open=opening, high=high, low=low, close=closing, volume=volume,
                       open_interest=interest, auxiliary_price=auxiliary, amount=None)
        else:
            opening = previous + r.varint()
            closing, high, low = [opening + r.varint() for _ in range(3)]
            previous = closing
            volume, amount = [wire_number(r.unpack("I")) for _ in range(2)]
            require(math.isfinite(volume) and 0 <= volume < 2**63, "K线成交量超出非负 int64")
            row.update(open=opening / 1000, high=high / 1000, low=low / 1000,
                       close=closing / 1000, volume=int(volume + 0.5), amount=amount)
            if args.index:
                row.update(up_count=r.unpack("H"), down_count=r.unpack("H"))
        rows.append(row)
    r.end()
    return rows


def parse_trades(data, args, code):
    r = Reader(data, 14 if args.expansion else 0)
    count = r.unpack("H")
    require(count <= args.count, "成交条数超过请求条数")
    base = r.unpack("f") if args.date and not args.expansion else None
    if base is not None:
        require(math.isfinite(base), "历史成交价格基数字段不是有限数值")
    rows, price = [], 0
    divisor = 1000 if code.startswith(BONDS + FUNDS) else 100
    for index in range(count):
        minute = r.unpack("H")
        row = {"index": args.start + index, "time": minute_label(minute)}
        if args.expansion:
            price_raw, volume, interest_change, nature = r.unpack("IIiH")
            row.update(price=price_raw / 1000, volume=volume,
                       open_interest_change=interest_change, nature_raw=nature)
        else:
            delta, volume, orders, status, tail = [r.varint() for _ in range(5)]
            price += delta
            require(price >= 0 and volume >= 0 and orders >= 0, "成交价格、成交量或笔数为负")
            row.update(price=price / divisor, volume_hand=volume, order_count=orders,
                       side={0: "buy", 1: "sell", 2: "neutral"}.get(status, "unknown"),
                       status_raw=status, tail_raw=tail)
        rows.append(row)
    r.end()
    return {"date": args.date, "price_base_raw": base, "rows": rows}


def parse_securities(data, args):
    r = Reader(data, 4 if args.expansion else 0)
    count = r.unpack("H")
    require(count <= args.count, "证券目录条数超过请求条数")
    rows = []
    for _ in range(count):
        raw = r.take(64 if args.expansion else 37)
        if args.expansion:
            row = {"market": raw[1], "category_raw": raw[0],
                   "code": text_decode(raw[5:14].rstrip(b"\0")),
                   "name": text_decode(raw[14:31].rstrip(b"\0")),
                   "description": text_decode(raw[31:40].rstrip(b"\0")),
                   "contract_multiplier": struct.unpack_from("<I", raw, 56)[0]}
        else:
            row = {"market": args.market, "code": raw[:6].decode("ascii"),
                   "multiple": struct.unpack_from("<H", raw, 6)[0],
                   "name": text_decode(raw[8:24].rstrip(b"\0")), "decimal": raw[28],
                   "volume_ratio_base": struct.unpack_from("<f", raw, 24)[0],
                   "pre_close": struct.unpack_from("<f", raw, 29)[0]}
        rows.append(row)
    return {"rows": rows, "tail_hex": r.take(len(data) - r.offset).hex()}


def parse_timeline(data, args):
    r = Reader(data, 18 if args.date else 10)
    count = r.unpack("H")
    require(count <= 2000, "扩展分时条数超过 2000")
    rows = []
    for index in range(count):
        minute, price, average, volume, interest = r.unpack("HffII")
        day_offset, day_minute = divmod(minute, 1440)
        rows.append({"index": index, "wire_minute": minute, "session_day_offset": day_offset,
                     "time": minute_label(day_minute) if day_offset <= 1 else None,
                     "price": price, "average_price": average, "volume": volume, "open_interest": interest})
    r.end()
    return rows


def market_request(args):
    expansion = args.expansion
    if hasattr(args, "count"):
        limit = 800 if args.command == "kline" else 1800 if args.command == "trades" else 100 if expansion else 1000
        bounded(args.count, 1, limit, "count")
        bounded(args.start, 0, 2**32 - 1 if expansion or args.command == "securities" else 65535, "start")
    if args.command == "raw":
        require(not (args.hex and args.body_file), "--hex 和 --body-file 只能选一个")
        body = Path(args.body_file).read_bytes() if args.body_file else bytes.fromhex(args.hex or "")
        return args.tcp_command, body, lambda data: {"bytes": len(data), "hex": data.hex()}
    if args.command == "securities":
        if args.count_only:
            command = 0x23F0 if expansion else 0x044E
            body = b"" if expansion else struct.pack("<HI", args.market, int(dt.datetime.now(dt.timezone(dt.timedelta(hours=8))).strftime("%Y%m%d")))
            def parse_count(data):
                if expansion:
                    require(data.startswith(b"TDX_DS"), "扩展目录数量缺少 TDX_DS 签名")
                return {"count": Reader(data, 19 if expansion else 0).unpack("I" if expansion else "H")}
            return command, body, parse_count
        body = struct.pack("<IH", args.start, args.count) if expansion else struct.pack("<HIII", args.market, args.start, args.count, 0)
        return 0x23F5 if expansion else 0x044D, body, lambda data: parse_securities(data, args)
    if args.command == "quote":
        codes = list(dict.fromkeys(security(value, expansion) for value in args.securities))
        if expansion:
            require(len(codes) == 1, "扩展行情每次查询一个合约")
            require(not args.depth, "扩展行情默认含五档，无需 --depth")
            market, code = codes[0]
            return 0x23FA, struct.pack("<B9s", market, code.encode("ascii")), lambda data: parse_expansion_quote(data, codes[0])
        bounded(len(codes), 1, 80, "批量证券数")
        body = struct.pack("<H", len(codes))
        for market, code in codes:
            body += struct.pack("<B6s", market, code.encode("ascii")) + (b"\0" * 4 if args.depth else b"")
        if not args.depth:
            body = bytes.fromhex("0500000000000000") + body
        return 0x0547 if args.depth else 0x054C, body, lambda data: parse_quotes(data, codes, args.depth)
    market, code = security(args.security, expansion)
    encoded = code.encode("ascii")
    symbol = struct.pack("<B9s", market, encoded) if expansion else struct.pack("<H6s", market, encoded)
    if args.command == "kline":
        period = PERIODS[args.period]
        require(not (args.index and expansion), "--index 仅适用于普通行情指数 K 线")
        body = symbol + (struct.pack("<HHIH", period, 1, args.start, args.count) if expansion else
                         struct.pack("<HHHH", period, 1, args.start, args.count) + bytes(26))
        return 0x23FF if expansion else 0x052D, body, lambda data: parse_klines(data, args)
    if args.command == "trades":
        body = symbol + struct.pack("<IH" if expansion else "<HH", args.start, args.count)
        if args.date:
            body = struct.pack("<I", args.date) + body
        command = (0x2406 if args.date else 0x23FC) if expansion else (0x0FC6 if args.date else 0x0FC5)
        return command, body, lambda data: parse_trades(data, args, code)
    require(args.command == "timeline" and expansion, "timeline 仅支持扩展行情")
    body = (struct.pack("<I", args.date) if args.date else b"") + symbol
    return 0x240C if args.date else 0x240B, body, lambda data: parse_timeline(data, args)


def tcp_preview(args, command, body):
    expansion = getattr(args, "expansion", False)
    initial = 0 if expansion else 0x01640801
    return {"transport": "tcp", "hosts": hosts_for(args), "command": f"0x{command:04X}",
            "handshake_frame_hex": request_frame(initial, 0x2454 if expansion else 0x000D,
                                                  EXPANSION_SETUP if expansion else b"\x01", expansion).hex(),
            "body_hex": body.hex(), "first_request_frame_hex": request_frame(initial + 1, command, body, expansion).hex()}


def run_market(args):
    command, body, parser = market_request(args)
    if args.dry_run:
        return tcp_preview(args, command, body)
    data = tcp_run(args, lambda connection: connection.call(command, body))
    if args.command == "raw" and args.binary:
        require(bool(args.output), "raw --binary 必须同时指定 -o 文件")
        atomic_write(args.output, data)
        return None
    return parser(data)


def remote_file_path(path):
    require(not path.startswith(("/", "\\")) and ".." not in path.split("/") and "\0" not in path,
            "远端路径必须是相对路径且不能含 .. 或 NUL")
    return path.encode("ascii")


def file_block_body(path, offset, count):
    encoded = remote_file_path(path)
    require(len(encoded) < 300, "远端路径超过 299 字节")
    return struct.pack("<II300s", offset, count, encoded)


def tcp_file(connection, path, args, with_info=True):
    total, expected_md5 = None, None
    if with_info:
        encoded = remote_file_path(path)
        require(len(encoded) < 40, "709 文件信息路径须少于 40 字节")
        r = Reader(connection.call(0x02C5, struct.pack("<40s", encoded)))
        total, has_md5 = r.unpack("IB")
        require(total <= args.max_bytes, "远端文件超过 --max-bytes")
        if has_md5:
            expected_md5 = r.take(32).decode("ascii").lower()
            require(bool(re.fullmatch("[0-9a-f]{32}", expected_md5)), "709 返回的 MD5 格式无效")
    data = bytearray()
    while total is None or len(data) < total:
        count = min(30000, total - len(data)) if total is not None else 30000
        r = Reader(connection.call(0x06B9, file_block_body(path, len(data), count)))
        size = r.unpack("I")
        require(size <= count, "1721 返回块大于请求块")
        block = r.take(size)
        r.end()
        require(len(data) + len(block) <= args.max_bytes, "远端文件超过 --max-bytes")
        data.extend(block)
        if not block or (total is None and len(block) < count):
            break
    require(total is None or len(data) == total, "文件下载提前结束")
    require(expected_md5 is None or hashlib.md5(data).hexdigest() == expected_md5, "文件 MD5 校验失败")
    return bytes(data)


def run_jsn(args):
    path = JSN_CATALOG.get(args.path, (args.path, ""))[0]
    path = path if path.startswith("bi/") else "bi/" + path
    encoded = remote_file_path(path)
    require(len(encoded) < 40, "JSN 资源路径须少于 40 字节（包括 bi/）")
    if args.dry_run:
        preview = tcp_preview(args, 0x02C5, struct.pack("<40s", encoded))
        preview.update(path=path, block_command="0x06B9", first_block_body_hex=file_block_body(path, 0, 30000).hex())
        return preview
    data = tcp_run(args, lambda connection: tcp_file(connection, path, args))
    if args.raw:
        require(bool(args.output), "jsn --raw 必须同时指定 -o 文件")
        atomic_write(args.output, data)
        return None
    return json_response(data)


def run_download(args):
    if args.tcp:
        path = args.path
        remote_file_path(path)
        if args.dry_run:
            return tcp_preview(args, 0x06B9, file_block_body(path, 0, 30000))
        data = tcp_run(args, lambda connection: tcp_file(connection, path, args, with_info=False))
    else:
        path = args.path
        if urllib.parse.urlsplit(path).scheme:
            url = path
        else:
            remote_file_path(path)
            url = DATA_URL + path
        if args.dry_run:
            return {"transport": "http", "method": "GET", "url": url}
        data = http_fetch(url, args)
    require(not args.md5 or hashlib.md5(data).hexdigest() == args.md5.lower(), "下载文件 MD5 校验失败")
    if args.output:
        atomic_write(args.output, data)
        print(json.dumps({"file": args.output, "bytes": len(data), "md5": hashlib.md5(data).hexdigest()}, ensure_ascii=False), file=sys.stderr)
        return None
    return {"bytes": len(data), "md5": hashlib.md5(data).hexdigest(), "text": text_decode(data)}


def pb_varint(value):
    require(0 <= value < 2**64, "protobuf varint 超出 uint64")
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def pb_field(number, value):
    if isinstance(value, str):
        value = value.encode("utf-8")
    if isinstance(value, bytes):
        return pb_varint(number * 8 + 2) + pb_varint(len(value)) + value
    return pb_varint(number * 8) + pb_varint(value)


def pb_fields(data):
    reader = Reader(data)
    def consume():
        value = 0
        for shift in range(0, 70, 7):
            byte = reader.unpack("B")
            require(shift < 63 or byte <= 1, "protobuf varint 超过 64 位")
            value |= (byte & 127) << shift
            if byte < 128:
                return value
        raise APIError("protobuf varint 未终止")
    fields = {}
    while reader.offset < len(data):
        tag = consume()
        number, wire = tag >> 3, tag & 7
        require(number > 0, "protobuf 字段号为零")
        if wire == 0:
            value = consume()
        elif wire == 2:
            value = reader.take(consume())
        elif wire in (1, 5):
            value = reader.take(8 if wire == 1 else 4)
        else:
            raise APIError(f"不支持 protobuf wire type {wire}")
        fields[number] = (wire, value)
    return fields


def pb_value(fields, number, wire, default):
    actual_wire, value = fields.get(number, (wire, default))
    require(actual_wire == wire, f"protobuf 字段 {number} wire type 不匹配")
    if wire == 0:
        value &= 0xFFFFFFFF
        return value - 2**32 if value >= 2**31 else value
    return value


def pbrpc_request(module, body, rpc_id=0, start=0):
    return (pb_field(1, pb_field(1, "1")) + (pb_field(2, rpc_id) if rpc_id else b"") +
            (pb_field(3, start) if start else b"") + pb_field(4, module) + pb_field(5, json_bytes(body)))


def query_pbrpc(url, module, body, args, fetch=http_fetch):
    data, rpc_id, expected_total, empty_count = bytearray(), 0, None, 0
    for _ in range(args.max_rounds):
        fields = pb_fields(fetch(url, args, pbrpc_request(module, body, rpc_id, len(data)), "application/octet-stream"))
        head = pb_fields(pb_value(fields, 1, 2, b""))
        code = pb_value(head, 1, 0, 0)
        require(code == 0, f"PBRPC 错误 {code}: {text_decode(pb_value(head, 2, 2, b''))}")
        new_id, start, total, size = [pb_value(fields, number, 0, 0) for number in range(2, 6)]
        chunk = pb_value(fields, 6, 2, b"")
        require(new_id >= 0 and start >= 0 and total >= 0 and size >= 0, "PBRPC 长度或会话号为负")
        require(size == len(chunk) and start == len(data), "PBRPC 分片长度或偏移不匹配")
        require(rpc_id == 0 or rpc_id == new_id, "PBRPC 会话号发生变化")
        require(expected_total is None or expected_total == total, "PBRPC 总长度发生变化")
        require(total <= args.max_bytes and len(data) + size <= args.max_bytes, "PBRPC 超过 --max-bytes")
        require(total == 0 or len(data) + size <= total, "PBRPC 分片超过总长度")
        expected_total, rpc_id = total, new_id
        data.extend(chunk)
        if (total > 0 and len(data) == total) or (total == 0 and chunk):
            return json_response(bytes(data))
        empty_count = 0 if chunk else empty_count + 1
        require(empty_count <= 3, "PBRPC 连续返回空分片")
        require(rpc_id > 0, "PBRPC 未完成但没有后续会话号")
    raise APIError("PBRPC 超过 --max-rounds，结果不完整")


def config_records(path):
    root = Path(path).expanduser()
    if root.is_dir() and (root / "T0002/cloud_cfg").is_dir():
        root = root / "T0002/cloud_cfg"
    files = [root] if root.is_file() else sorted(root.rglob("*.xml"))
    require(bool(files), f"没有找到 XML 模板: {root}")
    records = []
    # Attributes may contain > or single quotes in a double-quoted JSON body.
    tags = re.compile(r'''<datasource\b((?:[^'">]|"[^"]*"|'[^']*')*)>''', re.I | re.S)
    attributes = re.compile(r'''([\w-]+)\s*=\s*(["'])(.*?)\2''', re.S)
    for file in files:
        source = re.sub(r"<!--.*?-->", "", text_decode(file.read_bytes()), flags=re.S)
        for match in tags.finditer(source):
            attrs = {key.lower(): html.unescape(value) for key, _, value in attributes.findall(match[1])}
            fmt, body = attrs.get("reqformat", ""), attrs.get("body", "")
            if fmt not in ("2", "22", "11"):
                continue
            request_id = re.search(r'''["']?ReqId["']?\s*:\s*["']?(\d+)''', body, re.I)
            records.append({"source": str(file), "entry": attrs.get("name", ""),
                            "protocol": {"2": "tqlex", "22": "pbrpc", "11": "jsn"}[fmt],
                            "req_id": request_id[1] if request_id else "", "body": body})
    return records


def assignment(value):
    name, separator, content = value.partition("=")
    require(bool(separator) and bool(name), "参数应为 NAME=VALUE")
    return name, content


def parse_body(body, args, template=False):
    if template:
        body = re.sub(r"\$\$\$(STARTPOS|PAGEROWS)\$\s*\$\$",
                      lambda m: str(args.page if m[1].upper() == "STARTPOS" else args.page_size), body, flags=re.I)
        for name, value in map(assignment, args.set):
            body = body.replace(f"$${name}$$", value)
    unresolved = re.findall(r"\${2,}[^$]+\${2,}", body)
    require(not unresolved, "模板仍有未替换变量: " + ", ".join(dict.fromkeys(unresolved)))
    try:
        value = json.loads(body)
    except json.JSONDecodeError:
        if not template:
            raise
        value = ast.literal_eval(body)
    require(isinstance(value, (dict, list)) and (not isinstance(value, list) or bool(value)), "请求体必须为对象或非空数组")
    mapping = value[0] if isinstance(value, list) else value
    require(isinstance(mapping, dict), "请求数组第一项必须为对象")
    for name, content in map(assignment, args.param):
        try:
            parsed = json.loads(content)
        except json.JSONDecodeError:
            parsed = content
        key = next((key for key in mapping if str(key).lower() == name.lower()), name)
        mapping[key] = parsed
    json_bytes(value)  # Reject non-JSON Python literal values and non-finite numbers.
    return value


def cloud_spec(args):
    require(bool(args.body) != bool(args.config), "cloud 必须且只能指定 --body 或 --config")
    entry, module, protocol = args.entry, args.module, args.protocol
    if args.config:
        require(bool(args.req_id), "--config 查询须指定 --req-id；可先用 catalog --config 查看")
        matches = [record for record in config_records(args.config)
                   if record["req_id"] == args.req_id and record["protocol"] != "jsn"
                   and (not entry or record["entry"] == entry)
                   and (protocol == "auto" or protocol == record["protocol"])
                   and all(needle.lower() in record["body"].lower() for needle in args.contains)]
        unique = {(item["entry"], item["protocol"], item["body"]): item for item in matches}
        require(len(unique) == 1, f"模板匹配 {len(unique)} 种请求；用具体 XML 路径、--entry 或 --contains 缩小范围")
        record = next(iter(unique.values()))
        entry, protocol, body = record["entry"], record["protocol"], record["body"]
        if protocol == "pbrpc":
            match = re.fullmatch(r"\s*pb_rpc_req:\s*Moduledll\s*=\s*([^;]+);\s*ReqByte\s*=(.*)", body, re.I | re.S)
            require(bool(match), "PBRPC 模板不是 pb_rpc_req:Moduledll=...;ReqByte=... 格式")
            template_module, body = match.groups()
            module = module or template_module.strip()
        value = parse_body(body, args, template=True)
    else:
        require(not args.set, "--set 仅替换 XML 模板变量；直接 JSON 使用 --param")
        body = text_decode(sys.stdin.buffer.read()) if args.body == "-" else text_decode(Path(args.body[1:]).read_bytes()) if args.body.startswith("@") else args.body
        protocol = "pbrpc" if protocol == "auto" and module else "tqlex" if protocol == "auto" else protocol
        value = parse_body(body, args)
        if args.req_id:
            mapping = value[0] if isinstance(value, list) else value
            actual = next((str(v) for k, v in mapping.items() if k.lower() == "reqid"), "")
            require(actual == args.req_id, "--req-id 与 JSON 中 ReqId 不一致")
    require(bool(entry), "请指定实际服务名 --entry，例如 CWSearch.tzx_rcache")
    require(protocol != "pbrpc" or bool(module), "PBRPC 须指定 --module 或使用带模块名的模板")
    require(protocol != "tqlex" or not module, "TQLEX 不使用 --module；请选择 --protocol pbrpc")
    return entry, module, protocol, value


def cloud_call(args, entry, module, protocol, value, gateway=None):
    base = urllib.parse.urlsplit(gateway or args.gateway)
    query = [(key, val) for key, val in urllib.parse.parse_qsl(base.query) if key.lower() != "entry"]
    query.append(("Entry", entry))
    url = urllib.parse.urlunsplit(base._replace(query=urllib.parse.urlencode(query)))
    if args.dry_run:
        preview = {"transport": "http", "method": "POST", "url": url, "protocol": protocol,
                   "content_type": "application/octet-stream" if protocol == "pbrpc" else "application/json", "body": value}
        if protocol == "pbrpc":
            preview.update(module=module, first_request_hex=pbrpc_request(module, value).hex())
        return preview
    if protocol == "pbrpc":
        return query_pbrpc(url, module, value, args)
    return json_response(http_fetch(url, args, json_bytes(value)))


def run_cloud(args):
    return cloud_call(args, *cloud_spec(args))


def run_shortcut(args):
    if args.command == "holder":
        code = security(args.security)[1]
        body = {"Params": ["gdjcxq" if args.history else "gdjc", code, args.holder_id, args.variant, "1"]}
        return cloud_call(args, "CWServ.tdxf10_gg_gdyjcgmx", "", "tqlex", body,
                          args.gateway or "http://page1.tdx.com.cn:7615/TQLEX")
    if args.security:
        market, code = security(args.security)
        key = f"{'gg' if args.command == 'announcements' else 'ly'}:{market}_{code}"
    else:
        key = "ly:1_zxly"
    body = {"action": "get", "key": key}
    if args.command == "announcements":
        body.update(bin="1", qsid="tdx")
    return cloud_call(args, "CWSearch.tzx_rcache", "", "tqlex", body)


def run_catalog(args):
    if args.config:
        rows = config_records(args.config)
    else:
        rows = []
        for line in CLOUD_CATALOG_TEXT.strip().splitlines():
            protocol, ids, purpose, template = line.split("|")
            rows.extend({"protocol": protocol, "req_id": request_id, "purpose": purpose, "template": template}
                        for request_id in ids.split(","))
        rows.extend({"protocol": "tcp7727" if number >= 0x2300 else "tcp7709",
                     "command": f"0x{number:04X}", "purpose": purpose}
                    for number, purpose in TCP_CATALOG.items())
        rows.extend({"protocol": "jsn", "alias": alias, "path": "bi/" + path, "purpose": purpose}
                    for alias, (path, purpose) in JSN_CATALOG.items())
        rows.extend({"protocol": "http", "command": name, "purpose": purpose} for name, purpose in (
            ("announcements", "单票公告"), ("roadshows", "全市场或单票路演"),
            ("holder", "股东跨股票持仓或单票历史"), ("download", "专业财务、交易文件及清单")))
    return [row for row in rows if (args.protocol == "all" or row["protocol"] == args.protocol)
            and (not args.req_id or row.get("req_id") == args.req_id)
            and (not args.search or args.search.lower() in json.dumps(row, ensure_ascii=False).lower())]


HELP = """直接调用通达信上游的独立工具（Python 3.10+，仅标准库）。
HTTP 直接请求通达信 TQLEX/PBRPC 或 data.tdx.com.cn；TCP 直接连接 7709/7727。
无需启动本仓库服务，无需 pip install，复制本文件即可运行。

命令与输出：
  catalog        内置接口目录；也能检查客户端 XML 模板及完整请求体
  quote          最新行情 JSON；普通行情可选五档，扩展行情默认含五档
  kline          分钟/日/周/月 K 线 JSON（单页、未复权）
  trades         当日/历史成交明细 JSON（单页）
  securities     证券/合约目录 JSON，或查询数量
  timeline       港股、期货等扩展市场当日/历史分时 JSON
  cloud          TQLEX JSON 或 PBRPC 云查询；输入 JSON 或客户端 XML 模板
  announcements  单票公告 JSON
  roadshows      全市场或单票路演 JSON
  holder         股东跨股票持仓或单票历史 JSON
  jsn            7709 文件接口下载并解析 JSN；--raw 保存原字节
  download       下载 HTTPS 清单/ZIP/DAT，或 TCP zhb.zip
  raw            任意 TCP 命令；响应仅解帧/解压，输出 hex 或原始业务字节
  self-test      5 组离线协议契约检查，无需网络

边界与约定：
  普通市场必须显式指定：sz000001 / sh600000 / bj920001 / 0:000001。
  扩展市场用 --expansion 和市场号:合约代码（如 31:00700）。
  行情数量后缀 _hand 表示上游手数；K线 volume 保留协议数量；金额为元。
  扩展市场 volume 单位由合约决定；不统一换算为股。日期为交易所当地日期。
  TQLEX/JSN 保留上游 JSON 结构；不会补行情宿主列。PBRPC 自动拼接传输分片。
  业务分页由 --start/--count 或 JSON Page/PageSize 控制，不自动宣称抓取全量。
  raw 不做财务/竞价/排行等业务解码；ZIP/DAT 不解压、不解析财务字段。
  默认节点可能失效；可重复传 --host 按顺序重试，或读取 --connect-cfg。
  JSON 写 stdout，错误写 stderr；-o 原子写文件。退出码 0=成功、1=请求/解码失败、
  2=命令行格式错误、130=用户中断。通用选项放在子命令后。
  HTTP 默认遵循系统代理；--no-proxy 可禁用。TCP 总是直接连接。
"""
EXAMPLES = """常用示例（Bash；Windows 可用 --body @request.json 避免引号转义）：
  python tdx_api.py quote sz000001 sh600000
  python tdx_api.py quote sh600000 --depth --host 123.60.84.66:7709
  python tdx_api.py kline sz000001 --period day --count 20
  python tdx_api.py kline sh000001 --index --period 1m --count 240 -o bars.json
  python tdx_api.py quote 31:00700 --expansion
  python tdx_api.py trades sz000001 --date 20260907 --count 100
  python tdx_api.py announcements sz000001
  python tdx_api.py jsn themes
  python tdx_api.py jsn cgfxmx1/0000001.jsn -o holdings.json
  python tdx_api.py download tdxfin/gpcw.txt
  python tdx_api.py download tdxfin/gpcw20251231.zip -o gpcw20251231.zip
  python tdx_api.py cloud --entry CWSearch.tzx_rcache --body '{"action":"get","key":"gg:0_000001","bin":"1","qsid":"tdx"}'
  python tdx_api.py catalog --search 基金
  python tdx_api.py catalog --config /path/to/tdx --req-id 200340
  python tdx_api.py cloud --config /path/to/tdx/T0002/cloud_cfg/sszjtj.xml --req-id 200340 --dry-run
  python tdx_api.py raw 0x0004 --dry-run
  python tdx_api.py self-test

详细帮助：python tdx_api.py <命令> --help
全部帮助：python tdx_api.py --help-all
"""


def build_parser():
    formatter = argparse.RawDescriptionHelpFormatter
    parser = argparse.ArgumentParser(description=HELP, epilog=EXAMPLES, formatter_class=formatter)
    parser.add_argument("--version", action="version", version="%(prog)s " + VERSION)
    parser.add_argument("--help-all", action="store_true", help="输出全部子命令的详细帮助")
    commands = parser.add_subparsers(dest="command", metavar="COMMAND")
    subparsers = {}

    def command(name, description, examples=""):
        p = commands.add_parser(name, description=description, epilog=examples, formatter_class=formatter)
        p.add_argument("-o", "--output", help="输出文件（JSON；download/raw --binary/jsn --raw 为原始字节）")
        p.add_argument("--compact", action="store_true", help="JSON 输出不缩进")
        p.add_argument("--timeout", type=float, default=8, help="单次网络连接/读操作超时秒数，默认 8；不是整个任务的总时限")
        p.add_argument("--max-bytes", type=int, default=128 * 1024 * 1024, help="HTTP 响应/拼接文件/PBRPC 总字节上限，默认 134217728")
        p.add_argument("--no-proxy", action="store_true", help="HTTP 请求不使用系统代理")
        if name not in ("catalog", "self-test"):
            p.add_argument("--dry-run", action="store_true", help="只输出实际 URL/JSON/请求帧，不访问网络；分片展示首次请求")
        subparsers[name] = p
        return p

    def tcp(p, allow_expansion=True):
        p.add_argument("--host", action="append", metavar="HOST[:PORT]", help="上游 TCP 节点；可重复，依次失败重试；默认普通123.60.84.66:7709、扩展116.205.143.214:7727")
        p.add_argument("--connect-cfg", help="读取客户端 connect.cfg 的 HQHOST（最多前8个节点，仅普通行情）")
        if allow_expansion:
            p.add_argument("--expansion", action="store_true", help="使用扩展市场协议及默认端口 7727")
        else:
            p.set_defaults(expansion=False)

    def paging(p, count):
        p.add_argument("--start", type=int, default=0, help="上游分页偏移，默认 0；K线从最新端计数")
        p.add_argument("--count", type=int, default=count, help=f"请求一页的记录数，默认 {count}")

    p = command("catalog", "列出内置接口说明，或读取本地 XML 的 datasource。只读本地，不访问网络。\n内置云目录覆盖34个 TQLEX 和29个 PBRPC ReqId；目录项不是完整请求模板。")
    p.add_argument("--config", help="客户端安装目录、cloud_cfg 目录或单个 XML；输出 source/entry/protocol/req_id/body")
    p.add_argument("--protocol", choices=("all", "tqlex", "pbrpc", "tcp7709", "tcp7727", "jsn", "http"), default="all", help="按协议过滤")
    p.add_argument("--req-id", help="按请求号过滤")
    p.add_argument("--search", help="按名称、路径、请求号或请求体子串过滤")

    p = command("quote", "批量最新行情，返回 JSON 数组。普通代码最多80个；扩展市场每次一个合约。\n--depth 查询五档，含 bids/asks；原始扩展字节保留为 tail_hex。\n未返回的证券不会补零；time_raw/update_time_raw 保留上游整数。")
    p.add_argument("securities", nargs="+", help="显式市场及代码，如 sz000001 sh600000；扩展如 31:00700")
    p.add_argument("--depth", action="store_true", help="普通行情用 0x0547 查询五档，解 XOR 0x93")
    tcp(p)

    p = command("kline", "查询单页未复权 K 线，返回 JSON 数组：date/time/open/high/low/close/volume/amount。\n每页1..800条，返回顺序沿用上游（通常为时间升序）。\n普通指数需 --index 解析额外涨跌家数。扩展合约另含持仓与辅助价格，amount=null。")
    p.add_argument("security", help="显式市场及代码")
    p.add_argument("--period", choices=tuple(PERIODS), default="day", help="周期，默认 day；1m/5m/15m/30m/60m/day/week/month")
    p.add_argument("--index", action="store_true", help="普通行情指数响应带涨跌家数字段，如 sh000001")
    paging(p, 100)
    tcp(p)

    p = command("trades", "查询单页成交明细（1..1800条）。输出 date/price_base_raw/rows。\n普通行情含 price/volume_hand/order_count/side/status_raw；历史价格基数字段仅保留原值。\n扩展成交保留 nature_raw 和 open_interest_change，不推测不同市场的方向含义。")
    p.add_argument("security", help="显式市场及代码")
    p.add_argument("--date", type=date_value, help="历史交易日 YYYYMMDD 或 YYYY-MM-DD；省略则查询上游当前交易日")
    paging(p, 100)
    tcp(p)

    p = command("securities", "查询一页证券/合约目录。普通目录含市场、代码、名称、小数位、昨收等；\n扩展目录含市场、代码、名称、合约乘数等。普通每页1..1000条，扩展1..100条。")
    p.add_argument("--market", type=int, choices=(0, 1, 2), default=0, help="普通目录市场：0深圳、1上海、2北京，默认0；扩展目录不按此字段过滤")
    p.add_argument("--count-only", action="store_true", help="仅返回证券/合约总数")
    paging(p, 100)
    tcp(p)

    p = command("timeline", "扩展市场当日或历史分时，返回价格、均价、量、持仓及交易时段偏移。\n固定使用7727；普通市场分钟数据可使用 kline --period 1m。")
    p.add_argument("security", help="市场号:代码，如 31:00700")
    p.add_argument("--date", type=date_value, help="历史交易日 YYYYMMDD 或 YYYY-MM-DD")
    tcp(p, False)
    p.set_defaults(expansion=True)

    p = command("cloud", "直接向 TQLEX 网关 POST。--body 与 --config 二选一。\nTQLEX 返回上游 JSON；PBRPC 自动封 protobuf 并拼接传输分片后解析 JSON。\n只请求业务上的一页，分页字段由请求体控制。XML 支持UTF-8/GB18030/UTF-16。\n同 ReqId 有多种不同模板时须用 --contains/--entry/单个XML选择，不自动猜选。",
                "示例：\n  %(prog)s --entry HQServ.PBRPC_PEG --module mod_peg.dll --body @request.json --dry-run\n"
                "  %(prog)s --config /path/to/gp_gz_fsld.xml --req-id 200636 --set ID=123 --page-size 20 --dry-run\n"
                "  %(prog)s --config /path/to/cloud_cfg --req-id 200661 --contains \"'flag':1\" --param Page=0 --dry-run\n"
                "模板变量以实际 XML 为准；先用 catalog --config 检查完整 body。")
    p.add_argument("--entry", help="TQLEX Entry 服务名；模板模式可用作筛选")
    p.add_argument("--gateway", default=GATEWAY, help=f"网关基址，默认 {GATEWAY}，自动设置 Entry 查询参数")
    p.add_argument("--protocol", choices=("auto", "tqlex", "pbrpc"), default="auto", help="默认 auto：模板自带协议，直接JSON有 --module 时用PBRPC")
    p.add_argument("--module", help="PBRPC Moduledll，如 mod_peg.dll")
    p.add_argument("--body", help="完整 JSON 字符串、@文件路径，或 - 从 stdin 读取；必须是对象/非空数组")
    p.add_argument("--config", help="客户端安装目录、cloud_cfg目录或具体XML文件")
    p.add_argument("--req-id", help="模板请求号（--config时必填）；直接JSON模式用于校验请求号")
    p.add_argument("--contains", action="append", default=[], help="模板 body 必须包含的原文子串，可重复")
    p.add_argument("--set", action="append", default=[], metavar="NAME=TEXT", help="把模板 $$NAME$$ 替换为原文 TEXT；可重复。数值是否带引号取决于模板")
    p.add_argument("--param", action="append", default=[], metavar="KEY=JSON", help="覆盖解析后第一请求对象的顶层字段（键名不区分大小写）；值优先解析JSON，否则为字符串；代码可用 CODE='\"000001\"'")
    p.add_argument("--page", type=int, default=0, help="仅替换模板 $$$STARTPOS$ $$ 宏，默认0；已有固定 Page 用 --param Page=N 覆盖")
    p.add_argument("--page-size", type=int, default=20, help="仅替换模板 $$$PAGEROWS$ $$ 宏，默认20；固定字段用 --param 覆盖")
    p.add_argument("--max-rounds", type=int, default=128, help="PBRPC传输分片请求次数上限，默认128；未完成会报错")

    for name, description in (("announcements", "获取单票公告，输出上游 JSON（常含日期、标题、PDF及来源）。"),
                              ("roadshows", "获取全市场或单票路演，输出上游 JSON（标题、状态、时间、链接等）。"),
                              ("holder", "查询股东跨股票持仓；--history 查询该股东在单只股票的报告期持仓。\n股东ID/变体ID来自 cgfxmx2/<市场><代码>.jsn 等持仓资源中的详情链接。")):
        p = command(name, description)
        if name == "roadshows":
            p.add_argument("security", nargs="?", help="显式市场及代码；省略表示全市场最新路演")
        else:
            p.add_argument("security", help="显式市场及代码，如 sz000001")
        p.add_argument("--gateway", default=None if name == "holder" else GATEWAY, help="覆盖上游网关；holder默认page1.tdx.com.cn:7615，其余默认static.tdx.com.cn:7615")
        if name == "holder":
            p.add_argument("holder_id", help="上游股东ID，按原值传入")
            p.add_argument("--variant", default="", help="股东变体ID，默认空串")
            p.add_argument("--history", action="store_true", help="查询股东在指定股票的历史（gdjcxq）")

    p = command("jsn", "通过7709命令709/1721取得JSN，按服务端长度及MD5校验，解析为上游JSON。\n路径自动加 bi/；不是 HTTP URL。动态机构持仓路径市场号和代码之间没有下划线。",
                "示例：\n  %(prog)s themes\n  %(prog)s zttzty/主题ID.jsn\n  %(prog)s cgfxmx1/0000001.jsn\n  %(prog)s cgfxmx2/1600000.jsn --raw -o holders.jsn\n"
                "内置别名：" + ", ".join(JSN_CATALOG))
    p.add_argument("path", help="资源相对路径或内置别名；包含 bi/ 时也可直接传入")
    p.add_argument("--raw", action="store_true", help="保存文件原始字节，必须指定 -o；不解析JSON")
    tcp(p, False)

    p = command("download", "HTTP GET 下载专业财务/交易文件，或 --tcp 下载 zhb.zip。\n-o 保存原始字节；未指定 -o 时按 UTF-8/GB18030 文本输出 JSON（适用于清单）。\nZIP/DAT须指定-o；不自动解压、不解析财务或交易字段。\n--md5 可传清单中的摘要；TCP不经709的下载以短块/空块判断结束。",
                "示例：\n  %(prog)s tdxfin/gpcw.txt\n  %(prog)s tdxgp/gpszsh.txt\n  %(prog)s tdxgp/gpsz000001.dat -o gpsz000001.dat\n  %(prog)s zhb.zip --tcp -o zhb.zip")
    p.add_argument("path", help="data.tdx.com.cn 相对路径、完整HTTP(S) URL，或 --tcp 的远端路径")
    p.add_argument("--tcp", action="store_true", help="用7709的1721分块接口，如zhb.zip；不使用HTTP")
    p.add_argument("--md5", help="期望的文件MD5；不匹配则失败且不覆盖目标文件")
    tcp(p, False)

    p = command("raw", "发出指定TCP命令。自动握手、验证响应帧并按需zlib解压。\n输出 {bytes,hex}；--binary -o 保存解帧后的业务体。命令内部的XOR/业务字段不处理。\n--hex/--body-file 输入仅为业务体，不要包含12字节帧头。",
                "示例：\n  %(prog)s 0x0004\n  %(prog)s 0x044E --hex 0000d5233501 --dry-run\n  %(prog)s 0x23F0 --expansion\n  %(prog)s 0x0010 --body-file finance-request.bin --binary -o response.bin")
    p.add_argument("tcp_command", type=lambda value: int(value, 0), help="命令号，支持0x前缀或十进制；用catalog查看清单")
    group = p.add_mutually_exclusive_group()
    group.add_argument("--hex", help="业务体十六进制（允许空格）；省略表示空业务体")
    group.add_argument("--body-file", help="业务体二进制文件")
    p.add_argument("--binary", action="store_true", help="保存业务响应原始字节，必须指定 -o")
    tcp(p)
    command("self-test", "运行本文件自带的5组离线契约检查：TCP帧与解压、行情与K线、分时/成交/目录、\n云模板与JSON、PBRPC分片与文件完整性。无网络、不需要客户端安装。")
    return parser, subparsers


def run_self_test(args):
    import io
    import unittest
    from unittest import mock

    def options(**values):
        defaults = dict(timeout=1, expansion=False, max_bytes=100000, max_rounds=8,
                        count=10, start=0, date=None, index=False, period="day")
        return argparse.Namespace(**(defaults | values))

    def signed(value):
        magnitude = abs(value)
        data = bytearray([(magnitude & 63) | (64 if value < 0 else 0)])
        magnitude >>= 6
        while magnitude:
            data[-1] |= 128
            data.append(magnitude & 127)
            magnitude >>= 7
        return bytes(data)

    def variables(*values):
        return b"".join(signed(value) for value in values)

    class Contracts(unittest.TestCase):
        def test_tcp_frames_and_decompression(self):
            def response(message_id, command, body, compressed=False):
                wire = zlib.compress(body) if compressed else body
                return struct.pack("<4sBIBHHH", bytes.fromhex("b1cb7400"), 1, message_id,
                                   0, command, len(wire), len(body)) + wire

            class Stream:
                def __init__(self, data):
                    self.data, self.sent = data, []
                def sendall(self, data):
                    self.sent.append(data)
                def recv(self, count):
                    data, self.data = self.data[:min(count, 3)], self.data[min(count, 3):]
                    return data
                def close(self):
                    pass

            stream = Stream(response(0x01640801, 13, bytes(189)) + response(0x01640802, 4, b"abc" * 100, True))
            with mock.patch.object(socket, "create_connection", return_value=stream):
                with QuoteConnection("127.0.0.1:7709", options()) as connection:
                    self.assertEqual(connection.call(4), b"abc" * 100)
            self.assertEqual(stream.sent[0].hex(), "0c0108640101030003000d0001")
            self.assertEqual(stream.sent[1], request_frame(0x01640802, 4, b""))
            broken = Stream(response(7, 13, bytes(189)))
            with mock.patch.object(socket, "create_connection", return_value=broken), self.assertRaises(APIError):
                QuoteConnection("127.0.0.1", options())
            with self.assertRaises(APIError):
                Reader(b"\x80").varint()
            for number in (0, -1, 63, 64, -8192, 2**40):
                self.assertEqual(Reader(signed(number)).varint(), number)

        def test_quotes_and_klines(self):
            header = struct.pack("<B6sH", 0, b"000001", 1)
            prices = variables(1234, -34, 1, 10, -20)
            core = variables(100, 10) + struct.pack("<I", 0x41800000) + variables(40, 60, -2, 9)
            snapshot = header + prices + variables(93000, 0) + core
            quote = parse_quotes(b"\0\0\1\0" + snapshot, [(0, "000001")])[0]
            self.assertEqual((quote["price"], quote["pre_close"], quote["amount"]), (12.34, 12.0, 16.0))
            depth = header + prices + struct.pack("<I", 123) + variables(1) + core + variables(-1, 1, 5, 6) * 5
            quote = parse_quotes(bytes(byte ^ 0x93 for byte in b"\1\0" + depth), [(0, "000001")], True)[0]
            self.assertAlmostEqual(quote["bids"][0]["price"], 12.33)
            with self.assertRaises(APIError):
                parse_quotes(b"\0\0\1\0" + snapshot, [(1, "600000")])
            data = struct.pack("<HI", 1, 20260907) + variables(12000, 100, 200, -100) + struct.pack("<IIHH", 0x50000000, 0x41800000, 10, 20)
            row = parse_klines(data, options(index=True))[0]
            self.assertEqual((row["open"], row["close"], row["volume"], row["up_count"]), (12.0, 12.1, 2**33, 10))
            with self.assertRaises(APIError):
                parse_klines(data[:-1], options(index=True))
            expanded = bytes(18) + struct.pack("<HIffffIIf", 1, 20260907, 10, 12, 9, 11, 50, 60, 10.5)
            self.assertEqual(parse_klines(expanded, options(expansion=True))[0]["open_interest"], 50)
            _, body, _ = market_request(options(command="kline", security="sz000001"))
            self.assertEqual(len(body), 42)
            self.assertEqual(struct.unpack_from("<HHHH", body, 8), (4, 1, 0, 10))

        def test_trades_directories_and_timeline(self):
            data = struct.pack("<HH", 1, 571) + variables(1234, 5, 2, 0, 0)
            row = parse_trades(data, options(), "000001")["rows"][0]
            self.assertEqual((row["time"], row["price"], row["side"]), ("09:31", 12.34, "buy"))
            history = data[:2] + struct.pack("<f", 99.0) + data[2:]
            self.assertEqual(parse_trades(history, options(date=20260907), "000001")["rows"][0]["price"], 12.34)
            raw = bytearray(37)
            raw[:6], raw[8:12], raw[28] = b"000001", "平安".encode("gb18030"), 2
            struct.pack_into("<f", raw, 29, 12.34)
            self.assertEqual(parse_securities(b"\1\0" + raw, options(market=0))["rows"][0]["name"], "平安")
            timeline = bytes(10) + struct.pack("<HHffII", 1, 1500, 12.5, 12.3, 10, 50)
            point = parse_timeline(timeline, options())[0]
            self.assertEqual((point["time"], point["session_day_offset"]), ("01:00", 1))
            with self.assertRaises(APIError):
                security("000001")
            self.assertEqual(security("31:00700", True), (31, "00700"))

        def test_cloud_templates_and_json(self):
            args = options(no_proxy=True, dry_run=False, gateway="http://example.invalid/TQLEX?Entry=old")
            response = mock.MagicMock()
            response.__enter__.return_value.read.return_value = b'{"ErrorCode":0,"ResultSets":[]}'
            opener = mock.Mock()
            opener.open.return_value = response
            with mock.patch.object(urllib.request, "build_opener", return_value=opener):
                self.assertEqual(cloud_call(args, "CWSearch.tzx_rcache", "", "tqlex", {"action": "get"})["ErrorCode"], 0)
            request = opener.open.call_args.args[0]
            self.assertEqual(request.full_url, "http://example.invalid/TQLEX?Entry=CWSearch.tzx_rcache")
            self.assertEqual((request.get_method(), request.get_header("Content-type"), request.data),
                             ("POST", "application/json", b'{"action":"get"}'))
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "fixture.xml"
                body = "[{'ReqId':200636,'ID':'$$ID$$','Page':$$$STARTPOS$$$,'PageSize':$$$PAGEROWS$$$,'Filter':'x>1'}]"
                tag = '<datasource name="HQServ.example" reqformat="2" body="' + html.escape(body, quote=True) + '"/>'
                path.write_bytes(("<!--" + tag + "-->\n" + tag).encode("gb18030"))
                self.assertEqual(len(config_records(path)), 1)
                args = options(body=None, config=str(path), req_id="200636", entry=None, module=None,
                               protocol="auto", contains=[], set=["ID=001"], param=["PageSize=7"], page=2, page_size=20)
                entry, _, protocol, value = cloud_spec(args)
                self.assertEqual((entry, protocol), ("HQServ.example", "tqlex"))
                self.assertEqual(value[0], {"ReqId": 200636, "ID": "001", "Page": 2, "PageSize": 7, "Filter": "x>1"})
                path.write_text(tag + tag.replace("x&gt;1", "x&gt;2"), encoding="utf-8")
                with self.assertRaises(APIError):
                    cloud_spec(args)
            with self.assertRaises(APIError):
                json_response(b'{"ErrorCode":42,"ErrorInfo":"denied"}')
            self.assertEqual(json_response(b'[{"ErrorCode":0,"ResultSets":[]}]')[0]["ErrorCode"], 0)

        def test_pbrpc_and_file_integrity(self):
            content = b'{"ErrorCode":0,"ResultSets":[]}'
            offsets = []
            def fetch(url, args, body, content_type):
                fields = pb_fields(body)
                offset = pb_value(fields, 3, 0, 0)
                offsets.append(offset)
                chunk = content[offset:offset + 10]
                return (pb_field(1, pb_field(1, 0)) + pb_field(2, 123) + pb_field(3, offset) +
                        pb_field(4, len(content)) + pb_field(5, len(chunk)) + pb_field(6, chunk))
            result = query_pbrpc("http://example.invalid", "mod.dll", {"ReqId": 1}, options(), fetch)
            self.assertEqual(result["ResultSets"], [])
            self.assertEqual(offsets, [0, 10, 20, 30])
            with self.assertRaises(APIError):
                query_pbrpc("http://example.invalid", "mod.dll", {}, options(max_bytes=1), fetch)
            with self.assertRaises(APIError):
                query_pbrpc("http://example.invalid", "mod.dll", {}, options(),
                            lambda *unused: pb_field(3, 1) + pb_field(5, 1) + pb_field(6, b"x"))
            with self.assertRaises(APIError):
                pb_fields(b"\x32\x05xx")
            for good in (True, False):
                connection = mock.Mock()
                digest = hashlib.md5(content).hexdigest() if good else "0" * 32
                connection.call.side_effect = [struct.pack("<IB", len(content), 1) + digest.encode(),
                                               struct.pack("<I", len(content)) + content]
                if good:
                    self.assertEqual(tcp_file(connection, "bi/test.jsn", options()), content)
                    self.assertEqual(len(connection.call.call_args[0][1]), 308)
                else:
                    with self.assertRaises(APIError):
                        tcp_file(connection, "bi/test.jsn", options())

    stream = io.StringIO()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(Contracts)
    result = unittest.TextTestRunner(stream=stream, verbosity=0).run(suite)
    if not result.wasSuccessful():
        print(stream.getvalue(), file=sys.stderr)
        raise APIError("离线契约检查失败")
    return {"ok": True, "contracts": result.testsRun, "network": False}


def main(argv=None):
    parser, subparsers = build_parser()
    args = parser.parse_args(argv)
    if args.help_all:
        parser.print_help()
        for name, subparser in subparsers.items():
            print("\n" + "=" * 20 + " " + name + " " + "=" * 20)
            subparser.print_help()
        return 0
    if not args.command:
        parser.print_help()
        return 0
    try:
        require(math.isfinite(args.timeout) and args.timeout > 0, "--timeout 必须是正的有限秒数")
        bounded(args.max_bytes, 1, 2**31 - 1, "max-bytes")
        if getattr(args, "expansion", False):
            require(not args.connect_cfg, "--connect-cfg 的 HQHOST 仅用于普通行情，扩展行情请传 --host")
        if args.command == "cloud":
            bounded(args.max_rounds, 1, 10000, "max-rounds")
            bounded(args.page, 0, 2**31 - 1, "page")
            bounded(args.page_size, 1, 2**31 - 1, "page-size")
        if args.command == "raw" and args.binary or args.command == "jsn" and args.raw:
            require(args.output or args.dry_run, "保存原始字节须指定 -o 文件")
        if args.command == "download":
            require(not args.md5 or re.fullmatch(r"[0-9a-fA-F]{32}", args.md5), "--md5 须为32位十六进制")
            suffix = Path(urllib.parse.urlsplit(args.path).path).suffix.lower()
            require(suffix not in (".zip", ".dat") or args.output or args.dry_run, "ZIP/DAT 下载须指定 -o 文件")
        handlers = {"catalog": run_catalog, "cloud": run_cloud, "jsn": run_jsn, "download": run_download,
                    "announcements": run_shortcut, "roadshows": run_shortcut, "holder": run_shortcut,
                    "self-test": run_self_test}
        result = handlers.get(args.command, run_market)(args)
        if result is not None:
            emit(result, args)
        return 0
    except KeyboardInterrupt:
        print("已中断", file=sys.stderr)
        return 130
    except (APIError, OSError, ValueError, SyntaxError, TypeError, struct.error, zlib.error) as error:
        print(f"错误: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

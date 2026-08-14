"""Inspect TDX Level2 request bodies and captured response payloads.

The module implements structures recovered from TdxW.exe/tpbus.dll.  It is an
offline protocol tool: it deliberately does not implement login, entitlement,
or transport code.  Binary parsers are useful with payloads captured from a
locally authorised TDX session; the SDK JSON adapters accept the JSON passed to
``ProtocolSZSDK2TDX::SetAnsData``.
"""

from __future__ import annotations

import argparse
import base64
from collections import Counter
import csv
import io
import json
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence

import tdx_market_snapshot as snapshots


COMMAND_TRANSACTION = 1364
COMMAND_ORDER = 1374
DIRECT_REQUEST_SIZE = 26
DIRECT_RESPONSE_HEADER_SIZE = 6
DIRECT_MAX_RECORDS = 1500
DIRECT_PRICE_DIVISOR = 10_000

FUNC_INTRADAY = 4653
FUNC_TRANSACTION = 4655
FUNC_QUEUE = 4671
FUNC_DEPTH = 4680
SDK_DATA_TYPE_MULTI_LEVEL = 1803
SDK_DATA_TYPE_ORDER_QUEUE = 18031
SDK_MULTI_LEVEL_SIZE = 0x7D10
SDK_ORDER_QUEUE_SIZE = 0x4E2C
TPBUS_PUSH_QUOTE = 111
TPBUS_PUSH_BEST_QUEUES = 112
TPBUS_QUOTE_HEADER_SIZE = 99
TPBUS_QUOTE_LEVEL_SIZE = 20
TPBUS_QUEUE_HEADER_SIZE = 54

PROBE_DATA_TYPE_NAMES = {
    1801: "transaction",
    1802: "order",
    1803: "multi_level_depth",
    1804: "depth_arrays_50x50",
    1807: "quote_update",
    18031: "order_queue_at_price",
    18071: "quote_update_extended",
}
PROBE_PUSH_TYPE_NAMES = {
    111: "quote_with_depth_levels",
    112: "best_bid_ask_order_queues",
    113: "opaque_protobuf_113",
    114: "opaque_protobuf_114",
    116: "opaque_protobuf_116",
}

PROTOBUF_WIRE_TYPE_NAMES = {
    0: "varint",
    1: "fixed64",
    2: "length_delimited",
    5: "fixed32",
}


class Level2Error(RuntimeError):
    """Raised when a Level2 structure is invalid or incomplete."""


@dataclass(frozen=True)
class DirectRequest:
    kind: str
    command: int
    market_id: int
    code: str
    cursor: int
    count: int
    payload_hex: str


@dataclass(frozen=True)
class TransactionRecord:
    index: int
    time_offset_seconds: int
    time_seconds: int
    time_label: str
    price_raw: int
    price: float
    volume_raw: int
    order_count_raw: int
    status_raw: int
    side: str
    sequence_raw: int


@dataclass(frozen=True)
class OrderRecord:
    index: int
    time_offset_seconds: int
    time_seconds: int
    time_label: str
    price_raw: int
    price: float
    volume_raw: int
    order_type_raw: int
    order_type: str
    side_raw: int
    side: str
    action_raw: int
    action: str
    order_id_raw: int


@dataclass(frozen=True)
class DirectPage:
    kind: str
    count: int
    next_cursor: int
    consumed_bytes: int
    records: tuple[TransactionRecord | OrderRecord, ...]


@dataclass(frozen=True)
class SdkTransactionRecord:
    index: int
    time_seconds: int
    time_label: str
    price: float
    volume_raw: int
    status_raw: int
    side: str


@dataclass(frozen=True)
class SdkQueue:
    buy_count: int
    sell_count: int
    buy_quantities_hand: tuple[int, ...]
    sell_quantities_hand: tuple[int, ...]


@dataclass(frozen=True)
class SdkDepth:
    time_seconds: int
    time_label: str
    pre_close: float
    open: float
    high: float
    low: float
    last: float
    volume_raw: int
    amount: float
    open_interest: int
    buy_prices: tuple[float, ...]
    buy_volumes_raw: tuple[int, ...]
    sell_prices: tuple[float, ...]
    sell_volumes_raw: tuple[int, ...]


@dataclass(frozen=True)
class SdkBookLevel:
    index: int
    price: float
    volume_raw: int
    auxiliary_raw: int


@dataclass(frozen=True)
class SdkMultiLevelSnapshot:
    header_u32_0: int
    header_u32_1: int
    first_side_count: int
    second_side_count: int
    first_side_levels: tuple[SdkBookLevel, ...]
    second_side_levels: tuple[SdkBookLevel, ...]


@dataclass(frozen=True)
class SdkOrderQueueSnapshot:
    header_u32_0: int
    header_u32_1: int
    count: int
    quantities_raw: tuple[int, ...]


@dataclass(frozen=True)
class TpbusDepthLevel:
    index: int
    buy_price: float
    buy_volume_raw: int
    buy_seat_count: int
    sell_price: float
    sell_volume_raw: int
    sell_seat_count: int


@dataclass(frozen=True)
class TpbusQuotePush:
    market_id: int
    code: str
    depth_count: int
    hq_time_raw: int
    item_number: int
    close: float
    open: float
    high: float
    low: float
    last: float
    lead: float
    volume_raw: int
    rest_volume_raw: int
    amount_raw: float
    volume_in_stock_raw: int
    jjjz: float
    in_out_flag: int
    hktt_flag: int
    volume_unit: int
    ph_volume: float
    levels: tuple[TpbusDepthLevel, ...]


@dataclass(frozen=True)
class TpbusBestQueuesPush:
    market_id: int
    code: str
    refresh_number: int
    buy_price: float
    sell_price: float
    buy_count: int
    sell_count: int
    buy_quantities_raw: tuple[int, ...]
    sell_quantities_raw: tuple[int, ...]


def _read_protobuf_varint(payload: bytes, offset: int) -> tuple[int, int, str]:
    """Read one unsigned protobuf varint and retain its exact byte spelling."""

    start = offset
    value = 0
    for shift in range(0, 70, 7):
        if offset >= len(payload):
            raise Level2Error(f"protobuf varint 在偏移 {start} 处被截断")
        byte = payload[offset]
        offset += 1
        if shift == 63 and byte > 1:
            raise Level2Error(f"protobuf varint 在偏移 {start} 处超过 64 位")
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return value, offset, payload[start:offset].hex()
    raise Level2Error(f"protobuf varint 在偏移 {start} 处超过 10 字节")


def _protobuf_text_candidate(value: bytes) -> str | None:
    if not value:
        return None
    try:
        text = value.decode("utf-8")
    except UnicodeDecodeError:
        return None
    if not all(character.isprintable() or character in "\r\n\t" for character in text):
        return None
    return text


def inspect_protobuf_wire(
    payload: bytes,
    *,
    max_fields: int = 100,
    max_sample_bytes: int = 64,
) -> dict[str, Any]:
    """Inspect protobuf wire fields without inventing a message schema.

    The output deliberately does not assign business names or scalar types.
    Length-delimited values can be strings, bytes, packed scalars, or nested
    messages, so only their length and bounded byte/text samples are exposed.
    """

    if max_fields <= 0:
        raise Level2Error("max_fields 必须大于 0")
    if max_sample_bytes < 0:
        raise Level2Error("max_sample_bytes 不能为负数")
    fields: list[dict[str, Any]] = []
    offset = 0
    while offset < len(payload) and len(fields) < max_fields:
        field_offset = offset
        key, offset, key_hex = _read_protobuf_varint(payload, offset)
        field_number = key >> 3
        wire_type = key & 7
        if field_number == 0:
            raise Level2Error(f"protobuf 字段号 0 出现在偏移 {field_offset}")
        if wire_type not in PROTOBUF_WIRE_TYPE_NAMES:
            raise Level2Error(
                f"protobuf 偏移 {field_offset} 使用未支持的 wire type {wire_type}"
            )
        field: dict[str, Any] = {
            "index": len(fields),
            "offset": field_offset,
            "field_number": field_number,
            "wire_type": wire_type,
            "wire_type_name": PROTOBUF_WIRE_TYPE_NAMES[wire_type],
            "key_hex": key_hex,
        }
        if wire_type == 0:
            value, offset, value_hex = _read_protobuf_varint(payload, offset)
            field["value_unsigned"] = value
            field["value_hex"] = value_hex
        elif wire_type == 1:
            end = offset + 8
            if end > len(payload):
                raise Level2Error(f"protobuf fixed64 在偏移 {offset} 处被截断")
            field["value_unsigned"] = struct.unpack_from("<Q", payload, offset)[0]
            field["value_hex"] = payload[offset:end].hex()
            offset = end
        elif wire_type == 5:
            end = offset + 4
            if end > len(payload):
                raise Level2Error(f"protobuf fixed32 在偏移 {offset} 处被截断")
            field["value_unsigned"] = struct.unpack_from("<I", payload, offset)[0]
            field["value_hex"] = payload[offset:end].hex()
            offset = end
        else:
            length, offset, length_hex = _read_protobuf_varint(payload, offset)
            end = offset + length
            if end > len(payload):
                raise Level2Error(
                    f"protobuf length-delimited 字段在偏移 {field_offset} 声明 {length} 字节，"
                    f"实际只剩 {len(payload) - offset} 字节"
                )
            value = payload[offset:end]
            sample = value[:max_sample_bytes]
            field["length"] = length
            field["length_hex"] = length_hex
            field["sample_hex"] = sample.hex()
            field["sample_truncated"] = len(sample) < len(value)
            text = _protobuf_text_candidate(value)
            if text is not None and len(value) <= max_sample_bytes:
                field["text_candidate"] = text
            offset = end
        fields.append(field)
    return {
        "encoding": "protobuf_wire_unknown_schema",
        "size": len(payload),
        "field_count_reported": len(fields),
        "field_limit_reached": offset < len(payload),
        "next_offset": offset,
        "fields": fields,
        "interpretation_note": (
            "字段号和 wire type 可确认；业务字段名、signed/float/packed/nested 语义均未确认"
        ),
    }


def _u16(value: int, name: str, *, positive: bool = False) -> int:
    result = int(value)
    minimum = 1 if positive else 0
    if not minimum <= result <= 0xFFFF:
        raise Level2Error(f"{name} 必须为 {minimum}..65535")
    return result


def _u32(value: int, name: str) -> int:
    result = int(value)
    if not 0 <= result <= 0xFFFFFFFF:
        raise Level2Error(f"{name} 必须为 0..4294967295")
    return result


def _code6(value: str) -> str:
    code = str(value).strip()
    if len(code) != 6 or not code.isascii() or not code.isdigit():
        raise Level2Error(f"证券代码必须是 6 位 ASCII 数字：{value!r}")
    return code


def _fixed_code(value: str, width: int = 22) -> bytes:
    code = str(value).strip()
    try:
        encoded = code.encode("ascii")
    except UnicodeEncodeError as error:
        raise Level2Error(f"代码必须是 ASCII：{value!r}") from error
    if not encoded or len(encoded) >= width:
        raise Level2Error(f"代码长度必须为 1..{width - 1} 字节")
    return encoded + bytes(width - len(encoded))


def _decode_fixed_ascii(value: bytes, name: str) -> str:
    raw = value.partition(b"\0")[0]
    try:
        return raw.decode("ascii")
    except UnicodeDecodeError as error:
        raise Level2Error(f"{name} 不是 ASCII") from error


def _kind_command(kind: str) -> int:
    normalized = kind.strip().lower().replace("_", "-")
    if normalized in {"transaction", "trade", "tick"}:
        return COMMAND_TRANSACTION
    if normalized in {"order", "entrust"}:
        return COMMAND_ORDER
    raise Level2Error(f"未知 Level2 类型：{kind!r}")


def _command_kind(command: int) -> str:
    return {
        COMMAND_TRANSACTION: "transaction",
        COMMAND_ORDER: "order",
    }.get(command, f"command-{command}")


def build_direct_request(
    kind: str,
    market_id: int,
    code: str,
    *,
    cursor: int = 0,
    count: int = DIRECT_MAX_RECORDS,
) -> bytes:
    """Build the exact 26-byte TdxW direct L2 request (1364/1374)."""

    command = _kind_command(kind)
    market = _u16(market_id, "market_id")
    cursor = _u32(cursor, "cursor")
    count = _u16(count, "count", positive=True)
    if count > DIRECT_MAX_RECORDS:
        raise Level2Error(f"count 不得超过客户端上限 {DIRECT_MAX_RECORDS}")
    result = struct.pack(
        "<IIHHH6sIH",
        0,
        0x00100000,
        16,
        command,
        market,
        _code6(code).encode("ascii"),
        cursor,
        count,
    )
    assert len(result) == DIRECT_REQUEST_SIZE
    return result


def inspect_direct_request(payload: bytes) -> DirectRequest:
    if len(payload) != DIRECT_REQUEST_SIZE:
        raise Level2Error(
            f"直连 Level2 请求必须为 {DIRECT_REQUEST_SIZE} 字节，实际 {len(payload)}"
        )
    zero, flags, body_size, command, market, raw_code, cursor, count = (
        struct.unpack("<IIHHH6sIH", payload)
    )
    if zero != 0 or flags != 0x00100000 or body_size != 16:
        raise Level2Error(
            "请求头不匹配：应为 zero=0, flags=0x00100000, body_size=16"
        )
    if command not in {COMMAND_TRANSACTION, COMMAND_ORDER}:
        raise Level2Error(f"不支持的直连命令：{command}")
    try:
        code = raw_code.decode("ascii")
    except UnicodeDecodeError as error:
        raise Level2Error("证券代码不是 ASCII") from error
    return DirectRequest(
        kind=_command_kind(command),
        command=command,
        market_id=market,
        code=code,
        cursor=cursor,
        count=count,
        payload_hex=payload.hex(),
    )


def xor_payload(payload: bytes, key: int) -> bytes:
    key = int(key)
    if not 0 <= key <= 0xFF:
        raise Level2Error("xor_key 必须为 0..255")
    return bytes(value ^ key for value in payload)


def _time_label(seconds: int) -> str:
    if not 0 <= seconds < 24 * 60 * 60:
        return f"+{seconds}s"
    hour, remainder = divmod(seconds, 3600)
    minute, second = divmod(remainder, 60)
    return f"{hour:02d}:{minute:02d}:{second:02d}"


def _direct_time(value: int) -> tuple[int, str]:
    seconds = value + 6 * 3600
    return seconds, _time_label(seconds)


def _side(value: int) -> str:
    return {0: "buy", 1: "sell", 2: "neutral", -1: "unknown"}.get(
        value, f"status_{value}"
    )


def _ascii_field(value: int) -> str:
    if value == 0:
        return ""
    if 32 <= value <= 126:
        return chr(value)
    return f"0x{value:02x}"


def _consume_varint(payload: bytes, offset: int, record: int, field: str) -> tuple[int, int]:
    try:
        return snapshots.consume_varint(payload, offset)
    except snapshots.SnapshotError as error:
        raise Level2Error(f"第 {record + 1} 条记录的 {field} 不完整") from error


def parse_direct_payload(
    payload: bytes,
    kind: str,
    *,
    xor_key: int | None = None,
    allow_trailing: bool = False,
) -> DirectPage:
    """Decode a direct 1364/1374 response body after optional XOR."""

    command = _kind_command(kind)
    if xor_key is not None:
        payload = xor_payload(payload, xor_key)
    if len(payload) < DIRECT_RESPONSE_HEADER_SIZE:
        raise Level2Error("直连 Level2 响应少于 6 字节")
    count, next_cursor = struct.unpack_from("<HI", payload)
    if count & 0x8000:
        raise Level2Error(f"服务端返回错误计数：0x{count:04x}")
    if count > DIRECT_MAX_RECORDS:
        raise Level2Error(f"记录数 {count} 超过客户端上限 {DIRECT_MAX_RECORDS}")

    offset = DIRECT_RESPONSE_HEADER_SIZE
    price_acc = 0
    records: list[TransactionRecord | OrderRecord] = []
    for index in range(count):
        if offset + 2 > len(payload):
            raise Level2Error(f"第 {index + 1} 条记录缺少 2 字节时间")
        time_offset = struct.unpack_from("<H", payload, offset)[0]
        offset += 2
        time_seconds, time_label = _direct_time(time_offset)
        price_delta, offset = _consume_varint(payload, offset, index, "price_delta")
        volume, offset = _consume_varint(payload, offset, index, "volume")
        price_acc += price_delta
        if price_acc < 0:
            raise Level2Error(f"第 {index + 1} 条记录的累计价格为负：{price_acc}")

        if command == COMMAND_TRANSACTION:
            order_count, offset = _consume_varint(
                payload, offset, index, "order_count"
            )
            status, offset = _consume_varint(payload, offset, index, "status")
            sequence, offset = _consume_varint(payload, offset, index, "sequence")
            records.append(TransactionRecord(
                index=index,
                time_offset_seconds=time_offset,
                time_seconds=time_seconds,
                time_label=time_label,
                price_raw=price_acc,
                price=price_acc / DIRECT_PRICE_DIVISOR,
                volume_raw=volume,
                order_count_raw=order_count,
                status_raw=status,
                side=_side(status),
                sequence_raw=sequence,
            ))
        else:
            if offset + 3 > len(payload):
                raise Level2Error(f"第 {index + 1} 条委托缺少 3 个状态字节")
            order_type, side, action = payload[offset : offset + 3]
            offset += 3
            order_id, offset = _consume_varint(payload, offset, index, "order_id")
            records.append(OrderRecord(
                index=index,
                time_offset_seconds=time_offset,
                time_seconds=time_seconds,
                time_label=time_label,
                price_raw=price_acc,
                price=price_acc / DIRECT_PRICE_DIVISOR,
                volume_raw=volume,
                order_type_raw=order_type,
                order_type=_ascii_field(order_type),
                side_raw=side,
                side=_ascii_field(side),
                action_raw=action,
                action=_ascii_field(action),
                order_id_raw=order_id,
            ))
    if not allow_trailing and offset != len(payload):
        raise Level2Error(f"直连 Level2 响应残留 {len(payload) - offset} 字节")
    return DirectPage(
        kind=_command_kind(command),
        count=count,
        next_cursor=next_cursor,
        consumed_bytes=offset,
        records=tuple(records),
    )


def build_redirect_body_4653(
    market_id: int,
    code: str,
    *,
    has_attachinfo: bool = False,
    has_gzhgtime: bool = False,
) -> bytes:
    body = bytearray(40)
    struct.pack_into("<HH", body, 0, FUNC_INTRADAY, _u16(market_id, "market_id"))
    body[4:26] = _fixed_code(code)
    body[30] = int(bool(has_attachinfo))
    body[31] = int(bool(has_gzhgtime))
    return bytes(body)


def build_redirect_body_4655(
    market_id: int,
    code: str,
    *,
    wantnum: int = 80,
    has_attachinfo: bool = False,
) -> bytes:
    wantnum = _u16(wantnum, "wantnum", positive=True)
    if wantnum > 500:
        wantnum = 80
    body = bytearray(46)
    struct.pack_into("<HH", body, 0, FUNC_TRANSACTION, _u16(market_id, "market_id"))
    body[4:26] = _fixed_code(code)
    struct.pack_into("<H", body, 34, wantnum)
    body[36] = int(bool(has_attachinfo))
    return bytes(body)


def build_redirect_body_4680(market_id: int, code: str, *, depth: int = 10) -> bytes:
    if depth not in {5, 10}:
        raise Level2Error("depth 只能为 5 或 10")
    body = bytearray(37)
    struct.pack_into("<HH", body, 0, FUNC_DEPTH, _u16(market_id, "market_id"))
    body[4:26] = _fixed_code(code)
    body[26] = depth
    return bytes(body)


def fast_hq_subscribe_fields(
    market_id: int,
    code: str,
    lx: int,
    *,
    unsubscribe: bool = False,
) -> dict[str, int | str]:
    """Return the exact logical fields used by ``FastHQ.Subscribe``."""

    return {
        "CODE": _code6(code),
        "SC": _u16(market_id, "market_id"),
        "LX": int(lx),
        "PkgType": 0,
        "OperType": 0 if unsubscribe else 1,
        "PushType": 3,
        "BatchPush": 1,
    }


def _mapping(value: Any, name: str) -> Mapping[str, Any]:
    if isinstance(value, str):
        try:
            value = json.loads(value)
        except json.JSONDecodeError as error:
            raise Level2Error(f"{name} 不是有效 JSON 字符串") from error
    if not isinstance(value, Mapping):
        raise Level2Error(f"{name} 必须是 JSON 对象")
    return value


def _sdk_data(document: Mapping[str, Any]) -> Any:
    if "Data" not in document:
        raise Level2Error("SDK JSON 缺少 Data")
    value = document["Data"]
    if isinstance(value, str):
        try:
            return json.loads(value)
        except json.JSONDecodeError:
            return value
    return value


def _hhmmss(value: Any, *, centisecond_suffix: bool = False) -> int:
    try:
        raw = int(value)
    except (TypeError, ValueError) as error:
        raise Level2Error(f"时间字段无效：{value!r}") from error
    if centisecond_suffix:
        raw //= 100
    second = raw % 100
    minute = raw // 100 % 100
    hour = raw // 10000
    if hour > 23 or minute > 59 or second > 59:
        raise Level2Error(f"HHMMSS 时间无效：{raw}")
    return hour * 3600 + minute * 60 + second


def parse_sdk_transactions(document: Mapping[str, Any]) -> tuple[SdkTransactionRecord, ...]:
    data = _sdk_data(document)
    if not isinstance(data, list):
        raise Level2Error("4655 Data 必须是数组")
    records: list[SdkTransactionRecord] = []
    # tpbus iterates the SDK array from the last item to the first item.
    for index, raw in enumerate(reversed(data)):
        item = _mapping(raw, f"Data[{len(data) - index - 1}]")
        seconds = _hhmmss(item.get("transactionTime"), centisecond_suffix=True)
        status_text = str(item.get("transactionStatus", ""))
        status = 0 if status_text == "B" else 1 if status_text == "S" else -1
        try:
            price = float(item["transactionPrice"])
            volume = int(float(item["singleVolume"]))
        except (KeyError, TypeError, ValueError) as error:
            raise Level2Error("4655 价格或成交量字段无效") from error
        records.append(SdkTransactionRecord(
            index=index,
            time_seconds=seconds,
            time_label=_time_label(seconds),
            price=price,
            volume_raw=volume,
            status_raw=status,
            side=_side(status),
        ))
    return tuple(records)


def _quantity_array(side: Any, name: str, *, outer_index: int) -> list[int]:
    if not isinstance(side, list) or not side:
        return []
    selected_index = outer_index if outer_index >= 0 else len(side) + outer_index
    if not 0 <= selected_index < len(side):
        return []
    selected = _mapping(side[selected_index], f"{name}[{selected_index}]")
    values = selected.get("QUANTITY_", [])
    if not isinstance(values, list):
        raise Level2Error(f"{name}[0].QUANTITY_ 必须是数组")
    try:
        return [int(value) // 100 for value in values]
    except (TypeError, ValueError) as error:
        raise Level2Error(f"{name} 数量无效") from error


def parse_sdk_queue(document: Mapping[str, Any]) -> SdkQueue:
    data = _mapping(_sdk_data(document), "4671 Data")
    # The adapter selects buyList[0] and sellList[-1], then preserves each
    # selected QUANTITY_ array's order.
    buys = _quantity_array(data.get("buyList", []), "buyList", outer_index=0)
    sells = _quantity_array(data.get("sellList", []), "sellList", outer_index=-1)
    return SdkQueue(
        buy_count=len(buys),
        sell_count=len(sells),
        buy_quantities_hand=tuple(buys),
        sell_quantities_hand=tuple(sells),
    )


def _float_list(value: Any, name: str) -> tuple[float, ...]:
    if not isinstance(value, list):
        raise Level2Error(f"{name} 必须是数组")
    try:
        return tuple(float(item) for item in value)
    except (TypeError, ValueError) as error:
        raise Level2Error(f"{name} 包含无效数值") from error


def parse_sdk_depth(document: Mapping[str, Any], *, requested_depth: int = 10) -> SdkDepth:
    if requested_depth not in {5, 10}:
        raise Level2Error("requested_depth 只能为 5 或 10")
    data = _mapping(_sdk_data(document), "4680 Data")
    buy_prices = _float_list(data.get("buyPrices", []), "buyPrices")
    buy_volumes = _float_list(data.get("buyVolumes", []), "buyVolumes")
    sell_prices = _float_list(data.get("sellPrices", []), "sellPrices")
    sell_volumes = _float_list(data.get("sellVolumes", []), "sellVolumes")
    depth = min(requested_depth, max(len(buy_prices), len(sell_prices)))
    seconds = _hhmmss(str(data.get("datetime", ""))[-6:])

    def number(name: str) -> float:
        try:
            return float(data.get(name, 0))
        except (TypeError, ValueError) as error:
            raise Level2Error(f"{name} 无效") from error

    return SdkDepth(
        time_seconds=seconds,
        time_label=_time_label(seconds),
        pre_close=number("preClosePrice"),
        open=number("openPrice"),
        high=number("highPrice"),
        low=number("lowPrice"),
        last=number("lastPrice"),
        volume_raw=int(number("volume")),
        amount=number("amount"),
        open_interest=int(number("openInterest")),
        # The adapter writes buys in reverse index order and sells forward.
        buy_prices=tuple(reversed(buy_prices[:depth])),
        buy_volumes_raw=tuple(int(value * 1000) for value in reversed(buy_volumes[:depth])),
        sell_prices=tuple(sell_prices[:depth]),
        sell_volumes_raw=tuple(int(value * 1000) for value in sell_volumes[:depth]),
    )


def parse_sdk_payload(func_id: int, document: Mapping[str, Any]) -> Any:
    if func_id == FUNC_TRANSACTION:
        return parse_sdk_transactions(document)
    if func_id == FUNC_QUEUE:
        return parse_sdk_queue(document)
    if func_id == FUNC_DEPTH:
        return parse_sdk_depth(document)
    raise Level2Error(f"当前仅支持 SDK FuncID 4655/4671/4680，收到 {func_id}")


def _bounded_side_limits(first_count: int, second_count: int, limit: int) -> tuple[int, int]:
    first_limit = min(first_count, (limit + 1) // 2)
    second_limit = min(second_count, limit - first_limit)
    remaining = limit - first_limit - second_limit
    add_first = min(first_count - first_limit, remaining)
    first_limit += add_first
    remaining -= add_first
    second_limit += min(second_count - second_limit, remaining)
    return first_limit, second_limit


def parse_sdk_callback_binary(data_type: int, payload: bytes, *, limit: int = 20) -> Any:
    """Parse the fixed SDK callback bodies used by data types 1803/18031."""

    if limit < 0:
        raise Level2Error("limit 不能为负数")
    if data_type == SDK_DATA_TYPE_MULTI_LEVEL:
        if len(payload) < SDK_MULTI_LEVEL_SIZE:
            raise Level2Error(
                f"1803 回调至少需要 {SDK_MULTI_LEVEL_SIZE} 字节，收到 {len(payload)}"
            )
        header_0, header_1, first_count, second_count = struct.unpack_from("<IIII", payload)
        if first_count > 1000 or second_count > 1000:
            raise Level2Error(
                f"1803 档位数越界：first={first_count}, second={second_count}"
            )
        first_limit, second_limit = _bounded_side_limits(first_count, second_count, limit)

        def levels(
            count: int, price_offset: int, volume_offset: int, auxiliary_offset: int
        ) -> tuple[SdkBookLevel, ...]:
            return tuple(
                SdkBookLevel(
                    index=index,
                    price=struct.unpack_from("<d", payload, price_offset + index * 8)[0],
                    volume_raw=struct.unpack_from("<I", payload, volume_offset + index * 4)[0],
                    auxiliary_raw=struct.unpack_from("<H", payload, auxiliary_offset + index * 4)[0],
                )
                for index in range(count)
            )

        return SdkMultiLevelSnapshot(
            header_u32_0=header_0,
            header_u32_1=header_1,
            first_side_count=first_count,
            second_side_count=second_count,
            first_side_levels=levels(first_limit, 16, 8016, 12016),
            second_side_levels=levels(second_limit, 16016, 24016, 28016),
        )
    if data_type == SDK_DATA_TYPE_ORDER_QUEUE:
        if len(payload) < SDK_ORDER_QUEUE_SIZE:
            raise Level2Error(
                f"18031 回调至少需要 {SDK_ORDER_QUEUE_SIZE} 字节，收到 {len(payload)}"
            )
        header_0, header_1, count = struct.unpack_from("<III", payload)
        if count > 5000:
            raise Level2Error(f"18031 委托数越界：{count}")
        selected = min(count, limit)
        quantities = struct.unpack_from(f"<{selected}I", payload, 12) if selected else ()
        return SdkOrderQueueSnapshot(
            header_u32_0=header_0,
            header_u32_1=header_1,
            count=count,
            quantities_raw=tuple(quantities),
        )
    raise Level2Error(f"当前仅支持 SDK 回调类型 1803/18031，收到 {data_type}")


def parse_tpbus_push_binary(push_type: int, payload: bytes, *, limit: int = 20) -> Any:
    """Parse the raw tpbus FastHQ push bodies used by types 111/112."""

    if limit < 0:
        raise Level2Error("limit 不能为负数")
    if push_type == TPBUS_PUSH_QUOTE:
        if len(payload) < TPBUS_QUOTE_HEADER_SIZE:
            raise Level2Error(
                f"111 推送至少需要 {TPBUS_QUOTE_HEADER_SIZE} 字节，收到 {len(payload)}"
            )
        market_id = struct.unpack_from("<H", payload, 0)[0]
        code = _decode_fixed_ascii(payload[2:24], "111 证券代码")
        depth_count = struct.unpack_from("<b", payload, 24)[0]
        if depth_count < 0:
            raise Level2Error(f"111 档位数为负数：{depth_count}")
        required_size = TPBUS_QUOTE_HEADER_SIZE + depth_count * TPBUS_QUOTE_LEVEL_SIZE
        if len(payload) < required_size:
            raise Level2Error(
                f"111 推送按 {depth_count} 档至少需要 {required_size} 字节，收到 {len(payload)}"
            )
        selected = min(depth_count, limit)
        levels = []
        for index in range(selected):
            offset = TPBUS_QUOTE_HEADER_SIZE + index * TPBUS_QUOTE_LEVEL_SIZE
            buy_price, buy_volume, buy_seats, sell_price, sell_volume, sell_seats = (
                struct.unpack_from("<fIHfIH", payload, offset)
            )
            levels.append(
                TpbusDepthLevel(
                    index=index,
                    buy_price=buy_price,
                    buy_volume_raw=buy_volume,
                    buy_seat_count=buy_seats,
                    sell_price=sell_price,
                    sell_volume_raw=sell_volume,
                    sell_seat_count=sell_seats,
                )
            )
        return TpbusQuotePush(
            market_id=market_id,
            code=code,
            depth_count=depth_count,
            hq_time_raw=struct.unpack_from("<I", payload, 35)[0],
            item_number=struct.unpack_from("<I", payload, 39)[0],
            close=struct.unpack_from("<f", payload, 43)[0],
            open=struct.unpack_from("<f", payload, 47)[0],
            high=struct.unpack_from("<f", payload, 51)[0],
            low=struct.unpack_from("<f", payload, 55)[0],
            last=struct.unpack_from("<f", payload, 59)[0],
            lead=struct.unpack_from("<f", payload, 63)[0],
            volume_raw=struct.unpack_from("<I", payload, 67)[0],
            rest_volume_raw=struct.unpack_from("<I", payload, 71)[0],
            amount_raw=struct.unpack_from("<f", payload, 75)[0],
            volume_in_stock_raw=struct.unpack_from("<I", payload, 79)[0],
            jjjz=struct.unpack_from("<f", payload, 83)[0],
            in_out_flag=payload[87],
            hktt_flag=payload[88],
            volume_unit=payload[89],
            ph_volume=struct.unpack_from("<f", payload, 90)[0],
            levels=tuple(levels),
        )
    if push_type == TPBUS_PUSH_BEST_QUEUES:
        if len(payload) < TPBUS_QUEUE_HEADER_SIZE:
            raise Level2Error(
                f"112 推送至少需要 {TPBUS_QUEUE_HEADER_SIZE} 字节，收到 {len(payload)}"
            )
        market_id = struct.unpack_from("<H", payload, 0)[0]
        code = _decode_fixed_ascii(payload[2:24], "112 证券代码")
        refresh_number, buy_price, sell_price, buy_count, sell_count = struct.unpack_from(
            "<IffII", payload, 24
        )
        total_count = buy_count + sell_count
        required_size = TPBUS_QUEUE_HEADER_SIZE + total_count * 4
        if len(payload) < required_size:
            raise Level2Error(
                f"112 推送按 {buy_count}+{sell_count} 笔至少需要 {required_size} 字节，"
                f"收到 {len(payload)}"
            )
        buy_limit, sell_limit = _bounded_side_limits(buy_count, sell_count, limit)
        buy_values = (
            struct.unpack_from(f"<{buy_limit}I", payload, TPBUS_QUEUE_HEADER_SIZE)
            if buy_limit
            else ()
        )
        sell_offset = TPBUS_QUEUE_HEADER_SIZE + buy_count * 4
        sell_values = (
            struct.unpack_from(f"<{sell_limit}I", payload, sell_offset)
            if sell_limit
            else ()
        )
        return TpbusBestQueuesPush(
            market_id=market_id,
            code=code,
            refresh_number=refresh_number,
            buy_price=buy_price,
            sell_price=sell_price,
            buy_count=buy_count,
            sell_count=sell_count,
            buy_quantities_raw=tuple(buy_values),
            sell_quantities_raw=tuple(sell_values),
        )
    raise Level2Error(f"当前仅支持 tpbus 推送类型 111/112，收到 {push_type}")


def _load_blob(path: Path, encoding: str) -> bytes:
    raw = path.read_bytes()
    if encoding == "raw":
        return raw
    text = raw.decode("ascii").strip()
    try:
        return bytes.fromhex(text) if encoding == "hex" else base64.b64decode(text)
    except (ValueError, base64.binascii.Error) as error:
        raise Level2Error(f"无法按 {encoding} 解码 {path}") from error


def _jsonable(value: Any) -> Any:
    if hasattr(value, "__dataclass_fields__"):
        return {key: _jsonable(item) for key, item in asdict(value).items()}
    if isinstance(value, tuple):
        return [_jsonable(item) for item in value]
    if isinstance(value, list):
        return [_jsonable(item) for item in value]
    if isinstance(value, dict):
        return {key: _jsonable(item) for key, item in value.items()}
    return value


def _render_csv(records: Iterable[Any]) -> str:
    rows = [_jsonable(record) for record in records]
    if not rows:
        return ""
    output = io.StringIO()
    writer = csv.DictWriter(output, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue()


def _probe_integer(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _probe_label(event: Mapping[str, Any]) -> str:
    value = event.get("capture_label")
    return str(value) if value not in {None, ""} else "unlabeled"


def _probe_code(event: Mapping[str, Any]) -> str | None:
    value = event.get("code")
    if not isinstance(value, str) or not value:
        return None
    return value


def summarize_probe_events(events: Iterable[Mapping[str, Any]]) -> dict[str, Any]:
    """Summarize passive probe events without claiming ambiguous LX mappings."""

    event_counts: Counter[str] = Counter()
    label_states: dict[str, dict[str, Any]] = {}
    push_lx_evidence: dict[tuple[str, str, int, int], dict[str, Any]] = {}
    total = 0

    def label_state(label: str) -> dict[str, Any]:
        return label_states.setdefault(
            label,
            {
                "event_count": 0,
                "event_counts": Counter(),
                "sdk_data_types": Counter(),
                "protobuf_push_types": Counter(),
                "codes": {},
            },
        )

    def code_state(state: dict[str, Any], code: str) -> dict[str, Any]:
        return state["codes"].setdefault(
            code,
            {
                "event_count": 0,
                "sdk_data_types": Counter(),
                "fast_hq_lx": {},
                "push_types": Counter(),
                "decoded_records": Counter(),
            },
        )

    for event in events:
        if not isinstance(event, Mapping):
            continue
        total += 1
        event_name = str(event.get("event") or "unknown")
        event_counts[event_name] += 1
        label = _probe_label(event)
        state = label_state(label)
        state["event_count"] += 1
        state["event_counts"][event_name] += 1
        code = _probe_code(event)
        code_data = code_state(state, code) if code is not None else None
        if code_data is not None:
            code_data["event_count"] += 1

        data_type = _probe_integer(event.get("data_type"))
        if data_type is not None:
            state["sdk_data_types"][data_type] += 1
            if code_data is not None:
                code_data["sdk_data_types"][data_type] += 1

        if event_name == "fasthq-subscribe" and code_data is not None:
            lx = _probe_integer(event.get("lx"))
            if lx is not None:
                lx_state = code_data["fast_hq_lx"].setdefault(
                    lx, {"subscribe": 0, "unsubscribe": 0, "other": 0}
                )
                operation = str(event.get("operation") or "other")
                lx_state[operation if operation in lx_state else "other"] += 1

        if event_name == "fasthq-push" and code_data is not None:
            push_type = _probe_integer(event.get("push_type"))
            if push_type is not None:
                code_data["push_types"][push_type] += 1
                recent = event.get("recent_subscriptions")
                candidates: dict[int, int] = {}
                if isinstance(recent, list):
                    for item in recent:
                        if not isinstance(item, Mapping):
                            continue
                        lx = _probe_integer(item.get("lx"))
                        age = _probe_integer(item.get("age_ms"))
                        if lx is None:
                            continue
                        normalized_age = max(age or 0, 0)
                        candidates[lx] = min(candidates.get(lx, normalized_age), normalized_age)
                for lx, age in candidates.items():
                    evidence = push_lx_evidence.setdefault(
                        (label, code, push_type, lx),
                        {
                            "unambiguous_events": 0,
                            "ambiguous_events": 0,
                            "minimum_age_ms": age,
                        },
                    )
                    evidence[
                        "unambiguous_events" if len(candidates) == 1 else "ambiguous_events"
                    ] += 1
                    evidence["minimum_age_ms"] = min(evidence["minimum_age_ms"], age)

        if event_name == "fasthq-protobuf-push":
            push_type = _probe_integer(event.get("push_type"))
            if push_type is not None:
                state["protobuf_push_types"][push_type] += 1

        if event_name in {"decoded-transactions", "decoded-orders"} and code_data is not None:
            count = _probe_integer(event.get("count"))
            code_data["decoded_records"][event_name] += max(count or 0, 0)

    rendered_labels: dict[str, Any] = {}
    label_lx_mappings: list[dict[str, Any]] = []
    lx_data_type_mappings: list[dict[str, Any]] = []
    for label in sorted(label_states):
        state = label_states[label]
        rendered_codes: dict[str, Any] = {}
        for code in sorted(state["codes"]):
            item = state["codes"][code]
            lx_values = sorted(
                lx for lx, counts in item["fast_hq_lx"].items() if counts["subscribe"] > 0
            )
            data_types = sorted(item["sdk_data_types"])
            for lx in lx_values:
                label_lx_mappings.append(
                    {
                        "capture_label": label,
                        "code": code,
                        "lx": lx,
                        "subscribe_events": item["fast_hq_lx"][lx]["subscribe"],
                        "confidence": (
                            "contextual" if label != "unlabeled" and len(lx_values) == 1
                            else "ambiguous"
                        ),
                        "evidence": "same user-supplied capture label",
                    }
                )
            for lx in lx_values:
                for data_type in data_types:
                    lx_data_type_mappings.append(
                        {
                            "capture_label": label,
                            "code": code,
                            "lx": lx,
                            "data_type": data_type,
                            "data_type_name": PROBE_DATA_TYPE_NAMES.get(data_type, "unknown"),
                            "confidence": (
                                "contextual"
                                if label != "unlabeled"
                                and len(lx_values) == 1
                                and len(data_types) == 1
                                else "ambiguous"
                            ),
                            "evidence": "co-occurrence within one capture label",
                        }
                    )
            rendered_codes[code] = {
                "event_count": item["event_count"],
                "sdk_data_types": [
                    {
                        "data_type": value,
                        "name": PROBE_DATA_TYPE_NAMES.get(value, "unknown"),
                        "events": item["sdk_data_types"][value],
                    }
                    for value in data_types
                ],
                "fast_hq_lx": [
                    {"lx": value, **item["fast_hq_lx"][value]}
                    for value in sorted(item["fast_hq_lx"])
                ],
                "push_types": [
                    {
                        "push_type": value,
                        "name": PROBE_PUSH_TYPE_NAMES.get(value, "unknown"),
                        "events": item["push_types"][value],
                    }
                    for value in sorted(item["push_types"])
                ],
                "decoded_records": dict(sorted(item["decoded_records"].items())),
            }
        rendered_labels[label] = {
            "event_count": state["event_count"],
            "event_counts": dict(sorted(state["event_counts"].items())),
            "sdk_data_types": [
                {
                    "data_type": value,
                    "name": PROBE_DATA_TYPE_NAMES.get(value, "unknown"),
                    "events": state["sdk_data_types"][value],
                }
                for value in sorted(state["sdk_data_types"])
            ],
            "protobuf_push_types": [
                {
                    "push_type": value,
                    "name": PROBE_PUSH_TYPE_NAMES.get(value, "unknown"),
                    "events": state["protobuf_push_types"][value],
                }
                for value in sorted(state["protobuf_push_types"])
            ],
            "codes": rendered_codes,
        }

    lx_push_mappings = []
    for (label, code, push_type, lx), evidence in sorted(push_lx_evidence.items()):
        unambiguous = evidence["unambiguous_events"]
        confidence = "high" if unambiguous >= 3 else "medium" if unambiguous else "low"
        lx_push_mappings.append(
            {
                "capture_label": label,
                "code": code,
                "lx": lx,
                "push_type": push_type,
                "push_type_name": PROBE_PUSH_TYPE_NAMES.get(push_type, "unknown"),
                **evidence,
                "confidence": confidence,
                "evidence": "same-code recent subscription attached by the probe",
            }
        )

    return {
        "schema": "tdx-level2-probe-summary/v1",
        "events_total": total,
        "event_counts": dict(sorted(event_counts.items())),
        "labels": rendered_labels,
        "mappings": {
            "label_to_lx": label_lx_mappings,
            "lx_to_push_type": lx_push_mappings,
            "lx_to_sdk_data_type": lx_data_type_mappings,
        },
        "confidence_note": (
            "contextual/ambiguous mappings are hypotheses from capture labels or "
            "co-occurrence; only repeated single-candidate push evidence is rated high"
        ),
    }


def summarize_probe_files(paths: Sequence[Path]) -> dict[str, Any]:
    events: list[Mapping[str, Any]] = []
    invalid_lines: list[dict[str, Any]] = []
    for path in paths:
        with path.open("r", encoding="utf-8") as source:
            for line_number, line in enumerate(source, 1):
                if not line.strip():
                    continue
                try:
                    value = json.loads(line)
                except json.JSONDecodeError as error:
                    invalid_lines.append(
                        {"file": str(path), "line": line_number, "error": str(error)}
                    )
                    continue
                if not isinstance(value, Mapping):
                    invalid_lines.append(
                        {"file": str(path), "line": line_number, "error": "JSON value is not an object"}
                    )
                    continue
                events.append(value)
    summary = summarize_probe_events(events)
    summary["files"] = [str(path) for path in paths]
    summary["invalid_line_count"] = len(invalid_lines)
    summary["invalid_lines"] = invalid_lines[:100]
    return summary


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    direct = subparsers.add_parser("build-direct", help="生成 1364/1374 的 26 字节请求")
    direct.add_argument("--kind", required=True, choices=("transaction", "order"))
    direct.add_argument("--market", required=True, type=int)
    direct.add_argument("--code", required=True)
    direct.add_argument("--cursor", type=int, default=0)
    direct.add_argument("--count", type=int, default=DIRECT_MAX_RECORDS)
    direct.add_argument("--encoding", choices=("hex", "base64", "raw"), default="hex")
    direct.add_argument("--output", type=Path)

    parse = subparsers.add_parser("parse-direct", help="解析已捕获的 1364/1374 响应体")
    parse.add_argument("--kind", required=True, choices=("transaction", "order"))
    parse.add_argument("--input", required=True, type=Path)
    parse.add_argument("--encoding", choices=("raw", "hex", "base64"), default="raw")
    parse.add_argument("--xor-key", type=lambda value: int(value, 0))
    parse.add_argument("--format", choices=("json", "csv"), default="json")
    parse.add_argument("--output", type=Path)

    sdk = subparsers.add_parser("parse-sdk-json", help="解析 SDK 的 4655/4671/4680 JSON")
    sdk.add_argument("--func-id", required=True, type=int, choices=(4655, 4671, 4680))
    sdk.add_argument("--input", required=True, type=Path)
    sdk.add_argument("--output", type=Path)

    probe = subparsers.add_parser("summarize-probe", help="汇总被动探针 JSONL 并生成 LX 候选映射")
    probe.add_argument("--input", required=True, type=Path, action="append")
    probe.add_argument("--output", type=Path)

    binary = subparsers.add_parser("parse-sdk-binary", help="解析 SDK 1803/18031 固定回调体")
    binary.add_argument("--data-type", required=True, type=int, choices=(1803, 18031))
    binary.add_argument("--input", required=True, type=Path)
    binary.add_argument("--encoding", choices=("raw", "hex", "base64"), default="raw")
    binary.add_argument("--limit", type=int, default=20)
    binary.add_argument("--output", type=Path)

    tp_push = subparsers.add_parser("parse-tpbus-push", help="解析 tpbus 111/112 原始推送体")
    tp_push.add_argument("--push-type", required=True, type=int, choices=(111, 112))
    tp_push.add_argument("--input", required=True, type=Path)
    tp_push.add_argument("--encoding", choices=("raw", "hex", "base64"), default="raw")
    tp_push.add_argument("--limit", type=int, default=20)
    tp_push.add_argument("--output", type=Path)

    protobuf = subparsers.add_parser(
        "inspect-protobuf", help="无描述符检查 protobuf 字段号和 wire type"
    )
    protobuf.add_argument("--input", required=True, type=Path)
    protobuf.add_argument("--encoding", choices=("raw", "hex", "base64"), default="raw")
    protobuf.add_argument("--max-fields", type=int, default=100)
    protobuf.add_argument("--sample-bytes", type=int, default=64)
    protobuf.add_argument("--output", type=Path)
    return parser


def _write_or_print(data: bytes | str, output: Path | None) -> None:
    if output is not None:
        output.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(data, bytes):
            output.write_bytes(data)
        else:
            output.write_text(data, encoding="utf-8")
        return
    if isinstance(data, bytes):
        sys.stdout.buffer.write(data)
    else:
        print(data)


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "build-direct":
            payload = build_direct_request(
                args.kind, args.market, args.code, cursor=args.cursor, count=args.count
            )
            if args.encoding == "raw":
                rendered: bytes | str = payload
            elif args.encoding == "base64":
                rendered = base64.b64encode(payload).decode("ascii")
            else:
                rendered = payload.hex()
            _write_or_print(rendered, args.output)
            return 0
        if args.command == "parse-direct":
            page = parse_direct_payload(
                _load_blob(args.input, args.encoding),
                args.kind,
                xor_key=args.xor_key,
            )
            rendered = (
                _render_csv(page.records)
                if args.format == "csv"
                else json.dumps(_jsonable(page), ensure_ascii=False, indent=2)
            )
            _write_or_print(rendered, args.output)
            return 0
        if args.command == "parse-sdk-json":
            document = json.loads(args.input.read_text(encoding="utf-8"))
            result = parse_sdk_payload(args.func_id, _mapping(document, "SDK JSON"))
            _write_or_print(json.dumps(_jsonable(result), ensure_ascii=False, indent=2), args.output)
            return 0
        if args.command == "summarize-probe":
            result = summarize_probe_files(args.input)
            _write_or_print(json.dumps(result, ensure_ascii=False, indent=2), args.output)
            return 0
        if args.command == "parse-sdk-binary":
            result = parse_sdk_callback_binary(
                args.data_type,
                _load_blob(args.input, args.encoding),
                limit=args.limit,
            )
            _write_or_print(json.dumps(_jsonable(result), ensure_ascii=False, indent=2), args.output)
            return 0
        if args.command == "parse-tpbus-push":
            result = parse_tpbus_push_binary(
                args.push_type,
                _load_blob(args.input, args.encoding),
                limit=args.limit,
            )
            _write_or_print(json.dumps(_jsonable(result), ensure_ascii=False, indent=2), args.output)
            return 0
        if args.command == "inspect-protobuf":
            result = inspect_protobuf_wire(
                _load_blob(args.input, args.encoding),
                max_fields=args.max_fields,
                max_sample_bytes=args.sample_bytes,
            )
            _write_or_print(json.dumps(result, ensure_ascii=False, indent=2), args.output)
            return 0
        raise Level2Error(f"未知命令：{args.command}")
    except (Level2Error, OSError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

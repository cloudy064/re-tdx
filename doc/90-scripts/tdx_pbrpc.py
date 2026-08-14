#!/usr/bin/env python3
"""Query TDX reqformat=22/PBRPC services without launching TdxW."""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Sequence

import inventory_tdx_cloud_features as cloud_inventory
import update_tdx_blocks as updater


DEFAULT_BASE_URL = "http://static.tdx.com.cn:7615/TQLEX"


class PBRPCError(RuntimeError):
    """Raised when a PBRPC request or response is invalid."""


@dataclass(frozen=True)
class RPCResponse:
    code: int
    message: str
    rpc_id: int
    start_pos: int
    total_len: int
    ret_byte_num: int
    ret_byte: bytes


@dataclass(frozen=True)
class RequestSpec:
    entry: str
    module: str
    request: dict[str, object]
    source_file: str = ""


Transport = Callable[[str, bytes, float], bytes]


def encode_varint(value: int) -> bytes:
    if value < 0:
        raise ValueError("varint value must be non-negative")
    result = bytearray()
    while value >= 0x80:
        result.append((value & 0x7F) | 0x80)
        value >>= 7
    result.append(value)
    return bytes(result)


def encode_varint_field(number: int, value: int) -> bytes:
    return encode_varint(number << 3) + encode_varint(value)


def encode_bytes_field(number: int, value: str | bytes) -> bytes:
    data = value if isinstance(value, bytes) else value.encode("utf-8")
    return (
        encode_varint((number << 3) | 2)
        + encode_varint(len(data))
        + data
    )


def build_request(
    module: str,
    request_json: str,
    *,
    rpc_id: int = 0,
    start_pos: int = 0,
    charset: str = "1",
    target: int = 0,
    sso_token: str = "",
) -> bytes:
    # protocol_mp.proto:
    # PBPublicReqHead Head=1; int32 RpcID=2; int32 StartPos=3;
    # string Moduledll=4; string ReqByte=5.
    head = encode_bytes_field(1, charset)
    if sso_token:
        head += encode_bytes_field(4, sso_token)
    if target:
        head += encode_varint_field(5, target)

    result = encode_bytes_field(1, head)
    if rpc_id:
        result += encode_varint_field(2, rpc_id)
    if start_pos:
        result += encode_varint_field(3, start_pos)
    result += encode_bytes_field(4, module)
    result += encode_bytes_field(5, request_json)
    return result


def decode_varint(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while True:
        if offset >= len(data) or shift >= 70:
            raise PBRPCError("truncated or invalid protobuf varint")
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if byte < 0x80:
            return value, offset
        shift += 7


def decode_fields(data: bytes) -> list[tuple[int, int, int | bytes]]:
    fields: list[tuple[int, int, int | bytes]] = []
    offset = 0
    while offset < len(data):
        tag, offset = decode_varint(data, offset)
        number = tag >> 3
        wire_type = tag & 7
        if not number:
            raise PBRPCError("invalid protobuf field number 0")
        if wire_type == 0:
            value, offset = decode_varint(data, offset)
        elif wire_type == 2:
            length, offset = decode_varint(data, offset)
            end = offset + length
            if end > len(data):
                raise PBRPCError("truncated protobuf length-delimited field")
            value = data[offset:end]
            offset = end
        else:
            raise PBRPCError(f"unsupported protobuf wire type {wire_type}")
        fields.append((number, wire_type, value))
    return fields


def _field_map(data: bytes) -> dict[int, int | bytes]:
    return {number: value for number, _wire, value in decode_fields(data)}


def decode_int32(value: int | bytes) -> int:
    if not isinstance(value, int):
        raise PBRPCError("invalid protobuf int32 field")
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def parse_response(data: bytes) -> RPCResponse:
    fields = _field_map(data)
    head_data = fields.get(1, b"")
    if not isinstance(head_data, bytes):
        raise PBRPCError("invalid PBPublicAnsHead")
    head = _field_map(head_data) if head_data else {}
    message_data = head.get(2, b"")
    message = (
        message_data.decode("utf-8", "replace")
        if isinstance(message_data, bytes)
        else ""
    )
    ret_byte = fields.get(6, b"")
    if not isinstance(ret_byte, bytes):
        raise PBRPCError("invalid RetByte")
    return RPCResponse(
        code=decode_int32(head.get(1, 0)),
        message=message,
        rpc_id=decode_int32(fields.get(2, 0)),
        start_pos=decode_int32(fields.get(3, 0)),
        total_len=decode_int32(fields.get(4, 0)),
        ret_byte_num=decode_int32(fields.get(5, 0)),
        ret_byte=ret_byte,
    )


def http_transport(url: str, payload: bytes, timeout: float) -> bytes:
    request = urllib.request.Request(
        url,
        data=payload,
        headers={
            "Accept": "application/octet-stream",
            "Content-Type": "application/octet-stream",
            "User-Agent": "Mozilla/5.0",
        },
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read()


def query_pbrpc(
    entry: str,
    module: str,
    request: dict[str, object],
    *,
    base_url: str = DEFAULT_BASE_URL,
    timeout: float = 15.0,
    max_rounds: int = 32,
    retry_delay: float = 0.15,
    charset: str = "1",
    target: int = 0,
    sso_token: str = "",
    transport: Transport = http_transport,
    verbose: bool = False,
) -> bytes:
    request_json = json.dumps(
        request,
        ensure_ascii=False,
        separators=(",", ":"),
    )
    url = f"{base_url}?{urllib.parse.urlencode({'Entry': entry})}"
    rpc_id = 0
    start_pos = 0
    chunks: list[bytes] = []
    idle_rounds = 0

    for round_index in range(max_rounds):
        payload = build_request(
            module,
            request_json,
            rpc_id=rpc_id,
            start_pos=start_pos,
            charset=charset,
            target=target,
            sso_token=sso_token,
        )
        response = parse_response(transport(url, payload, timeout))
        if response.code:
            raise PBRPCError(
                f"server returned code {response.code}: {response.message}"
            )
        if response.rpc_id <= 0:
            raise PBRPCError(
                f"server rejected request with RpcID {response.rpc_id}"
            )
        if rpc_id and response.rpc_id != rpc_id:
            raise PBRPCError(
                f"RpcID changed from {rpc_id} to {response.rpc_id}"
            )
        rpc_id = response.rpc_id
        if response.ret_byte_num != len(response.ret_byte):
            raise PBRPCError(
                "RetByteNum does not match the received RetByte length"
            )
        if response.ret_byte:
            if response.start_pos != start_pos:
                raise PBRPCError(
                    f"unexpected StartPos {response.start_pos}, "
                    f"expected {start_pos}"
                )
            chunks.append(response.ret_byte)
            start_pos += len(response.ret_byte)
            idle_rounds = 0
        else:
            idle_rounds += 1
        if verbose:
            print(
                f"round={round_index + 1} rpc_id={rpc_id} "
                f"start={response.start_pos} total={response.total_len} "
                f"ret={len(response.ret_byte)}",
                file=sys.stderr,
            )
        if response.total_len and start_pos >= response.total_len:
            break
        if response.ret_byte and not response.total_len:
            break
        if idle_rounds >= 3:
            raise PBRPCError("server returned no data for three rounds")
        if retry_delay:
            time.sleep(retry_delay)
    else:
        raise PBRPCError(f"response did not complete in {max_rounds} rounds")

    result = b"".join(chunks)
    if response.total_len and len(result) != response.total_len:
        raise PBRPCError(
            f"assembled {len(result)} bytes, expected {response.total_len}"
        )
    return result


def parse_assignments(values: Sequence[str]) -> dict[str, str]:
    result: dict[str, str] = {}
    for value in values:
        name, separator, item = value.partition("=")
        if not separator or not name:
            raise PBRPCError(f"expected NAME=VALUE, got {value!r}")
        result[name] = item
    return result


def parse_descriptor(body: str) -> tuple[str, str]:
    prefix = "pb_rpc_req:"
    if not body.startswith(prefix):
        raise PBRPCError("datasource body is not a pb_rpc_req descriptor")
    marker = ";ReqByte="
    head, separator, request_json = body[len(prefix) :].partition(marker)
    if not separator:
        raise PBRPCError("pb_rpc_req descriptor has no ReqByte")
    module = ""
    for assignment in head.split(";"):
        name, equal, value = assignment.partition("=")
        if equal and name.strip().casefold() == "moduledll":
            module = value.strip()
            break
    if not module:
        raise PBRPCError("pb_rpc_req descriptor has no Moduledll")
    return module, request_json.strip()


def find_config_spec(
    root: Path,
    request_id: str,
    *,
    entry: str = "",
    source_file: str = "",
    body_contains: Sequence[str] = (),
    replacements: dict[str, str] | None = None,
) -> RequestSpec:
    replacements = replacements or {}
    body_needles = tuple(value.casefold() for value in body_contains)
    candidates = [
        record
        for record in cloud_inventory.inventory(root)
        if record.request_format == "22"
        and record.request_id == request_id
        and (not entry or record.datasource_name == entry)
        and (
            not source_file
            or record.source_file.casefold() == source_file.casefold()
        )
        and all(needle in record.body.casefold() for needle in body_needles)
    ]
    if not candidates:
        qualifier = (
            " containing " + ", ".join(repr(value) for value in body_contains)
            if body_contains
            else ""
        )
        raise PBRPCError(
            f"no reqformat=22 config found for ReqId {request_id}{qualifier}"
        )
    candidates.sort(
        key=lambda record: (
            len(record.placeholders.split(","))
            if record.placeholders
            else 0,
            record.source_file.casefold(),
        )
    )
    record = candidates[0]
    body = record.body
    for name, value in replacements.items():
        body = body.replace(f"$${name}$$", value)
    unresolved = cloud_inventory.PLACEHOLDER_RE.findall(body)
    if unresolved:
        raise PBRPCError(
            "unresolved config placeholders: "
            + ", ".join(dict.fromkeys(unresolved))
        )
    module, request_json = parse_descriptor(body)
    try:
        request = json.loads(request_json)
    except json.JSONDecodeError as error:
        raise PBRPCError(f"invalid ReqByte JSON: {error}") from error
    if not isinstance(request, dict):
        raise PBRPCError("ReqByte JSON must be an object")
    return RequestSpec(
        entry=record.datasource_name,
        module=module,
        request=request,
        source_file=record.source_file,
    )


def list_configs(root: Path) -> str:
    rows: dict[tuple[str, str, str], set[str]] = {}
    for record in cloud_inventory.inventory(root):
        if record.request_format != "22" or not record.request_id:
            continue
        key = (record.request_id, record.datasource_name, record.module)
        rows.setdefault(key, set()).add(record.source_file)
    lines = ["ReqId\tEntry\tModule\tFiles"]
    for (request_id, entry, module), files in sorted(
        rows.items(),
        key=lambda item: (int(item[0][0]), item[0][1].casefold()),
    ):
        lines.append(
            f"{request_id}\t{entry}\t{module}\t{','.join(sorted(files))}"
        )
    return "\n".join(lines)


def decode_result(data: bytes) -> object:
    for encoding in ("utf-8", "gb18030"):
        try:
            # Several PBRPC modules append a C-string terminator to otherwise
            # valid JSON.  Keep ordinary whitespace intact, but discard only
            # trailing NUL bytes before handing the payload to json.loads().
            text = data.decode(encoding).rstrip("\x00")
            break
        except UnicodeDecodeError:
            continue
    else:
        return {"encoding": "hex", "data": data.hex()}
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return text


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "调用通达信 reqformat=22/PBRPC 服务。可按客户端 ReqId 自动读取"
            " cloud_cfg，也可显式指定 Entry、模块和请求 JSON。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument("--list", action="store_true", help="列出 PBRPC 配置")
    parser.add_argument("--req-id", help="ReqId；默认从 cloud_cfg 自动选择配置")
    parser.add_argument("--entry", help="逻辑服务 Entry")
    parser.add_argument("--module", help="服务端模块 DLL 名称")
    parser.add_argument("--source-file", help="限定 cloud_cfg XML 文件名")
    parser.add_argument(
        "--body-contains",
        action="append",
        default=[],
        metavar="TEXT",
        help="仅选择请求模板正文包含 TEXT 的配置；可重复",
    )
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="替换 XML 中的 $$NAME$$；可重复",
    )
    parser.add_argument(
        "--param",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="新增或覆盖 ReqByte JSON 字段；值按字符串处理",
    )
    parser.add_argument(
        "--request-json",
        help="显式 ReqByte JSON 对象；使用该参数时需同时指定 Entry/Module",
    )
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--max-rounds", type=int, default=32)
    parser.add_argument("--retry-delay", type=float, default=0.15)
    parser.add_argument("--output", type=Path, help="保存 UTF-8 JSON")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        if args.list:
            print(list_configs(root))
            return 0

        replacements = parse_assignments(args.set)
        overrides = parse_assignments(args.param)
        if args.request_json:
            if not args.entry or not args.module:
                raise PBRPCError(
                    "--request-json requires both --entry and --module"
                )
            request = json.loads(args.request_json)
            if not isinstance(request, dict):
                raise PBRPCError("--request-json must contain a JSON object")
            spec = RequestSpec(args.entry, args.module, request)
        else:
            if not args.req_id:
                raise PBRPCError("provide --req-id, --request-json, or --list")
            spec = find_config_spec(
                root,
                args.req_id,
                entry=args.entry or "",
                source_file=args.source_file or "",
                body_contains=args.body_contains,
                replacements=replacements,
            )
            if args.module:
                spec = RequestSpec(
                    spec.entry,
                    args.module,
                    spec.request,
                    spec.source_file,
                )
        request = dict(spec.request)
        request.update(overrides)
        if args.req_id:
            request["ReqId"] = args.req_id

        if args.verbose:
            print(
                f"config={spec.source_file or '<explicit>'} "
                f"entry={spec.entry} module={spec.module}",
                file=sys.stderr,
            )
        raw = query_pbrpc(
            spec.entry,
            spec.module,
            request,
            base_url=args.base_url,
            timeout=args.timeout,
            max_rounds=args.max_rounds,
            retry_delay=args.retry_delay,
            verbose=args.verbose,
        )
        rendered = json.dumps(
            decode_result(raw),
            ensure_ascii=False,
            indent=2,
        )
        if args.output:
            output = args.output.expanduser().resolve()
            updater.atomic_write_text(output, rendered + "\n", "utf-8")
            print(f"已保存：{output}", file=sys.stderr)
        else:
            print(rendered)
    except (
        OSError,
        UnicodeError,
        ValueError,
        json.JSONDecodeError,
        urllib.error.URLError,
        updater.UpdateError,
        PBRPCError,
    ) as error:
        print(f"PBRPC 请求失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

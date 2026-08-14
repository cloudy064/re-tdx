#!/usr/bin/env python3
"""Query TDX reqformat=2/TQLEX JSON services without launching TdxW."""

from __future__ import annotations

import argparse
import ast
import copy
import json
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Sequence

import inventory_tdx_cloud_features as cloud_inventory
import tdx_pbrpc as pbrpc
import update_tdx_blocks as updater


DEFAULT_BASE_URL = pbrpc.DEFAULT_BASE_URL
PAGING_MACRO_RE = re.compile(
    r"\$\$\$(STARTPOS|PAGEROWS)\$\s*\$\$",
    re.I,
)


class TQLEXError(RuntimeError):
    """Raised when a TQLEX JSON request or response is invalid."""


@dataclass(frozen=True)
class RequestSpec:
    entry: str
    request: list[object] | dict[str, object]
    source_file: str = ""


Transport = Callable[[str, bytes, float], bytes]


def parse_config_body(
    body: str,
    *,
    replacements: dict[str, str] | None = None,
    page: int = 0,
    page_size: int = 20,
) -> list[object] | dict[str, object]:
    replacements = replacements or {}

    def replace_paging(match: re.Match[str]) -> str:
        name = match.group(1).casefold()
        return str(page if name == "startpos" else page_size)

    rendered = PAGING_MACRO_RE.sub(replace_paging, body)
    for name, value in replacements.items():
        rendered = rendered.replace(f"$${name}$$", value)
    unresolved = cloud_inventory.PLACEHOLDER_RE.findall(rendered)
    if unresolved:
        raise TQLEXError(
            "unresolved config placeholders: "
            + ", ".join(dict.fromkeys(unresolved))
        )

    try:
        value = json.loads(rendered)
    except json.JSONDecodeError:
        try:
            value = ast.literal_eval(rendered)
        except (SyntaxError, ValueError) as error:
            raise TQLEXError(f"invalid request JSON: {error}") from error
    if not isinstance(value, (list, dict)):
        raise TQLEXError("request body must be a JSON array or object")
    if isinstance(value, list) and not value:
        raise TQLEXError("request array must not be empty")
    return value


def request_mapping(
    request: list[object] | dict[str, object],
) -> dict[str, object]:
    value: object = request[0] if isinstance(request, list) else request
    if not isinstance(value, dict):
        raise TQLEXError("first request item must be a JSON object")
    return value


def request_key(
    mapping: dict[str, object],
    name: str,
) -> str | None:
    folded = name.casefold()
    return next(
        (
            key
            for key in mapping
            if isinstance(key, str) and key.casefold() == folded
        ),
        None,
    )


def get_request_value(
    mapping: dict[str, object],
    name: str,
    default: object = None,
) -> object:
    key = request_key(mapping, name)
    return mapping[key] if key is not None else default


def set_request_value(
    mapping: dict[str, object],
    name: str,
    value: object,
) -> None:
    mapping[request_key(mapping, name) or name] = value


def find_config_spec(
    root: Path,
    request_id: str,
    *,
    entry: str = "",
    source_file: str = "",
    body_contains: Sequence[str] = (),
    replacements: dict[str, str] | None = None,
    page: int = 0,
    page_size: int = 20,
) -> RequestSpec:
    body_needles = tuple(value.casefold() for value in body_contains)
    candidates = [
        record
        for record in cloud_inventory.inventory(root)
        if record.request_format == "2"
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
        raise TQLEXError(
            f"no reqformat=2 config found for ReqId {request_id}{qualifier}"
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
    request = parse_config_body(
        record.body,
        replacements=replacements,
        page=page,
        page_size=page_size,
    )
    return RequestSpec(
        entry=record.datasource_name,
        request=request,
        source_file=record.source_file,
    )


def list_configs(root: Path) -> str:
    rows: dict[tuple[str, str], set[str]] = {}
    for record in cloud_inventory.inventory(root):
        if record.request_format != "2" or not record.request_id:
            continue
        key = (record.request_id, record.datasource_name)
        rows.setdefault(key, set()).add(record.source_file)
    lines = ["ReqId\tEntry\tFiles"]
    for (request_id, entry), files in sorted(
        rows.items(),
        key=lambda item: (int(item[0][0]), item[0][1].casefold()),
    ):
        lines.append(
            f"{request_id}\t{entry}\t{','.join(sorted(files))}"
        )
    return "\n".join(lines)


def http_transport(url: str, payload: bytes, timeout: float) -> bytes:
    request = urllib.request.Request(
        url,
        data=payload,
        headers={
            "Accept": "application/json",
            "Content-Type": "application/json",
            "User-Agent": "Mozilla/5.0",
        },
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read()


def query_tqlex(
    entry: str,
    request: list[object] | dict[str, object],
    *,
    base_url: str = DEFAULT_BASE_URL,
    timeout: float = 15.0,
    transport: Transport = http_transport,
) -> dict[str, object]:
    url = f"{base_url}?{urllib.parse.urlencode({'Entry': entry})}"
    payload = json.dumps(
        request,
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")
    result = pbrpc.decode_result(transport(url, payload, timeout))
    if not isinstance(result, dict):
        raise TQLEXError(
            f"server response must be a JSON object, got {type(result).__name__}"
        )
    if result.get("error"):
        raise TQLEXError(f"server returned error: {result['error']}")
    error_code = result.get("ErrorCode", 0)
    try:
        numeric_error = int(error_code)
    except (TypeError, ValueError) as error:
        raise TQLEXError(f"invalid ErrorCode {error_code!r}") from error
    if numeric_error:
        raise TQLEXError(
            f"server returned ErrorCode {numeric_error}: "
            f"{result.get('ErrorInfo', '')}"
        )
    return result


def result_set_rows(response: dict[str, object]) -> int:
    result_sets = response.get("ResultSets", [])
    if not isinstance(result_sets, list):
        raise TQLEXError("ResultSets must be an array")
    counts: list[int] = []
    for result_set in result_sets:
        if not isinstance(result_set, dict):
            raise TQLEXError("ResultSets items must be objects")
        content = result_set.get("Content", [])
        if not isinstance(content, list):
            raise TQLEXError("ResultSet Content must be an array")
        counts.append(len(content))
    return max(counts, default=0)


def merge_pages(
    pages: Sequence[dict[str, object]],
    *,
    paged_result_sets: set[int] | None = None,
) -> dict[str, object]:
    if not pages:
        raise TQLEXError("cannot merge an empty response list")
    merged = copy.deepcopy(pages[0])
    merged_sets = merged.get("ResultSets", [])
    if not isinstance(merged_sets, list):
        raise TQLEXError("ResultSets must be an array")
    for page in pages[1:]:
        page_sets = page.get("ResultSets", [])
        if not isinstance(page_sets, list) or len(page_sets) != len(merged_sets):
            raise TQLEXError("result-set count changed while paging")
        for index, (target, source) in enumerate(zip(merged_sets, page_sets)):
            if not isinstance(target, dict) or not isinstance(source, dict):
                raise TQLEXError("ResultSets items must be objects")
            if target.get("ColDes", []) != source.get("ColDes", []):
                raise TQLEXError("result-set columns changed while paging")
            if (
                paged_result_sets is not None
                and index not in paged_result_sets
            ):
                continue
            target_content = target.setdefault("Content", [])
            source_content = source.get("Content", [])
            if not isinstance(target_content, list) or not isinstance(
                source_content,
                list,
            ):
                raise TQLEXError("ResultSet Content must be an array")
            target_content.extend(copy.deepcopy(source_content))
            target["RowNum"] = len(target_content)
    return merged


def query_all_pages(
    entry: str,
    request: list[object] | dict[str, object],
    *,
    start_page: int = 0,
    page_size: int = 20,
    max_pages: int = 100,
    base_url: str = DEFAULT_BASE_URL,
    timeout: float = 15.0,
    transport: Transport = http_transport,
    verbose: bool = False,
) -> dict[str, object]:
    if start_page < 0:
        raise TQLEXError("start page must be non-negative")
    if page_size <= 0:
        raise TQLEXError("page size must be positive")
    if max_pages <= 0:
        raise TQLEXError("max pages must be positive")
    pages: list[dict[str, object]] = []
    signatures: set[str] = set()
    paged_result_sets: set[int] | None = None
    for offset in range(max_pages):
        page_number = start_page + offset
        current = copy.deepcopy(request)
        mapping = request_mapping(current)
        set_request_value(mapping, "Page", str(page_number))
        set_request_value(mapping, "PageSize", str(page_size))
        response = query_tqlex(
            entry,
            current,
            base_url=base_url,
            timeout=timeout,
            transport=transport,
        )
        rows = result_set_rows(response)
        if paged_result_sets is None:
            result_sets = response.get("ResultSets", [])
            if not isinstance(result_sets, list):
                raise TQLEXError("ResultSets must be an array")
            paged_result_sets = {
                index
                for index, result_set in enumerate(result_sets)
                if isinstance(result_set, dict)
                and isinstance(result_set.get("Content", []), list)
                and len(result_set.get("Content", [])) == page_size
            }
        signature = json.dumps(
            [
                result_set.get("Content", [])
                for result_set in response.get("ResultSets", [])
                if isinstance(result_set, dict)
            ],
            ensure_ascii=False,
            separators=(",", ":"),
        )
        if rows and signature in signatures:
            raise TQLEXError("server repeated a page; refusing an infinite loop")
        signatures.add(signature)
        pages.append(response)
        if verbose:
            print(f"page={page_number} rows={rows}", file=sys.stderr)
        if rows < page_size:
            return merge_pages(
                pages,
                paged_result_sets=paged_result_sets,
            )
    raise TQLEXError(f"response did not finish in {max_pages} pages")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "调用通达信 reqformat=2/TQLEX JSON 服务。可按 ReqId 自动读取"
            " cloud_cfg，也可显式指定 Entry 和请求 JSON。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument("--list", action="store_true", help="列出 JSON 配置")
    parser.add_argument("--req-id", help="ReqId；默认从 cloud_cfg 选择配置")
    parser.add_argument("--entry", help="逻辑服务 Entry")
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
        help="新增或覆盖请求对象字段；值按字符串处理",
    )
    parser.add_argument(
        "--request-json",
        help="显式 JSON 数组或对象；使用时必须同时指定 Entry",
    )
    parser.add_argument("--page", type=int, help="覆盖 Page；默认使用模板")
    parser.add_argument(
        "--page-size",
        type=int,
        help="覆盖 PageSize；模板分页宏默认使用 20",
    )
    parser.add_argument(
        "--all-pages",
        action="store_true",
        help="从 Page=0 开始请求，直到返回行数小于 PageSize",
    )
    parser.add_argument("--max-pages", type=int, default=100)
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--timeout", type=float, default=15.0)
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

        replacements = pbrpc.parse_assignments(args.set)
        overrides = pbrpc.parse_assignments(args.param)
        macro_page = args.page if args.page is not None else 0
        macro_page_size = (
            args.page_size if args.page_size is not None else 20
        )
        if args.request_json:
            if not args.entry:
                raise TQLEXError("--request-json requires --entry")
            request = json.loads(args.request_json)
            if not isinstance(request, (list, dict)):
                raise TQLEXError("--request-json must be an array or object")
            spec = RequestSpec(args.entry, request)
        else:
            if not args.req_id:
                raise TQLEXError(
                    "provide --req-id, --request-json, or --list"
                )
            spec = find_config_spec(
                root,
                args.req_id,
                entry=args.entry or "",
                source_file=args.source_file or "",
                body_contains=args.body_contains,
                replacements=replacements,
                page=macro_page,
                page_size=macro_page_size,
            )

        request = copy.deepcopy(spec.request)
        mapping = request_mapping(request)
        mapping.update(overrides)
        if args.req_id:
            set_request_value(mapping, "ReqId", args.req_id)
        if args.page is not None:
            set_request_value(mapping, "Page", str(args.page))
        if args.page_size is not None:
            set_request_value(mapping, "PageSize", str(args.page_size))

        if args.verbose:
            print(
                f"config={spec.source_file or '<explicit>'} "
                f"entry={spec.entry}",
                file=sys.stderr,
            )
        if args.all_pages:
            raw_page_size = get_request_value(
                mapping,
                "PageSize",
                macro_page_size,
            )
            try:
                page_size = int(raw_page_size)
            except (TypeError, ValueError) as error:
                raise TQLEXError(
                    f"invalid PageSize {raw_page_size!r}"
                ) from error
            result = query_all_pages(
                spec.entry,
                request,
                start_page=args.page if args.page is not None else 0,
                page_size=page_size,
                max_pages=args.max_pages,
                base_url=args.base_url,
                timeout=args.timeout,
                verbose=args.verbose,
            )
        else:
            result = query_tqlex(
                spec.entry,
                request,
                base_url=args.base_url,
                timeout=args.timeout,
            )

        rendered = json.dumps(result, ensure_ascii=False, indent=2)
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
        pbrpc.PBRPCError,
        TQLEXError,
    ) as error:
        print(f"TQLEX 请求失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

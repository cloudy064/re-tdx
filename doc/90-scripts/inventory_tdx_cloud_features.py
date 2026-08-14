#!/usr/bin/env python3
"""Inventory TDX cloud feature XMLs without launching the client."""

from __future__ import annotations

import argparse
import csv
import html
import io
import json
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Sequence

import update_tdx_blocks as updater


DATASOURCE_RE = re.compile(r"<datasource\b(?P<attrs>[^>]*)>", re.I | re.S)
COMMENT_RE = re.compile(r"<!--.*?-->", re.S)
ELEMENT_RE = re.compile(
    r"<(?P<tag>component|gridcol|ctrl|item)\b(?P<attrs>[^>]*)>",
    re.I | re.S,
)
ATTRIBUTE_RE = re.compile(
    r"(?P<name>[\w$-]+)\s*=\s*(?P<quote>[\"'])(?P<value>.*?)(?P=quote)",
    re.S,
)
REQ_ID_RE = re.compile(
    r"(?:[\"']?ReqId[\"']?)\s*[:=]\s*[\"']?(?P<value>\d+)",
    re.I,
)
MODULE_RE = re.compile(
    r"(?:[\"']?modname[\"']?)\s*[:=]\s*[\"']?(?P<value>[^,\"';}\]\s]+)",
    re.I,
)
PLACEHOLDER_RE = re.compile(r"(?<!\$)\$\$([^$]+)\$\$(?!\$)")


@dataclass(frozen=True)
class CloudFeature:
    source_file: str
    datasource_name: str
    request_format: str
    request_id: str
    module: str
    cache: str
    titles: str
    output_fields: str
    placeholders: str
    body: str


def parse_attributes(raw: str) -> dict[str, str]:
    return {
        match.group("name").casefold(): html.unescape(match.group("value"))
        for match in ATTRIBUTE_RE.finditer(raw)
    }


def unique_join(values: Iterable[str], separator: str = " | ") -> str:
    return separator.join(dict.fromkeys(value.strip() for value in values if value.strip()))


def extract_titles(text: str) -> tuple[str, ...]:
    values: list[str] = []
    for match in ELEMENT_RE.finditer(text):
        attributes = parse_attributes(match.group("attrs"))
        tag = match.group("tag").casefold()
        if tag == "component":
            value = attributes.get("caption", "")
        elif tag == "ctrl":
            value = attributes.get("text", "")
        elif tag == "item":
            value = attributes.get("text", "")
        else:
            continue
        if value:
            values.append(value)
    return tuple(dict.fromkeys(values))


def extract_output_fields(text: str) -> tuple[str, ...]:
    values: list[str] = []
    for match in ELEMENT_RE.finditer(text):
        if match.group("tag").casefold() != "gridcol":
            continue
        attributes = parse_attributes(match.group("attrs"))
        name = attributes.get("name", "")
        caption = attributes.get("caption", "")
        if name:
            values.append(f"{name}:{caption}" if caption else name)
    return tuple(dict.fromkeys(values))


def read_text_guess(path: Path) -> str:
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "gb18030", "utf-16"):
        try:
            return data.decode(encoding)
        except UnicodeError:
            continue
    return data.decode("gb18030", errors="replace")


def parse_cloud_feature_file(path: Path) -> list[CloudFeature]:
    # Old and disabled service templates are often kept inside XML comments.
    # They are useful reverse-engineering evidence, but are not part of the
    # active client feature surface and must not be reported as live configs.
    text = COMMENT_RE.sub("", read_text_guess(path))
    titles = unique_join(extract_titles(text)[:16])
    fields = unique_join(extract_output_fields(text)[:80])
    records: list[CloudFeature] = []
    for match in DATASOURCE_RE.finditer(text):
        attributes = parse_attributes(match.group("attrs"))
        body = attributes.get("body", "").strip()
        request_id_match = REQ_ID_RE.search(body)
        module_match = MODULE_RE.search(body)
        records.append(
            CloudFeature(
                source_file=path.name,
                datasource_name=attributes.get("name", "").strip(),
                request_format=attributes.get("reqformat", "").strip(),
                request_id=(
                    request_id_match.group("value")
                    if request_id_match
                    else ""
                ),
                module=(
                    module_match.group("value").strip("\"'")
                    if module_match
                    else ""
                ),
                cache=attributes.get("cache", ""),
                titles=titles,
                output_fields=fields,
                placeholders=unique_join(
                    PLACEHOLDER_RE.findall(body),
                    separator=",",
                ),
                body=" ".join(body.split()),
            )
        )
    return records


def inventory(root: Path) -> list[CloudFeature]:
    directory = root / "T0002" / "cloud_cfg"
    if not directory.is_dir():
        raise updater.UpdateError(f"未找到云功能配置目录：{directory}")
    records: list[CloudFeature] = []
    for path in sorted(directory.glob("*.xml"), key=lambda item: item.name.casefold()):
        records.extend(parse_cloud_feature_file(path))
    return records


def render_csv(records: Sequence[CloudFeature]) -> str:
    output = io.StringIO(newline="")
    fields = list(CloudFeature.__dataclass_fields__)
    writer = csv.DictWriter(output, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(asdict(record) for record in records)
    return output.getvalue()


def render_markdown(records: Sequence[CloudFeature], root: Path) -> str:
    configured = [record for record in records if record.datasource_name]
    endpoints: dict[str, list[CloudFeature]] = {}
    for record in configured:
        endpoints.setdefault(record.datasource_name, []).append(record)
    lines = [
        "# 通达信云功能配置清单",
        "",
        f"- 安装目录：`{root}`",
        f"- datasource 配置：{len(records)}",
        f"- 有名称的服务入口：{len(configured)}",
        f"- 唯一 Entry：{len(endpoints)}",
        f"- 唯一 ReqId：{len({record.request_id for record in records if record.request_id})}",
        "",
        "| Entry | 配置数 | ReqId | 模块 | 示例配置文件 |",
        "| --- | ---: | --- | --- | --- |",
    ]
    for name, items in sorted(
        endpoints.items(),
        key=lambda pair: (-len(pair[1]), pair[0].casefold()),
    ):
        request_ids = unique_join(
            (item.request_id for item in items),
            separator=", ",
        )
        modules = unique_join(
            (item.module for item in items),
            separator=", ",
        )
        files = unique_join(
            (item.source_file for item in items[:5]),
            separator=", ",
        )
        lines.append(
            f"| `{name}` | {len(items)} | {request_ids or '—'} | "
            f"{modules or '—'} | {files} |"
        )
    lines.append("")
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "只读扫描 T0002/cloud_cfg，导出通达信功能页使用的服务 Entry、"
            "ReqId、模块、参数模板和返回字段。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装根目录；默认自动寻找")
    parser.add_argument(
        "--format",
        choices=("json", "csv", "markdown"),
        default="markdown",
        help="输出格式（默认 markdown）",
    )
    parser.add_argument("--output", type=Path, help="输出文件；默认写到标准输出")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        records = inventory(root)
        if args.format == "json":
            text = json.dumps(
                [asdict(record) for record in records],
                ensure_ascii=False,
                indent=2,
            )
        elif args.format == "csv":
            text = render_csv(records)
        else:
            text = render_markdown(records, root)
        if args.output is None:
            sys.stdout.write(text)
            if not text.endswith("\n"):
                sys.stdout.write("\n")
        else:
            output = args.output.expanduser().resolve()
            updater.atomic_write_text(
                output,
                text + ("" if text.endswith("\n") else "\n"),
                "utf-8-sig" if args.format == "csv" else "utf-8",
            )
            print(f"已导出 {len(records)} 条配置：{output}", file=sys.stderr)
    except (OSError, UnicodeError, updater.UpdateError) as error:
        print(f"扫描失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

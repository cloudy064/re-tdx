#!/usr/bin/env python3
"""Inventory and download TDX reqformat=11 ``.jsn`` resources over 7709/TCP."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path, PurePosixPath
from typing import Callable, Iterable, Sequence

import download_tdx_minute as transport
import inventory_tdx_cloud_features as cloud
import update_tdx_blocks as updater


TYPE_FILE_INFO = 709
TYPE_FILE_CHUNK = 1721
CHUNK_SIZE = 30_000
FILE_INFO_PATH_SIZE = 40
FILE_CHUNK_PATH_SIZE = 100
FILE_CHUNK_DATA_SIZE = 308
TEMPLATE_TOKEN_RE = re.compile(r"\$\$\$[A-Za-z0-9_]+\$\$")
CFG_UNIT_RE = re.compile(r"<unit\b(?P<attrs>[^>]*)>", re.I | re.S)


VERIFIED_CFG_FAMILIES = {
    "institution-holdings": tuple(
        [f"func_cgfx{number}_1.jsn" for number in range(101, 118)]
        + [
            "func_cgfxhy101_1.jsn",
            "func_cgfxhy103_1.jsn",
            "func_cgfxhy104_1.jsn",
            "func_cgfxhy105_1.jsn",
            "func_tzcg104_1.jsn",
            "func_jgcg108_1.jsn",
            "func_tbgz108_1.jsn",
        ]
    ),
    "shareholder-counts": (
        "func_gdrs101_1.jsn",
        "func_gdrs102_1.jsn",
        "func_gdrs104_1.jsn",
        "func_gdrs106_1.jsn",
        "func_gdrs107_1.jsn",
    ),
    "lhb-analysis": tuple(
        f"func_lhbfx{number}_1.jsn"
        for number in (101, 103, 104, 105, 106, 107, 108, 110)
    ),
    "capital-strength": tuple(
        f"func_qszj{number}_1.jsn" for number in range(101, 106)
    ),
    "corporate-actions": tuple(
        [f"func_zcjc{number}_1.jsn" for number in range(101, 112)]
        + [
            "func_gqzy101_1.jsn",
            "func_gqzy102_1.jsn",
            "func_gqzy103_1.jsn",
            "func_gqzy105_1.jsn",
            "func_gqzy106_1.jsn",
            "func_gqzy108_1.jsn",
            "func_gqzy109_1.jsn",
            "func_gsrl206_1.jsn",
            "func_hgrztj101_1.jsn",
            "func_hgrztj102_1.jsn",
            "func_hgrztj103_1.jsn",
            "func_hgtj101_1.jsn",
            "func_jqgz103_1.jsn",
            "func_qxfa104_1.jsn",
            "func_cggg101_1.jsn",
            "func_gdzjc102_1.jsn",
            "func_gghg101_1.jsn",
        ]
    ),
    "block-trading": (
        "func_dzjy101_1.jsn",
        "func_dzjy104_1.jsn",
        "func_dzjy107_1.jsn",
        "func_dzjy108_1.jsn",
        "func_dzjy109_1.jsn",
        "func_dzjy1010_1.jsn",
        "func_dzjy1012_1.jsn",
    ),
    "equity-groups": tuple(
        f"func_gqgg{number}_1.jsn" for number in range(101, 105)
    ),
    "margin-financing": tuple(
        f"func_rzrq{number}_1.jsn"
        for number in (
            101, 102, 103, 104, 107, 108, 109,
            110, 111, 112, 113, 114, 120, 201,
        )
    ),
    "stock-connect": tuple(
        [
            f"func_hsgt{number}_1.jsn"
            for number in (
                101, 102, 103, 104, 113, 114,
                201, 202, 204, 205, 206, 207, 208,
                211, 212, 301,
            )
        ]
        + [
            "func_gghq_hsgt_lgt_1.jsn",
            "func_gghq_hsgt_ggt_1.jsn",
        ]
    ),
    "industry-lhb": tuple(
        f"func_hylhb{number}_1.jsn" for number in (101, 102, 104)
    ),
    "market-calendar": (
        "func_cjrl101_1.jsn",
        "func_cjrl105_1.jsn",
    ),
    "futures-statistics": (
        "func_qhtj101_1.jsn",
        "func_qhtj103_1.jsn",
        "func_qhtj104_1.jsn",
    ),
    "ipo-bond-issuance": (
        "func_ipotj101_1.jsn",
        "zq_ssfxr201.jsn",
        "zq_wssfxr201.jsn",
    ),
    "price-limit-analysis": tuple(
        f"func_zdtfx{number}_1.jsn"
        for number in (101, 102, 103, 106, 107)
    ),
    "commodity-themes": (
        "func_zjtc101_1.jsn",
        "func_zjtc103_1.jsn",
    ),
    "premium-stocks": ("func_bygtj102_1.jsn",),
    "market-anomalies": (
        "func_dpyd101_1.jsn",
        "func_jysjk101_1.jsn",
        "func_jysjk102_1.jsn",
        "func_ygzl101_1.jsn",
        "func_lbtt101_1.jsn",
    ),
    "etf-fund-flow": (
        "func_etfsg101_1.jsn",
        "func_etfsg102_1.jsn",
    ),
    "event-research": (
        "func_rdhs101_1.jsn",
        "func_rdhs102_1.jsn",
        "func_sjqd101_1.jsn",
        "func_bwyq101_1.jsn",
        "func_zxjx101_1.jsn",
        "func_zxjx103_1.jsn",
        "func_ztxx101_1.jsn",
        "func_xwlb101_1.jsn",
    ),
    "economic-indicators": ("func_jjzb101_1.jsn",),
    "industry-region-logic": (
        "func_ydyl101_1.jsn",
        "func_ydyl102_1.jsn",
    ),
    "earnings-forecast": ("func_yjygtj101_1.jsn",),
    "active-fund-holdings": ("func_zdjjzczc101_1.jsn",),
    "institution-seat-activity": tuple(
        f"func_jgzc{number}_1.jsn" for number in range(101, 105)
    ),
    "stock-industry-ratings": (
        "func_ggpj101_1.jsn",
        "func_hypj101_1.jsn",
    ),
    "convertible-bond-terms": (
        "kzz_kzzsy201_1.jsn",
        "func_kzz_tkjd201.jsn",
        "func_kzz_lltk201.jsn",
        "func_kzz_hstk201.jsn",
        "func_kzz_shtk201.jsn",
        "func_kzz_xztk201.jsn",
    ),
}

KNOWN_CFG_DETAIL_TEMPLATES = {
    # The four chart-only names produced by this placeholder currently return
    # zero-length metadata.  Mark the raw tokenized filename as intentionally
    # unavailable so it is not mistaken for a static downloadable resource.
    "func_hsgt10$$unitid$$.jsn": (),
    "cgfxmx1": ("cgfxmx1/$$$SC$$$$$ZQDM$$.jsn",),
    "cgfxmx2": ("cgfxmx2/$$$SC$$$$$ZQDM$$.jsn",),
    "lhbfx": ("lhbfx/$$$ZQDM$$.jsn",),
    "zcjc": ("zcjc/$$$SC$$$$$ZQDM$$.jsn",),
    "gqzy": ("gqzy/$$$SC$$$$$ZQDM$$.jsn",),
    "xtzy": ("xtzy/$$$ZQDM$$.jsn",),
    "dzjy1": ("dzjy1/$$$ZQDM$$.jsn",),
    "dzjy2": ("dzjy2/$$$ZQDM$$.jsn",),
    "dzjy3": ("dzjy3/$$$SC$$$$$ZQDM$$.jsn",),
    "dzjy13": ("dzjy13/$$$SC$$$$$ZQDM$$.jsn",),
    "cggg": ("cggg/$$$SC$$$$$ZQDM$$.jsn",),
    "gdzjc1": ("gdzjc1/$$$ZQDM$$.jsn",),
    "gghg": ("gghg/$$$SC$$$$$ZQDM$$.jsn",),
    "gqgg": ("gqgg/$$$ZQDM$$.jsn",),
    "rzrq1": ("rzrq1/$$$SC$$$$$ZQDM$$.jsn",),
    "rzrq2": ("rzrq2/$$$SC$$$$$ZQDM$$.jsn",),
    "rzrq3": ("rzrq3/$$$SC$$$$$ZQDM$$.jsn",),
    "rzrq4": ("rzrq4/$$$SC$$$$$ZQDM$$.jsn",),
    "rzrq5": ("rzrq5/$$$ZQDM$$.jsn",),
    "rzrq6": ("rzrq6/$$$ZQDM$$.jsn",),
    "rzrq7": ("rzrq7/$$$ZQDM$$.jsn",),
    "rzrq8": ("rzrq8/$$$ZQDM$$.jsn",),
    "hsgt": ("hsgt/$$$ZQDM$$.jsn",),
    # The current quarterly master uses a composite ``jd<code>`` key while
    # the other holding masters bind the ordinary market+security key.
    "hsgtcg1": (
        "hsgtcg1/$$$SC$$$$$ZQDM$$.jsn",
        "hsgtcg1/$$$ZQDM$$.jsn",
    ),
    "hsgtcg2": (
        "hsgtcg2/$$$SC$$$$$ZQDM$$.jsn",
        "hsgtcg2/$$$ZQDM$$.jsn",
    ),
    "ggthy": ("ggthy/$$$SC$$$$$ZQDM$$.jsn",),
    "ggthy1": ("ggthy1/$$$SC$$$$$ZQDM$$.jsn",),
    "cjrl": ("cjrl/$$$ZQDM$$.jsn",),
    "qhtj1": ("qhtj1/$$$SC$$$$$ZQDM$$.jsn",),
    "qhtj2": ("qhtj2/$$$SC$$$$$ZQDM$$.jsn",),
    "ipotj102": ("ipotj102/$$$ZQDM$$.jsn",),
    "ipotj103": ("ipotj103/$$$ZQDM$$.jsn",),
    "ipotj104": ("ipotj104/$$$ZQDM$$.jsn",),
    "bygtj1": ("bygtj1/$$$ZQDM$$.jsn",),
    "bygtj3": ("bygtj3/$$$ZQDM$$.jsn",),
    "zdtfx1": ("zdtfx1/$$$SC$$$$$ZQDM$$.jsn",),
    "zdtfx2": ("zdtfx2/$$$ZQDM$$.jsn",),
    "zdtfx3": ("zdtfx3/$$$ZQDM$$.jsn",),
    "zjtc1": ("zjtc1/$$$ZQDM$$.jsn",),
    "zjtc2": ("zjtc2/$$$ZQDM$$.jsn",),
    "zjtc3": ("zjtc3/$$$ZQDM$$.jsn",),
    "zjtc4": ("zjtc4/$$$ZQDM$$.jsn",),
    "zjtc5": ("zjtc5/$$$ZQDM$$.jsn",),
    "jjzb1": ("jjzb1/$$$ZQDM$$.jsn",),
    "jjzb2": ("jjzb2/$$$ZQDM$$.jsn",),
    "ggjx": ("ggjx/$$$SC$$$$$ZQDM$$.jsn",),
    "ydyl1": ("ydyl1/$$$ZQDM$$.jsn",),
    "ztxx": ("ztxx/$$$ZQDM$$.jsn",),
    "ygzl": ("ygzl/$$$ZQDM$$.jsn",),
    "sjqd": ("sjqd/$$$ZQDM$$.jsn",),
    # ``yjyg`` binds the aggregate master's composite industry+report-period
    # key.  The detail rows then carry their ordinary stock market and code.
    "yjyg": ("yjyg/$$$ZQDM$$.jsn",),
    "zdjjzczc": ("zdjjzczc/$$$SC$$$$$ZQDM$$.jsn",),
    "ggpj": ("ggpj/$$$SC$$$$$ZQDM$$.jsn",),
    "hypj": ("hypj/$$$SC$$$$$ZQDM$$.jsn",),
    # The XML versions retain these master-detail paths inside commented UI
    # panes.  Current servers still expose the resources, and require the
    # convertible bond's own market+code rather than its underlying stock.
    "kzz_hstk": ("kzz_hstk/$$$SC$$$$$ZQDM$$.jsn",),
    "kzz_shtk": ("kzz_shtk/$$$SC$$$$$ZQDM$$.jsn",),
    "kzz_xztk": ("kzz_xztk/$$$SC$$$$$ZQDM$$.jsn",),
    # ``$$UNITID$$`` is replaced by the controlling list unit before the
    # selected row's ``$ZQDM`` is appended.  Expand the finite unit set here
    # so every server path remains directly downloadable from the inventory.
    "hgrztj$$unitid$$": tuple(
        f"hgrztj{unit_id}/$$$ZQDM$$.jsn"
        for unit_id in (21701, 21702, 21703)
    ),
    "yybph$$unitid$$": tuple(
        f"yybph{unit_id}/$$$ZQDM$$.jsn"
        for unit_id in (22401, 22402, 22403, 22404)
    ),
    "hylhb$$unitid$$": tuple(
        f"hylhb{unit_id}/$$$SC$$$$$ZQDM$$.jsn"
        for unit_id in (13801, 13901, 14001)
    ),
    "lsyd$$unitid$$": tuple(
        f"lsyd{unit_id}/$$$SC$$$$$ZQDM$$.jsn"
        for unit_id in (22801, 22802, 22803, 22804)
    ),
}


class JsnDownloadError(RuntimeError):
    """Raised when reqformat=11 inventory or transport data is invalid."""


class JsnResourceMissingError(JsnDownloadError):
    """Raised when a valid server reports that a selected resource is absent."""


@dataclass(frozen=True)
class JsnResource:
    resource: str
    source_files: tuple[str, ...]
    placeholders: tuple[str, ...]


@dataclass(frozen=True)
class RemoteFileInfo:
    size: int
    md5: str
    has_md5: bool


@dataclass(frozen=True)
class DownloadedJsn:
    resource: str
    remote_path: str
    output_path: Path
    size: int
    md5: str
    endpoint: transport.HostEndpoint
    server_name: str


def unique(values: Iterable[str]) -> tuple[str, ...]:
    return tuple(dict.fromkeys(value for value in values if value))


def inventory_resources(root: Path) -> tuple[JsnResource, ...]:
    grouped: dict[str, dict[str, list[str]]] = {}
    for feature in cloud.inventory(root):
        if feature.request_format != "11":
            continue
        raw_resource = feature.body.strip().replace("\\", "/")
        if not raw_resource:
            continue
        resource = normalize_resource_path(raw_resource)
        entry = grouped.setdefault(
            resource,
            {"source_files": [], "placeholders": []},
        )
        entry["source_files"].append(feature.source_file)
        entry["placeholders"].extend(
            TEMPLATE_TOKEN_RE.findall(resource)
        )
    return tuple(
        JsnResource(
            resource=resource,
            source_files=unique(grouped[resource]["source_files"]),
            placeholders=unique(grouped[resource]["placeholders"]),
        )
        for resource in sorted(grouped, key=str.casefold)
    )


def inventory_cfg_resources(root: Path) -> tuple[JsnResource, ...]:
    """Recover JSN resources referenced by list-unit ``file=`` attributes."""
    grouped: dict[str, dict[str, list[str]]] = {}
    directory = root / "T0002" / "cloud_cfg"
    if not directory.is_dir():
        raise JsnDownloadError(f"cloud_cfg 目录不存在：{directory}")
    for path in sorted(directory.glob("*.cfg"), key=lambda item: item.name.casefold()):
        text = cloud.COMMENT_RE.sub("", cloud.read_text_guess(path))
        for match in CFG_UNIT_RE.finditer(text):
            attributes = cloud.parse_attributes(match.group("attrs"))
            raw_resource = attributes.get("file", "").strip().replace("\\", "/")
            detail_templates = KNOWN_CFG_DETAIL_TEMPLATES.get(
                raw_resource.casefold()
            )
            if detail_templates is not None:
                candidate_resources = detail_templates
            elif raw_resource.casefold().endswith(".jsn"):
                candidate_resources = (normalize_resource_path(raw_resource),)
            else:
                continue
            for resource in candidate_resources:
                entry = grouped.setdefault(
                    resource,
                    {"source_files": [], "placeholders": []},
                )
                entry["source_files"].append(path.name)
                entry["placeholders"].extend(
                    TEMPLATE_TOKEN_RE.findall(resource)
                )
    return tuple(
        JsnResource(
            resource=resource,
            source_files=unique(grouped[resource]["source_files"]),
            placeholders=unique(grouped[resource]["placeholders"]),
        )
        for resource in sorted(grouped, key=str.casefold)
    )


def merge_resource_inventories(
    *inventories: Sequence[JsnResource],
) -> tuple[JsnResource, ...]:
    grouped: dict[str, dict[str, list[str]]] = {}
    for inventory in inventories:
        for item in inventory:
            entry = grouped.setdefault(
                item.resource,
                {"source_files": [], "placeholders": []},
            )
            entry["source_files"].extend(item.source_files)
            entry["placeholders"].extend(item.placeholders)
    return tuple(
        JsnResource(
            resource=resource,
            source_files=unique(grouped[resource]["source_files"]),
            placeholders=unique(grouped[resource]["placeholders"]),
        )
        for resource in sorted(grouped, key=str.casefold)
    )


def expand_template(
    resource: str,
    *,
    market: int | None = None,
    code: str | None = None,
) -> str:
    values = {
        "$$$SC$$": "" if market is None else str(market),
        "$$$ZQDM$$": "" if code is None else code,
    }
    result = resource
    for token, value in values.items():
        if token in result and value:
            result = result.replace(token, value)
    unresolved = TEMPLATE_TOKEN_RE.findall(result)
    if unresolved:
        raise JsnDownloadError(
            f"资源模板仍有未替换参数：{resource!r} -> {', '.join(unresolved)}"
        )
    return result


def normalize_resource_path(resource: str) -> str:
    normalized = resource.strip().replace("\\", "/").lstrip("/")
    path = PurePosixPath(normalized)
    if not normalized or path.is_absolute() or ".." in path.parts:
        raise JsnDownloadError(f"资源路径无效：{resource!r}")
    if path.suffix.casefold() != ".jsn":
        raise JsnDownloadError(f"资源不是 .jsn 文件：{resource!r}")
    # CBiFileHandle prefixes bare filenames with ``list/``.  Paths that
    # already identify another namespace (for example ggxc/ or zttzty/)
    # are retained by the corresponding master-detail page.
    if len(path.parts) == 1:
        path = PurePosixPath("list") / path
    return path.as_posix()


def make_remote_path(resource: str, prefix: str = "bi") -> str:
    resource = normalize_resource_path(resource)
    prefix = prefix.strip().strip("/")
    if prefix not in {"bi", "bib", "bi_diy"}:
        raise JsnDownloadError(f"未知远端前缀：{prefix!r}")
    return f"{prefix}/{resource}"


def encode_fixed_path(path: str, size: int) -> bytes:
    try:
        encoded = path.encode("ascii")
    except UnicodeEncodeError as error:
        raise JsnDownloadError(f"远端路径不是 ASCII：{path!r}") from error
    if len(encoded) >= size:
        raise JsnDownloadError(
            f"远端路径过长：{len(encoded)} 字节，协议上限 {size - 1}"
        )
    return encoded.ljust(size, b"\0")


def build_file_info_request(remote_path: str) -> bytes:
    return encode_fixed_path(remote_path, FILE_INFO_PATH_SIZE)


def parse_file_info(data: bytes) -> RemoteFileInfo:
    if len(data) < 38:
        raise JsnDownloadError(
            f"文件元数据响应过短：期望至少 38 字节，实际 {len(data)}"
        )
    size = int.from_bytes(data[:4], "little", signed=False)
    has_md5 = data[4] != 0
    md5 = data[5:37].split(b"\0", 1)[0].decode("ascii", errors="strict")
    if size < 1:
        raise JsnResourceMissingError("服务器返回的文件长度为 0")
    if has_md5 and not re.fullmatch(r"[0-9a-fA-F]{32}", md5):
        raise JsnDownloadError(f"服务器返回的 MD5 无效：{md5!r}")
    return RemoteFileInfo(size=size, md5=md5.casefold(), has_md5=has_md5)


def build_file_chunk_request(
    remote_path: str,
    offset: int,
    count: int = CHUNK_SIZE,
) -> bytes:
    if offset < 0:
        raise JsnDownloadError(f"文件偏移不能为负数：{offset}")
    if not (1 <= count <= CHUNK_SIZE):
        raise JsnDownloadError(f"分片长度必须在 1—{CHUNK_SIZE}：{count}")
    data = (
        struct.pack("<II", offset, count)
        + encode_fixed_path(remote_path, FILE_CHUNK_PATH_SIZE)
    )
    return data.ljust(FILE_CHUNK_DATA_SIZE, b"\0")


def parse_file_chunk(data: bytes, requested: int) -> bytes:
    if len(data) < 4:
        raise JsnDownloadError("文件分片响应缺少长度字段")
    size = int.from_bytes(data[:4], "little", signed=False)
    if size > requested:
        raise JsnDownloadError(
            f"文件分片超过请求长度：请求 {requested}，收到 {size}"
        )
    if len(data) < size + 4:
        raise JsnDownloadError(
            f"文件分片被截断：声明 {size}，实际 {len(data) - 4}"
        )
    return data[4 : size + 4]


def query_file_info(
    connection: transport.QuoteConnection,
    remote_path: str,
) -> RemoteFileInfo:
    response = connection.call(
        TYPE_FILE_INFO,
        build_file_info_request(remote_path),
    )
    return parse_file_info(response.data)


def download_to_path(
    connection: transport.QuoteConnection,
    remote_path: str,
    destination: Path,
    *,
    progress: Callable[[int, int], None] | None = None,
) -> RemoteFileInfo:
    info = query_file_info(connection, remote_path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_name = ""
    digest = hashlib.md5()
    downloaded = 0
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=f".{destination.name}.",
            suffix=".part",
            dir=destination.parent,
            delete=False,
        ) as output:
            temporary_name = output.name
            while downloaded < info.size:
                requested = min(CHUNK_SIZE, info.size - downloaded)
                response = connection.call(
                    TYPE_FILE_CHUNK,
                    build_file_chunk_request(
                        remote_path,
                        downloaded,
                        requested,
                    ),
                )
                chunk = parse_file_chunk(response.data, requested)
                if not chunk:
                    raise JsnDownloadError(
                        f"服务器在偏移 {downloaded} 返回空分片"
                    )
                output.write(chunk)
                digest.update(chunk)
                downloaded += len(chunk)
                if progress is not None:
                    progress(downloaded, info.size)
        if downloaded != info.size:
            raise JsnDownloadError(
                f"文件长度不符：声明 {info.size}，下载 {downloaded}"
            )
        actual_md5 = digest.hexdigest()
        if info.has_md5 and actual_md5 != info.md5:
            raise JsnDownloadError(
                f"文件 MD5 不符：声明 {info.md5}，实际 {actual_md5}"
            )
        os.replace(temporary_name, destination)
        temporary_name = ""
        return info
    finally:
        if temporary_name:
            try:
                Path(temporary_name).unlink()
            except FileNotFoundError:
                pass


def summarize_jsn(path: Path) -> dict[str, object]:
    raw = path.read_bytes()
    text = raw.decode("gb18030")
    value = json.loads(text)
    summary: dict[str, object] = {
        "bytes": len(raw),
        "root_type": type(value).__name__,
    }
    if isinstance(value, list):
        summary["groups"] = len(value)
        rows = 0
        headers: list[list[str]] = []
        for group in value:
            if not isinstance(group, dict):
                continue
            data = group.get("data")
            if isinstance(data, list):
                rows += len(data)
            header = group.get("colheader")
            if isinstance(header, list) and len(headers) < 5:
                headers.append([str(item) for item in header])
        summary["rows"] = rows
        summary["headers"] = headers
    return summary


def select_resources(
    inventory: Sequence[JsnResource],
    explicit: Sequence[str],
    patterns: Sequence[str],
    all_resources: bool,
    static_resources: bool = False,
) -> tuple[str, ...]:
    selected = list(explicit)
    if all_resources:
        selected.extend(item.resource for item in inventory)
    if static_resources:
        selected.extend(
            item.resource for item in inventory if not item.placeholders
        )
    for pattern in patterns:
        regex = re.compile(pattern, re.I)
        selected.extend(
            item.resource
            for item in inventory
            if regex.search(item.resource)
            or any(regex.search(name) for name in item.source_files)
        )
    return unique(selected)


def select_resource_families(
    inventory: Sequence[JsnResource],
    families: Sequence[str],
) -> tuple[str, ...]:
    available = {
        PurePosixPath(item.resource).name.casefold(): item.resource
        for item in inventory
    }
    selected: list[str] = []
    for family in families:
        for filename in VERIFIED_CFG_FAMILIES[family]:
            resource = available.get(filename.casefold())
            if resource:
                selected.append(resource)
    return unique(selected)


def output_path_for(
    resource: str,
    output_dir: Path,
    output: Path | None,
    total: int,
) -> Path:
    if output is not None:
        if total != 1:
            raise JsnDownloadError("--output 只能和单个资源一起使用")
        return output.expanduser().resolve()
    relative = PurePosixPath(normalize_resource_path(resource))
    return output_dir.joinpath(*relative.parts)


def connect_first(
    endpoints: Sequence[transport.HostEndpoint],
    timeout: float,
) -> tuple[transport.QuoteConnection, list[str]]:
    failures: list[str] = []
    for endpoint in endpoints:
        connection = transport.QuoteConnection(endpoint, timeout)
        try:
            connection.__enter__()
            return connection, failures
        except (OSError, transport.DownloadError) as error:
            failures.append(f"{endpoint.address}: {error}")
    detail = "\n".join(failures[:8])
    raise JsnDownloadError(f"所有行情主站连接失败：\n{detail}")


def render_inventory(resources: Sequence[JsnResource], as_json: bool) -> str:
    if as_json:
        return json.dumps(
            [asdict(resource) for resource in resources],
            ensure_ascii=False,
            indent=2,
        )
    lines = [
        f"JSN 唯一资源：{len(resources)}",
        "资源路径\t来源配置",
    ]
    lines.extend(
        f"{item.resource}\t{','.join(item.source_files)}"
        for item in resources
    )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "扫描 reqformat=11 配置，并通过通达信 7709/TCP 的 "
            "709/1721 命令探测或下载 .jsn 数据资源。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录；默认自动寻找")
    parser.add_argument("--list", action="store_true", help="列出配置中的 .jsn 资源")
    parser.add_argument(
        "--include-cfg",
        action="store_true",
        help="同时扫描列表 .cfg 的 file= 引用（只扩展清单，不自动全量下载）",
    )
    parser.add_argument(
        "--family",
        action="append",
        choices=tuple(VERIFIED_CFG_FAMILIES),
        default=[],
        help="选择已经验证的 CFG 高价值资源族；可重复，并自动启用 CFG 清单",
    )
    parser.add_argument("--json", action="store_true", help="清单或结果输出 JSON")
    parser.add_argument("--resource", action="append", default=[], help="资源相对路径；可重复")
    parser.add_argument(
        "--match",
        action="append",
        default=[],
        help="按资源路径或来源配置正则选择；可重复",
    )
    parser.add_argument("--all", action="store_true", help="选择清单中的全部资源")
    parser.add_argument(
        "--all-static",
        action="store_true",
        help="选择全部不含动态占位符的资源",
    )
    parser.add_argument(
        "--market",
        type=int,
        help="替换 $$$SC$$；除 0/1/2 外也支持港股等两位市场号",
    )
    parser.add_argument("--code", help="替换 $$$ZQDM$$")
    parser.add_argument(
        "--key",
        help="替换非证券语义的 $$$ZQDM$$，如日期、事件、机构或分组 ID",
    )
    parser.add_argument(
        "--prefix",
        choices=("bi", "bib", "bi_diy"),
        default="bi",
        help="服务器资源前缀（默认 bi）",
    )
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--probe", action="store_true", help="只查询长度和 MD5")
    action.add_argument("--download", action="store_true", help="下载并校验资源")
    parser.add_argument("--host", action="append", default=[], help="指定 host[:port]；可重复")
    parser.add_argument("--timeout", type=float, default=8.0, help="网络超时秒数")
    parser.add_argument(
        "--max-resources",
        type=int,
        default=100,
        help="单次联网资源上限；0 表示不限制（默认 100）",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-jsn",
        help="下载目录",
    )
    parser.add_argument("--output", type=Path, help="单个资源的输出文件")
    parser.add_argument(
        "--client-cache",
        action="store_true",
        help="写入 T0002/cloud_cache，使客户端可直接复用",
    )
    parser.add_argument("--summary", action="store_true", help="下载后解析 JSON 并显示概要")
    parser.add_argument(
        "--skip-missing",
        action="store_true",
        help="批量探测/下载时跳过服务器尚未生成的空资源",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    connection: transport.QuoteConnection | None = None
    try:
        if args.max_resources < 0:
            raise JsnDownloadError("--max-resources 不能为负数")
        if args.code and args.key and args.code != args.key:
            raise JsnDownloadError("--code 和 --key 不能指定不同的 $$$ZQDM$$")
        root = updater.find_tdx_root(args.root)
        resources = inventory_resources(root)
        if args.include_cfg or args.family:
            resources = merge_resource_inventories(
                resources,
                inventory_cfg_resources(root),
            )
        selected = select_resources(
            resources,
            args.resource,
            args.match,
            args.all,
            args.all_static,
        )
        selected = unique(
            (*selected, *select_resource_families(resources, args.family))
        )
        if args.list or not (selected or args.probe or args.download):
            print(render_inventory(resources, args.json))
            if not selected:
                return 0
        if not selected:
            raise JsnDownloadError("请用 --resource、--match 或 --all 选择资源")
        expanded = tuple(
            normalize_resource_path(
                expand_template(
                    item,
                    market=args.market,
                    code=args.key or args.code,
                )
            )
            for item in selected
        )
        if (
            (args.probe or args.download)
            and args.max_resources
            and len(expanded) > args.max_resources
        ):
            raise JsnDownloadError(
                f"选中 {len(expanded)} 个资源，超过 --max-resources "
                f"{args.max_resources}；请缩小范围或显式调高上限"
            )
        if not (args.probe or args.download):
            plan = [
                {
                    "resource": resource,
                    "remote_path": make_remote_path(resource, args.prefix),
                    "action": "dry-run",
                }
                for resource in expanded
            ]
            if args.json or len(plan) != 1:
                print(json.dumps(plan, ensure_ascii=False, indent=2))
            else:
                print(
                    f"计划探测 {plan[0]['remote_path']}；"
                    "加 --probe 联网查询，或加 --download 下载"
                )
            return 0
        if args.host:
            endpoints = transport.unique_endpoints(
                transport.parse_endpoint(value) for value in args.host
            )
        else:
            endpoints = transport.load_hq_hosts(root / "connect.cfg")
        connection, failures = connect_first(endpoints, args.timeout)
        if failures:
            print(
                f"前 {len(failures)} 个主站连接失败，已切换到 "
                f"{connection.endpoint.address}",
                file=sys.stderr,
            )
        output_dir = (
            root / "T0002" / "cloud_cache"
            if args.client_cache
            else args.output_dir.expanduser().resolve()
        )
        results: list[dict[str, object]] = []
        for resource in expanded:
            remote_path = make_remote_path(resource, args.prefix)
            try:
                if args.download:
                    destination = output_path_for(
                        resource,
                        output_dir,
                        args.output,
                        len(expanded),
                    )
                    last_percent = -1

                    def report_progress(done: int, total: int) -> None:
                        nonlocal last_percent
                        percent = done * 100 // total
                        if percent >= last_percent + 5 or done == total:
                            print(
                                f"\r{resource}: {done}/{total} ({percent}%)",
                                end="",
                                file=sys.stderr,
                                flush=True,
                            )
                            last_percent = percent

                    info = download_to_path(
                        connection,
                        remote_path,
                        destination,
                        progress=report_progress,
                    )
                    print(file=sys.stderr)
                    record: dict[str, object] = {
                        "resource": resource,
                        "remote_path": remote_path,
                        "output": str(destination),
                        "size": info.size,
                        "md5": info.md5,
                        "endpoint": connection.endpoint.address,
                        "server": connection.server_name,
                    }
                    if args.summary:
                        record["summary"] = summarize_jsn(destination)
                    results.append(record)
                else:
                    info = query_file_info(connection, remote_path)
                    results.append(
                        {
                            "resource": resource,
                            "remote_path": remote_path,
                            "size": info.size,
                            "md5": info.md5,
                            "endpoint": connection.endpoint.address,
                            "server": connection.server_name,
                        }
                    )
            except JsnResourceMissingError:
                if not args.skip_missing:
                    raise
                results.append(
                    {
                        "resource": resource,
                        "remote_path": remote_path,
                        "status": "missing",
                        "endpoint": connection.endpoint.address,
                        "server": connection.server_name,
                    }
                )
        if (
            args.json
            or len(results) != 1
            or args.summary
            or any(item.get("status") == "missing" for item in results)
        ):
            print(json.dumps(results, ensure_ascii=False, indent=2))
        else:
            item = results[0]
            print(
                f"{item['resource']}: {item['size']} bytes, "
                f"md5={item['md5']}, server={item['endpoint']}"
            )
    except (
        OSError,
        UnicodeError,
        ValueError,
        re.error,
        json.JSONDecodeError,
        updater.UpdateError,
        transport.DownloadError,
        JsnDownloadError,
    ) as error:
        print(f"操作失败：{error}", file=sys.stderr)
        return 1
    finally:
        if connection is not None:
            connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

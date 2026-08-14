#!/usr/bin/env python3
"""One-command updater for the offline TDX block explorer and optional CSVs."""

from __future__ import annotations

import argparse
import os
import sys
import tempfile
import time
import webbrowser
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

import build_tdx_blocks_html as html_builder
import extract_tdx_blocks as extractor


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = PROJECT_ROOT / "output" / "tdx-blocks.html"


class UpdateError(RuntimeError):
    """Raised when an installation or stable source snapshot is unavailable."""


@dataclass(frozen=True)
class SourceState:
    relative_path: str
    size: int
    modified_ns: int


@dataclass(frozen=True)
class UpdateResult:
    root: Path
    output: Path
    csv_outputs: tuple[Path, ...]
    source_date: str
    blocks: int
    memberships: int
    securities: int
    unresolved_names: int
    html_size: int
    attempts: int


def is_tdx_root(path: Path) -> bool:
    cache = path / "T0002" / "hq_cache"
    return (
        path.is_dir()
        and cache.is_dir()
        and (cache / "tdxzs3.cfg").is_file()
        and (cache / "tdxhy.cfg").is_file()
        and (cache / "infoharbor_block.dat").is_file()
    )


def registry_candidates() -> Iterable[Path]:
    if os.name != "nt":
        return
    try:
        import winreg
    except ImportError:
        return

    app_path = (
        r"Software\Microsoft\Windows\CurrentVersion"
        r"\App Paths\TdxW.exe"
    )
    roots = (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE)
    access_modes = (
        winreg.KEY_READ,
        winreg.KEY_READ | getattr(winreg, "KEY_WOW64_32KEY", 0),
        winreg.KEY_READ | getattr(winreg, "KEY_WOW64_64KEY", 0),
    )
    for root in roots:
        for access in access_modes:
            try:
                with winreg.OpenKey(root, app_path, 0, access) as key:
                    executable, _ = winreg.QueryValueEx(key, None)
            except OSError:
                continue
            if executable:
                yield Path(str(executable).strip('"')).parent


def installation_candidates() -> Iterable[Path]:
    for variable in ("TDX_ROOT", "TDX_HOME"):
        value = os.environ.get(variable)
        if value:
            yield Path(value)

    yield from registry_candidates()

    current = Path.cwd().resolve()
    yield current
    yield current / "new_tdx"
    for parent in current.parents:
        yield parent / "new_tdx"

    if os.name == "nt":
        for letter in "CDEFGHIJKLMNOPQRSTUVWXYZ":
            drive = Path(f"{letter}:\\")
            if not drive.exists():
                continue
            yield drive / "new_tdx"
            yield drive / "tdx"
            yield drive / "通达信"
    else:
        yield Path("/opt/new_tdx")


def find_tdx_root(explicit: Path | None = None) -> Path:
    if explicit is not None:
        resolved = explicit.expanduser().resolve()
        if not is_tdx_root(resolved):
            raise UpdateError(
                f"指定目录不是可用的通达信安装根目录：{resolved}"
            )
        return resolved

    seen: set[str] = set()
    for candidate in installation_candidates():
        try:
            resolved = candidate.expanduser().resolve()
        except OSError:
            continue
        key = str(resolved).casefold()
        if key in seen:
            continue
        seen.add(key)
        if is_tdx_root(resolved):
            return resolved
    raise UpdateError(
        "未自动找到通达信安装目录；请使用 --root C:\\new_tdx 指定。"
    )


def required_source_paths(root: Path, families: set[str]) -> list[Path]:
    cache = root / "T0002" / "hq_cache"
    paths = [cache / filename for filename in extractor.TNF_FILES.values()]
    if families & {"industry", "research-industry"}:
        paths.extend((cache / "tdxzs3.cfg", cache / "tdxhy.cfg"))
    if families & {"concept", "style", "index"}:
        paths.append(cache / "infoharbor_block.dat")
    return sorted(paths, key=lambda path: path.name.casefold())


def capture_source_state(
    root: Path,
    families: set[str],
) -> tuple[SourceState, ...]:
    states: list[SourceState] = []
    for path in required_source_paths(root, families):
        try:
            stat = path.stat()
        except FileNotFoundError as error:
            raise UpdateError(f"缺少数据源文件：{path}") from error
        states.append(
            SourceState(
                relative_path=str(path.relative_to(root)),
                size=stat.st_size,
                modified_ns=stat.st_mtime_ns,
            )
        )
    return tuple(states)


def extract_stable_snapshot(
    root: Path,
    families: set[str],
    retries: int,
) -> tuple[
    dict[tuple[int, str], extractor.Security],
    list[extractor.Block],
    list[extractor.BlockMember],
    int,
]:
    last_error: BaseException | None = None
    for attempt in range(1, retries + 1):
        try:
            before = capture_source_state(root, families)
            securities, blocks, members = extractor.extract(root, families)
            after = capture_source_state(root, families)
        except (
            OSError,
            UnicodeError,
            extractor.BlockFormatError,
            UpdateError,
        ) as error:
            last_error = error
            if attempt < retries:
                print(
                    f"数据文件可能正在写入，正在重试 "
                    f"({attempt}/{retries})：{error}",
                    file=sys.stderr,
                )
                time.sleep(0.2)
                continue
            break
        if before == after:
            return securities, blocks, members, attempt
        if attempt < retries:
            print(
                f"数据文件在读取期间发生变化，正在重试 "
                f"({attempt}/{retries})…",
                file=sys.stderr,
            )
            time.sleep(0.2)
    reason = f"；最后错误：{last_error}" if last_error else ""
    raise UpdateError(
        f"连续 {retries} 次未取得稳定数据快照{reason}。"
        "请等待通达信更新完成后重试。"
    )


def atomic_write_text(path: Path, text: str, encoding: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=str(path.parent),
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(
            descriptor,
            "w",
            encoding=encoding,
            newline="",
        ) as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(str(temporary), str(path))
    except BaseException:
        try:
            os.close(descriptor)
        except OSError:
            pass
        try:
            temporary.unlink()
        except OSError:
            pass
        raise


def write_csv_outputs_atomic(
    directory: Path,
    blocks: list[extractor.Block],
    members: list[extractor.BlockMember],
) -> tuple[Path, Path]:
    blocks_path = directory / "tdx-blocks.csv"
    members_path = directory / "tdx-block-members.csv"
    atomic_write_text(
        blocks_path,
        extractor.render_csv(
            blocks,
            list(extractor.Block.__dataclass_fields__),
        ),
        "utf-8-sig",
    )
    atomic_write_text(
        members_path,
        extractor.render_csv(
            members,
            list(extractor.BlockMember.__dataclass_fields__),
        ),
        "utf-8-sig",
    )
    return blocks_path, members_path


def update(
    root: Path,
    output: Path,
    families: set[str],
    csv_dir: Path | None = None,
    retries: int = 3,
) -> UpdateResult:
    if retries < 1:
        raise UpdateError("retries 必须大于等于 1")
    securities, blocks, members, attempts = extract_stable_snapshot(
        root,
        families,
        retries,
    )
    document = html_builder.compact_payload(blocks, members)
    html, _, _ = html_builder.render_html(document)
    resolved_output = output.expanduser().resolve()
    atomic_write_text(resolved_output, html, "utf-8")

    csv_outputs: tuple[Path, ...] = ()
    if csv_dir is not None:
        resolved_csv_dir = csv_dir.expanduser().resolve()
        csv_outputs = write_csv_outputs_atomic(
            resolved_csv_dir,
            blocks,
            members,
        )

    return UpdateResult(
        root=root,
        output=resolved_output,
        csv_outputs=csv_outputs,
        source_date=str(document["source_date"]),
        blocks=len(blocks),
        memberships=len(members),
        securities=len({member.security_id for member in members}),
        unresolved_names=sum(not member.name_resolved for member in members),
        html_size=resolved_output.stat().st_size,
        attempts=attempts,
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "自动读取通达信最新公开缓存，更新行业/概念等板块及其"
            "成分证券的单文件离线网页。"
        )
    )
    parser.add_argument(
        "--root",
        type=Path,
        help="通达信安装根目录；省略时自动寻找",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"HTML 输出路径（默认：{DEFAULT_OUTPUT}）",
    )
    parser.add_argument(
        "--csv-dir",
        type=Path,
        help="同时更新两张 CSV 的目录；省略则只生成 HTML",
    )
    parser.add_argument(
        "--family",
        action="append",
        choices=extractor.FAMILY_CHOICES,
        help="只更新指定类别；可重复使用，默认全部",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=3,
        help="源文件在读取期间变化时的最大尝试次数（默认：3）",
    )
    parser.add_argument(
        "--open",
        action="store_true",
        dest="open_after",
        help="更新成功后用默认浏览器打开页面",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = find_tdx_root(args.root)
        families = set(args.family or extractor.FAMILY_CHOICES)
        print(f"通达信目录：{root}", file=sys.stderr)
        result = update(
            root=root,
            output=args.output,
            families=families,
            csv_dir=args.csv_dir,
            retries=args.retries,
        )
    except (
        OSError,
        UnicodeError,
        extractor.BlockFormatError,
        UpdateError,
    ) as error:
        print(f"更新失败：{error}", file=sys.stderr)
        return 1

    print(
        f"更新完成：数据日期 {result.source_date or '未知'}，"
        f"{result.blocks} 个板块，"
        f"{result.memberships} 条关系，"
        f"{result.securities} 只相关证券，"
        f"{result.unresolved_names} 条名称待解析",
        file=sys.stderr,
    )
    print(
        f"HTML：{result.output} "
        f"({html_builder.human_size(result.html_size)})",
        file=sys.stderr,
    )
    for path in result.csv_outputs:
        print(f"CSV：{path}", file=sys.stderr)
    if result.attempts > 1:
        print(
            f"为取得稳定快照共读取 {result.attempts} 次",
            file=sys.stderr,
        )
    if args.open_after:
        webbrowser.open(result.output.as_uri())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

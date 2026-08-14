#!/usr/bin/env python3
"""Read-only inventory of a TDX installation.

The script intentionally ignores configuration contents and user data.  It
only records directory/file counts, selected binary metadata, PE dependencies,
and exported symbols.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


ARTIFACT_GROUPS: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("main", ("tdxw.exe",)),
    ("data parser", ("tdataparse.dll",)),
    ("host network", ("tdxasiocomm.dll",)),
    ("network plugin", ("tasiocomm.dll", "tasiocomm1.dll")),
    ("encryption plugin", ("tencrypt.dll",)),
    ("big-data model", ("tbigdata.dll", "tbigdatad.dll")),
    ("event bus", ("tpbus.dll",)),
    ("device/auth", ("tjyaid.dll",)),
    ("pool", ("tpool.dll",)),
    ("mini quote", ("addinminiquote.dll", "addinminiquoteex.dll")),
    ("Python plugin", ("tpyth.dll",)),
    ("data SDK", ("tdxdatasdk.dll",)),
    ("quote core", ("tq.dll", "tqsrun.dll")),
)

EXPORT_RE = re.compile(
    r"^\s*(?P<ordinal>\d+)\s+"
    r"(?P<hint>[0-9A-Fa-f]+)\s+"
    r"(?P<rva>[0-9A-Fa-f]+)\s+"
    r"(?P<name>\S.*)\s*$"
)
DEPENDENCY_RE = re.compile(r"^\s+(?P<name>[^\s]+\.dll)\s*$", re.IGNORECASE)


@dataclass(frozen=True)
class Artifact:
    group: str
    path: Path
    size: int
    sha256: str | None = None


@dataclass(frozen=True)
class Export:
    ordinal: int
    hint: int
    rva: int
    name: str


def parse_exports(text: str) -> list[Export]:
    exports: list[Export] = []
    for line in text.splitlines():
        match = EXPORT_RE.match(line)
        if not match:
            continue
        exports.append(
            Export(
                ordinal=int(match.group("ordinal")),
                hint=int(match.group("hint"), 16),
                rva=int(match.group("rva"), 16),
                name=match.group("name").strip(),
            )
        )
    return exports


def parse_dependencies(text: str) -> list[str]:
    dependencies: list[str] = []
    seen: set[str] = set()
    for line in text.splitlines():
        match = DEPENDENCY_RE.match(line)
        if not match:
            continue
        name = match.group("name")
        folded = name.casefold()
        if folded not in seen:
            dependencies.append(name)
            seen.add(folded)
    return dependencies


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def walk_files(root: Path) -> Iterable[Path]:
    for current, directories, filenames in os.walk(root, followlinks=False):
        directories.sort(key=str.casefold)
        filenames.sort(key=str.casefold)
        current_path = Path(current)
        for filename in filenames:
            yield current_path / filename


def inventory(
    root: Path, include_hashes: bool = False
) -> tuple[list[Artifact], dict[str, int], list[str]]:
    names_to_groups: dict[str, list[str]] = {}
    for group, names in ARTIFACT_GROUPS:
        for name in names:
            names_to_groups.setdefault(name.casefold(), []).append(group)

    artifacts: list[Artifact] = []
    total_files = 0
    total_bytes = 0
    dll_files = 0
    exe_files = 0
    for path in walk_files(root):
        try:
            size = path.stat().st_size
        except OSError:
            continue
        total_files += 1
        total_bytes += size
        suffix = path.suffix.casefold()
        dll_files += suffix == ".dll"
        exe_files += suffix == ".exe"
        for group in names_to_groups.get(path.name.casefold(), ()):
            artifacts.append(
                Artifact(
                    group=group,
                    path=path,
                    size=size,
                    sha256=sha256_file(path) if include_hashes else None,
                )
            )

    counts = {
        "total_files": total_files,
        "total_bytes": total_bytes,
        "dll_files": dll_files,
        "exe_files": exe_files,
        "top_level_files": sum(1 for path in root.iterdir() if path.is_file()),
        "top_level_directories": sum(
            1 for path in root.iterdir() if path.is_dir()
        ),
    }
    top_directories = sorted(
        (path.name for path in root.iterdir() if path.is_dir()),
        key=str.casefold,
    )
    return artifacts, counts, top_directories


def find_dumpbin(explicit: Path | None) -> Path | None:
    if explicit is not None:
        return explicit
    executable = shutil.which("dumpbin")
    if executable:
        return Path(executable)

    program_files = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    candidates = sorted(
        program_files.glob(
            "Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/"
            "bin/Hostx64/x64/dumpbin.exe"
        ),
        reverse=True,
    )
    return candidates[0] if candidates else None


def run_dumpbin(dumpbin: Path, option: str, target: Path) -> str:
    result = subprocess.run(
        [str(dumpbin), option, str(target)],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode:
        raise RuntimeError(
            f"dumpbin failed for {target} ({result.returncode}): "
            f"{result.stderr.strip()}"
        )
    return result.stdout


def relative_display(root: Path, path: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def render_markdown(
    root: Path,
    artifacts: Sequence[Artifact],
    counts: dict[str, int],
    top_directories: Sequence[str],
    dumpbin: Path | None,
) -> str:
    lines = [
        "# TDX installation inventory",
        "",
        f"- Root: `{root}`",
        f"- Files: {counts['total_files']:,}",
        f"- Size: {counts['total_bytes']:,} bytes",
        f"- DLL / EXE: {counts['dll_files']} / {counts['exe_files']}",
        (
            "- Top level: "
            f"{counts['top_level_directories']} directories / "
            f"{counts['top_level_files']} files"
        ),
        "",
        "## Top-level directories",
        "",
        ", ".join(f"`{name}`" for name in top_directories) or "(none)",
        "",
        "## Selected artifacts",
        "",
        "| Group | Path | Size | SHA-256 |",
        "|---|---|---:|---|",
    ]
    by_group: dict[str, list[Artifact]] = {}
    for artifact in artifacts:
        by_group.setdefault(artifact.group, []).append(artifact)

    for group, _ in ARTIFACT_GROUPS:
        matches = sorted(
            by_group.get(group, ()),
            key=lambda item: str(item.path).casefold(),
        )
        if not matches:
            lines.append(f"| {group} | *(not found)* | — | — |")
            continue
        for artifact in matches:
            digest = f"`{artifact.sha256}`" if artifact.sha256 else "not requested"
            lines.append(
                f"| {group} | `{relative_display(root, artifact.path)}` | "
                f"{artifact.size:,} | {digest} |"
            )

    lines.extend(["", "## PE evidence", ""])
    if dumpbin is None:
        lines.append("`dumpbin.exe` was not found; dependencies/exports skipped.")
        return "\n".join(lines) + "\n"

    for artifact in artifacts:
        if artifact.path.suffix.casefold() not in {".dll", ".exe"}:
            continue
        display = relative_display(root, artifact.path)
        try:
            dependents = parse_dependencies(
                run_dumpbin(dumpbin, "/dependents", artifact.path)
            )
            exports = parse_exports(run_dumpbin(dumpbin, "/exports", artifact.path))
        except RuntimeError as error:
            lines.extend([f"### `{display}`", "", f"Scan error: `{error}`", ""])
            continue

        lines.extend(
            [
                f"### `{display}`",
                "",
                "**Dependencies:** "
                + (", ".join(f"`{name}`" for name in dependents) or "(none)"),
                "",
                "**Exports:**",
                "",
            ]
        )
        if exports:
            lines.extend(
                f"- `{item.name}` (ordinal {item.ordinal}, RVA 0x{item.rva:08X})"
                for item in exports
            )
        else:
            lines.append("(none)")
        lines.append("")
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Create a read-only inventory of a TDX installation. Configuration "
            "contents and user data are never read."
        )
    )
    parser.add_argument("--root", type=Path, required=True, help="TDX install root")
    parser.add_argument(
        "--dumpbin",
        type=Path,
        help="Path to dumpbin.exe (auto-detected when omitted)",
    )
    parser.add_argument(
        "--hash",
        action="store_true",
        help="Calculate SHA-256 for selected binaries",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Write Markdown here instead of stdout",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    root = args.root.resolve()
    if not root.is_dir():
        print(f"error: install root is not a directory: {root}", file=sys.stderr)
        return 2

    dumpbin = find_dumpbin(args.dumpbin)
    if dumpbin is not None and not dumpbin.is_file():
        print(f"error: dumpbin does not exist: {dumpbin}", file=sys.stderr)
        return 2

    artifacts, counts, top_directories = inventory(root, args.hash)
    report = render_markdown(root, artifacts, counts, top_directories, dumpbin)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    else:
        sys.stdout.write(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

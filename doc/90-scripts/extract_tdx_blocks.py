#!/usr/bin/env python3
"""Export TDX sectors/indices and their security members.

Supported sources under T0002/hq_cache:

* tdxzs3.cfg + tdxhy.cfg: TDX and research industry hierarchies;
* infoharbor_block.dat: concept, style, and index blocks;
* shs.tnf/szs.tnf/bjs.tnf: security names.

The script is offline and read-only.  It does not read custom blocknew files.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Sequence


FAMILY_NAMES = {
    "industry": "通达信行业",
    "research-industry": "研究行业",
    "concept": "概念板块",
    "style": "风格板块",
    "index": "指数板块",
}

FAMILY_CHOICES = tuple(FAMILY_NAMES)

MARKETS = {
    0: ("SZ", "深圳"),
    1: ("SH", "上海"),
    2: ("BJ", "北京"),
}

TNF_FILES = {
    0: "szs.tnf",
    1: "shs.tnf",
    2: "bjs.tnf",
}


class BlockFormatError(ValueError):
    """Raised when a block or security source does not validate."""


@dataclass(frozen=True)
class Security:
    market_id: int
    market: str
    market_name: str
    code: str
    name: str

    @property
    def security_id(self) -> str:
        return f"{self.market}{self.code}"


@dataclass
class Block:
    block_id: str
    family: str
    family_name: str
    block_code: str
    name: str
    source_key: str
    parent_block_id: str
    level: int
    is_leaf: bool
    declared_count: int | None
    member_count: int
    start_date: str
    update_date: str
    source_file: str


@dataclass(frozen=True)
class BlockMember:
    block_id: str
    family: str
    family_name: str
    block_code: str
    block_name: str
    security_id: str
    market_id: int
    market: str
    code: str
    security_name: str
    membership: str
    name_resolved: bool


@dataclass(frozen=True)
class IndustryAssignment:
    market_id: int
    code: str
    industry_code: str
    research_industry_code: str


def read_gb18030(path: Path) -> str:
    try:
        return path.read_bytes().decode("gb18030")
    except UnicodeDecodeError as error:
        raise BlockFormatError(f"invalid GB18030 file: {path}") from error


def decode_fixed(data: bytes, encoding: str) -> str:
    raw = data.split(b"\0", 1)[0]
    return raw.decode(encoding, errors="replace").strip()


def detect_tnf_layout(data: bytes) -> tuple[int, int]:
    if len(data) < 50:
        raise BlockFormatError("TNF file is shorter than its 50-byte header")
    for record_size, name_offset in ((360, 31), (314, 23)):
        if (len(data) - 50) % record_size == 0:
            return record_size, name_offset
    raise BlockFormatError(
        f"unsupported TNF length {len(data)}; expected 50-byte header plus "
        "360- or 314-byte records"
    )


def parse_tnf(data: bytes, market_id: int) -> list[Security]:
    if market_id not in MARKETS:
        raise BlockFormatError(f"unsupported market id: {market_id}")
    record_size, name_offset = detect_tnf_layout(data)
    market, market_name = MARKETS[market_id]
    securities: list[Security] = []
    for offset in range(50, len(data), record_size):
        record = data[offset : offset + record_size]
        code = decode_fixed(record[:6], "ascii")
        if not code:
            continue
        name = decode_fixed(record[name_offset : name_offset + 32], "gb18030")
        securities.append(
            Security(
                market_id=market_id,
                market=market,
                market_name=market_name,
                code=code,
                name=name,
            )
        )
    return securities


def load_security_master(cache: Path) -> dict[tuple[int, str], Security]:
    master: dict[tuple[int, str], Security] = {}
    for market_id, filename in TNF_FILES.items():
        path = cache / filename
        if not path.is_file():
            raise BlockFormatError(f"security master file is missing: {path}")
        for security in parse_tnf(path.read_bytes(), market_id):
            master[(market_id, security.code)] = security
    return master


def industry_parent_key(source_key: str) -> str:
    if len(source_key) <= 3:
        return ""
    return source_key[:-2]


def industry_level(source_key: str) -> int:
    if not source_key or source_key[0] not in {"T", "X"}:
        raise BlockFormatError(f"invalid industry source key: {source_key!r}")
    return (len(source_key) - 1) // 2


def parse_industry_catalog(text: str) -> list[Block]:
    raw: list[tuple[str, str, str, str, bool]] = []
    seen_source_keys: set[str] = set()
    for line_number, line in enumerate(text.splitlines(), 1):
        if not line:
            continue
        fields = line.split("|")
        if len(fields) != 6:
            raise BlockFormatError(
                f"tdxzs3.cfg line {line_number}: expected 6 fields"
            )
        name, block_code, kind, _, leaf, source_key = fields
        if kind not in {"2", "12"}:
            continue
        if source_key in seen_source_keys:
            raise BlockFormatError(
                f"duplicate industry source key: {source_key}"
            )
        seen_source_keys.add(source_key)
        family = "industry" if kind == "2" else "research-industry"
        raw.append((family, name, block_code, source_key, bool(int(leaf))))

    ids = {
        source_key: f"{family}:{source_key}"
        for family, _, _, source_key, _ in raw
    }
    blocks: list[Block] = []
    for family, name, block_code, source_key, is_leaf in raw:
        parent_key = industry_parent_key(source_key)
        blocks.append(
            Block(
                block_id=ids[source_key],
                family=family,
                family_name=FAMILY_NAMES[family],
                block_code=block_code,
                name=name,
                source_key=source_key,
                parent_block_id=ids.get(parent_key, ""),
                level=industry_level(source_key),
                is_leaf=is_leaf,
                declared_count=None,
                member_count=0,
                start_date="",
                update_date="",
                source_file="tdxzs3.cfg+tdxhy.cfg",
            )
        )
    return blocks


def parse_industry_assignments(text: str) -> list[IndustryAssignment]:
    assignments: list[IndustryAssignment] = []
    for line_number, line in enumerate(text.splitlines(), 1):
        if not line:
            continue
        fields = line.split("|")
        if len(fields) != 6:
            raise BlockFormatError(
                f"tdxhy.cfg line {line_number}: expected 6 fields"
            )
        try:
            market_id = int(fields[0])
        except ValueError as error:
            raise BlockFormatError(
                f"tdxhy.cfg line {line_number}: invalid market id"
            ) from error
        assignments.append(
            IndustryAssignment(
                market_id=market_id,
                code=fields[1],
                industry_code=fields[2],
                research_industry_code=fields[5],
            )
        )
    return assignments


def code_prefixes(source_key: str) -> Iterable[str]:
    if not source_key:
        return
    for length in range(3, len(source_key) + 1, 2):
        yield source_key[:length]


def unresolved_security(market_id: int, code: str) -> Security:
    market, market_name = MARKETS.get(
        market_id, (f"M{market_id}", f"市场{market_id}")
    )
    return Security(market_id, market, market_name, code, "")


def build_industry_members(
    blocks: list[Block],
    assignments: list[IndustryAssignment],
    securities: dict[tuple[int, str], Security],
) -> list[BlockMember]:
    by_source = {block.source_key: block for block in blocks}
    members: list[BlockMember] = []
    counts: dict[str, int] = {}
    for assignment in assignments:
        for family, assigned_code in (
            ("industry", assignment.industry_code),
            ("research-industry", assignment.research_industry_code),
        ):
            for prefix in code_prefixes(assigned_code):
                block = by_source.get(prefix)
                if block is None or block.family != family:
                    continue
                security = securities.get(
                    (assignment.market_id, assignment.code),
                    unresolved_security(assignment.market_id, assignment.code),
                )
                members.append(
                    BlockMember(
                        block_id=block.block_id,
                        family=block.family,
                        family_name=block.family_name,
                        block_code=block.block_code,
                        block_name=block.name,
                        security_id=security.security_id,
                        market_id=security.market_id,
                        market=security.market,
                        code=security.code,
                        security_name=security.name,
                        membership=(
                            "direct"
                            if prefix == assigned_code
                            else "descendant"
                        ),
                        name_resolved=bool(security.name),
                    )
                )
                counts[block.block_id] = counts.get(block.block_id, 0) + 1
    for block in blocks:
        block.member_count = counts.get(block.block_id, 0)
    return members


def infoharbor_family(prefix: str) -> str | None:
    return {
        "GN": "concept",
        "FG": "style",
        "ZS": "index",
    }.get(prefix)


def parse_infoharbor(
    text: str,
    securities: dict[tuple[int, str], Security],
) -> tuple[list[Block], list[BlockMember]]:
    blocks: list[Block] = []
    members: list[BlockMember] = []
    current: Block | None = None
    current_count = 0

    def finish_current() -> None:
        nonlocal current
        if current is None:
            return
        if current.declared_count != current_count:
            raise BlockFormatError(
                f"{current.block_id}: declared {current.declared_count} "
                f"members but parsed {current_count}"
            )
        current.member_count = current_count

    for line_number, line in enumerate(text.splitlines(), 1):
        if not line:
            continue
        if line.startswith("#"):
            finish_current()
            fields = line[1:].split(",")
            if len(fields) != 7 or "_" not in fields[0]:
                raise BlockFormatError(
                    f"infoharbor_block.dat line {line_number}: invalid header"
                )
            prefix, name = fields[0].split("_", 1)
            family = infoharbor_family(prefix)
            if family is None:
                raise BlockFormatError(
                    f"unsupported infoharbor family: {prefix}"
                )
            declared_count = int(fields[1] or 0)
            block_code = fields[2]
            stable_key = block_code or name
            current = Block(
                block_id=f"{family}:{stable_key}",
                family=family,
                family_name=FAMILY_NAMES[family],
                block_code=block_code,
                name=name,
                source_key=fields[0],
                parent_block_id="",
                level=1,
                is_leaf=True,
                declared_count=declared_count,
                member_count=0,
                start_date=fields[3],
                update_date=fields[4],
                source_file="infoharbor_block.dat",
            )
            blocks.append(current)
            current_count = 0
            continue

        if current is None:
            raise BlockFormatError(
                f"infoharbor_block.dat line {line_number}: member before header"
            )
        for token in line.split(","):
            if not token:
                continue
            if "#" not in token:
                raise BlockFormatError(
                    f"infoharbor_block.dat line {line_number}: invalid member"
                )
            market_text, code = token.split("#", 1)
            market_id = int(market_text)
            security = securities.get(
                (market_id, code),
                unresolved_security(market_id, code),
            )
            members.append(
                BlockMember(
                    block_id=current.block_id,
                    family=current.family,
                    family_name=current.family_name,
                    block_code=current.block_code,
                    block_name=current.name,
                    security_id=security.security_id,
                    market_id=security.market_id,
                    market=security.market,
                    code=security.code,
                    security_name=security.name,
                    membership="direct",
                    name_resolved=bool(security.name),
                )
            )
            current_count += 1
    finish_current()
    return blocks, members


def extract(
    root: Path,
    families: set[str],
) -> tuple[dict[tuple[int, str], Security], list[Block], list[BlockMember]]:
    cache = root / "T0002" / "hq_cache"
    if not cache.is_dir():
        raise BlockFormatError(f"TDX hq_cache directory is missing: {cache}")
    securities = load_security_master(cache)

    blocks: list[Block] = []
    members: list[BlockMember] = []
    if families & {"industry", "research-industry"}:
        industry_blocks = parse_industry_catalog(
            read_gb18030(cache / "tdxzs3.cfg")
        )
        industry_blocks = [
            block for block in industry_blocks if block.family in families
        ]
        assignments = parse_industry_assignments(
            read_gb18030(cache / "tdxhy.cfg")
        )
        blocks.extend(industry_blocks)
        members.extend(
            build_industry_members(industry_blocks, assignments, securities)
        )

    if families & {"concept", "style", "index"}:
        other_blocks, other_members = parse_infoharbor(
            read_gb18030(cache / "infoharbor_block.dat"),
            securities,
        )
        selected_ids = {
            block.block_id for block in other_blocks if block.family in families
        }
        blocks.extend(
            block for block in other_blocks if block.block_id in selected_ids
        )
        members.extend(
            member for member in other_members if member.block_id in selected_ids
        )

    blocks.sort(key=lambda item: (item.family, item.block_code, item.name))
    members.sort(
        key=lambda item: (
            item.family,
            item.block_code,
            item.block_name,
            item.market_id,
            item.code,
        )
    )
    return securities, blocks, members


def render_csv(rows: Iterable[object], fieldnames: list[str]) -> str:
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=fieldnames, lineterminator="\n")
    writer.writeheader()
    writer.writerows(asdict(row) for row in rows)
    return output.getvalue()


def write_text(path: Path, text: str, encoding: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding=encoding, newline="") as stream:
        stream.write(text)


def write_outputs(
    output_dir: Path,
    format_: str,
    root: Path,
    blocks: list[Block],
    members: list[BlockMember],
) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    if format_ == "json":
        path = output_dir / "tdx-blocks.json"
        document = {
            "schema_version": 1,
            "source_root": str(root),
            "families": {
                family: {
                    "blocks": sum(block.family == family for block in blocks),
                    "memberships": sum(
                        member.family == family for member in members
                    ),
                }
                for family in FAMILY_CHOICES
                if any(block.family == family for block in blocks)
            },
            "blocks": [asdict(block) for block in blocks],
            "members": [asdict(member) for member in members],
        }
        write_text(
            path,
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            "utf-8",
        )
        return [path]

    blocks_path = output_dir / "tdx-blocks.csv"
    members_path = output_dir / "tdx-block-members.csv"
    write_text(
        blocks_path,
        render_csv(blocks, list(Block.__dataclass_fields__)),
        "utf-8-sig",
    )
    write_text(
        members_path,
        render_csv(members, list(BlockMember.__dataclass_fields__)),
        "utf-8-sig",
    )
    return [blocks_path, members_path]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Read TDX public cache files offline and export industry, concept, "
            "style, and index blocks with their security members."
        )
    )
    parser.add_argument("--root", type=Path, required=True, help="TDX install root")
    parser.add_argument(
        "--family",
        action="append",
        choices=FAMILY_CHOICES,
        help="family to include; repeat as needed (default: all)",
    )
    parser.add_argument(
        "--format",
        choices=("csv", "json"),
        default="csv",
        help="output format (default: csv)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="directory for generated files",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    root = args.root.resolve()
    if not root.is_dir():
        print(f"error: TDX install root is missing: {root}", file=sys.stderr)
        return 2
    families = set(args.family or FAMILY_CHOICES)
    try:
        securities, blocks, members = extract(root, families)
        outputs = write_outputs(
            args.output_dir.resolve(),
            args.format,
            root,
            blocks,
            members,
        )
    except (OSError, UnicodeError, BlockFormatError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    unresolved = sum(not member.name_resolved for member in members)
    summary = ", ".join(
        f"{family}={sum(block.family == family for block in blocks)}"
        for family in FAMILY_CHOICES
        if family in families
    )
    print(
        f"exported {len(blocks)} blocks ({summary}), "
        f"{len(members)} memberships, {len(securities)} securities, "
        f"{unresolved} unresolved member names -> "
        + ", ".join(str(path) for path in outputs),
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

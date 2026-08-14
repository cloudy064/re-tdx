#!/usr/bin/env python3
"""Extract built-in formula metadata from a supported TCalc.dll.

The extractor is offline and read-only.  It parses the PE section table, maps
known RVAs for a fingerprinted TCalc build, and reads only the inline metadata
of each embedded 5072-byte formula record.  It does not load the DLL or touch
PriGS.dat/PriCS.dat.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Sequence


RECORD_SIZE = 5072

KIND_KEYS = {
    0: "technical",
    1: "selection",
    2: "expert",
    3: "color-k",
    4: "reserved",
}

KIND_NAMES = {
    0: "技术指标公式",
    1: "条件选股公式",
    2: "专家系统公式",
    3: "五彩K线公式",
    4: "内部保留集合",
}

KIND_CHOICES = ("all", "technical", "selection", "expert", "color-k")


class TCalcFormatError(ValueError):
    """Raised when a PE image or TCalc profile does not validate."""


@dataclass(frozen=True)
class Section:
    name: str
    virtual_address: int
    virtual_size: int
    raw_offset: int
    raw_size: int


@dataclass(frozen=True)
class TableSpec:
    kind: int
    records_rva: int
    count_rva: int
    expected_count: int


@dataclass(frozen=True)
class TreeSpec:
    kind: int
    records_rva: int
    count_rva: int
    expected_count: int


@dataclass(frozen=True)
class BuildProfile:
    name: str
    sha256: str
    description: str
    tables: tuple[TableSpec, ...]
    trees: tuple[TreeSpec, ...]


@dataclass(frozen=True)
class Category:
    id: int
    parent: int
    name: str
    flags: int


@dataclass(frozen=True)
class Formula:
    kind: int
    kind_key: str
    kind_name: str
    index: int
    code: str
    name: str
    category_id: int
    category_name: str
    display_flags: int
    attribute_flags: int
    source: str
    is_custom: bool


PROFILES = (
    BuildProfile(
        name="tdx-2025-11-14",
        sha256=(
            "13facaa52dac552c5be1f63331781219de9bf798c443af8f5e4afc193a02e7f5"
        ),
        description="PE32 TCalc.dll, file timestamp 2025-11-14 16:10:21",
        tables=(
            TableSpec(0, 0x00133888, 0x002466E8, 222),
            TableSpec(1, 0x002466F0, 0x002466EC, 107),
            TableSpec(2, 0x002CAEE0, 0x002DD810, 15),
            TableSpec(3, 0x002DD818, 0x002DD814, 35),
        ),
        trees=(
            TreeSpec(0, 0x00127DB0, 0x00127DAC, 16),
            TreeSpec(1, 0x00128070, 0x00128178, 6),
        ),
    ),
)


class PEImage:
    """Minimal PE32/PE32+ reader with validated RVA-to-file mapping."""

    def __init__(self, data: bytes) -> None:
        self.data = data
        if len(data) < 0x40 or data[:2] != b"MZ":
            raise TCalcFormatError("not a DOS/PE image")

        pe_offset = self._unpack_from("<I", 0x3C)[0]
        if self._slice(pe_offset, 4) != b"PE\0\0":
            raise TCalcFormatError("PE signature not found")

        coff_offset = pe_offset + 4
        number_of_sections = self._unpack_from("<H", coff_offset + 2)[0]
        optional_size = self._unpack_from("<H", coff_offset + 16)[0]
        optional_offset = coff_offset + 20
        magic = self._unpack_from("<H", optional_offset)[0]
        if magic == 0x10B:
            self.image_base = self._unpack_from("<I", optional_offset + 28)[0]
        elif magic == 0x20B:
            self.image_base = self._unpack_from("<Q", optional_offset + 24)[0]
        else:
            raise TCalcFormatError(
                f"unsupported PE optional-header magic 0x{magic:04X}"
            )
        self.size_of_headers = self._unpack_from(
            "<I", optional_offset + 60
        )[0]

        section_offset = optional_offset + optional_size
        sections: list[Section] = []
        for index in range(number_of_sections):
            offset = section_offset + index * 40
            header = self._slice(offset, 40)
            name = header[:8].split(b"\0", 1)[0].decode(
                "ascii", errors="replace"
            )
            virtual_size, virtual_address, raw_size, raw_offset = (
                struct.unpack_from("<IIII", header, 8)
            )
            sections.append(
                Section(
                    name=name,
                    virtual_address=virtual_address,
                    virtual_size=virtual_size,
                    raw_offset=raw_offset,
                    raw_size=raw_size,
                )
            )
        self.sections = tuple(sections)

    @classmethod
    def from_path(cls, path: Path) -> "PEImage":
        return cls(path.read_bytes())

    def _slice(self, offset: int, size: int) -> bytes:
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise TCalcFormatError(
                f"file range is out of bounds: offset=0x{offset:X}, size={size}"
            )
        return self.data[offset : offset + size]

    def _unpack_from(self, format_: str, offset: int) -> tuple[int, ...]:
        size = struct.calcsize(format_)
        return struct.unpack(format_, self._slice(offset, size))

    def rva_to_offset(self, rva: int, size: int = 1) -> int:
        if 0 <= rva < self.size_of_headers:
            self._slice(rva, size)
            return rva

        for section in self.sections:
            span = max(section.virtual_size, section.raw_size)
            if section.virtual_address <= rva < section.virtual_address + span:
                relative = rva - section.virtual_address
                if relative + size > section.raw_size:
                    raise TCalcFormatError(
                        f"RVA 0x{rva:X} points into unbacked bytes of "
                        f"section {section.name!r}"
                    )
                offset = section.raw_offset + relative
                self._slice(offset, size)
                return offset
        raise TCalcFormatError(f"RVA 0x{rva:X} is not mapped by any PE section")

    def read_rva(self, rva: int, size: int) -> bytes:
        return self._slice(self.rva_to_offset(rva, size), size)

    def u16(self, rva: int) -> int:
        return struct.unpack("<H", self.read_rva(rva, 2))[0]

    def u32(self, rva: int) -> int:
        return struct.unpack("<I", self.read_rva(rva, 4))[0]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def decode_fixed_string(data: bytes) -> str:
    raw = data.split(b"\0", 1)[0]
    try:
        return raw.decode("gb18030")
    except UnicodeDecodeError as error:
        raise TCalcFormatError(
            f"invalid GB18030 metadata string: {raw[:32]!r}"
        ) from error


def formula_source(flags: int) -> str:
    if flags & 0x01:
        return "system"
    if flags & 0x10:
        return "temporary"
    if flags & 0x800:
        return "default"
    return "user"


def profile_by_hash(digest: str) -> BuildProfile | None:
    folded = digest.casefold()
    return next(
        (profile for profile in PROFILES if profile.sha256 == folded),
        None,
    )


def selected_kind_ids(choice: str) -> set[int]:
    if choice == "all":
        return {0, 1, 2, 3}
    for kind, key in KIND_KEYS.items():
        if key == choice:
            return {kind}
    raise ValueError(f"unknown formula kind: {choice}")


def extract_categories(
    image: PEImage, profile: BuildProfile
) -> dict[int, list[Category]]:
    result: dict[int, list[Category]] = {}
    for spec in profile.trees:
        count = image.u16(spec.count_rva)
        if count != spec.expected_count:
            raise TCalcFormatError(
                f"{profile.name}: kind {spec.kind} category count is {count}, "
                f"expected {spec.expected_count}"
            )
        categories: list[Category] = []
        for index in range(count):
            record = image.read_rva(spec.records_rva + index * 44, 44)
            category_id, parent = struct.unpack_from("<II", record)
            categories.append(
                Category(
                    id=category_id,
                    parent=parent,
                    name=decode_fixed_string(record[8:40]),
                    flags=struct.unpack_from("<I", record, 40)[0],
                )
            )
        result[spec.kind] = categories
    return result


def extract_formulas(
    image: PEImage,
    profile: BuildProfile,
    kinds: set[int],
) -> tuple[dict[int, list[Category]], list[Formula]]:
    categories = extract_categories(image, profile)
    category_names = {
        kind: {item.id: item.name for item in items}
        for kind, items in categories.items()
    }
    formulas: list[Formula] = []
    for spec in profile.tables:
        count = image.u32(spec.count_rva)
        if count != spec.expected_count:
            raise TCalcFormatError(
                f"{profile.name}: kind {spec.kind} formula count is {count}, "
                f"expected {spec.expected_count}"
            )
        if spec.kind not in kinds:
            continue
        for index in range(count):
            record = image.read_rva(
                spec.records_rva + index * RECORD_SIZE,
                RECORD_SIZE,
            )
            code = decode_fixed_string(record[3:17])
            name = decode_fixed_string(record[17:67])
            if not code or not name:
                raise TCalcFormatError(
                    f"empty formula metadata at kind={spec.kind}, index={index}"
                )
            category_id = record[67]
            attribute_flags = struct.unpack_from("<I", record, 5068)[0]
            formulas.append(
                Formula(
                    kind=spec.kind,
                    kind_key=KIND_KEYS[spec.kind],
                    kind_name=KIND_NAMES[spec.kind],
                    index=index,
                    code=code,
                    name=name,
                    category_id=category_id,
                    category_name=category_names.get(spec.kind, {}).get(
                        category_id, ""
                    ),
                    display_flags=struct.unpack_from("<H", record, 68)[0],
                    attribute_flags=attribute_flags,
                    source=formula_source(attribute_flags),
                    is_custom=bool(attribute_flags & 0x02),
                )
            )
    return categories, formulas


def render_json(
    source: Path,
    digest: str,
    profile: BuildProfile,
    categories: dict[int, list[Category]],
    formulas: list[Formula],
) -> str:
    counts = {
        KIND_KEYS[kind]: sum(item.kind == kind for item in formulas)
        for kind in sorted({item.kind for item in formulas})
    }
    document = {
        "schema_version": 1,
        "source_file": source.name,
        "sha256": digest,
        "profile": profile.name,
        "counts": counts,
        "categories": {
            KIND_KEYS[kind]: [asdict(item) for item in items]
            for kind, items in sorted(categories.items())
            if any(formula.kind == kind for formula in formulas)
        },
        "formulas": [asdict(item) for item in formulas],
    }
    return json.dumps(document, ensure_ascii=False, indent=2) + "\n"


def render_csv(formulas: list[Formula]) -> str:
    output = io.StringIO(newline="")
    fieldnames = list(Formula.__dataclass_fields__)
    writer = csv.DictWriter(
        output,
        fieldnames=fieldnames,
        lineterminator="\n",
    )
    writer.writeheader()
    writer.writerows(asdict(item) for item in formulas)
    return output.getvalue()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Read a fingerprinted TCalc.dll offline and export its built-in "
            "formula metadata. The DLL is never loaded and user formula files "
            "are never opened."
        )
    )
    parser.add_argument("--dll", type=Path, required=True, help="TCalc.dll path")
    parser.add_argument(
        "--kind",
        choices=KIND_CHOICES,
        default="technical",
        help="formula family to export (default: technical)",
    )
    parser.add_argument(
        "--format",
        choices=("json", "csv"),
        default="json",
        help="output format (default: json)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="write here instead of stdout; CSV files use UTF-8 with BOM",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    dll = args.dll.resolve()
    if not dll.is_file():
        print(f"error: DLL does not exist: {dll}", file=sys.stderr)
        return 2

    try:
        image = PEImage.from_path(dll)
        digest = sha256_bytes(image.data)
        profile = profile_by_hash(digest)
        if profile is None:
            supported = ", ".join(item.name for item in PROFILES)
            raise TCalcFormatError(
                f"unsupported TCalc.dll SHA-256 {digest}; "
                f"known profiles: {supported}"
            )
        categories, formulas = extract_formulas(
            image,
            profile,
            selected_kind_ids(args.kind),
        )
        if args.format == "csv":
            report = render_csv(formulas)
        else:
            report = render_json(dll, digest, profile, categories, formulas)
    except (OSError, TCalcFormatError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        encoding = "utf-8-sig" if args.format == "csv" else "utf-8"
        with args.output.open("w", encoding=encoding, newline="") as stream:
            stream.write(report)
        destination = str(args.output.resolve())
    else:
        sys.stdout.write(report)
        destination = "stdout"

    counts: dict[str, int] = {}
    for formula in formulas:
        counts[formula.kind_key] = counts.get(formula.kind_key, 0) + 1
    summary = ", ".join(f"{key}={value}" for key, value in counts.items())
    print(
        f"extracted {len(formulas)} formulas ({summary}) to {destination}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

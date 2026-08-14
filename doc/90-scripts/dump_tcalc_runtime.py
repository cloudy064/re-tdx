#!/usr/bin/env python3
"""Dump the initialized TCalc formula index from a running TdxW.exe.

The script fingerprints both loaded module files before calling TCalc.  It
then invokes only the read-only CMainCalcInterface getters in the already
initialized process.  It never calls InitMain, SaveIndex, CompileGSIndex, or
formula mutation APIs.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Sequence

from extract_tcalc_formulas import (
    Formula,
    KIND_CHOICES,
    KIND_KEYS,
    KIND_NAMES,
    formula_source,
    render_csv,
    selected_kind_ids,
    sha256_bytes,
)


@dataclass(frozen=True)
class RuntimeProfile:
    name: str
    tdxw_sha256: str
    tcalc_sha256: str
    interface_slot_rva: int


RUNTIME_PROFILES = (
    RuntimeProfile(
        name="tdx-2025-11-14",
        tdxw_sha256=(
            "f5f2e6025a4d80bb1afbcb2a51c753d3c1909e09aa9d1bc8f7c3b701b081f74c"
        ),
        tcalc_sha256=(
            "13facaa52dac552c5be1f63331781219de9bf798c443af8f5e4afc193a02e7f5"
        ),
        interface_slot_rva=0x00CFCF54,
    ),
)


JS_SOURCE = r"""
'use strict';

const EXPORTS = {
  getIndexNum: '?GetIndexNum@CMainCalcInterface@@QAEJE@Z',
  getIndexInfo: '?GetIndexInfo@CMainCalcInterface@@QAEPAUtag_INDEXINFO@@EJ@Z',
  getTreeInfo: '?GetTreeInfo@CMainCalcInterface@@QAEJPAXHH@Z'
};

function moduleOrThrow(name) {
  const module = Process.findModuleByName(name);
  if (module === null) {
    throw new Error(name + ' is not loaded');
  }
  return module;
}

function exportOrThrow(module, name) {
  const address = module.findExportByName(name);
  if (address === null) {
    throw new Error('missing export ' + name);
  }
  return address;
}

function hexCString(address, maximum) {
  let result = '';
  for (let i = 0; i < maximum; ++i) {
    const value = address.add(i).readU8();
    if (value === 0) {
      break;
    }
    result += ('0' + value.toString(16)).slice(-2);
  }
  return result;
}

function validateInterface(slotRva) {
  const host = moduleOrThrow('TdxW.exe');
  const calc = moduleOrThrow('TCalc.dll');
  const slot = host.base.add(slotRva);
  const slotRange = Process.findRangeByAddress(slot);
  if (slotRange === null || slotRange.protection.indexOf('r') < 0) {
    throw new Error('interface slot is not readable: ' + slot);
  }
  const iface = slot.readPointer();
  if (iface.isNull()) {
    throw new Error(
      'CMainCalcInterface is not initialized; open a K-line/indicator or ' +
      'formula-manager view and retry'
    );
  }
  const ifaceRange = Process.findRangeByAddress(iface);
  if (ifaceRange === null || ifaceRange.protection.indexOf('r') < 0) {
    throw new Error('CMainCalcInterface pointer is not readable: ' + iface);
  }
  const vtable = iface.readPointer();
  const calcEnd = calc.base.add(calc.size);
  if (vtable.compare(calc.base) < 0 || vtable.compare(calcEnd) >= 0) {
    throw new Error(
      'unexpected CMainCalcInterface vtable ' + vtable +
      ' outside TCalc.dll'
    );
  }
  return {host, calc, slot, iface, vtable};
}

function readTree(getTreeInfo, iface, kind, mode) {
  const count = getTreeInfo(iface, ptr(0), mode, -1);
  if (count < 0 || count > 1000) {
    throw new Error('invalid tree count for mode ' + mode + ': ' + count);
  }
  if (count === 0) {
    return [];
  }
  const buffer = Memory.alloc(count * 44);
  const copied = getTreeInfo(iface, buffer, mode, -1);
  if (copied !== count) {
    throw new Error(
      'tree count changed for mode ' + mode + ': ' + count + ' -> ' + copied
    );
  }
  const records = [];
  for (let i = 0; i < count; ++i) {
    const record = buffer.add(i * 44);
    records.push({
      kind: kind,
      id: record.readU32(),
      parent: record.add(4).readU32(),
      name_hex: hexCString(record.add(8), 32),
      flags: record.add(40).readU32()
    });
  }
  return records;
}

rpc.exports = {
  modules: function () {
    const host = moduleOrThrow('TdxW.exe');
    const calc = moduleOrThrow('TCalc.dll');
    return {
      pid: Process.id,
      arch: Process.arch,
      pointer_size: Process.pointerSize,
      tdxw_path: host.path,
      tdxw_base: host.base.toString(),
      tdxw_size: host.size,
      tcalc_path: calc.path,
      tcalc_base: calc.base.toString(),
      tcalc_size: calc.size
    };
  },

  probe: function (slotRva) {
    const state = validateInterface(slotRva);
    return {
      slot: state.slot.toString(),
      interface: state.iface.toString(),
      vtable: state.vtable.toString()
    };
  },

  dump: function (slotRva, kinds) {
    const state = validateInterface(slotRva);
    const getIndexNum = new NativeFunction(
      exportOrThrow(state.calc, EXPORTS.getIndexNum),
      'int',
      ['pointer', 'uchar'],
      'thiscall'
    );
    const getIndexInfo = new NativeFunction(
      exportOrThrow(state.calc, EXPORTS.getIndexInfo),
      'pointer',
      ['pointer', 'uchar', 'int'],
      'thiscall'
    );
    const getTreeInfo = new NativeFunction(
      exportOrThrow(state.calc, EXPORTS.getTreeInfo),
      'int',
      ['pointer', 'pointer', 'int', 'int'],
      'thiscall'
    );

    const limits = [5000, 2000, 500, 500, 500];
    const formulas = [];
    const counts = {};
    for (const kind of kinds) {
      if (kind < 0 || kind > 4) {
        throw new Error('kind is out of range: ' + kind);
      }
      const count = getIndexNum(state.iface, kind);
      if (count < 0 || count > limits[kind]) {
        throw new Error('invalid formula count for kind ' + kind + ': ' + count);
      }
      counts[kind.toString()] = count;
      for (let index = 0; index < count; ++index) {
        const info = getIndexInfo(state.iface, kind, index);
        if (info.isNull()) {
          throw new Error(
            'GetIndexInfo returned null for kind=' + kind + ', index=' + index
          );
        }
        formulas.push({
          kind: kind,
          index: index,
          code_hex: hexCString(info.add(3), 14),
          name_hex: hexCString(info.add(17), 50),
          category_id: info.add(67).readU8(),
          display_flags: info.add(68).readU16(),
          attribute_flags: info.add(5068).readU32()
        });
      }
    }

    let categories = [];
    if (kinds.indexOf(0) !== -1) {
      categories = categories.concat(readTree(getTreeInfo, state.iface, 0, 0));
    }
    if (kinds.indexOf(1) !== -1) {
      categories = categories.concat(readTree(getTreeInfo, state.iface, 1, 1));
    }
    return {counts, categories, formulas};
  }
};
"""


def decode_hex_string(value: str) -> str:
    return bytes.fromhex(value).decode("gb18030")


def runtime_profile(digest: str) -> RuntimeProfile | None:
    folded = digest.casefold()
    return next(
        (item for item in RUNTIME_PROFILES if item.tdxw_sha256 == folded),
        None,
    )


def normalize_dump(
    raw: dict[str, Any],
) -> tuple[dict[int, list[dict[str, Any]]], list[Formula]]:
    categories: dict[int, list[dict[str, Any]]] = {}
    category_names: dict[int, dict[int, str]] = {}
    for item in raw["categories"]:
        kind = int(item["kind"])
        normalized = {
            "id": int(item["id"]),
            "parent": int(item["parent"]),
            "name": decode_hex_string(item["name_hex"]),
            "flags": int(item["flags"]),
        }
        categories.setdefault(kind, []).append(normalized)
        category_names.setdefault(kind, {})[normalized["id"]] = normalized["name"]

    formulas: list[Formula] = []
    for item in raw["formulas"]:
        kind = int(item["kind"])
        flags = int(item["attribute_flags"])
        category_id = int(item["category_id"])
        formulas.append(
            Formula(
                kind=kind,
                kind_key=KIND_KEYS[kind],
                kind_name=KIND_NAMES[kind],
                index=int(item["index"]),
                code=decode_hex_string(item["code_hex"]),
                name=decode_hex_string(item["name_hex"]),
                category_id=category_id,
                category_name=category_names.get(kind, {}).get(category_id, ""),
                display_flags=int(item["display_flags"]),
                attribute_flags=flags,
                source=formula_source(flags),
                is_custom=bool(flags & 0x02),
            )
        )
    return categories, formulas


def render_json(
    modules: dict[str, Any],
    probe: dict[str, Any],
    profile: RuntimeProfile,
    tdxw_sha256: str,
    tcalc_sha256: str,
    raw: dict[str, Any],
    categories: dict[int, list[dict[str, Any]]],
    formulas: list[Formula],
) -> str:
    document = {
        "schema_version": 1,
        "profile": profile.name,
        "runtime": {
            "pid": modules["pid"],
            "arch": modules["arch"],
            "pointer_size": modules["pointer_size"],
            "tdxw_file": Path(modules["tdxw_path"]).name,
            "tdxw_sha256": tdxw_sha256,
            "tcalc_file": Path(modules["tcalc_path"]).name,
            "tcalc_sha256": tcalc_sha256,
            "interface_slot": probe["slot"],
            "interface": probe["interface"],
            "vtable": probe["vtable"],
        },
        "counts": {
            KIND_KEYS[int(kind)]: int(count)
            for kind, count in raw["counts"].items()
        },
        "categories": {
            KIND_KEYS[kind]: items for kind, items in sorted(categories.items())
        },
        "formulas": [asdict(item) for item in formulas],
    }
    return json.dumps(document, ensure_ascii=False, indent=2) + "\n"


def find_process_id(device: Any, process_name: str) -> int:
    matches = [
        process.pid
        for process in device.enumerate_processes()
        if process.name.casefold() == process_name.casefold()
    ]
    if not matches:
        raise RuntimeError(f"process is not running: {process_name}")
    if len(matches) > 1:
        raise RuntimeError(
            f"multiple {process_name} processes found; use --pid: {matches}"
        )
    return matches[0]


def rpc_exports(script: Any) -> Any:
    return getattr(script, "exports_sync", None) or script.exports


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Attach to an initialized, fingerprinted TdxW.exe and dump TCalc "
            "formula metadata through read-only getter methods."
        )
    )
    process = parser.add_mutually_exclusive_group()
    process.add_argument("--pid", type=int, help="TdxW.exe process ID")
    process.add_argument(
        "--process",
        default="TdxW.exe",
        help="process name when --pid is omitted (default: TdxW.exe)",
    )
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
    try:
        import frida
    except ImportError:
        print(
            "error: Python package 'frida' is required for runtime extraction",
            file=sys.stderr,
        )
        return 2

    device = frida.get_local_device()
    try:
        pid = args.pid or find_process_id(device, args.process)
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    session = None
    try:
        session = device.attach(pid)
        script = session.create_script(JS_SOURCE)
        script.load()
        rpc = rpc_exports(script)

        modules = rpc.modules()
        tdxw_path = Path(modules["tdxw_path"])
        tcalc_path = Path(modules["tcalc_path"])
        tdxw_sha256 = sha256_bytes(tdxw_path.read_bytes())
        tcalc_sha256 = sha256_bytes(tcalc_path.read_bytes())
        profile = runtime_profile(tdxw_sha256)
        if profile is None:
            raise RuntimeError(
                f"unsupported TdxW.exe SHA-256 {tdxw_sha256}"
            )
        if tcalc_sha256 != profile.tcalc_sha256:
            raise RuntimeError(
                f"TCalc.dll SHA-256 mismatch: {tcalc_sha256}; "
                f"expected {profile.tcalc_sha256}"
            )

        probe = rpc.probe(profile.interface_slot_rva)
        kinds = sorted(selected_kind_ids(args.kind))
        raw = rpc.dump(profile.interface_slot_rva, kinds)
        categories, formulas = normalize_dump(raw)
        if args.format == "csv":
            report = render_csv(formulas)
        else:
            report = render_json(
                modules,
                probe,
                profile,
                tdxw_sha256,
                tcalc_sha256,
                raw,
                categories,
                formulas,
            )
    except Exception as error:
        print(f"error: runtime extraction failed: {error}", file=sys.stderr)
        return 1
    finally:
        if session is not None:
            session.detach()

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
        f"dumped {len(formulas)} formulas ({summary}) from PID {pid} "
        f"to {destination}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

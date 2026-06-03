#!/usr/bin/env python3
"""Decode MM2 random/fixed encounter configuration.

Random step rolls use ``attrib.dat`` bytes ``0x09``–``0x0D`` (per screen).
Monster type pools live in embedded exe data (``mm2_data_00.bin`` via
``file_off = 0x7FFE - a4_disp``). Fixed fights are ``event.dat`` OP_12/13.

See EXTRACTED/docs/35-encounter-tables.md.

Usage:
    python tools/decode_encounters.py
    python tools/decode_encounters.py --json EXTRACTED/encounter_tables.json
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from attrib_codec import AttribFile  # noqa: E402

DATA_HUNK = ROOT / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"
ATTRIB_DAT = ROOT / "attrib.dat"

# A4-relative displacements (positive = subtract from 0x7FFE for file offset).
A4_ENCOUNTER_AREA_INDEX = 0x718A  # lea @ 0x9E96 (arena/ticket path)
A4_DISPOSITION_MOD = 0x6FC0  # lea @ 0x11F14
A4_PARTY_XP_BUDGET = 0x6FCA  # cmp.l @ 0x12110

def _area_name(area_id: int) -> str:
    return f"Area {area_id}"


def _a4_slice(data: bytes, disp: int, size: int) -> bytes:
    off = 0x7FFE - disp
    if off < 0 or off + size > len(data):
        raise ValueError(f"A4-${disp:04X} -> file 0x{off:X} out of range")
    return data[off : off + size]


def load_embedded(data: bytes) -> dict:
    area_raw = _a4_slice(data, A4_ENCOUNTER_AREA_INDEX, 60)
    disp = list(_a4_slice(data, A4_DISPOSITION_MOD, 4))
    xp_budget = struct.unpack(">I", _a4_slice(data, A4_PARTY_XP_BUDGET, 4))[0]
    return {
        "area_pool_index_raw": list(area_raw),
        "area_pool_index_file_off": 0x7FFE - A4_ENCOUNTER_AREA_INDEX,
        "disposition_mod": disp,
        "party_xp_budget_initial": xp_budget,
    }


def load_attrib_tuning(path: Path) -> list[dict]:
    records = AttribFile.decode(path.read_bytes()).records
    out = []
    for rec in records:
        raw = rec.raw
        out.append(
            {
                "area": rec.area_id,
                "name": _area_name(rec.area_id),
                "rate_denom": raw[0x09],
                "group_size_gate": raw[0x0A],
                "max_monsters": raw[0x0B],
                "min_monsters": raw[0x0C],
                "retreat_difficulty": raw[0x0D],
            }
        )
    return out


def build_report(attrib_path: Path, data_path: Path) -> dict:
    embedded = load_embedded(data_path.read_bytes())
    tuning = load_attrib_tuning(attrib_path)
    for row in tuning:
        idx = row["area"]
        row["arena_pool_byte"] = embedded["area_pool_index_raw"][idx]
    return {
        "attrib_path": str(attrib_path),
        "data_hunk_path": str(data_path),
        "embedded": embedded,
        "per_area": tuning,
        "notes": [
            "attrib 0x09: RNG(1, rate_denom)==1 gates random step roll @ 0x10A2",
            "attrib 0x0A-0x0C: group size clamps in 0x11F0A / 0x12124",
            "attrib 0x0D: run/retreat threshold @ 0x116CA / 0x13148",
            "A4-$718A: indexed by current screen @ 0x9E96 (arena ticket fights); "
            "full random-pool blob layout still partial",
            "Fixed fights: event.dat OP_12/13 @ 0x16300 -> A4-$11DE[]",
        ],
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="Dump MM2 encounter tables")
    ap.add_argument("--attrib", type=Path, default=ATTRIB_DAT)
    ap.add_argument("--data", type=Path, default=DATA_HUNK)
    ap.add_argument("--json", type=Path, help="Write JSON report")
    args = ap.parse_args()

    report = build_report(args.attrib, args.data)
    if args.json:
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"Wrote {args.json}")

    print("Per-area encounter tuning (attrib 0x09-0x0D):")
    print(" area  rate grp  max min run  arena_byte  name")
    for row in report["per_area"]:
        print(
            f" {row['area']:3d}  {row['rate_denom']:4d} {row['group_size_gate']:3d}"
            f" {row['max_monsters']:3d} {row['min_monsters']:3d} {row['retreat_difficulty']:3d}"
            f"     {row['arena_pool_byte']:3d}       {row['name']}"
        )
    emb = report["embedded"]
    print(f"\nEmbedded A4-${A4_ENCOUNTER_AREA_INDEX:04X} @ data+0x{emb['area_pool_index_file_off']:X}:")
    print(" ", emb["area_pool_index_raw"])
    print("Disposition mod:", emb["disposition_mod"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

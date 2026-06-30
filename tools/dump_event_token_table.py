#!/usr/bin/env python3
"""Dump the event VM token-length table (A4-$6CC8) from the ROM data hunk.

The skip helper @ 0x157FC reads ``word_table[token_byte * 2]`` and adds that
delta to ``event_parse_pos`` (A4-$7956). The table is **real static data** in
the data hunk, NOT something we can infer from opcode arg counts — so this tool
reads it directly from ``EXTRACTED/ghidra/mm2_data_00.bin``.

Layout: A4 = data_base + 0x7FFE, so the table file offset is
``0x7FFE - 0x6CC8 = 0x1336``; 256 big-endian u16 entries (68k ``move.w``).

It also cross-checks each delta against the decoder's ``1 + argc`` assumption
and reports any opcode where the real skip length differs (e.g. OP_25, whose
handler reads 2 bytes but whose skip delta is only 2 = opcode + 1).

Output: EXTRACTED/event_token_len_table.json
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from decode_event import OPCODES  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"
OUT = ROOT / "EXTRACTED" / "event_token_len_table.json"

A4_BASE = 0x7FFE
TABLE_DISP = 0x6CC8           # A4-$6CC8
TABLE_OFF = A4_BASE - TABLE_DISP  # 0x1336
ENTRY_COUNT = 256


def read_real_table(data: bytes) -> list[int]:
    return [struct.unpack_from(">H", data, TABLE_OFF + 2 * i)[0] for i in range(ENTRY_COUNT)]


def build_rows(real: list[int]) -> tuple[list[dict], list[dict]]:
    rows: list[dict] = []
    mismatches: list[dict] = []
    for b in range(ENTRY_COUNT):
        delta = real[b]
        spec = OPCODES.get(b)
        if spec is None:
            note = "data/unused" if b > 0x32 else "unknown"
            argc_delta = None
        else:
            argc = spec["argc"]
            argc_delta = 2 if argc is None else 1 + int(argc)
            note = str(spec["name"])
        row = {"byte": b, "delta": delta, "note": note}
        if argc_delta is not None and delta != argc_delta:
            row["argc_delta"] = argc_delta
            mismatches.append(
                {"byte": b, "name": note, "rom_delta": delta, "argc_delta": argc_delta}
            )
        rows.append(row)
    return rows, mismatches


def main() -> None:
    if not DATA.exists():
        sys.exit(f"missing data hunk: {DATA}\n(extract it first; see EXTRACTED/ghidra/)")
    data = DATA.read_bytes()
    real = read_real_table(data)
    rows, mismatches = build_rows(real)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(
        json.dumps(
            {
                "source": "EXTRACTED/ghidra/mm2_data_00.bin @ A4-$6CC8 (file off 0x1336)",
                "word_indexed": True,
                "entries": rows,
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"Wrote {OUT} ({len(rows)} entries) from real ROM table @ 0x{TABLE_OFF:04X}")

    # Opcode-range deltas, for quick eyeballing.
    print("\nReal skip deltas for opcodes 0x00..0x32:")
    for b in range(0x33):
        print(f"  op {b:02X}: delta={real[b]:3d}  {rows[b]['note']}")

    if mismatches:
        print("\nDeltas that differ from decoder 1+argc (skip length != exec length):")
        for m in mismatches:
            print(
                f"  op 0x{m['byte']:02X} {m['name']}: ROM skip={m['rom_delta']} "
                f"vs 1+argc={m['argc_delta']}"
            )
    else:
        print("\nAll opcode deltas match 1+argc.")


if __name__ == "__main__":
    main()

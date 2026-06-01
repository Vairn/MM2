#!/usr/bin/env python3
"""
Decode MM2 Amiga copy-protection challenge records.

ASM usage anchor (EXTRACTED/mm2.capstone.asm):
  - challenge pick / parse around 0x26D38..0x26E48
  - table base read via LEA -$6476(A4) (displacement 0x9B8A)
  - random index range: 0x33 entries
  - each entry: 3 words (w0, w1, w2)

Known table location in current EXTRACTED/mm2.asm listing:
  - start address: 0x27530
  - count: 0x33 entries
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path

TABLE_START_DEFAULT = 0x27530
ENTRY_COUNT_DEFAULT = 0x33
WORDS_PER_ENTRY = 3


@dataclass
class CopyProtEntry:
    index: int
    w0: int
    w1: int
    w2: int
    page_tens: int
    page_ones: int
    page: int
    paragraph: int
    line: int
    checksum: int


def parse_dc_l_memory_from_asm(asm_text: str) -> dict[int, int]:
    """
    Build absolute-address -> byte map from IRA-style DC.L lines:
      DC.L $aabbccdd,$... ;274bc
    """
    mem: dict[int, int] = {}
    rx = re.compile(r"DC\.L\s+\$([0-9a-fA-F,$\s]+)\s*;([0-9a-fA-F]+)")
    for line in asm_text.splitlines():
        m = rx.search(line)
        if not m:
            continue
        dcl_vals = [x.strip().lstrip("$") for x in m.group(1).split(",") if x.strip()]
        base = int(m.group(2), 16)
        off = 0
        for token in dcl_vals:
            if len(token) != 8:
                continue
            raw = int(token, 16).to_bytes(4, "big")
            for b in raw:
                mem[base + off] = b
                off += 1
    return mem


def extract_table_bytes_from_mem(mem: dict[int, int], start: int, size: int) -> bytes:
    out = bytearray()
    for a in range(start, start + size):
        if a not in mem:
            raise ValueError(f"missing byte at 0x{a:06X} in parsed asm memory map")
        out.append(mem[a])
    return bytes(out)


def decode_entries(raw: bytes, count: int) -> list[CopyProtEntry]:
    need = count * WORDS_PER_ENTRY * 2
    if len(raw) < need:
        raise ValueError(f"raw table too short: need {need} bytes, got {len(raw)}")

    out: list[CopyProtEntry] = []
    for i in range(count):
        o = i * 6
        w0 = int.from_bytes(raw[o : o + 2], "big")
        w1 = int.from_bytes(raw[o + 2 : o + 4], "big")
        w2 = int.from_bytes(raw[o + 4 : o + 6], "big")

        page_tens = (w0 >> 8) & 0x0F
        page_ones = w0 & 0x0F
        paragraph = (w1 >> 8) & 0x0F
        line = w1 & 0x7F

        out.append(
            CopyProtEntry(
                index=i,
                w0=w0,
                w1=w1,
                w2=w2,
                page_tens=page_tens,
                page_ones=page_ones,
                page=page_tens * 10 + page_ones,
                paragraph=paragraph,
                line=line,
                checksum=w2,
            )
        )
    return out


def encode_entries(entries: list[CopyProtEntry]) -> bytes:
    out = bytearray()
    for e in entries:
        out += int(e.w0 & 0xFFFF).to_bytes(2, "big")
        out += int(e.w1 & 0xFFFF).to_bytes(2, "big")
        out += int(e.w2 & 0xFFFF).to_bytes(2, "big")
    return bytes(out)


def main() -> None:
    ap = argparse.ArgumentParser(description="Decode MM2 copy-protection challenge table.")
    ap.add_argument(
        "--asm",
        default=r"c:\_20260421_\D-REC\development\MM2\EXTRACTED\mm2.asm",
        help="Path to IRA listing (mm2.asm) used as byte source",
    )
    ap.add_argument(
        "--start",
        type=lambda v: int(v, 0),
        default=TABLE_START_DEFAULT,
        help="Absolute table start address in listing (default 0x27530)",
    )
    ap.add_argument(
        "--count",
        type=lambda v: int(v, 0),
        default=ENTRY_COUNT_DEFAULT,
        help="Entry count (default 0x33)",
    )
    ap.add_argument("--json-out", help="Write decoded entries JSON")
    ap.add_argument("--raw-out", help="Write extracted raw table bytes")
    args = ap.parse_args()

    asm_text = Path(args.asm).read_text(encoding="utf-8", errors="replace")
    mem = parse_dc_l_memory_from_asm(asm_text)
    raw = extract_table_bytes_from_mem(mem, args.start, args.count * 6)
    entries = decode_entries(raw, args.count)

    if args.raw_out:
        Path(args.raw_out).write_bytes(raw)
    if args.json_out:
        Path(args.json_out).write_text(
            json.dumps(
                {
                    "table_start": args.start,
                    "count": args.count,
                    "entry_size_bytes": 6,
                    "entries": [asdict(e) for e in entries],
                },
                indent=2,
            ),
            encoding="utf-8",
        )

    print(f"decoded entries={len(entries)} start=0x{args.start:06X}")
    for e in entries[:12]:
        print(
            f"[{e.index:02d}] page={e.page:02d} para={e.paragraph} line={e.line:02d} "
            f"checksum=0x{e.checksum:04X} raw=({e.w0:04X},{e.w1:04X},{e.w2:04X})"
        )


if __name__ == "__main__":
    main()


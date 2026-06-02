#!/usr/bin/env python3
"""Dump audio opcode dispatch map around 0x22C24."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CAP_PATH = ROOT / "EXTRACTED" / "mm2.capstone.asm"
OUT_PATH = ROOT / "EXTRACTED" / "tmp_mm2_audio_dispatch.txt"


LINE_RE = re.compile(r"^([0-9a-fA-F]{6})\s+(.*)$")
SLOT_JSR_RE = re.compile(r"jsr\s+-\$([0-9a-fA-F]{4})\(a4\)", re.IGNORECASE)


def parse_lines() -> list[tuple[int, str]]:
    rows: list[tuple[int, str]] = []
    for ln in CAP_PATH.read_text(encoding="utf-8", errors="replace").splitlines():
        m = LINE_RE.match(ln)
        if not m:
            continue
        rows.append((int(m.group(1), 16), m.group(2)))
    return rows


def slice_block(rows: list[tuple[int, str]], start: int, end: int) -> list[tuple[int, str]]:
    return [(a, t) for (a, t) in rows if start <= a <= end]


def text_from_block(block: list[tuple[int, str]]) -> str:
    return "\n".join(t for _, t in block)


def main() -> None:
    rows = parse_lines()

    # Dispatcher body observed from capstone around 0x22C24..0x22CC8.
    dispatch_block = slice_block(rows, 0x22C24, 0x22CC8)
    # Parse cmp+jmp pairs in-order.
    uniq_map: list[tuple[int, int]] = []
    for i in range(len(dispatch_block) - 1):
        _, t0 = dispatch_block[i]
        _, t1 = dispatch_block[i + 1]
        mcmp = re.search(r"cmp\.w\s+#\$([0-9a-fA-F]+), d0", t0, re.IGNORECASE)
        if not mcmp:
            continue
        mjmp_pc = re.search(r"jmp\s+\$([0-9a-fA-F]+)\(pc\)", t1, re.IGNORECASE)
        if mjmp_pc:
            uniq_map.append((int(mcmp.group(1), 16), int(mjmp_pc.group(1), 16)))
            continue
        mjmp_a4 = re.search(r"jmp\s+-\$([0-9a-fA-F]+)\(a4\)", t1, re.IGNORECASE)
        if mjmp_a4:
            # Keep slot target as negative displacement marker encoded as 0x80000000|disp.
            uniq_map.append((int(mcmp.group(1), 16), 0x80000000 | int(mjmp_a4.group(1), 16)))

    if not uniq_map:
        # Fallback extracted manually from 0x22C24 block.
        uniq_map = [
            (0x01, 0x22CCC),
            (0x02, 0x22CF0),
            (0x03, 0x22D40),
            (0x04, 0x80000000 | 0x7BA8),
            (0x05, 0x22DAE),
            (0x06, 0x22DF4),
            (0x08, 0x80000000 | 0x7B9C),
            (0x09, 0x22F88),
            (0x0B, 0x2303A),
            (0x0C, 0x231F6),
            (0x0E, 0x232D8),
            (0x10, 0x23322),
            (0x13, 0x2367C),
            (0x15, 0x2387A),
            (0x16, 0x23A18),
            (0x17, 0x23C8C),
        ]

    out: list[str] = []
    out.append(f"Source: {CAP_PATH}")
    out.append("Dispatcher: 0x22C24")
    out.append("")
    out.append("Opcode -> handler:")
    for op, tgt in uniq_map:
        if tgt & 0x80000000:
            out.append(f"- 0x{op:02X} -> -${(tgt & 0xFFFF):04X}(A4)")
        else:
            out.append(f"- 0x{op:02X} -> 0x{tgt:05X}")

    out.append("")
    out.append("Handler slot calls (first ~0x160 bytes per handler):")
    for op, tgt in uniq_map:
        if tgt & 0x80000000:
            out.append(f"- op 0x{op:02X} @-${(tgt & 0xFFFF):04X}(A4): direct slot jump")
            continue
        hblock = slice_block(rows, tgt, tgt + 0x160)
        slot_calls: list[str] = []
        for _, txt in hblock:
            sm = SLOT_JSR_RE.search(txt)
            if sm:
                slot_calls.append(sm.group(1).upper())
        slot_calls_uniq: list[str] = []
        for s in slot_calls:
            if s not in slot_calls_uniq:
                slot_calls_uniq.append(s)
        out.append(
            f"- op 0x{op:02X} @0x{tgt:05X}: slots "
            + (", ".join(f"-$" + s for s in slot_calls_uniq) if slot_calls_uniq else "(none)")
        )

    OUT_PATH.write_text("\n".join(out), encoding="utf-8")
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    main()


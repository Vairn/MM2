#!/usr/bin/env python3
"""Map JSR -disp(A4) thunk calls to concrete code targets.

This scans EXTRACTED/mm2.asm for `JSR -NNNN(A4)` callsites, then resolves each
disp through the A4 thunk table in EXTRACTED/hunks/mm2_data_00.bin.
"""

from __future__ import annotations

import re
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASM_PATH = ROOT / "EXTRACTED" / "mm2.asm"
DATA_HUNK = ROOT / "EXTRACTED" / "hunks" / "mm2_data_00.bin"
CODE_HUNK = ROOT / "EXTRACTED" / "hunks" / "mm2_code_00.bin"
OUT_PATH = ROOT / "EXTRACTED" / "tmp_mm2_thunk_map.txt"

A4_BASE = 0x7FFE
CALL_RE = re.compile(r"JSR\s+-([0-9]+)\(A4\).*;([0-9a-fA-F]{5}):\s*4eac([0-9a-fA-F]{4})")


def be_u16(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 2], "big")


def be_u32(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 4], "big")


def main() -> None:
    asm_lines = ASM_PATH.read_text(encoding="utf-8", errors="replace").splitlines()
    data = DATA_HUNK.read_bytes()
    code = CODE_HUNK.read_bytes()

    calls: dict[int, list[int]] = defaultdict(list)  # disp -> callsite addrs
    raw_ops = Counter()
    for line in asm_lines:
        m = CALL_RE.search(line)
        if not m:
            continue
        disp = int(m.group(1))
        addr = int(m.group(2), 16)
        op = m.group(3).lower()
        calls[disp].append(addr)
        raw_ops[op] += 1

    out: list[str] = []
    out.append(f"Source asm: {ASM_PATH}")
    out.append(f"Data hunk: {DATA_HUNK}")
    out.append(f"Code hunk: {CODE_HUNK}")
    out.append("")
    out.append("Per-thunk mapping:")

    for disp in sorted(calls):
        thunk_off = A4_BASE - disp
        stub = data[thunk_off : thunk_off + 12]
        op = be_u16(stub, 0)
        target = be_u32(stub, 2) if len(stub) >= 6 else 0
        out.append(
            f"- -${disp:04X}(A4) off=0x{thunk_off:04X} calls={len(calls[disp])} "
            f"stub_op=0x{op:04X} target=0x{target:05X}"
        )
        out.append("  callsites: " + " ".join(f"0x{x:05X}" for x in calls[disp][:12]))
        if len(calls[disp]) > 12:
            out.append(f"  ... +{len(calls[disp]) - 12} more")
        out.append("  stub_u8: " + " ".join(f"{b:02X}" for b in stub))
        if 0 <= target < len(code):
            out.append("  target_u8: " + " ".join(f"{b:02X}" for b in code[target : target + 16]))

    out.append("")
    out.append("Top JSR opcodes (from asm comments):")
    for op, count in raw_ops.most_common(30):
        out.append(f"- 4eac{op}: {count}")

    OUT_PATH.write_text("\n".join(out), encoding="utf-8")
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    main()


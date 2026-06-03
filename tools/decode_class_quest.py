#!/usr/bin/env python3
"""Dump class-quest reward tables (A4-$7154 / -$7148 / -$7184).

Used by Mount Farview reward handler @ 0x9D76. See
EXTRACTED/docs/36-class-quest-hp-bug.md.

Usage:
    python tools/decode_class_quest.py
"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"

ROSTER_FIELDS = {
    0x5E: "HP_max",
    0x5F: "HP_max_hi",
    0x60: "HP_temp",
    0x74: "HP_current",
    0x79: "class_guild_mask",
    0x7E: "padding",
}


def a4_slice(data: bytes, disp: int, size: int) -> bytes:
    off = 0x7FFE - disp
    return data[off : off + size]


def main() -> int:
    data = DATA.read_bytes()
    offsets = list(a4_slice(data, 0x7154, 64))
    masks = list(a4_slice(data, 0x7148, 64))
    rewards = a4_slice(data, 0x7184, 32)

    print("Class-quest tables (data hunk offsets 0xEAA / 0xEB6 / 0xE7A)")
    print("idx  -7154(off)  -7148(mask)  bug_write@  field")
    print("---- ------------ ------------ ---------- -----")
    for idx in range(64):
        off = offsets[idx]
        mask = masks[idx]
        tgt = (off + 0x79) & 0xFF
        field = ROSTER_FIELDS.get(tgt, "")
        flag = " <-- HP?" if tgt in (0x5E, 0x5F, 0x74, 0x75) else ""
        if off or mask or field or idx < 16:
            print(
                f"{idx:3d}  {off:10d}  0x{mask:02X}        0x{tgt:02X}       "
                f"{field}{flag}"
            )

    print("\nA4-$7184 reward dwords (big-endian, first 8):")
    for i in range(0, 32, 4):
        val = int.from_bytes(rewards[i : i + 4], "big")
        print(f"  [{i // 4}] 0x{val:08X} ({val})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Extract hardcoded code-driven MM2 tone sequence tables.

Current focus: routine around code offset 0x688C, which indexes:
- A4-$73A8 (pitch selector words)
- A4-$7388 (note selector words)
"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA_HUNK = ROOT / "EXTRACTED" / "hunks" / "mm2_data_00.bin"
OUT_TXT = ROOT / "EXTRACTED" / "tmp_mm2_code_sequences.txt"

A4_BASE = 0x7FFE


def be_u16(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 2], "big")


def a4_off(neg_disp: int) -> int:
    return A4_BASE - neg_disp


def main() -> None:
    data = DATA_HUNK.read_bytes()
    tbl_73a8 = [be_u16(data, a4_off(0x73A8) + i * 2) for i in range(16)]
    tbl_7388 = [be_u16(data, a4_off(0x7388) + i * 2) for i in range(16)]

    # Routine @0x688C picks base by -79B1(A4): N/W/S => 0/1/2, then base*=4.
    families = [("N", 0), ("W", 1), ("S", 2)]

    lines: list[str] = []
    lines.append("Source: code routine around 0x688C (mm2.capstone.asm)")
    lines.append("Tables: A4-$73A8 and A4-$7388")
    lines.append("")
    lines.append("Per-step values (step 0..6):")
    lines.append("- idx = base*4 + step")
    lines.append("- pitch_raw = tbl_73a8[idx]")
    lines.append("- if step >= 4 then pitch_raw += 0x000C")
    lines.append("- note_raw = tbl_7388[idx]")
    lines.append("- pitch_arg_for_-7C4A = (pitch_raw << 3) + 8")
    lines.append("")

    for name, base in families:
        lines.append(f"[family {name}] base={base}")
        for step in range(7):
            idx = base * 4 + step
            pitch_raw = tbl_73a8[idx]
            if step >= 4:
                pitch_raw += 0x000C
            note_raw = tbl_7388[idx]
            pitch_arg = (pitch_raw << 3) + 8
            lines.append(
                f"step={step} idx={idx:02d} pitch_raw=0x{pitch_raw:04X} "
                f"note_raw=0x{note_raw:04X} pitch_arg=0x{pitch_arg:04X}"
            )
        lines.append("")

    OUT_TXT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT_TXT}")


if __name__ == "__main__":
    main()


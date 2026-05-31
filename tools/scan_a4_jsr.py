#!/usr/bin/env python3
"""List unique JSR d(A4) displacements from capstone asm."""
import re
from pathlib import Path

text = Path(__file__).resolve().parents[1] / "EXTRACTED" / "mm2.capstone.asm"
text = text.read_text(encoding="utf-8", errors="replace")
offs: set[int] = set()
for m in re.finditer(r"4eac([0-9a-f]{4})", text, re.I):
    w = int(m.group(1), 16)
    if w >= 0x8000:
        offs.add(0x10000 - w)
print(f"unique negative A4 JSR offsets: {len(offs)}\n")
anchor = 0x7FFE
for o in sorted(offs):
    ea = (anchor - o) & 0xFFFFFFFF
    print(f"  -${o:04X}(A4)  ->  ${ea:04X}  (if A4=${anchor:04X})")

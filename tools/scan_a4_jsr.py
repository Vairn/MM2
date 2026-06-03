#!/usr/bin/env python3
"""List unique JSR d(A4) displacements from capstone asm, with symbol names."""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from mm2_symbols import DEFAULT_SYMBOLS, load_symbols, lookup_a4_thunk  # noqa: E402

text = ROOT / "EXTRACTED" / "mm2.capstone.asm"
text = text.read_text(encoding="utf-8", errors="replace")
symbols = load_symbols(DEFAULT_SYMBOLS)

offs: set[int] = set()
for m in re.finditer(r"4eac([0-9a-f]{4})", text, re.I):
    w = int(m.group(1), 16)
    if w >= 0x8000:
        offs.add(0x10000 - w)

named = sum(1 for o in offs if lookup_a4_thunk(symbols, o))
print(f"unique negative A4 JSR offsets: {len(offs)}  ({named} named in {DEFAULT_SYMBOLS.name})\n")
anchor = 0x7FFE
for o in sorted(offs):
    ea = (anchor - o) & 0xFFFFFFFF
    entry = lookup_a4_thunk(symbols, o)
    tag = f"  ; {entry['name']}" if entry else ""
    print(f"  -${o:04X}(A4)  ->  ${ea:04X}  (A4=${anchor:04X}){tag}")

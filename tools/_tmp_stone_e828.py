#!/usr/bin/env python3
from pathlib import Path
import re

asm = Path("EXTRACTED/mm2.capstone.annotated.asm").read_text(encoding="utf-8", errors="replace")

# Print all instruction lines in e7f0..e8d8
for line in asm.splitlines():
    m = re.match(r"^([0-9a-f]{6})\s+", line)
    if not m:
        continue
    a = int(m.group(1), 16)
    if 0xE7F0 <= a < 0xE8D8:
        print(line)

print("\n=== andi on condition in spell range ===")
for line in asm.splitlines():
    m = re.match(r"^([0-9a-f]{6})\s+", line)
    if not m:
        continue
    a = int(m.group(1), 16)
    if 0xB200 <= a <= 0xC800 and "andi" in line and "0026" in line:
        print(line)

print("\n=== cmpi #$40/$80/$c0 on condition near B3 ===")
for line in asm.splitlines():
    m = re.match(r"^([0-9a-f]{6})\s+", line)
    if not m:
        continue
    a = int(m.group(1), 16)
    if 0xB300 <= a <= 0xB500 and ("0040" in line or "0080" in line or "00c0" in line or "00ff" in line):
        print(line)

# Map sparse cler code for Stone - dump stub after Rejuvenate
# From prior: Rejuvenate B332, next B3B2 is Water Transmute
# Stone might be in CDB8 path at c9xx
print("\n=== C9xx party-pick stubs ===")


def dump(addr, n=40):
    i = asm.find(f"{addr:06x}")
    if i < 0:
        print(hex(addr), "missing")
        return
    lines = []
    for line in asm[i : i + 4000].splitlines():
        if re.match(r"^[0-9a-f]{6}\s+", line):
            lines.append(line)
        if len(lines) >= n:
            break
    print(f"==== {addr:#x} ====")
    print("\n".join(lines))


for a in [0xC96E, 0xC9B6, 0xC9DC, 0xCA00, 0xB644, 0xC294, 0xC2D4, 0xC358, 0xC39C, 0xC488, 0xC546]:
    dump(a, 35)

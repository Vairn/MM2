#!/usr/bin/env python3
from pathlib import Path
import re

asm = Path("EXTRACTED/mm2.capstone.annotated.asm").read_text(encoding="utf-8", errors="replace")


def dump(addr, n=70):
    i = asm.find(f"{addr:06x}")
    if i < 0:
        print(hex(addr), "missing")
        return
    lines = []
    for line in asm[i : i + 8000].splitlines():
        if re.match(r"^[0-9a-f]{6}\s+", line):
            lines.append(line)
        if len(lines) >= n:
            break
    print(f"==== {addr:#x} ====")
    print("\n".join(lines))
    print()


dump(0xC9B6, 80)  # Resurrection
dump(0xB524, 50)  # Uncurse?

# Find Recharge: add.b to charges +$40/+ $2e after item pick
print("=== add to charges near C2/B5 ===")
for line in asm.splitlines():
    m = re.match(r"^([0-9a-f]{6})\s+", line)
    if not m:
        continue
    a = int(m.group(1), 16)
    if 0xB500 <= a <= 0xC500 and ("0040" in line or "002e" in line) and (
        "add" in line or "addq" in line or "move.b" in line
    ):
        print(line)

# Etherealize: often sets a GS flag for phasing
print("\n=== -$79xx sets in C3xx (Etherealize?) ===")
for line in asm.splitlines():
    m = re.match(r"^(00c3[0-9a-f]{2})\s+", line)
    if m and ("79" in line or "861" in line):
        print(line)

# Duplication / Enchant - search empty backpack write
dump(0xAEAC, 40)
dump(0xAF02, 40)

# Gem cost 50 = #$32 on gems
print("\n=== gem #$32 / #$14 special costs ===")
for line in asm.splitlines():
    m = re.match(r"^([0-9a-f]{6})\s+", line)
    if not m:
        continue
    a = int(m.group(1), 16)
    if 0xB000 <= a <= 0xCA00 and ("0032" in line or "#$32" in line or "0050" in line):
        if "gem" in line.lower() or "005c" in line or "3f3c0032" in line or "3f3c0050" in line:
            print(line)
# broader
for line in asm.splitlines():
    if re.match(r"^00b5[0-9a-f]{2}\s+", line) and "3f3c0032" in line:
        print(line)
    if re.match(r"^00b5[0-9a-f]{2}\s+", line) and "005c" in line:
        print(line)

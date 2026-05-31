#!/usr/bin/env python3
"""Trace item record access patterns in MM2 disassembly."""

import re
import sys

asm_path = r"C:\_20260421_\D-REC\development\MM2\EXTRACTED\mm2.capstone.asm"

with open(asm_path, "r", errors="replace") as f:
    lines = f.readlines()

print(f"Total asm lines: {len(lines)}")

# items.dat is at code offset 0x29868.
# Look for references to that address (LEA, PEA, MOVE to push filename)
targets = ["29868", "29888"]
print("\n=== References to items.dat string (0x29868) ===")
for i, line in enumerate(lines):
    for addr in targets:
        if addr in line.lower():
            start = max(0, i - 5)
            end = min(len(lines), i + 6)
            for j in range(start, end):
                marker = ">>>" if j == i else "   "
                print(f"{marker} L{j+1:5d}: {lines[j].rstrip()}")
            print()
            break

# Now search for code that uses 20-byte record size (0x14)
# Common patterns: mulu #$14, muls #$14, or asl/lsl patterns
print("\n=== Multiply by 20 (0x14) - record indexing ===")
count = 0
for i, line in enumerate(lines):
    if re.search(r"mul[us].*#\$14", line, re.I) or re.search(r"mul[us].*#20", line, re.I):
        start = max(0, i - 3)
        end = min(len(lines), i + 8)
        for j in range(start, end):
            marker = ">>>" if j == i else "   "
            print(f"{marker} L{j+1:5d}: {lines[j].rstrip()}")
        print()
        count += 1
        if count > 15:
            print("... (truncated)")
            break

# Also look for 12-byte name offset patterns (accessing fields after name)
print("\n=== Access at offset +12/+13/+14/+16/+18 from base (item fields) ===")
count = 0
for i, line in enumerate(lines):
    if re.search(r"#\$c[,\s)]", line, re.I) and ("move" in line.lower() or "add" in line.lower()):
        # Check if in context with mulu $14
        context = "".join(lines[max(0,i-10):i+1])
        if "14" in context or "mulu" in context.lower():
            start = max(0, i - 4)
            end = min(len(lines), i + 4)
            for j in range(start, end):
                marker = ">>>" if j == i else "   "
                print(f"{marker} L{j+1:5d}: {lines[j].rstrip()}")
            print()
            count += 1
            if count > 10:
                break

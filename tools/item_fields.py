#!/usr/bin/env python3
"""Analyze item record field access patterns from MM2 disassembly."""

import re

asm_path = r"C:\_20260421_\D-REC\development\MM2\EXTRACTED\mm2.capstone.asm"

with open(asm_path, "r", errors="replace") as f:
    lines = f.readlines()

# Find all accesses to offsets 0x00-0x13 from (a0) in the vicinity of muls #$14
item_field_accesses = {}
for i, line in enumerate(lines):
    # Match patterns like $12(a0), $c(a0), $d(a0), (a0) etc
    m = re.search(r"\$([0-9a-fA-F]+)\(a0\)", line)
    if not m:
        # Also match bare (a0) = offset 0
        if "(a0)" in line and not re.search(r"\$[0-9a-fA-F]+\(a0\)", line):
            if re.search(r"\(a0\)", line):
                offset_val = 0
            else:
                continue
        else:
            continue
    else:
        offset_val = int(m.group(1), 16)

    if offset_val > 0x13:
        continue

    # Check if muls #$14 appears within 15 lines before
    context_start = max(0, i - 15)
    context = "".join(lines[context_start : i + 1])
    if "0014" not in context:
        continue

    if offset_val not in item_field_accesses:
        item_field_accesses[offset_val] = []
    item_field_accesses[offset_val].append((i + 1, line.rstrip()))

print("=== Item Record Field Access Analysis ===")
print(f"Record size: 20 bytes (0x14)")
print(f"Name field: bytes 0-11 (12 chars, ASCII, space-padded)")
print()
print("Field offset -> access count and instruction types:")
for off in sorted(item_field_accesses.keys()):
    accesses = item_field_accesses[off]
    # Categorize by instruction type
    reads = [a for a in accesses if "move" in a[1].lower() and "(a0" in a[1] and "-(a" not in a[1].split("(a0")[0]]
    writes = [a for a in accesses if "move" in a[1].lower() and "(a0" in a[1] and a[1].lower().strip().endswith(f"(a0)") or f"${off:x}(a0)" in a[1].split(",")[-1] if "," in a[1]]
    print(f"\n  +0x{off:02X} (byte {off:2d})  [{len(accesses)} refs]")
    for linenum, text in accesses[:5]:
        print(f"    L{linenum:5d}: {text}")
    if len(accesses) > 5:
        print(f"    ... and {len(accesses) - 5} more")

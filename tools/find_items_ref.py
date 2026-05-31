#!/usr/bin/env python3
"""Find items.dat references in the MM2 binary and trace how records are read."""

import sys

binary_path = sys.argv[1] if len(sys.argv) > 1 else r"C:\_20260421_\D-REC\development\MM2\mm2"
data = open(binary_path, "rb").read()

# Find "items.dat" string
for needle in [b"items.dat", b"ITEMS.DAT", b"Items.dat", b"items", b"ITEMS"]:
    pos = data.find(needle)
    if pos >= 0:
        code_off = pos - 0x20
        ctx = data[max(0, pos - 8) : pos + 20]
        print(f'Found "{needle.decode()}" at file offset 0x{pos:x} (code offset 0x{code_off:x})')
        print(f"  Context: {ctx}")
        print(f"  Hex: {ctx.hex(' ')}")
        print()

# List ALL ASCII strings in code region 0x220-0x400
print("--- All strings in code region 0x220-0x400 ---")
region_start = 0x240  # file offsets (code+0x20)
region_end = 0x420
current = []
start_pos = 0
for i in range(region_start, min(region_end, len(data))):
    b = data[i]
    if 32 <= b < 127:
        if not current:
            start_pos = i
        current.append(chr(b))
    else:
        if len(current) >= 3:
            s = "".join(current)
            code_off = start_pos - 0x20
            print(f"  code 0x{code_off:04x}: \"{s}\"")
        current = []

# Search broader range for items
print("\n--- Searching full binary for 'items' ---")
search_start = 0
while True:
    pos = data.find(b"items", search_start)
    if pos < 0:
        pos = data.find(b"Items", search_start)
    if pos < 0:
        pos = data.find(b"ITEMS", search_start)
    if pos < 0:
        break
    code_off = pos - 0x20
    ctx = data[pos:pos+16]
    print(f"  code 0x{code_off:x}: \"{ctx.decode('ascii', errors='replace')}\"")
    search_start = pos + 1

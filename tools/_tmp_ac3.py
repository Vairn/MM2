from pathlib import Path
import struct

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()

# Find all move.b $24(a0/a1/a2..)
for reg in range(8):
    # move.b $24(aN), d0 = 1028+reg 0024? 
    # 1028 = move.b (a0), ... with extension
    # Actually move.b $24(a0),d0 = 1028 0024
    op = 0x1028 + reg
    pat = struct.pack(">HH", op, 0x0024)
    idx = 0
    while True:
        i = code.find(pat, idx)
        if i < 0:
            break
        print(f"move.b \$24(a{reg}),d0 @{i:#x}")
        idx = i + 2

# add.b dX, $24(a0) variants: d128 0024 already tried
# Maybe word AC? 
print("--- word $24 ---")
for pat in [b"\x30\x28\x00\x24", b"\xd0\x68\x00\x24", b"\x31\x68\x00\x24"]:
    idx = 0
    while True:
        i = code.find(pat, idx)
        if i < 0:
            break
        print(pat.hex(), hex(i))
        idx = i + 2

# Invis might boost via GS read in 10478 path - check if there's add after AC read
# At 104ba: move.b $24(a0), d0
print("context 104b8", code[0x104B8:0x104D0].hex())

# Search FAQ AC+ implementation: maybe addq to Shield counter reused?
# Or set -$799C / Bless-adjacent
# Check item effect for Invisocloak - spell id mapping in 0x137F0

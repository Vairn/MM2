from pathlib import Path
import struct

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()

# Find Invisibility via dens CDB8 - which stub for S3/3?
# Search for writes to likely invis GS fields
# Common: addq to a buff counter, or set party AC boost
# From SpellBook flat16 = Invisibility

# Parse CDB8 dense and match known; find unknown near Fly/Jump
pairs = []
off = 0xCDC6
while off + 8 <= 0xCDC6 + 0x200:
    w0 = struct.unpack_from(">I", code, off)[0]
    w1 = struct.unpack_from(">I", code, off + 4)[0]
    if ((w0 >> 16) & 0xFFFF) != 0x4EBA or ((w1 >> 16) & 0xFFFF) != 0x6000:
        break
    disp = struct.unpack(">h", struct.pack(">H", w0 & 0xFFFF))[0]
    pairs.append((off, off + 2 + disp))
    off += 8

# Also search lea/move involving possible invis offsets
# Check a974 (dens0) - Etherealize?
for addr in (0xA974, 0xAEAC, 0xB318, 0xB332, 0xBB0A, 0xA99A):
    print(f"--- {addr:#x} first bytes {code[addr:addr+4].hex()}")

# Find string 'Invisibility' only in name table; find who sets AC or similar
# Search jsr targets that might be invis - look at remake unused sorc flats
# S flat16 Invisibility - search sparse code for 16

# Dump Surface - search for exit to overland / entry_coord unpack
# dens for Surface: look for move from attrib entry
for i, (t, s) in enumerate(pairs):
    print(f"C dens[{i:2d}] -> {s:06x}")

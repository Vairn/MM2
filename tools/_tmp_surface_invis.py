from pathlib import Path
import struct

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()

# Sparse CDB8: code -> stub. Find which sparse codes map near known spells
# CFF8 sparse at 0xD1AE, CDB8 at 0xCF1E
# Remake uses flat0; exploration uses sparse picker codes.

# Search for Surface: unpack entry_coord 0x0E from attrib
# Pattern: move.b -$560C or andi #$f on coords after screen change
for i in range(0xB000, 0xB600, 2):
    # look for move from -0x560C (aa00+? 560C as a4: aa00 is -0x5600 area)
    # ENTRY_COORD = -0x560C → disp 0xA9F4? 
    pass

# Find lea -$560C or move.b -$560C
pat = struct.pack(">h", -0x560C & 0xFFFF)
idx = 0
while True:
    i = code.find(pat, idx)
    if i < 0 or i > 0x20000:
        break
    # show context if near spell region
    if 0xA000 <= i <= 0xC000:
        for a in range(i, max(0, i - 0x60), -2):
            if code[a : a + 2] == b"\x4e\x55":
                print(f"entry_coord ref @{i:#x} in func {a:#x}")
                break
    idx = i + 2

# Invisibility: FAQ often +AC. Search add to armor_class $24 in spell region
# Or set a duration counter. Check Shield-like addq at unknown offs
print("--- addq.b #1 to GS in A8-B6 ---")
for i in range(0xA800, 0xB600):
    # 522c XXXX = addq.b #1, d16(a4)
    if code[i : i + 2] == b"\x52\x2c":
        disp = struct.unpack_from(">h", code, i + 2)[0]
        if -0x7A00 <= disp <= -0x7900:
            for a in range(i, max(0, i - 0x30), -2):
                if code[a : a + 2] == b"\x4e\x55":
                    print(f"func {a:#x}: addq #1, {disp:#x} @{i:#x}")
                    break

# What does 0x10002 call -$7CDA do? Find absolute if it's in code
# A4-$7CDA = runtime. Search similar jsr patterns after group attack in decomp
print("--- 105b6 (special charge?) ---")
i = 0x105B6
for _ in range(30):
    w = struct.unpack_from(">H", code, i)[0]
    if w == 0x4EBA:
        rel = struct.unpack_from(">h", code, i + 2)[0]
        print(f"{i:06x} jsr {i+2+rel:06x}")
        i += 4
        continue
    if w == 0x4E75:
        print(f"{i:06x} rts")
        break
    print(f"{i:06x} {code[i:i+4].hex()}")
    i += 2

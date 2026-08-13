from pathlib import Path
import struct

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()

# Continue AI from 107a4
i = 0x107A4
while i < 0x10860:
    w = struct.unpack_from(">H", code, i)[0]
    if w == 0x4EBA:
        rel = struct.unpack_from(">h", code, i + 2)[0]
        print(f"{i:06x} jsr {i+2+rel:06x}")
        i += 4
        continue
    if w == 0x4EAC:
        off = struct.unpack_from(">h", code, i + 2)[0]
        print(f"{i:06x} jsr a4({off})")
        i += 4
        continue
    if code[i : i + 2] == b"\x4a\x2c":
        disp = struct.unpack_from(">h", code, i + 2)[0]
        print(f"{i:06x} tst.b {disp}(a4)")
        i += 4
        continue
    if code[i : i + 2] == b"\x0c\x2c":
        imm = code[i + 3]
        disp = struct.unpack_from(">h", code, i + 4)[0]
        print(f"{i:06x} cmpi.b #{imm:#x}, {disp}(a4)")
        i += 6
        continue
    print(f"{i:06x} {code[i:i+6].hex()}")
    i += 2

# Find Invisibility: search CFF8/CDB8 sparse for code that might map
# Check a8fa - Location? earlier dump
# Search move.b #1 to unknown buffs near 0xBxxx that remake hasn't ported
print("--- candidates setting GS ---")
for addr in range(0xA800, 0xB600, 2):
    if code[addr : addr + 2] != b"\x19\x7c":
        continue
    imm = code[addr + 3]
    disp = struct.unpack_from(">h", code, addr + 4)[0]
    if imm == 1 and disp in (-0x79A5, -0x799C, -0x7999, -0x7998, -0x79A0 - 5):
        print(f"{addr:#x} move.b #1, {disp:#x}(a4)")
    if imm == 1 and -0x79B0 <= disp <= -0x7990:
        # find function start
        for a in range(addr, max(0, addr - 0x40), -2):
            if code[a : a + 2] == b"\x4e\x55":
                print(f"func {a:#x}: move.b #1, {disp}(a4) @{addr:#x}")
                break

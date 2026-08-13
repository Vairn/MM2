from pathlib import Path
import struct

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()

# dens map for known flats
off = 0xD006
for i in range(48):
    w0 = struct.unpack_from(">I", code, off)[0]
    disp = struct.unpack(">h", struct.pack(">H", w0 & 0xFFFF))[0]
    stub = off + 2 + disp
    known = {
        2: "EnergyBlast",
        3: "FlameArrow",
        6: "Sleep",
        8: "ElectricArrow",
        14: "AcidStream",
        16: "Invisibility?",
        17: "Lightning",
        18: "Web",
    }
    if i in known:
        print(f"dens[{i}] {stub:06x} {known[i]}")
    off += 8

# Search add.b of GS byte into d0 near 104c0 callers / AC uses
# At 3b8a and 5b8a - dump those funcs
path = Path("EXTRACTED/mm2.capstone.annotated.asm")
lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
for addr in ("003b50", "003b80", "005b50", "005b80"):
    for i, l in enumerate(lines):
        if l.startswith(addr + " "):
            print("===", addr, "===")
            print("\n".join(lines[i : i + 40]))
            break

# Search for invis duration: string or addq to AC-related
# Maybe it's in exploration CDB8 - dens that add to all party AC
# Loop party: for each member add.b #N, $24
# Pattern: cmp party count, addq to $24
print("--- scan for 0024 in A8-C0 ---")
idx = 0xA800
while idx < 0xC000:
    if code[idx : idx + 2] == b"\x00\x24":
        print(f"{idx-2:06x} {code[idx-2:idx+4].hex()}")
    idx += 2

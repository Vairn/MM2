from pathlib import Path
import struct

data = Path("EXTRACTED/hunks/mm2_data_00.bin").read_bytes()
asm = Path("EXTRACTED/mm2.capstone.asm").read_text(encoding="utf-8", errors="replace").splitlines()

def thunk(off_neg):
    off = 0x7FFE - off_neg
    b = data[off : off + 6]
    print(f"A4-{off_neg:#x} DATA {off:#x}: {b.hex()}")
    if b[:2] == bytes([0x4E, 0xF9]):
        print(f"  -> {int.from_bytes(b[2:6], 'big'):#x}")

thunk(0x7E96)
thunk(0x7E42)

# dump 0x7DCC region
print("\n=== 0x7DCC ===")
for line in asm:
    try:
        a = int(line[:6], 16)
    except Exception:
        continue
    if 0x7DCC <= a <= 0x7F20:
        print(line.encode("ascii", "replace").decode())

# callers of 4eac816a (-$7E96: 0x10000-0x7E96=0x816A)
print("\n=== jsr -$7E96 sites ===")
for i, line in enumerate(asm):
    if "4eac816a" not in line.replace(" ", "").lower():
        continue
    print(f"\n--- {line[:6]} ---")
    for j in range(max(0, i - 10), min(len(asm), i + 2)):
        print(asm[j].encode("ascii", "replace").decode())

# combat spawn: search near 0x12C66 / encounter for sound ids
print("\n=== sounds near 0x12c00-0x13000 ===")
for i, line in enumerate(asm):
    try:
        a = int(line[:6], 16)
    except Exception:
        continue
    if 0x12C00 <= a <= 0x13100 and "4eac81be" in line.replace(" ", "").lower():
        print(line.encode("ascii", "replace").decode())
        for j in range(max(0, i - 6), i + 1):
            print(" ", asm[j].encode("ascii", "replace").decode())

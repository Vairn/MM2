from pathlib import Path

asm = Path("EXTRACTED/mm2.capstone.asm").read_text(encoding="utf-8", errors="replace").splitlines()

print("=== 0x11646-0x11700 ===")
for line in asm:
    try:
        a = int(line[:6], 16)
    except Exception:
        continue
    if 0x11646 <= a <= 0x11700:
        print(line.encode("ascii", "replace").decode())

print("\n=== callers / refs involving 7dcc ===")
for line in asm:
    low = line.lower()
    if "7dcc" in low:
        print(line.encode("ascii", "replace").decode())

# play_tone_env = -$7E24 = 4eac81dc
print("\n=== play_tone_env (-$7E24) in 0x10000-0x14000 ===")
for line in asm:
    try:
        a = int(line[:6], 16)
    except Exception:
        continue
    if 0x10000 <= a <= 0x14000 and "4eac81dc" in line.replace(" ", "").lower():
        print(line.encode("ascii", "replace").decode())

# Any sound at combat round loop start 0x12a22
print("\n=== 0x12a22-0x12b00 ===")
for line in asm:
    try:
        a = int(line[:6], 16)
    except Exception:
        continue
    if 0x12A22 <= a <= 0x12B00:
        print(line.encode("ascii", "replace").decode())

# resolve -$7BD8 and -$7BDE
data = Path("EXTRACTED/hunks/mm2_data_00.bin").read_bytes()
for name, neg in [("7BD8", 0x7BD8), ("7BDE", 0x7BDE), ("7E96", 0x7E96)]:
    off = 0x7FFE - neg
    b = data[off : off + 6]
    tgt = int.from_bytes(b[2:6], "big") if b[:2] == bytes([0x4E, 0xF9]) else None
    print(f"-{name} -> {tgt and hex(tgt)}")

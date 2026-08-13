from pathlib import Path

d = Path("EXTRACTED/hunks/mm2_data_00.bin").read_bytes()
base = -0x6F2E


def a4_off(a4):
    return 0x10D0 + (a4 - base)


for name, a4, n in [
    ("7136", -0x7136, 8),
    ("713c", -0x713C, 8),
    ("711c", -0x711C, 28),
    ("7102", -0x7102, 16),
    ("70f5", -0x70F5, 16),
    ("6ece", -0x6ECE, 64),
]:
    off = a4_off(a4)
    print(name, hex(off), list(d[off : off + n]))

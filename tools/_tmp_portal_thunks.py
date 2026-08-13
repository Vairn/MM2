#!/usr/bin/env python3
import struct
from pathlib import Path

data = Path(__file__).resolve().parents[1] / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"
raw = data.read_bytes()
base_7dfa = 0x0204
for name, a4off in [
    ("7DFA", 0x7DFA),
    ("7DBE", 0x7DBE),
    ("7DB8", 0x7DB8),
    ("7DB2", 0x7DB2),
    ("7DAC", 0x7DAC),
    ("7DA6", 0x7DA6),
    ("7DA0", 0x7DA0),
    ("7D9A", 0x7D9A),
]:
    slot = (0x7DFA - a4off) // 6
    fo = base_7dfa + slot * 6
    chunk = raw[fo : fo + 6]
    if len(chunk) == 6 and chunk[0:2] == b"\x4e\xf9":
        dest = struct.unpack(">I", chunk[2:6])[0]
        print(f"-{name} slot{slot} file {fo:04x} -> {dest:#x}")
    else:
        print(f"-{name} slot{slot} file {fo:04x} raw {chunk.hex()}")

# Dump portal leaves
asm = (Path(__file__).resolve().parents[1] / "EXTRACTED" / "mm2.capstone.asm").read_text(
    errors="replace"
).splitlines()
for a in ["00d634", "00d700", "00d800", "00d900", "00da00", "00db00"]:
    pass

# Find targets from above once printed; also search jsr patterns near travel
for needle in ["00d634", "00df04", "00a118"]:
    for i, l in enumerate(asm):
        if l.startswith(needle + " "):
            print("====", needle, "====")
            for x in asm[i : i + 45]:
                print(x)
            print()
            break

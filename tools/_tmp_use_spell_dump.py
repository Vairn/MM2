#!/usr/bin/env python3
from pathlib import Path
import re

asm = Path("EXTRACTED/mm2.capstone.annotated.asm").read_text(encoding="utf-8", errors="replace")


def dump(addr: int, n: int = 80) -> None:
    needle = f"{addr:06x}"
    i = asm.find(needle)
    if i < 0:
        print(hex(addr), "MISSING")
        return
    lines = []
    for line in asm[i : i + 12000].splitlines():
        if re.match(r"^[0-9a-f]{6}\s+", line):
            lines.append(line)
        if len(lines) >= n:
            break
    print(f"==== {addr:#x} ====")
    print("\n".join(lines))
    print()


for a in [
    0xE828,
    0xF470,
    0xF4DE,
    0x13AE2,
    0x13B80,
    0xE1E8,
    0xB3E6,  # Town Portal
    0xC6AE,  # near Divine Intervention / Resurrection?
    0xC750,
    0xB524,  # Uncurse candidate
]:
    dump(a, 55)

# Find Stone to Flesh: search andi on condition near B3xx or C9xx with stone-ish
print("=== candidates with andi.b on $26 near B3/C9 ===")
for line in asm.splitlines():
    if re.match(r"^00(b3|b4|c9|ca)[0-9a-f]{2}\s+", line) and (
        "0026" in line or "andi" in line or "stone" in line.lower()
    ):
        print(line)

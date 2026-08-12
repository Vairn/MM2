#!/usr/bin/env python3
from pathlib import Path
import re

lines = Path("EXTRACTED/mm2.capstone.asm").read_text(encoding="utf-8", errors="replace").splitlines()
hits = []
for i, l in enumerate(lines):
    if re.search(r"jsr\s+\$d390\(pc\)", l, re.I):
        m = re.match(r"^([0-9a-f]{6})", l)
        if m:
            hits.append((int(m.group(1), 16), i))

# Remake / doc name hints by stub start
NAMES = {
    0xB9C4: "Invisibility S3/3",
    0xBB5C: "Shield S4/5",
    0xBB86: "Time Distortion S4/6",
    0xBCBC: "Entrapment S6/2",
    0xBFC4: "Bless C1/3",
    0xBFEE: "Turn Undead→C028",
    0xC6D6: "Divine Intervention C9/1",
}

for addr, idx in hits:
    start = addr
    for j in range(idx, max(0, idx - 50), -1):
        m = re.match(r"^([0-9a-f]{6})\s+4e55", lines[j])
        if m:
            start = int(m.group(1), 16)
            break
    ops = []
    for j in range(idx, min(len(lines), idx + 15)):
        mm = re.match(r"^([0-9a-f]{6})\s+\S+\s+(.*)$", lines[j])
        if mm:
            ops.append(mm.group(2).strip()[:50])
    name = NAMES.get(start, "")
    print(f"{start:#06x}  d390@{addr:#06x}  {name:24}  {ops[0] if ops else ''}")

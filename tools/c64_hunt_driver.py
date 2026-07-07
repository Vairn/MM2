#!/usr/bin/env python3
"""Hunt for the runtime IEC/unpack driver in plaintext on the C64 disks.

The boot proves the unpack codec lives in a runtime-loaded driver installed
at $0100 (and a 256-byte table copied from $4546). That driver is fetched by
the T18S16 KERNAL LOAD of "NEW WORLD COMP." via the custom fastloader. If a
copy of that driver sits UNPACKED on any track we can lift the codec without
emulation. Scan every sector for low-entropy (code-like) content that also
references the tell-tale addresses.

Run: python tools/c64_hunt_driver.py
"""
from __future__ import annotations

import math
from collections import Counter
from pathlib import Path

from c64_d64_layout import ROOT, SPT, read_sector

DISKS = [ROOT / f"EXTRACTED/c64/MM2-{i}.D64" for i in range(1, 7)]

# Byte signatures that reference the driver's fixed addresses.
SIGS = {
    "JMP $0100": bytes([0x4C, 0x00, 0x01]),
    "JSR $0100": bytes([0x20, 0x00, 0x01]),
    "any $4546 ref (lo,hi)": bytes([0x46, 0x45]),  # operand of *$4546
    "STA/LDA $00CB region": bytes([0xCB, 0x00]),
    "$02A7 ref": bytes([0xA7, 0x02]),
    "STA $DD00": bytes([0x8D, 0x00, 0xDD]),
    "LDA $DD00": bytes([0xAD, 0x00, 0xDD]),
}


def ent(b: bytes) -> float:
    if not b:
        return 0.0
    c = Counter(b)
    n = len(b)
    return -sum((v / n) * math.log2(v / n) for v in c.values())


def main() -> None:
    for disk in DISKS:
        if not disk.exists():
            continue
        data = disk.read_bytes()
        hits = []
        for track in range(1, 36):
            for sector in range(SPT[track - 1]):
                sec = read_sector(data, track, sector)
                payload = sec[2:]
                e = ent(payload)
                found = {name: payload.count(sig) for name, sig in SIGS.items()
                         if sig in payload}
                # A driver would be code-like (entropy < ~6.3) AND reference
                # one of the fixed low addresses, OR contain the $0100 vector.
                interesting = (
                    ("JMP $0100" in found or "JSR $0100" in found)
                    or ("STA $DD00" in found and e < 6.5)
                )
                if found and (interesting or e < 5.2):
                    hits.append((track, sector, round(e, 2), found))
        print(f"\n### {disk.name}: {len(hits)} candidate sectors")
        for t, s, e, found in hits:
            tags = ", ".join(f"{k}x{v}" for k, v in found.items())
            print(f"  T{t:02d}S{s:02d} ent={e:<4} {tags}")


if __name__ == "__main__":
    main()

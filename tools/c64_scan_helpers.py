#!/usr/bin/env python3
"""One-off scans for C64 RE — run from repo root via tools/."""
from __future__ import annotations

import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from c64_d64_layout import read_sector, SPT  # noqa: E402

KERNAL_SIGS = {
    bytes([0x20, 0xFF, 0xBA]): "JSR SETLFS",
    bytes([0x20, 0xFF, 0xBD]): "JSR SETNAM",
    bytes([0x20, 0xFF, 0xD5]): "JSR LOAD",
}
PAT_DD = bytes([0x8D, 0x00, 0xDD])


def scan_t18():
    for d in sorted(Path("EXTRACTED/c64").glob("MM2-*.D64")):
        data = d.read_bytes()
        print(d.name)
        for s in range(SPT[17]):
            sec = read_sector(data, 18, s)
            payload = sec[2:]
            hits = [n for sig, n in KERNAL_SIGS.items() if sig in payload]
            iec = payload.count(PAT_DD)
            nw = b"NEW WORLD" in payload
            if hits or iec >= 2 or nw:
                print(f"  T18S{s:02d} link=({sec[0]},{sec[1]}) kernal={hits} iec_sta={iec} newworld={nw}")


def valid_links():
    for d in sorted(Path("EXTRACTED/c64").glob("MM2-*.D64")):
        data = d.read_bytes()
        chains = []
        for t in range(1, 36):
            for s in range(SPT[t - 1]):
                sec = read_sector(data, t, s)
                nt, ns = sec[0], sec[1]
                if nt and 1 <= nt <= 35 and ns < SPT[nt - 1]:
                    chains.append((t, s, nt, ns))
        by_track = Counter(t for t, _, _, _ in chains)
        print(f"{d.name}: {len(chains)} valid links, top: {by_track.most_common(6)}")


if __name__ == "__main__":
    print("=== T18 scan ===")
    scan_t18()
    print("\n=== Valid sector links ===")
    valid_links()

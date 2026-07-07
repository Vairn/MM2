#!/usr/bin/env python3
"""Initial RE scan for SNES / Genesis MM ROM images."""

from __future__ import annotations

import argparse
import collections
import hashlib
import math
import re
import struct
import sys
from pathlib import Path


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    freq = collections.Counter(data)
    n = len(data)
    return -sum((c / n) * math.log2(c / n) for c in freq.values())


def snes_header(data: bytes) -> dict:
    out: dict = {}
    for label, off in ("LoROM", 0x7FC0), ("HiROM", 0xFFC0):
        title = data[off : off + 21].decode("ascii", "replace").rstrip()
        if title.strip("\x00 "):
            out[label] = {
                "offset": off,
                "title": title,
                "map_mode": data[off + 21],
                "rom_type": data[off + 22],
                "rom_size": data[off + 23],
                "sram_size": data[off + 24],
                "destination": data[off + 25],
                "license": data[off + 26],
                "version": data[off + 27],
                "checksum": struct.unpack_from("<H", data, off + 28)[0],
                "complement": struct.unpack_from("<H", data, off + 30)[0],
            }
    return out


def genesis_header(data: bytes) -> dict:
    return {
        "system": data[0x100:0x110].decode("ascii", "replace").strip(),
        "publisher": data[0x110:0x150].decode("ascii", "replace").strip(),
        "domestic": data[0x150:0x180].decode("ascii", "replace").strip(),
        "international": data[0x180:0x1B0].decode("ascii", "replace").strip(),
        "serial": data[0x1B0:0x1BA].decode("ascii", "replace").strip(),
        "checksum": struct.unpack_from(">I", data, 0x18E)[0],
        "rom_end": struct.unpack_from(">I", data, 0x1A4)[0],
    }


def keyword_strings(data: bytes, min_len: int = 6) -> list[tuple[int, str]]:
    keys = (
        b"magic",
        b"new world",
        b"might",
        b"copyright",
        b"castle",
        b"middlegate",
        b"corak",
        b"sega",
        b"nintendo",
        b"elite",
        b"simis",
    )
    hits: list[tuple[int, str]] = []
    for m in re.finditer(rb"[\x20-\x7e]{%d,}" % min_len, data):
        s = m.group()
        if any(k in s.lower() for k in keys):
            hits.append((m.start(), s.decode("ascii", "replace")))
    return hits


def scan_snes(data: bytes) -> dict:
    patterns = {
        "STZ_2115": bytes([0x9C, 0x15, 0x21]),
        "STZ_212C": bytes([0x9C, 0x2C, 0x21]),
        "STA_2107": bytes([0x8D, 0x07, 0x21]),
        "STA_2118": bytes([0x8D, 0x18, 0x21]),
        "STA_2121": bytes([0x8D, 0x21, 0x21]),
    }
    pat_hits = {}
    for name, pat in patterns.items():
        offs = [i for i in range(len(data) - len(pat)) if data[i : i + len(pat)] == pat]
        pat_hits[name] = {"count": len(offs), "first": [hex(o) for o in offs[:8]]}

    walls = [
        "castle",
        "castlet",
        "castlef",
        "cave",
        "cavet",
        "cavef",
        "desert",
        "ocean",
        "outb",
        "outdoor1",
        "outdoor2",
        "outdoor3",
        "outf",
        "sky",
        "swamp",
        "town",
    ]
    wall_hits = {}
    for w in walls:
        for variant in (w, w.upper()):
            i = data.find(variant.encode())
            if i >= 0:
                wall_hits[variant] = hex(i)

    bank_entropy = []
    for b in range(0, len(data), 0x8000):
        chunk = data[b : b + 0x8000]
        if len(chunk) >= 0x100:
            bank_entropy.append({"offset": hex(b), "entropy": round(entropy(chunk), 3)})
    bank_entropy.sort(key=lambda x: x["entropy"], reverse=True)

    return {
        "ppu_patterns": pat_hits,
        "wall_name_hits": wall_hits,
        "top_entropy_banks": bank_entropy[:12],
        "lz2_10fb_pairs": sum(
            1 for i in range(len(data) - 1) if data[i] == 0x10 and data[i + 1] == 0xFB
        ),
    }


def scan_genesis(data: bytes) -> dict:
    # 68k common: move.l #imm, d0 patterns near VDP setup
    vdp = bytes([0x00, 0xBF, 0xC0, 0x00])  # VDP data port A0xxxx
    vdp_hits = [i for i in range(len(data) - 4) if data[i : i + 4] == vdp]
    return {
        "vdp_data_port_refs": len(vdp_hits),
        "vdp_first": [hex(o) for o in vdp_hits[:8]],
        "chunk_entropy_64k": sorted(
            [
                {"offset": hex(b), "entropy": round(entropy(data[b : b + 0x10000]), 3)}
                for b in range(0, len(data), 0x10000)
                if b + 0x100 <= len(data)
            ],
            key=lambda x: x["entropy"],
            reverse=True,
        )[:8],
    }


def analyze(path: Path) -> dict:
    data = path.read_bytes()
    kind = "snes" if path.suffix.lower() in {".sfc", ".smc"} else "genesis"
    out = {
        "path": str(path),
        "kind": kind,
        "size": len(data),
        "md5": hashlib.md5(data).hexdigest(),
        "keyword_strings": keyword_strings(data)[:40],
    }
    if kind == "snes":
        out["header"] = snes_header(data)
        out["scan"] = scan_snes(data)
    else:
        out["header"] = genesis_header(data)
        out["scan"] = scan_genesis(data)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", nargs="+", type=Path)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    results = [analyze(p) for p in args.rom]
    if args.json:
        import json

        print(json.dumps(results, indent=2))
        return 0

    import sys

    for r in results:
        sys.stdout.buffer.write(f"=== {r['kind'].upper()} {r['path']} ===\n".encode())
        sys.stdout.buffer.write(f"size={r['size']:,} md5={r['md5']}\n".encode())
        sys.stdout.buffer.write(f"header: {r['header']!r}\n".encode())
        sys.stdout.buffer.write(f"scan: {r['scan']!r}\n".encode())
        sys.stdout.buffer.write(b"keyword strings (first 15):\n")
        for off, s in r["keyword_strings"][:15]:
            sys.stdout.buffer.write(f"  {off:#06x}: {s!r}\n".encode())
        sys.stdout.buffer.write(b"\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

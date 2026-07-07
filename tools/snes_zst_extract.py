#!/usr/bin/env python3
"""Extract WRAM / VRAM / CGRAM from ZSNES .zst save states (V0.6 layout).

Offsets verified against ZsKnight's format (romhacking.net doc #170) and a
Chrono Trigger .zst from the user's SNES Gems collection (2026-07).
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

ZST_MAGIC = b"ZSNES Save State File V0.6"
# Fixed layout through VRAM in the CPU/PPU portion (199699 bytes core block).
ZST_WRAM7E = 0x0C13  # 3091
ZST_WRAM7F = ZST_WRAM7E + 0x10000
ZST_VRAM = ZST_WRAM7F + 0x10000
ZST_CGRAM = ZST_VRAM - 0x400 - 0x400 - 0x400  # pcgram + cgram + oamram padding block

WRAM_STAGING = (
    {"wram": 0x7E00, "vram": 0x2D40, "size": 3200, "label": "title/field chunk A"},
    {"wram": 0x4C80, "vram": 0x3380, "size": 3200, "label": "chunk B"},
    {"wram": 0x5900, "vram": 0x3940, "size": 3200, "label": "chunk C"},
    {"wram": 0x4000, "vram": 0x1420, "size": 960, "label": "exploration (when $C4 set)"},
)


@dataclass
class ZstMemory:
    path: Path
    wram7e: bytes
    wram7f: bytes
    vram: bytes
    cgram: bytes

    @classmethod
    def load(cls, path: Path) -> ZstMemory:
        data = path.read_bytes()
        if not data.startswith(ZST_MAGIC):
            raise ValueError(f"Not a ZSNES V0.6 save state: {path}")
        need = ZST_VRAM + 0x10000
        if len(data) < need:
            raise ValueError(f"Save state too small ({len(data)} B, need {need})")
        return cls(
            path=path,
            wram7e=data[ZST_WRAM7E : ZST_WRAM7E + 0x10000],
            wram7f=data[ZST_WRAM7F : ZST_WRAM7F + 0x10000],
            vram=data[ZST_VRAM : ZST_VRAM + 0x10000],
            cgram=data[ZST_CGRAM : ZST_CGRAM + 0x200],
        )

    def write_bins(self, out_dir: Path, prefix: str = "") -> dict[str, Path]:
        out_dir.mkdir(parents=True, exist_ok=True)
        stem = prefix or self.path.stem
        paths = {
            "vram": out_dir / f"{stem}.vram.bin",
            "cgram": out_dir / f"{stem}.cgram.bin",
            "wram7e": out_dir / f"{stem}.wram7e.bin",
            "wram7f": out_dir / f"{stem}.wram7f.bin",
        }
        paths["vram"].write_bytes(self.vram)
        paths["cgram"].write_bytes(self.cgram)
        paths["wram7e"].write_bytes(self.wram7e)
        paths["wram7f"].write_bytes(self.wram7f)
        return paths

    def compare_staging(self) -> list[dict]:
        """Compare WRAM DMA staging offsets to VRAM destinations from $00E650."""
        rows: list[dict] = []
        for entry in WRAM_STAGING:
            woff = entry["wram"]
            voff = entry["vram"]
            size = entry["size"]
            w = self.wram7e[woff : woff + size]
            v = self.vram[voff : voff + size]
            matches = sum(a == b for a, b in zip(w, v))
            rows.append(
                {
                    "label": entry["label"],
                    "wram7e": f"${0x7E0000 + woff:06X}",
                    "vram": f"${voff:04X}",
                    "size": size,
                    "matches": matches,
                    "ratio": matches / size if size else 0.0,
                }
            )
        pal_w = self.wram7e[0x7E00 : 0x7E00 + 0x200]
        pal_c = self.cgram[:0x200]
        pal_matches = sum(a == b for a, b in zip(pal_w, pal_c))
        rows.append(
            {
                "label": "palette WRAM7E:7E00 vs CGRAM",
                "wram7e": "$007E:7E00",
                "vram": "CGRAM",
                "size": 0x200,
                "matches": pal_matches,
                "ratio": pal_matches / 0x200,
            }
        )
        return rows


def tile_likeness(vram: bytes, offset: int, tiles: int = 16) -> float:
    """Heuristic: fraction of 32-byte SNES 4bpp tiles with >=4 distinct bytes."""
    if offset + tiles * 32 > len(vram):
        return 0.0
    ok = 0
    for t in range(tiles):
        block = vram[offset + t * 32 : offset + (t + 1) * 32]
        if len(set(block)) >= 4:
            ok += 1
    return ok / tiles


def main() -> int:
    import argparse
    import json

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("zst", type=Path, help="ZSNES .zst save state")
    ap.add_argument("-o", "--out-dir", type=Path, help="Write .vram.bin / .cgram.bin / .wram7e.bin")
    ap.add_argument("--prefix", default="", help="Output filename prefix")
    ap.add_argument("--compare-staging", action="store_true", help="WRAM staging vs VRAM table")
    ap.add_argument("--json", type=Path, help="Write compare-staging JSON here")
    args = ap.parse_args()

    mem = ZstMemory.load(args.zst)
    if args.out_dir:
        written = mem.write_bins(args.out_dir, args.prefix)
        for kind, path in written.items():
            print(f"wrote {kind}: {path} ({path.stat().st_size} B)")

    nz_vram = sum(1 for b in mem.vram if b)
    print(f"VRAM non-zero bytes: {nz_vram}/65536")
    for probe in (0x1420, 0x2D40, 0x3380, 0x3940, 0x4000):
        tl = tile_likeness(mem.vram, probe)
        sample = mem.vram[probe : probe + 16].hex()
        print(f"  VRAM ${probe:04X}: tile-likeness={tl:.0%} sample={sample}")

    if args.compare_staging or args.json:
        rows = mem.compare_staging()
        for row in rows:
            print(
                f"{row['label']}: {row['matches']}/{row['size']} "
                f"({row['ratio']:.1%}) {row['wram7e']} -> {row['vram']}"
            )
        if args.json:
            args.json.parent.mkdir(parents=True, exist_ok=True)
            args.json.write_text(json.dumps(rows, indent=2), encoding="utf-8")
            print(f"wrote {args.json}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

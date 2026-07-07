#!/usr/bin/env python3
"""Pretty-print the corrected SNES MM2 DMA table (ROM / CGRAM / WRAM->VRAM)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def row(s: dict) -> str:
    dv = s["dest_vram"] if s["dest_vram"] is not None else "----"
    extra = s.get("resolved_wram") or s.get("addr24") or ""
    return (
        f"{s['setup_offset']:>8}  {s.get('kind'):8}  {s.get('snes_addr'):>10}  "
        f"file={str(s.get('file_offset')):>9}  size={s['size_bytes']:>6}  "
        f"bbad={s['bbad']:>4}  vram={dv:>6}  {extra}"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("table", type=Path)
    args = ap.parse_args()
    d = json.loads(args.table.read_text())
    seen = set()
    uniq = []
    for s in d["all_setups"]:
        k = (s.get("kind"), s.get("snes_addr"), s.get("file_offset"),
             s["size_bytes"], s["dest_vram"], s["bbad"])
        if k in seen:
            continue
        seen.add(k)
        uniq.append(s)

    print("== ROM -> VRAM (CHR/tilemap candidates) ==")
    for s in sorted(uniq, key=lambda x: x.get("file_offset") or "z"):
        if s.get("kind") == "rom" and s["bbad"] != "0x22":
            print(row(s))
    print("\n== ROM -> CGRAM (palette) ==")
    for s in uniq:
        if s.get("kind") == "rom" and s["bbad"] == "0x22":
            print(row(s))
    print("\n== WRAM/SRAM/other -> CGRAM (palette) ==")
    for s in uniq:
        if s["bbad"] == "0x22" and s.get("kind") != "rom":
            print(row(s))
    print("\n== WRAM/SRAM/other -> VRAM (staged) ==")
    for s in uniq:
        if s.get("kind") in ("wram", "sram", "other", "rom_oob") and s["bbad"] != "0x22":
            print(row(s))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

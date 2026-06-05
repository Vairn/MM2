#!/usr/bin/env python3
"""Phase 4: encode the sci-fi town sheets as drop-in .32 assets.

Decodes town.32 / townf.32 / townt.32, applies the index->material palette
swap (pure recolour; pixel indices unchanged), and re-encodes them with the
validated planar encoder into EXTRACTED/gfx_scifi/out/. Originals untouched.

Verifies, for each sheet:
  * the 16 FREE palette slots (.anm overlay reservation) are byte-identical
  * re-decoding the output gives pixel-identical indices to the original
"""
from __future__ import annotations

from pathlib import Path

from encode_image32 import encode_image32
from scifi_town_common import (
    ROOT,
    USED_INDICES,
    build_scifi_palette,
    decode_bytes_indexed,
    decode_sheet_indexed,
)

OUT = ROOT / "EXTRACTED" / "gfx_scifi" / "out"
SHEETS = ("town.32", "townf.32", "townt.32")
FREE_INDICES = [i for i in range(32) if i not in USED_INDICES]


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    all_ok = True
    for name in SHEETS:
        frames, orig_pal, depth = decode_sheet_indexed(ROOT / name)
        scifi_pal = build_scifi_palette(orig_pal)

        # FREE slots must stay byte-identical (reserved for .anm overlays).
        free_ok = all(scifi_pal[i] == orig_pal[i] for i in FREE_INDICES)

        enc = encode_image32(frames, scifi_pal, depth)
        (OUT / name).write_bytes(enc)

        # Re-decode and confirm indices are unchanged (silhouettes preserved).
        frames2, pal2, depth2 = decode_bytes_indexed(enc)
        idx_ok = len(frames2) == len(frames) and all(
            a.idx == b.idx and (a.w, a.h, a.flags) == (b.w, b.h, b.flags)
            for a, b in zip(frames, frames2)
        )
        pal_ok = pal2 == scifi_pal and depth2 == depth

        ok = free_ok and idx_ok and pal_ok
        all_ok = all_ok and ok
        print(f"{'OK  ' if ok else 'FAIL'} {name:10s} frames={len(frames):3d} "
              f"free_slots={'preserved' if free_ok else 'CHANGED'} "
              f"indices={'identical' if idx_ok else 'DIFFER'} "
              f"palette={'applied' if pal_ok else 'BAD'} -> {OUT / name}")

    print(f"\nFREE palette indices (reserved for .anm): {FREE_INDICES}")
    print("All sci-fi town assets written." if all_ok else "ERRORS — see above.")


if __name__ == "__main__":
    main()

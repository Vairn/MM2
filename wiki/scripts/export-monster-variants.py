#!/usr/bin/env python3
"""Export monster variant GIFs + VitePress markdown for the wiki gallery."""
from __future__ import annotations

import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WIKI = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from export_monster_variants import (  # noqa: E402
    DEFAULT_GOG,
    collect_rows_from_disk,
    build_markdown,
    main as export_main,
)


def main() -> int:
    gallery = WIKI / "public" / "gallery" / "monster-variants"
    md_out = WIKI / "docs" / "gallery" / "monster-variants.md"
    extracted = ROOT / "EXTRACTED" / "monster_variants"
    gog = DEFAULT_GOG
    monsters = ROOT / "monsters.dat"

    argv = [
        "-o",
        str(gallery),
        "--markdown",
        str(md_out),
        "--image-prefix",
        "/gallery/monster-variants",
        "--no-html",
        "--data",
        str(ROOT),
        "--monsters",
        str(monsters),
        "--gog",
        str(gog),
    ]

    if (gog / "MONSTERS.4").is_file() and (gog / "MONSTERS.16").is_file():
        if gallery.exists():
            shutil.rmtree(gallery)
        return export_main(argv)

    # No GOG install — reuse EXTRACTED GIFs if present.
    if not extracted.is_dir() or not any(extracted.glob("*_amiga.gif")):
        print(
            "skip monster variants: need GOG MONSTERS.* or pre-built EXTRACTED/monster_variants",
            file=sys.stderr,
        )
        return 0

    print(f"copying GIFs from {extracted} -> {gallery}")
    if gallery.exists():
        shutil.rmtree(gallery)
    shutil.copytree(extracted, gallery, ignore=shutil.ignore_patterns("index.html", "manifest.txt"))

    mon_raw = monsters.read_bytes()
    rows, stats = collect_rows_from_disk(gallery, mon_raw)
    build_markdown(rows, md_out, stats, image_prefix="/gallery/monster-variants")
    print(f"Wrote {md_out} ({len(rows)} rows, from cached GIFs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

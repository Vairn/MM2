#!/usr/bin/env python3
"""Build VitePress MM1 gallery pages (maps + WALLPIX)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WIKI = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from export_mm1_gallery import export_mm1_gallery  # noqa: E402


def main() -> int:
    export_mm1_gallery(
        WIKI / "public" / "gallery" / "mm1",
        docs_out=WIKI / "docs" / "gallery",
        image_prefix="/gallery/mm1",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

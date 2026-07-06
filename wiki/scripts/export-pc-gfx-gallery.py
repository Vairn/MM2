#!/usr/bin/env python3
"""Wiki wrapper — export PC DOS (.4/.16) gallery PNGs + markdown pages."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from pc_gfx_export import export_all  # noqa: E402


def main() -> int:
    wiki = Path(__file__).resolve().parents[1]
    return export_all(
        gog_dir=None,
        pc_gfx_out=ROOT / "EXTRACTED" / "pc_gfx",
        gallery_out=wiki / "public" / "gallery",
        docs_out=wiki / "docs" / "gallery",
    )


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Wiki wrapper — export SNES CHR gallery PNGs + markdown pages."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from snes_gfx_export import export_all  # noqa: E402


def main() -> int:
    wiki = Path(__file__).resolve().parents[1]
    return export_all(
        wiki / "public" / "gallery",
        wiki / "docs" / "gallery",
    )


if __name__ == "__main__":
    raise SystemExit(main())

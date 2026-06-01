#!/usr/bin/env python3
"""Decode book.32 frame 0 for the wiki logo (same codec as render_view_refs.py)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from render_view_refs import load_frame  # noqa: E402


def main() -> int:
    data_dir = ROOT
    book = data_dir / "book.32"
    if not book.exists():
        book = data_dir / "EXTRACTED" / "book.32"
    if not book.exists():
        print(f"skip (missing): book.32", file=sys.stderr)
        return 0

    out = Path(__file__).resolve().parents[1] / "public" / "book-f00.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    img = load_frame("book.32", 0, book.parent)
    img.save(out)
    print(f"exported {out} ({img.size[0]}x{img.size[1]})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

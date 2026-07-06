#!/usr/bin/env python3
"""Export platform graphics index (monsters + walls) for the VitePress wiki."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WIKI = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from gen_gfx_compare_html import DEFAULT_GOG, main as compare_main  # noqa: E402


def main() -> int:
    gallery = WIKI / "public" / "gallery" / "platform-compare"
    md_out = WIKI / "docs" / "gallery" / "platform-compare.md"
    gog = DEFAULT_GOG

    if not (gog / "MONSTERS.4").is_file():
        print("skip platform compare: GOG MONSTERS.4 not found", file=sys.stderr)
        return 0

    return compare_main(
        [
            "-o",
            str(gallery),
            "--markdown",
            str(md_out),
            "--html-prefix",
            "/gallery/platform-compare/index.html",
            "--data",
            str(ROOT),
            "--gog",
            str(gog),
        ]
    )


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""DISABLED — integer-NEAREST crush of soft AI sheets still looks like downsample mush.

Use tools/author_agui_pixel_art.py (paint on the final grid; concept = colour ref only).
"""
from __future__ import annotations

import sys

print(
    "REFUSED: remaster_agui_art.py scaled soft concept sheets into 12x12/28x28.\n"
    "That still reads as shithouse even with NEAREST.\n"
    "Use:\n"
    "  python tools/author_agui_pixel_art.py\n"
    "  python tools/pack_agui_ui.py\n"
    "  python tools/ui_pack_preview.py\n",
    file=sys.stderr,
)
raise SystemExit(2)

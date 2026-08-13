#!/usr/bin/env python3
"""DEPRECATED placeholder generator — DO NOT use for shipping art.

This historically painted rectangle 'seed' sprites. That is the shithouse path.
Use tools/remaster_agui_art.py with real concept sheets instead.
"""
from __future__ import annotations

import sys

print(
    "REFUSED: gen_agui_seed_art.py only produced rectangle placeholders.\n"
    "That violates the Agui art rule (no shithouse / no crush-downsample mush).\n"
    "Use:\n"
    "  python tools/remaster_agui_art.py\n"
    "  python tools/pack_agui_ui.py\n"
    "  python tools/ui_pack_preview.py\n",
    file=sys.stderr,
)
raise SystemExit(2)

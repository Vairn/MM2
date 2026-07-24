#!/usr/bin/env python3
"""1:1 and ×2 nearest contact sheet for Agui atlas review.

Rejects blurry inspection workflows — always nearest-neighbor scale.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

ROOT = Path(__file__).resolve().parents[1]
AGUI = ROOT / "game" / "data" / "ui" / "agui"


def main() -> int:
    meta_path = AGUI / "agui_atlas.json"
    rgba_path = AGUI / "agui_atlas.rgba"
    if not meta_path.is_file() or not rgba_path.is_file():
        print("Missing atlas — run pack_agui_ui.py", file=sys.stderr)
        return 1
    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    w, h = meta["width"], meta["height"]
    raw = rgba_path.read_bytes()
    if len(raw) != w * h * 4:
        print(f"RGBA size mismatch: {len(raw)} vs {w*h*4}", file=sys.stderr)
        return 1
    atlas = Image.frombytes("RGBA", (w, h), raw)

    # Contact sheet of individual sprites at 1:1 and 2×
    sprites = meta["sprites"]
    cols = 4
    cell_w = 64
    cell_h = 80
    rows = (len(sprites) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cell_w, rows * cell_h), (16, 16, 24))
    draw = ImageDraw.Draw(sheet)
    for i, sp in enumerate(sprites):
        col, row = i % cols, i // cols
        ox, oy = col * cell_w, row * cell_h
        crop = atlas.crop((sp["x"], sp["y"], sp["x"] + sp["w"], sp["y"] + sp["h"]))
        sheet.paste(crop, (ox + 2, oy + 2), crop)
        zoom = crop.resize((sp["w"] * 2, sp["h"] * 2), Image.Resampling.NEAREST)
        sheet.paste(zoom, (ox + 2 + sp["w"] + 4, oy + 2), zoom)
        draw.text((ox + 2, oy + cell_h - 12), sp["name"].split("/")[-1][:10], fill=(192, 160, 96))

    out1 = AGUI / "preview_atlas_1x.png"
    out2 = AGUI / "preview_contact.png"
    atlas.save(out1)
    sheet.save(out2)
    print(f"Wrote {out1.relative_to(ROOT)} (atlas 1:1)")
    print(f"Wrote {out2.relative_to(ROOT)} (sprites 1:1 + nearest ×2)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

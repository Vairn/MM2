#!/usr/bin/env python3
"""Phase 2: lock the sci-fi look + derive a 32-colour Amiga palette.

Takes the generated concept mockup, snaps it to the Amiga 4-bit-per-channel
(0RGB) grid, quantizes to 31 colours, and reserves index 0 as the transparency
key (matching the .32 blit-mask convention). Writes:

  EXTRACTED/gfx_scifi/concept/scifi_town_concept.png   quantized 32-colour view
  EXTRACTED/gfx_scifi/concept/scifi_palette.png        palette swatch
  EXTRACTED/gfx_scifi/concept/scifi_palette.json       [[r,g,b]*32], idx0=key
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
CONCEPT_DIR = ROOT / "EXTRACTED" / "gfx_scifi" / "concept"
# The raw mockup lives in the agent asset dir; copy/look there or in concept dir.
RAW_CANDIDATES = [
    Path.home() / ".cursor" / "projects" / "c-20260421-D-REC-development-MM2" / "assets" / "scifi_town_concept_raw.png",
    CONCEPT_DIR / "scifi_town_concept_raw.png",
]

# Index 0 transparency key (near-black, on the 4-bit grid).
KEY_COLOR = (0, 0, 0)

# Curated 32-colour sci-fi palette (Amiga 4-bit/channel grid, multiples of 17).
# Auto-quantizing the (very dark) concept lost the bright accents, so the look
# is locked here as deliberate material ramps. idx0 stays the transparency key.
SCIFI_PALETTE: list[tuple[int, int, int]] = [
    KEY_COLOR,             # 0  transparency key
    # --- steel blue-grey metal bulkhead ramp (light -> dark) ---
    (221, 238, 255),       # 1  metal hi-light
    (170, 187, 204),       # 2  metal light
    (136, 153, 170),       # 3
    (102, 119, 153),       # 4
    (85, 102, 136),        # 5
    (68, 85, 119),         # 6
    (51, 68, 102),         # 7
    (34, 51, 85),          # 8
    (34, 34, 68),          # 9
    (17, 17, 51),          # 10 metal shadow
    # --- cyan emissive (lights / blast-door seam) ramp (core -> shadow) ---
    (221, 255, 255),       # 11 cyan core
    (153, 238, 255),       # 12 bright cyan
    (51, 204, 238),        # 13 cyan
    (34, 153, 187),        # 14 mid cyan
    (17, 102, 136),        # 15 dim cyan
    (17, 68, 102),         # 16 deep cyan
    # --- amber / warning ramp ---
    (255, 221, 136),       # 17 pale amber
    (255, 170, 51),        # 18 amber
    (238, 119, 17),        # 19 orange
    (170, 85, 17),         # 20 dark amber
    (102, 51, 17),         # 21 deep brown
    # --- dark tech-deck / floor neutrals ---
    (51, 68, 68),          # 22
    (34, 51, 51),          # 23
    (34, 51, 68),          # 24
    (17, 34, 34),          # 25
    (17, 34, 51),          # 26
    (0, 17, 34),           # 27 very dark teal
    (0, 17, 17),           # 28
    # --- extras ---
    (255, 255, 255),       # 29 pure white spec / status LED
    (136, 238, 170),       # 30 green status LED
    (51, 51, 68),          # 31 neutral grey-blue mid
]
assert len(SCIFI_PALETTE) == 32


def snap4(c: int) -> int:
    return round(c / 255 * 15) * 17


def snap_image(im: Image.Image) -> Image.Image:
    im = im.convert("RGB")
    lut = [snap4(i) for i in range(256)]
    return im.point(lut * 3)


def find_raw() -> Path:
    for p in RAW_CANDIDATES:
        if p.exists():
            return p
    print("!! raw concept not found in:", *[str(p) for p in RAW_CANDIDATES], sep="\n  ")
    sys.exit(1)


def palette_swatch(pal: list[tuple[int, int, int]], title: str) -> Image.Image:
    cell = 28
    cols = 8
    rows = (len(pal) + cols - 1) // cols
    im = Image.new("RGBA", (cols * cell, rows * cell + 16), (24, 24, 30, 255))
    d = ImageDraw.Draw(im)
    d.text((4, 3), title, fill=(220, 220, 230, 255))
    for i, (r, g, b) in enumerate(pal):
        cx = (i % cols) * cell
        cy = (i // cols) * cell + 16
        d.rectangle([cx, cy, cx + cell - 1, cy + cell - 1], fill=(r, g, b, 255))
        d.rectangle([cx, cy, cx + cell - 1, cy + cell - 1], outline=(60, 60, 70, 255))
        if i == 0:
            d.line([cx, cy, cx + cell - 1, cy + cell - 1], fill=(255, 0, 0, 255))
    return im


def main() -> None:
    CONCEPT_DIR.mkdir(parents=True, exist_ok=True)
    raw = find_raw()
    src = snap_image(Image.open(raw))

    palette = list(SCIFI_PALETTE)  # 32 entries, idx0 = transparent key.

    # Render a preview of the concept mapped onto the final 32-colour palette.
    flat_pal: list[int] = []
    for r, g, b in palette:
        flat_pal += [r, g, b]
    flat_pal += [0, 0, 0] * (256 - len(palette))
    pal_img = Image.new("P", (1, 1))
    pal_img.putpalette(flat_pal)
    preview = src.quantize(palette=pal_img, dither=Image.Dither.NONE).convert("RGB")
    preview.save(CONCEPT_DIR / "scifi_town_concept.png")

    palette_swatch(palette, "sci-fi palette (idx0 = transparent key)").save(
        CONCEPT_DIR / "scifi_palette.png"
    )
    (CONCEPT_DIR / "scifi_palette.json").write_text(
        json.dumps([list(c) for c in palette], indent=2)
    )
    print(f"32-colour sci-fi palette written to {CONCEPT_DIR}")
    print("idx0 (key):", palette[0])
    for i, c in enumerate(palette[1:], 1):
        print(f"  {i:2d}: {c}")


if __name__ == "__main__":
    main()

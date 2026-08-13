#!/usr/bin/env python3
"""Pack Agui indexed PNGs into SDL atlas (agui_atlas.rgba + agui_atlas.json).

Validates:
  - exact expected sizes (native authoring — rejects oversized sources)
  - palette locked to palette.json (off-palette pixels rejected)
  - no dither requirement (indexed pens only)

AGA planar remapping (pens 32–63) is documented for a follow-on pack path;
this tool emits RGBA for the SDL compositor.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

ROOT = Path(__file__).resolve().parents[1]
AGUI = ROOT / "game" / "data" / "ui" / "agui"

# name -> (w, h) — final pixel sizes only
EXPECTED: dict[str, tuple[int, int]] = {
    "chrome/frame": (216, 128),
    "chrome/gem_on": (6, 6),
    "chrome/gem_off": (6, 6),
    "chrome/face_gem": (18, 10),
}
for n in (
    "cast", "shoot", "unlock", "bash", "rest", "search", "map", "quick",
    "attack", "fight", "run", "block", "use",
    "dpad_n", "dpad_s", "dpad_w", "dpad_e", "dpad_wait",
):
    EXPECTED[f"icons/{n}"] = (12, 12)
for i in range(8):
    EXPECTED[f"faces/face_{i:02d}"] = (28, 28)


def load_palette(path: Path) -> list[tuple[int, int, int]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    return [tuple(c) for c in data["colors"]]


def nearest_pen(rgb: tuple[int, int, int], palette: list[tuple[int, int, int]]) -> int:
    best = 0
    best_d = 1 << 30
    for i, p in enumerate(palette):
        d = (rgb[0] - p[0]) ** 2 + (rgb[1] - p[1]) ** 2 + (rgb[2] - p[2]) ** 2
        if d < best_d:
            best_d = d
            best = i
    return best


def load_sprite(path: Path, expect: tuple[int, int], palette: list[tuple[int, int, int]],
                max_off: int) -> tuple[list[tuple[int, int, int, int]], int]:
    """Return RGBA pixels + off-palette count. Rejects wrong size."""
    img = Image.open(path)
    if img.size != expect:
        raise SystemExit(
            f"REJECT {path}: size {img.size} != expected {expect} "
            f"(author at native resolution — do not downsample)"
        )
    if img.mode != "P":
        # Convert via nearest palette without dither
        img = img.convert("RGB")
        off = 0
        pixels: list[tuple[int, int, int, int]] = []
        for y in range(img.height):
            for x in range(img.width):
                rgb = img.getpixel((x, y))
                assert isinstance(rgb, tuple)
                pen = nearest_pen(rgb[:3], palette)
                if palette[pen] != rgb[:3]:
                    off += 1
                if pen == 0:
                    pixels.append((0, 0, 0, 0))
                else:
                    r, g, b = palette[pen]
                    pixels.append((r, g, b, 255))
        if off > max_off:
            raise SystemExit(f"REJECT {path}: {off} off-palette pixels (max {max_off})")
        return pixels, off

    # Indexed: verify palette entries match (or remap)
    off = 0
    pixels = []
    for y in range(img.height):
        for x in range(img.width):
            pen = int(img.getpixel((x, y)))
            if pen < 0 or pen >= len(palette):
                off += 1
                pen = 0
            if pen == 0:
                pixels.append((0, 0, 0, 0))
            else:
                r, g, b = palette[pen]
                pixels.append((r, g, b, 255))
    if off > max_off:
        raise SystemExit(f"REJECT {path}: {off} invalid pens (max {max_off})")
    return pixels, off


def pack(max_off: int) -> int:
    palette = load_palette(AGUI / "palette.json")
    sprites: list[tuple[str, int, int, list[tuple[int, int, int, int]]]] = []
    for name, expect in EXPECTED.items():
        path = AGUI / f"{name}.png"
        if not path.is_file():
            print(f"WARN missing {path}", file=sys.stderr)
            continue
        pix, off = load_sprite(path, expect, palette, max_off)
        if off:
            print(f"  note {name}: {off} off-palette snapped")
        sprites.append((name, expect[0], expect[1], pix))

    if not sprites:
        print("No sprites found — run tools/gen_agui_seed_art.py first", file=sys.stderr)
        return 1

    # Shelf-pack into atlas
    pad = 1
    x = pad
    y = pad
    row_h = 0
    atlas_w = 256
    placements: list[dict] = []
    # First pass: compute height
    for name, w, h, _ in sprites:
        if x + w + pad > atlas_w:
            x = pad
            y += row_h + pad
            row_h = 0
        placements.append({"name": name, "x": x, "y": y, "w": w, "h": h})
        x += w + pad
        row_h = max(row_h, h)
    atlas_h = y + row_h + pad
    # Align to 4
    atlas_h = (atlas_h + 3) & ~3

    rgba = bytearray(atlas_w * atlas_h * 4)
    for place, (_name, w, h, pix) in zip(placements, sprites):
        px, py = place["x"], place["y"]
        for row in range(h):
            for col in range(w):
                r, g, b, a = pix[row * w + col]
                o = ((py + row) * atlas_w + (px + col)) * 4
                rgba[o : o + 4] = bytes((r, g, b, a))

    out_rgba = AGUI / "agui_atlas.rgba"
    out_json = AGUI / "agui_atlas.json"
    out_rgba.write_bytes(rgba)
    meta = {
        "width": atlas_w,
        "height": atlas_h,
        "format": "rgba8",
        "palette": "palette.json",
        "aga_pen_base": 32,
        "aga_planar": "not packed yet — see 41-aga-port-plan.md §10",
        "sprites": placements,
    }
    out_json.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {out_rgba.relative_to(ROOT)} ({atlas_w}x{atlas_h}, {len(placements)} sprites)")
    print(f"Wrote {out_json.relative_to(ROOT)}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--max-off-palette", type=int, default=0,
                    help="Max off-palette pixels allowed per sprite (default 0)")
    args = ap.parse_args()
    return pack(args.max_off_palette)


if __name__ == "__main__":
    raise SystemExit(main())

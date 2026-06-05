#!/usr/bin/env python3
"""Crop tileable sci-fi surface PNGs from the corridor concept art.

Writes EXTRACTED/gfx_scifi/textures/{wall,floor,ceiling,door,light}.png
for use by render_scifi_corridor_3d.py.

Usage:
  python tools/extract_scifi_textures.py
  python tools/extract_scifi_textures.py --source path/to/concept.png --out dir
"""
from __future__ import annotations

import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install pillow")

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "EXTRACTED" / "gfx_scifi" / "concept" / "scifi_town_concept.png"
DEFAULT_OUT = ROOT / "EXTRACTED" / "gfx_scifi" / "textures"

# wall: flat panel between sconce and door — no vents, no sconce, no mirror seam.
CROPS = {
    "wall": (0.22, 0.20, 0.34, 0.80),
    "floor": (0.22, 0.72, 0.78, 0.98),
    "ceiling": (0.10, 0.00, 0.90, 0.16),
    "door": (0.36, 0.16, 0.64, 0.88),
}


def _crop_box(w: int, h: int, norm: tuple[float, float, float, float]) -> tuple[int, int, int, int]:
    x0, y0, x1, y1 = norm
    return (int(x0 * w), int(y0 * h), int(x1 * w), int(y1 * h))


def make_tileable(im: Image.Image, axis: str = "x") -> Image.Image:
    """Mirror-blend edges so the crop repeats cleanly when tiled."""
    im = im.convert("RGB")
    w, h = im.size
    if axis in ("x", "both") and w >= 4:
        strip = max(2, w // 8)
        px = im.load()
        for y in range(h):
            for i in range(strip):
                t = i / strip
                lr = px[i, y]
                rr = px[w - 1 - i, y]
                blend = tuple(int(lr[c] * (1 - t) + rr[c] * t) for c in range(3))
                px[i, y] = blend
                px[w - 1 - i, y] = blend
    if axis in ("y", "both") and h >= 4:
        strip = max(2, h // 8)
        px = im.load()
        for x in range(w):
            for i in range(strip):
                t = i / strip
                top = px[x, i]
                bot = px[x, h - 1 - i]
                blend = tuple(int(top[c] * (1 - t) + bot[c] * t) for c in range(3))
                px[x, i] = blend
                px[x, h - 1 - i] = blend
    return im


def extract(source: Path, out_dir: Path) -> list[Path]:
    if not source.exists():
        raise FileNotFoundError(f"concept not found: {source}")
    out_dir.mkdir(parents=True, exist_ok=True)
    master = Image.open(source).convert("RGB")
    w, h = master.size
    written: list[Path] = []
    tile_axis = {
        "wall": "none",
        "floor": "both",
        "ceiling": "x",
        "door": "none",
    }
    for name, norm in CROPS.items():
        box = _crop_box(w, h, norm)
        patch = master.crop(box)
        axis = tile_axis.get(name, "none")
        if axis != "none":
            patch = make_tileable(patch, axis)
        path = out_dir / f"{name}.png"
        patch.save(path)
        written.append(path)
        print(f"  {name}.png  {patch.size[0]}x{patch.size[1]}  <- {box}")
    return written


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()
    print(f"extract textures from {args.source}")
    paths = extract(args.source, args.out)
    print(f"wrote {len(paths)} files -> {args.out}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Render Middlegate (screen 0) map grids to PNG images.

Outputs:
- visual page grid (map.dat page 0)
- collision page grid (map.dat page 1)

Cells are drawn with visible gaps, coordinates, and hex values.
"""

from __future__ import annotations

from pathlib import Path
from typing import Sequence

try:
    from PIL import Image, ImageDraw
except ImportError:
    raise SystemExit("pip install pillow")


ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "map.dat"
OUT_DIR = ROOT / "EXTRACTED" / "docs" / "img"

GRID = 16
SCREEN_SIZE = 512
PAGE_SIZE = 256


def draw_grid(page: bytes, title: str, out_path: Path) -> None:
    cell = 36
    gap = 3
    margin = 44
    w = margin * 2 + GRID * cell + (GRID - 1) * gap
    h = margin * 2 + GRID * cell + (GRID - 1) * gap

    img = Image.new("RGBA", (w, h), (12, 12, 18, 255))
    d = ImageDraw.Draw(img)

    # Title
    d.text((12, 10), title, fill=(230, 230, 235, 255))

    # Axis labels
    for x in range(GRID):
        px = margin + x * (cell + gap) + 10
        d.text((px, margin - 20), f"{x:X}", fill=(170, 180, 220, 255))
    for sy in range(GRID):  # sy=0 top = map y15
        py = margin + sy * (cell + gap) + 10
        map_y = GRID - 1 - sy
        d.text((12, py), f"{map_y:X}", fill=(170, 180, 220, 255))

    for sy in range(GRID):
        map_y = GRID - 1 - sy
        for x in range(GRID):
            idx = map_y * GRID + x
            v = page[idx]

            x0 = margin + x * (cell + gap)
            y0 = margin + sy * (cell + gap)
            x1 = x0 + cell
            y1 = y0 + cell

            # color buckets just to make blocks visually separable
            if v == 0:
                fill = (40, 40, 55, 255)
            elif v < 0x40:
                fill = (52, 68, 90, 255)
            elif v < 0x80:
                fill = (72, 64, 96, 255)
            elif v < 0xC0:
                fill = (96, 62, 80, 255)
            else:
                fill = (110, 72, 60, 255)

            d.rectangle((x0, y0, x1, y1), fill=fill, outline=(145, 145, 165, 255), width=1)
            d.text((x0 + 8, y0 + 10), f"{v:02X}", fill=(245, 245, 245, 255))

    out_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(out_path)


def draw_bitmask_grid(page: bytes, title: str, out_path: Path, collision_mode: bool = False) -> None:
    """Draw a larger grid with per-cell bitmask decode."""
    cell = 58
    gap = 3
    margin = 44
    w = margin * 2 + GRID * cell + (GRID - 1) * gap
    h = margin * 2 + GRID * cell + (GRID - 1) * gap

    img = Image.new("RGBA", (w, h), (10, 10, 16, 255))
    d = ImageDraw.Draw(img)
    d.text((12, 10), title, fill=(230, 230, 235, 255))

    for x in range(GRID):
        px = margin + x * (cell + gap) + 8
        d.text((px, margin - 20), f"{x:X}", fill=(170, 180, 220, 255))
    for sy in range(GRID):
        py = margin + sy * (cell + gap) + 8
        map_y = GRID - 1 - sy
        d.text((12, py), f"{map_y:X}", fill=(170, 180, 220, 255))

    for sy in range(GRID):
        map_y = GRID - 1 - sy
        for x in range(GRID):
            idx = map_y * GRID + x
            v = page[idx]

            n = (v >> 0) & 0x3
            e = (v >> 2) & 0x3
            s = (v >> 4) & 0x3
            wv = (v >> 6) & 0x3

            x0 = margin + x * (cell + gap)
            y0 = margin + sy * (cell + gap)
            x1 = x0 + cell
            y1 = y0 + cell
            d.rectangle((x0, y0, x1, y1), fill=(54, 58, 72, 255), outline=(130, 130, 150, 255), width=1)

            d.text((x0 + 4, y0 + 4), f"{v:02X}  b{v:08b}", fill=(245, 245, 250, 255))
            if collision_mode:
                ev = 1 if (v & 0x80) else 0
                d.text((x0 + 4, y0 + 22), f"N{n} E{e} S{s} W{wv}", fill=(190, 220, 255, 255))
                d.text((x0 + 4, y0 + 38), f"event:{ev}", fill=(255, 130, 130, 255))
            else:
                d.text((x0 + 4, y0 + 22), f"N{n} E{e} S{s} W{wv}", fill=(190, 220, 255, 255))

    out_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(out_path)


def main() -> int:
    data = MAP_PATH.read_bytes()
    off = 0 * SCREEN_SIZE  # Middlegate
    visual = data[off : off + PAGE_SIZE]
    collision = data[off + PAGE_SIZE : off + SCREEN_SIZE]

    draw_grid(
        visual,
        "Middlegate visual page (screen 0, page 0)",
        OUT_DIR / "middlegate-visual-grid.png",
    )
    draw_grid(
        collision,
        "Middlegate collision page (screen 0, page 1)",
        OUT_DIR / "middlegate-collision-grid.png",
    )
    draw_bitmask_grid(
        visual,
        "Middlegate visual bitmask decode (N@0 E@2 S@4 W@6)",
        OUT_DIR / "middlegate-visual-bitmask.png",
        collision_mode=False,
    )
    draw_bitmask_grid(
        collision,
        "Middlegate collision bitmask decode (event on bit 7)",
        OUT_DIR / "middlegate-collision-bitmask.png",
        collision_mode=True,
    )
    print(f"wrote {OUT_DIR / 'middlegate-visual-grid.png'}")
    print(f"wrote {OUT_DIR / 'middlegate-collision-grid.png'}")
    print(f"wrote {OUT_DIR / 'middlegate-visual-bitmask.png'}")
    print(f"wrote {OUT_DIR / 'middlegate-collision-bitmask.png'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


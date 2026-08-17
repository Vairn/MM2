#!/usr/bin/env python3
"""Quantize user-generated item art into Agui 12×12 indexed PNGs.

Does NOT invent sprites. Reads your image-AI exports and writes
game/data/ui/agui/items/iXX.png (hex id, lowercase).

Accepted source sizes:
  12×12          — already native; palette-snap only
  96×96 (8×)     — nearest-neighbor to 12×12 (each logical pixel must be an 8×8 block)
  192×192 (16×)  — nearest-neighbor to 12×12

Soft / anti-aliased sources will snap poorly; regenerate those.

Examples:
  python tools/ingest_agui_item_icons.py --from-dir D:/exports/mm2_items
  python tools/ingest_agui_item_icons.py --sheet batch_01.png --start 0x01 --cols 8 --cell 96
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

ROOT = Path(__file__).resolve().parents[1]
AGUI = ROOT / "game" / "data" / "ui" / "agui"
OUT = AGUI / "items"
PALETTE_PATH = AGUI / "palette.json"

NAME_RE = re.compile(r"(?:^|[^\dA-Fa-f])i?([0-9A-Fa-f]{2})(?:\.\w+)?$")


def load_palette() -> list[tuple[int, int, int]]:
    data = json.loads(PALETTE_PATH.read_text(encoding="utf-8"))
    return [tuple(int(c) for c in rgb) for rgb in data["colors"]]


def nearest_pen(rgb: tuple[int, int, int], palette: list[tuple[int, int, int]]) -> int:
    best, best_d = 0, 1 << 30
    for i, p in enumerate(palette):
        d = (rgb[0] - p[0]) ** 2 + (rgb[1] - p[1]) ** 2 + (rgb[2] - p[2]) ** 2
        if d < best_d:
            best, best_d = i, d
    return best


def is_transparent(px: tuple) -> bool:
    if len(px) < 4:
        return px[0] == 0 and px[1] == 0 and px[2] == 0
    return px[3] < 16


def to_native(img: Image.Image) -> Image.Image:
    """Return a 12×12 RGB/RGBA image (nearest-neighbor if integer scale)."""
    w, h = img.size
    if w != h:
        raise SystemExit(f"REJECT {w}x{h}: item icons must be square")
    if w == 12:
        return img
    if w % 12 != 0:
        raise SystemExit(
            f"REJECT {w}x{h}: size must be 12 or an integer multiple of 12 "
            f"(96=8× or 192=16× logical pixels)"
        )
    scale = w // 12
    if scale not in (8, 16):
        print(f"WARN {w}x{h}: unusual scale {scale}× — NN down to 12×12 anyway",
              file=sys.stderr)
    return img.resize((12, 12), Image.Resampling.NEAREST)


def snap_indexed(img: Image.Image, palette: list[tuple[int, int, int]]) -> tuple[Image.Image, int]:
    """12×12 indexed PNG, pen 0 = transparent. Returns (image, off-palette count)."""
    src = img.convert("RGBA")
    out = Image.new("P", (12, 12), 0)
    flat: list[int] = []
    for rgb in palette:
        flat.extend(rgb)
    while len(flat) < 256 * 3:
        flat.extend([0, 0, 0])
    out.putpalette(flat)
    off = 0
    for y in range(12):
        for x in range(12):
            px = src.getpixel((x, y))
            assert isinstance(px, tuple)
            if is_transparent(px):
                out.putpixel((x, y), 0)
                continue
            rgb = (int(px[0]), int(px[1]), int(px[2]))
            pen = nearest_pen(rgb, palette)
            if palette[pen] != rgb:
                off += 1
            if pen == 0:
                pen = 7  # near-black, keep ink off the transparent index
            out.putpixel((x, y), pen)
    return out, off


def save_item(item_id: int, img: Image.Image, palette: list[tuple[int, int, int]],
              max_off: int) -> None:
    if item_id < 1 or item_id > 255:
        raise SystemExit(f"item id {item_id:#x} out of range 0x01..0xFF")
    native = to_native(img)
    indexed, off = snap_indexed(native, palette)
    if off > max_off:
        raise SystemExit(
            f"REJECT items/i{item_id:02x}.png: {off} off-palette pixels "
            f"(max {max_off}). Re-generate on the locked palette / hard grid."
        )
    OUT.mkdir(parents=True, exist_ok=True)
    dest = OUT / f"i{item_id:02x}.png"
    indexed.save(dest)
    note = f" ({off} snapped)" if off else ""
    print(f"  items/{dest.name} 12x12{note}")


def parse_id_token(token: str) -> int:
    token = token.strip()
    if token.lower().startswith("0x"):
        return int(token, 16)
    if re.fullmatch(r"[0-9A-Fa-f]{1,2}", token):
        return int(token, 16)
    raise SystemExit(f"bad id {token!r} — use 0x01 or 01")


def id_from_filename(path: Path) -> int | None:
    m = NAME_RE.search(path.stem)
    if not m:
        return None
    return int(m.group(1), 16)


def ingest_dir(folder: Path, palette: list[tuple[int, int, int]], max_off: int) -> int:
    files = sorted(p for p in folder.iterdir() if p.suffix.lower() in {".png", ".gif", ".bmp"})
    n = 0
    for path in files:
        item_id = id_from_filename(path)
        if item_id is None or item_id == 0:
            print(f"skip {path.name} (name must contain hex id, e.g. i3e.png)", file=sys.stderr)
            continue
        save_item(item_id, Image.open(path), palette, max_off)
        n += 1
    return n


def ingest_sheet(path: Path, start: int, cols: int, cell: int,
                 palette: list[tuple[int, int, int]], max_off: int) -> int:
    img = Image.open(path).convert("RGBA")
    w, h = img.size
    if w % cell != 0 or h % cell != 0:
        raise SystemExit(f"REJECT sheet {w}x{h}: not a multiple of --cell {cell}")
    sheet_cols = w // cell
    rows = h // cell
    if cols <= 0:
        cols = sheet_cols
    n = 0
    item_id = start
    for row in range(rows):
        for col in range(min(cols, sheet_cols)):
            if item_id > 255:
                return n
            box = (col * cell, row * cell, (col + 1) * cell, (row + 1) * cell)
            cell_img = img.crop(box)
            # Skip fully transparent leftover cells.
            extrema = cell_img.getextrema()
            if len(extrema) == 4 and extrema[3][1] < 16:
                continue
            save_item(item_id, cell_img, palette, max_off)
            n += 1
            item_id += 1
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--from-dir", type=Path, help="Folder of iXX.png / 3e.png exports")
    ap.add_argument("--sheet", type=Path, help="Sprite sheet (row-major cells)")
    ap.add_argument("--start", default="0x01", help="First item id on the sheet (hex)")
    ap.add_argument("--cols", type=int, default=8, help="Sheet columns (default 8)")
    ap.add_argument("--cell", type=int, default=96, help="Sheet cell size in px (default 96)")
    ap.add_argument("--max-off-palette", type=int, default=12,
                    help="Max off-palette pixels allowed per icon before reject (default 12)")
    args = ap.parse_args()

    if not args.from_dir and not args.sheet:
        ap.print_help()
        return 2

    palette = load_palette()
    n = 0
    if args.from_dir:
        n += ingest_dir(args.from_dir, palette, args.max_off_palette)
    if args.sheet:
        n += ingest_sheet(args.sheet, parse_id_token(args.start), args.cols, args.cell,
                          palette, args.max_off_palette)
    print(f"Wrote {n} icons under {OUT.relative_to(ROOT)}")
    print("Pack with: python tools/pack_agui_ui.py")
    return 0 if n else 1


if __name__ == "__main__":
    raise SystemExit(main())

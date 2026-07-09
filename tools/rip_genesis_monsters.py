#!/usr/bin/env python3
"""Rip Genesis MM2 picture sprites from the $11A catalog.

ASM anchors (confirmed):
  $11A(A5) = 0x33CF4 — catalog walk: id → CHR LZ + layout LZ + 16-colour CRAM
  Catalog base          0x33D4A
  Meta table            0x913B4 (via 0x74CE) — w2 / w3 / extras
  Anim row table        0x91738 (via 0x7512) — keyed by meta w3; each u16:
                        hi byte = layout row, lo byte = anim duration (ticks)
  Blit plane stride     $F2 bytes = 121 tile words (0x7788 / 0x7900)

Each catalog entry:
  u16 id
  CHR LZ @ entry+2
  10-byte pad
  layout LZ — 121 words × N frames (linear upload stream for VDP sprites)
  32-byte CRAM palette

IMPORTANT (ASM):
  - Layout is a *linear* 121-tile stream uploaded into sprite CHR.
  - Loader @0x75C2 hardcodes ONE 9-sprite SAT mosaic (@0x761A) for every
    $11A load (UI and combat via $240→0x7556). Sizes sum to 121.
  - Meta.w2 is never read for placement; 0x91738 (via w3) supplies anim row
    sequence (hi byte per word); combat still portrait uses layout row 0
    (@0x7756→0x7788).
  - Atlas = SAT mosaic on layout row 0 (combat path), not densest frame.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from genesis_lz_decompress import decompress_lz  # noqa: E402

CATALOG_BASE = 0x33D4A
META_BASE = 0x913B4
PLANE_W = 121  # $F2 / 2
CELL_W = 4  # VDP max sprite width in tiles
CELL_H = 4
CELL = CELL_W * CELL_H  # 16

# Hardcoded SAT mosaic from loader @0x761A — used for ALL $11A picture ids.
# (ΔX, ΔY, W, H) in tiles; tile counts sum to 121.
# Sizes from VDP size bytes ($0F/$0E/$0B/$0A): H=bits1-0+1, V=bits3-2+1.
# Earlier rip had W/H swapped on non-square slots (4×3 vs 3×4).
SAT_MOSAIC: list[tuple[int, int, int, int]] = [
    (0, 0, 4, 4),  # $0F01
    (4, 0, 4, 4),  # $0F02
    (8, 0, 3, 4),  # $0E03  was wrongly 4×3
    (0, 4, 4, 4),  # $0F04
    (4, 4, 4, 4),  # $0F05
    (8, 4, 3, 4),  # $0E06  was wrongly 4×3
    (0, 8, 4, 3),  # $0B07  was wrongly 3×4
    (4, 8, 4, 3),  # $0B08  was wrongly 3×4
    (8, 8, 3, 3),  # $0A00
]


def cram(w: int) -> tuple[int, int, int]:
    r = (w >> 1) & 7
    g = (w >> 5) & 7
    b = (w >> 9) & 7
    s = lambda v: v * 255 // 7
    return s(r), s(g), s(b)


def parse_11a(rom: bytes) -> dict[int, dict]:
    a = CATALOG_BASE
    ents: dict[int, dict] = {}
    while True:
        eid = struct.unpack_from(">H", rom, a)[0]
        if eid == 0:
            break
        sz1 = struct.unpack_from(">I", rom, a + 2)[0]
        sec2 = a + sz1 + 0xA
        sz2 = struct.unpack_from(">I", rom, sec2)[0]
        pal = sec2 + sz2 + 8
        nxt = sec2 + sz2 + 0x28
        ents[eid] = {"base": a, "sec2": sec2, "pal": pal, "next": nxt, "sz1": sz1}
        a = nxt
    return ents


def parse_meta(rom: bytes) -> dict[int, dict]:
    a = META_BASE
    ents: dict[int, dict] = {}
    while True:
        eid = struct.unpack_from(">H", rom, a)[0]
        if eid == 0xFFFF:
            break
        w2, w3, w4 = struct.unpack_from(">HHH", rom, a + 2)
        extra = [struct.unpack_from(">H", rom, a + 8 + i)[0] for i in range(0, w4 * 2, 2)]
        ents[eid] = {"w2": w2, "w3": w3, "w4": w4, "extra": extra, "off": a}
        a += w4 * 2 + 8
    return ents


def blit_cm(
    stream: list[int],
    tw: int,
    th: int,
    chr_data: bytes,
    pal: list[tuple[int, int, int]],
    ntiles: int,
) -> Image.Image:
    """Column-major VDP sprite tile order."""
    img = Image.new("RGBA", (tw * 8, th * 8), (0, 0, 0, 0))
    px = img.load()
    for i, w in enumerate(stream[: tw * th]):
        idx = w & 0x7FF
        if idx == 0:
            continue
        ti = idx - 1
        if ti < 0 or ti >= ntiles:
            continue
        flip_h = bool(w & 0x0800)
        flip_v = bool(w & 0x1000)
        tile = chr_data[ti * 32 : (ti + 1) * 32]
        x, y = i // th, i % th
        for yy in range(8):
            for xx in range(8):
                bi = yy * 4 + xx // 2
                n = (tile[bi] >> 4) if (xx & 1) == 0 else (tile[bi] & 0xF)
                if not n:
                    continue
                dx = 7 - xx if flip_h else xx
                dy = 7 - yy if flip_v else yy
                px[x * 8 + dx, y * 8 + dy] = (*pal[n], 255)
    return img


def load_pic(rom: bytes, eid: int, ents: dict, meta: dict) -> dict | None:
    if eid not in ents:
        return None
    e = ents[eid]
    m = meta.get(eid, {"w2": 0, "w3": 0, "w4": 0, "extra": []})
    chr_data = decompress_lz(rom[e["base"] + 2 :])
    layout = decompress_lz(rom[e["sec2"] :])
    pal = [cram(struct.unpack_from(">H", rom, e["pal"] + i)[0]) for i in range(0, 32, 2)]
    ntiles = len(chr_data) // 32
    words = [struct.unpack_from(">H", layout, i)[0] for i in range(0, len(layout) & ~1, 2)]
    if len(words) % PLANE_W:
        return None
    frames = len(words) // PLANE_W
    return {
        "eid": eid,
        "frames": frames,
        "nspr": PLANE_W // CELL,
        "chr": chr_data,
        "pal": pal,
        "ntiles": ntiles,
        "words": words,
        "base": e["base"],
        "w2": m["w2"],
        "w3": m["w3"],
        "extra": m["extra"],
    }


def compose_cells(pic: dict, frame: int) -> list[Image.Image]:
    """Carve occupied 4×4 CM cells from one linear plane row."""
    row = pic["words"][frame * PLANE_W : (frame + 1) * PLANE_W]
    slots: list[Image.Image] = []
    for s in range(pic["nspr"]):
        stream = row[s * CELL : (s + 1) * CELL]
        nz = sum(1 for w in stream if w & 0x7FF)
        if nz < 2:
            continue
        slots.append(blit_cm(stream, CELL_W, CELL_H, pic["chr"], pic["pal"], pic["ntiles"]))
    return slots


def compose_contact(slots: list[Image.Image], cols: int = 4) -> Image.Image:
    """Labeled-free contact sheet of cells (atlas building block)."""
    if not slots:
        return Image.new("RGBA", (CELL_W * 8, CELL_H * 8), (0, 0, 0, 0))
    cw, ch = CELL_W * 8, CELL_H * 8
    pad = 2
    cols = min(cols, len(slots))
    rows = (len(slots) + cols - 1) // cols
    out = Image.new("RGBA", (cols * (cw + pad) + pad, rows * (ch + pad) + pad), (0, 0, 0, 0))
    for i, sp in enumerate(slots):
        x = pad + (i % cols) * (cw + pad)
        y = pad + (i // cols) * (ch + pad)
        out.paste(sp, (x, y), sp)
    return out


def compose_sat(
    pic: dict, frame: int, sprites: list[tuple[int, int, int, int]]
) -> Image.Image:
    row = pic["words"][frame * PLANE_W : (frame + 1) * PLANE_W]
    max_x = max(x + w for x, y, w, h in sprites) * 8
    max_y = max(y + h for x, y, w, h in sprites) * 8
    canvas = Image.new("RGBA", (max_x, max_y), (0, 0, 0, 0))
    off = 0
    for x, y, w, h in sprites:
        cell = blit_cm(row[off : off + w * h], w, h, pic["chr"], pic["pal"], pic["ntiles"])
        canvas.paste(cell, (x * 8, y * 8), cell)
        off += w * h
    return canvas


PLACE_BASE = 0x91738


def parse_place_table(rom: bytes) -> dict[int, dict]:
    """Anim row table @0x91738 (via 0x7512). Each word: hi=layout row, lo=duration."""
    a = PLACE_BASE
    ents: dict[int, dict] = {}
    while True:
        key = struct.unpack_from(">H", rom, a)[0]
        if key == 0xFFFF:
            break
        n = struct.unpack_from(">H", rom, a + 2)[0]
        words = [struct.unpack_from(">H", rom, a + 4 + i * 2)[0] for i in range(n)]
        ents[key] = {
            "rows": [w >> 8 for w in words],
            "durations": [w & 0xFF for w in words],
            "words": words,
            "off": a,
        }
        a += 4 + n * 2
    return ents


def combat_portrait_row(_pic: dict, place: dict | None) -> int:
    """Loader @0x7756 pushes row 0 into blit @0x7788 for combat/UI still."""
    return 0


def anim_rows(w3: int, place: dict[int, dict]) -> list[int]:
    e = place.get(w3)
    return e["rows"] if e else []


def _copy_tile_vdp(src: bytes, flip_h: bool, flip_v: bool) -> bytes:
    """Match $120/$126/$12C/$132 staging writes (pixel-equivalent)."""
    if not flip_h and not flip_v:
        return bytes(src)
    out = bytearray(32)
    for yy in range(8):
        for xx in range(8):
            sy = 7 - yy if flip_v else yy
            sx = 7 - xx if flip_h else xx
            bi = sy * 4 + sx // 2
            n = (src[bi] >> 4) if (sx & 1) == 0 else (src[bi] & 0xF)
            bo = yy * 4 + xx // 2
            if xx & 1:
                out[bo] = (out[bo] & 0xF0) | n
            else:
                out[bo] = (out[bo] & 0x0F) | (n << 4)
    return bytes(out)


def simulate_blit7788(pic: dict, row: int) -> list[bytes]:
    """Replay blit @0x7788: 121 layout words → 121×32-byte staging (-$24BE)."""
    stream = pic["words"][row * PLANE_W : (row + 1) * PLANE_W]
    staging: list[bytes] = [bytes(32) for _ in range(PLANE_W)]
    for d6, w in enumerate(stream):
        idx = w & 0x7FF
        if idx == 0:
            continue
        ti = idx - 1
        if ti < 0 or ti >= pic["ntiles"]:
            continue
        src = pic["chr"][ti * 32 : (ti + 1) * 32]
        flip_h = bool(w & 0x0800)
        flip_v = bool(w & 0x1000)
        staging[d6] = _copy_tile_vdp(src, flip_h, flip_v)
    return staging


def _rgba_from_tile(tile: bytes, pal: list[tuple[int, int, int]]) -> Image.Image:
    im = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    px = im.load()
    for yy in range(8):
        for xx in range(8):
            bi = yy * 4 + xx // 2
            n = (tile[bi] >> 4) if (xx & 1) == 0 else (tile[bi] & 0xF)
            if n:
                px[xx, yy] = (*pal[n], 255)
    return im


def compose_staging(
    staging: list[bytes],
    pal: list[tuple[int, int, int]],
    sprites: list[tuple[int, int, int, int]],
    *,
    mask_empty: bool = False,
) -> Image.Image:
    """Compose SAT mosaic from post-blit staging (CM order within each sprite)."""
    max_x = max(x + w for x, y, w, h in sprites) * 8
    max_y = max(y + h for x, y, w, h in sprites) * 8
    canvas = Image.new("RGBA", (max_x, max_y), (0, 0, 0, 0))
    off = 0
    for x, y, w, h in sprites:
        tiles = staging[off : off + w * h]
        if mask_empty and not any(any(b != 0 for b in t) for t in tiles):
            off += w * h
            continue
        cell = Image.new("RGBA", (w * 8, h * 8), (0, 0, 0, 0))
        cpx = cell.load()
        for i, tile in enumerate(tiles):
            tx, ty = i // h, i % h
            tim = _rgba_from_tile(tile, pal)
            cell.paste(tim, (tx * 8, ty * 8), tim)
        canvas.paste(cell, (x * 8, y * 8), cell)
        off += w * h
    return canvas


def compose_simulated(
    pic: dict,
    row: int,
    sprites: list[tuple[int, int, int, int]] = SAT_MOSAIC,
    *,
    mask_empty: bool = False,
) -> Image.Image:
    staging = simulate_blit7788(pic, row)
    return compose_staging(staging, pic["pal"], sprites, mask_empty=mask_empty)


def staging_diff_count(a: list[bytes], b: list[bytes]) -> int:
    n = 0
    for ta, tb in zip(a, b):
        if ta != tb:
            n += 1
    return n


def densest_frame(pic: dict) -> int:
    best = 0
    best_nz = 0
    for fr in range(pic["frames"]):
        row = pic["words"][fr * PLANE_W : (fr + 1) * PLANE_W]
        nz = sum(1 for w in row if w & 0x7FF)
        if nz > best_nz:
            best_nz = nz
            best = fr
    return best


    best = 0
    best_nz = 0
    for fr in range(pic["frames"]):
        row = pic["words"][fr * PLANE_W : (fr + 1) * PLANE_W]
        nz = sum(1 for w in row if w & 0x7FF)
        if nz > best_nz:
            best_nz = nz
            best = fr
    return best


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "rom",
        nargs="?",
        default=str(
            ROOT
            / "EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen"
        ),
    )
    ap.add_argument(
        "-o",
        "--out",
        default=str(ROOT / "EXTRACTED/genesis/gfx/catalog/monsters"),
    )
    ap.add_argument("--ids", default="", help="Comma list; default = all catalog ids")
    args = ap.parse_args()

    rom = Path(args.rom).read_bytes()
    out = Path(args.out)
    atlas_dir = out / "atlases"
    out.mkdir(parents=True, exist_ok=True)
    atlas_dir.mkdir(parents=True, exist_ok=True)

    ents = parse_11a(rom)
    meta = parse_meta(rom)
    if args.ids.strip():
        ids = [int(x) for x in args.ids.split(",") if x.strip()]
    else:
        ids = sorted(ents.keys())

    catalog = []
    portraits: dict[int, Image.Image] = {}

    for eid in ids:
        pic = load_pic(rom, eid, ents, meta)
        if not pic:
            print(f"skip {eid}")
            continue
        best_fr = combat_portrait_row(pic, None)
        portrait = compose_simulated(pic, best_fr)
        portrait.save(out / f"mon_{eid:03d}.png")
        portrait.resize((portrait.size[0] * 3, portrait.size[1] * 3), Image.NEAREST).save(
            out / f"mon_{eid:03d}_x3.png"
        )

        # Cell contact sheet (debug companion)
        slots = compose_cells(pic, best_fr)
        sheet = compose_contact(slots)
        sheet.save(out / f"mon_{eid:03d}_cells.png")

        # Anim: SAT mosaic across frames
        frames = [compose_simulated(pic, f) for f in range(pic["frames"])]
        if frames:
            max_w = max(fr.size[0] for fr in frames)
            max_h = max(fr.size[1] for fr in frames)
            anim = Image.new("RGBA", (max_w * len(frames), max_h), (0, 0, 0, 0))
            for i, fr in enumerate(frames):
                anim.paste(fr, (i * max_w, 0), fr)
            anim.save(out / f"mon_{eid:03d}_anim.png")

        # Tile sheet
        cols = 16
        r = (pic["ntiles"] + cols - 1) // cols
        tsheet = Image.new("RGB", (cols * 8, max(1, r) * 8), (0, 0, 0))
        sp = tsheet.load()
        for t in range(pic["ntiles"]):
            tile = pic["chr"][t * 32 : (t + 1) * 32]
            for y in range(8):
                for x in range(8):
                    bi = y * 4 + x // 2
                    n = (tile[bi] >> 4) if (x & 1) == 0 else (tile[bi] & 0xF)
                    sp[(t % cols) * 8 + x, (t // cols) * 8 + y] = pic["pal"][n]
        tsheet.save(out / f"mon_{eid:03d}_tiles.png")

        portraits[eid] = portrait
        entry = {
            "id": eid,
            "meta_w2": pic["w2"],
            "meta_w3": pic["w3"],
            "frames": pic["frames"],
            "best_frame": best_fr,
            "tiles": pic["ntiles"],
            "portrait_px": list(portrait.size),
            "base": hex(pic["base"]),
            "note": "composed via hardcoded 9-sprite SAT @0x761A",
        }
        catalog.append(entry)
        print(
            f"id={eid:3d} frames={pic['frames']} best={best_fr} "
            f"portrait={portrait.size} tiles={pic['ntiles']}"
        )

    # Atlas — SAT-composed portraits
    if portraits:
        pad_w = 96 + 8
        pad_h = 96 + 18
        cols = 8
        rows_a = (len(catalog) + cols - 1) // cols
        atlas = Image.new("RGB", (cols * pad_w, rows_a * pad_h), (16, 16, 16))
        draw = ImageDraw.Draw(atlas)
        for i, c in enumerate(catalog):
            im = portraits[c["id"]]
            x = (i % cols) * pad_w + 4
            y = (i // cols) * pad_h + 14
            atlas.paste(im, (x, y), im)
            draw.text((x, y - 12), f"{c['id']}", fill=(255, 220, 0))
        atlas.save(atlas_dir / "atlas_monsters.png")
        atlas.resize((atlas.size[0] * 2, atlas.size[1] * 2), Image.NEAREST).save(
            atlas_dir / "atlas_monsters_x2.png"
        )
        print("atlas", atlas.size)

    analysis = ROOT / "EXTRACTED/genesis/analysis/monster_catalog.json"
    analysis.parent.mkdir(parents=True, exist_ok=True)
    analysis.write_text(json.dumps(catalog, indent=2))
    print("wrote", analysis, "n=", len(catalog))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

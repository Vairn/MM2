#!/usr/bin/env python3
"""Compose SNES MM2 title screen + first-person wall/sky scenes.

Title ($069DDD):
  CHR DMA $06:BA5C (32 KiB) -> VRAM $4000
  Palette $06:9FBC (64 colors via $00E648)
  BGMODE=$F1 (Mode 1 + 16x16 BG1/BG2 tiles), BG1SC=$7C, BG2SC=$78, BG12NBA=$44
  Tilemaps: 16x16 words from $06:EA5C (BG1) / $06:EC5C (BG2)
  uploaded by $069F50 (X=cols, Y=rows; row stride +$20 in VRAM)

Exploration ($00E9DC+ / $008Dxx palette):
  Sky CHR $0D:8000 -> VRAM $5920 (134 tiles = 11x11 sheet)
  Walls $0D:BFE0 / $0D:D160 -> VRAM $4010 (140 tiles = 20x7 sheet)
  Tilemaps via $00E4F1 (mostly identity layouts of those sheets):
    $10:8000 11x11  base=$0119 attr=$04
    $0D:E2E0 20x8   base=$0192 attr=$08   ($A2=0/1 near + trim)
    $0D:E424 20x7   base=$0192 attr=$08   (wall A identity / far)
    $0D:E540 20x7   base=$0192 attr=$08   ($A2=2/5)

  Palette sources (via $00E289, X=CGRAM index, Y=count):
    town/cavern masonry: $13:8034 -> X=$40 (greys + wood; not $13:8000 browns)
    castle masonry:      $13:8014 -> X=$40
    sky day:             $1B:8000 -> X=$20 when outdoor/$28!=0
    sky dusk:            $1D:806A -> X=$30 when outdoor/$28==0, A2=0/1
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from snes_decompress import bgr555_to_rgb, decode_4bpp_tile, lorom_to_file  # noqa: E402

try:
    from PIL import Image
except ImportError:
    Image = None  # type: ignore


def read_pal(rom: bytes, file_off: int, n: int = 16) -> list[tuple[int, int, int]]:
    return [bgr555_to_rgb(struct.unpack_from("<H", rom, file_off + i * 2)[0]) for i in range(n)]


def load_chr(rom: bytes, file_off: int, nbytes: int) -> list[list[list[int]]]:
    data = rom[file_off : file_off + nbytes]
    return [decode_4bpp_tile(data[i * 32 : i * 32 + 32]) for i in range(len(data) // 32)]


def snes_tile_word(w: int) -> tuple[int, int, bool, bool]:
    return w & 0x3FF, (w >> 10) & 7, bool(w & 0x4000), bool(w & 0x8000)


def render_16x16_tilemap(
    chr_tiles: list,
    pal: list[tuple[int, int, int]],
    map_words: list[int],
    cols: int,
    rows: int,
    scale: int = 1,
    transparent0: bool = True,
    bg: tuple[int, int, int, int] = (0, 0, 0, 0),
) -> Image.Image:
    """BGMODE bit4/5: each map entry is a 16x16 block (t, t+1, t+16, t+17)."""
    img = Image.new("RGBA", (cols * 16, rows * 16), bg)
    px = img.load()
    for r in range(rows):
        for c in range(cols):
            w = map_words[r * cols + c]
            if w == 0 and transparent0:
                continue
            t, sub, xf, yf = snes_tile_word(w)
            subpal = pal[sub * 16 : (sub + 1) * 16]
            if len(subpal) < 16:
                subpal = (list(subpal) + [(0, 0, 0)] * 16)[:16]
            block = [[0] * 16 for _ in range(16)]
            for sy in range(16):
                for sx in range(16):
                    ti = t + (sx // 8) + (sy // 8) * 16
                    if ti < len(chr_tiles):
                        block[sy][sx] = chr_tiles[ti][sy % 8][sx % 8]
            for sy in range(16):
                for sx in range(16):
                    ssx = 15 - sx if xf else sx
                    ssy = 15 - sy if yf else sy
                    col = block[ssy][ssx]
                    if transparent0 and col == 0:
                        continue
                    px[c * 16 + sx, r * 16 + sy] = (*subpal[col], 255)
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def render_chr_grid(
    tiles: list,
    pal16: list[tuple[int, int, int]],
    cols: int,
    rows: int,
    scale: int = 3,
) -> Image.Image:
    img = Image.new("RGB", (cols * 8, rows * 8), (0, 0, 0))
    px = img.load()
    for t in range(min(len(tiles), cols * rows)):
        ox, oy = (t % cols) * 8, (t // cols) * 8
        tile = tiles[t]
        for y in range(8):
            for x in range(8):
                px[ox + x, oy + y] = pal16[tile[y][x]]
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def e4f1_decode(
    rom: bytes, bank: int, addr: int, tile_base: int, attr_or: int
) -> tuple[int, int, list[int]]:
    fo = lorom_to_file(bank, addr)
    width, height = rom[fo + 1], rom[fo + 3]
    pos = fo + 4
    words: list[int] = []
    base_lo, base_hi = tile_base & 0xFF, (tile_base >> 8) & 0xFF
    for _ in range(height):
        for _ in range(width):
            t_hi, t_lo = rom[pos], rom[pos + 1]
            pos += 2
            lo = (t_lo + base_lo) & 0xFF
            carry = 1 if (t_lo + base_lo) > 0xFF else 0
            hi = (t_hi + carry) & 0xFF
            hi = (hi | attr_or) & 0xFF
            hi = (hi + base_hi) & 0xFF
            words.append(lo | (hi << 8))
    return width, height, words


def render_e4f1(
    tiles: list,
    rom: bytes,
    bank: int,
    addr: int,
    tile_base: int,
    attr_or: int,
    pal16: list[tuple[int, int, int]],
    scale: int = 3,
) -> Image.Image:
    w, h, words = e4f1_decode(rom, bank, addr, tile_base, attr_or)
    img = Image.new("RGB", (w * 8, h * 8), (0, 0, 0))
    px = img.load()
    for r in range(h):
        for c in range(w):
            word = words[r * w + c]
            tile_i, _sub, xf, yf = snes_tile_word(word)
            local = tile_i - tile_base
            if local < 0 or local >= len(tiles):
                continue
            tile = tiles[local]
            for y in range(8):
                for x in range(8):
                    sx = 7 - x if xf else x
                    sy = 7 - y if yf else y
                    px[c * 8 + x, r * 8 + y] = pal16[tile[sy][sx]]
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def compose_title(rom: bytes, out: Path, scale: int = 3) -> None:
    chr_tiles = load_chr(rom, lorom_to_file(0x06, 0xBA5C), 0x3000)
    # Full 64-color title palette (4 subpals); maps select sub via bits 10-12
    pal = []
    fo = lorom_to_file(0x06, 0x9FBC)
    for i in range(64):
        pal.append(bgr555_to_rgb(struct.unpack_from("<H", rom, fo + i * 2)[0]))

    cols = rows = 16
    bg1_off = lorom_to_file(0x06, 0xEA5C)
    bg2_off = lorom_to_file(0x06, 0xEC5C)
    bg1 = [struct.unpack_from("<H", rom, bg1_off + i * 2)[0] for i in range(256)]
    bg2 = [struct.unpack_from("<H", rom, bg2_off + i * 2)[0] for i in range(256)]

    layer2 = render_16x16_tilemap(chr_tiles, pal, bg2, cols, rows, scale=1)
    layer1 = render_16x16_tilemap(chr_tiles, pal, bg1, cols, rows, scale=1)
    img = Image.new("RGBA", (cols * 16, rows * 16), (0, 0, 40, 255))
    img.alpha_composite(layer2)
    img.alpha_composite(layer1)
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"title -> {out} ({img.width}x{img.height})")

    render_16x16_tilemap(
        chr_tiles, pal, bg1, cols, rows, scale=scale, bg=(0, 0, 40, 255)
    ).save(out.with_name("title_bg1_only.png"))
    render_16x16_tilemap(
        chr_tiles, pal, bg2, cols, rows, scale=scale, bg=(0, 0, 40, 255)
    ).save(out.with_name("title_bg2_only.png"))


def compose_walls(rom: bytes, outdir: Path, scale: int = 3) -> None:
    """Wall/sky sheets with ASM-locked pals from $008E7A / explore DMA."""
    outdir.mkdir(parents=True, exist_ok=True)
    # Indoor masonry lives in CGRAM $40 (attr $08 → BG subpal 2).
    pal_town = read_pal(rom, lorom_to_file(0x13, 0x8034), 16)  # A=0/1 day $40
    pal_castle = read_pal(rom, lorom_to_file(0x13, 0x8014), 16)  # A=2/5 day $40
    pal_sky_day = read_pal(rom, lorom_to_file(0x1B, 0x8000), 16)
    pal_sky_dusk = read_pal(rom, lorom_to_file(0x1D, 0x806A), 16)

    wall_a = load_chr(rom, lorom_to_file(0x0D, 0xBFE0), 4480)
    wall_b = load_chr(rom, lorom_to_file(0x0D, 0xD160), 4480)
    sky = load_chr(rom, lorom_to_file(0x0D, 0x8000), 4288)

    render_chr_grid(wall_a, pal_town, 20, 7, scale).save(outdir / "wallA_20x7.png")
    render_chr_grid(wall_b, pal_castle, 20, 7, scale).save(outdir / "wallB_20x7.png")
    render_chr_grid(sky, pal_sky_day, 11, 11, scale).save(outdir / "sky_11x11_day.png")
    render_chr_grid(sky, pal_sky_dusk, 11, 11, scale).save(outdir / "sky_11x11_dusk.png")

    render_e4f1(wall_a, rom, 0x0D, 0xE424, 0x0192, 0x08, pal_town, scale).save(
        outdir / "wallA_map_E424.png"
    )
    render_e4f1(wall_a, rom, 0x0D, 0xE2E0, 0x0192, 0x08, pal_town, scale).save(
        outdir / "wallA_map_E2E0.png"
    )
    render_e4f1(wall_b, rom, 0x0D, 0xE540, 0x0192, 0x08, pal_castle, scale).save(
        outdir / "wallB_map_E540.png"
    )
    render_e4f1(sky, rom, 0x10, 0x8000, 0x0119, 0x04, pal_sky_day, scale).save(
        outdir / "sky_map_108000_day.png"
    )
    print(f"walls/sky sheets -> {outdir}")


def compose_explore(rom: bytes, out: Path, scene: int = 0, scale: int = 3) -> None:
    pal_sky = read_pal(rom, lorom_to_file(0x1B, 0x8000), 16)
    sky = load_chr(rom, lorom_to_file(0x0D, 0x8000), 4288)

    if scene in (0, 1):
        wall = load_chr(rom, lorom_to_file(0x0D, 0xBFE0), 4480)
        wall_addr = 0xE2E0
        wall_base_tiles = 0x0192
        pal_wall = read_pal(rom, lorom_to_file(0x13, 0x8034), 16)
    else:
        wall = load_chr(rom, lorom_to_file(0x0D, 0xD160), 4480)
        wall_addr = 0xE540
        wall_base_tiles = 0x0192
        pal_wall = read_pal(rom, lorom_to_file(0x13, 0x8014), 16)

    sky_img = render_e4f1(sky, rom, 0x10, 0x8000, 0x0119, 0x04, pal_sky, scale=1)
    wall_img = render_e4f1(
        wall, rom, 0x0D, wall_addr, wall_base_tiles, 0x08, pal_wall, scale=1
    )

    W = max(sky_img.width, wall_img.width)
    H = sky_img.height + wall_img.height
    img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    img.paste(sky_img.convert("RGBA"), ((W - sky_img.width) // 2, 0))
    img.paste(wall_img.convert("RGBA"), ((W - wall_img.width) // 2, sky_img.height))
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"explore A2={scene} -> {out} ({img.width}x{img.height})")


def decode_8bpp_tile(data: bytes) -> list[list[int]]:
    """SNES 8bpp = two stacked 4bpp tiles (planes 0-3 + 4-7)."""
    lo = decode_4bpp_tile(data[0:32])
    hi = decode_4bpp_tile(data[32:64])
    return [[lo[y][x] | (hi[y][x] << 4) for x in range(8)] for y in range(8)]


def compose_boot_title(rom: bytes, out: Path, scale: int = 2) -> None:
    """Illustrated boot/title art from $00FE80.

    CHR (8bpp): $1C:8000 (32 KiB) + $08:CB00 (0x34FE) + $1F:A5D7 (0x2B02)
      = 896 tiles → VRAM $0000 contiguous
    Palette: $10:FD76 (255 words via $00E289 X=0 Y=$FF)
    Tilemap: $07:F626 32x28 identity via $00E4F1 (base=0 attr=0)
    """
    pal_off = lorom_to_file(0x10, 0xFD76)
    pal = [bgr555_to_rgb(struct.unpack_from("<H", rom, pal_off + i * 2)[0]) for i in range(256)]

    blob = bytearray()
    blob += rom[lorom_to_file(0x1C, 0x8000) : lorom_to_file(0x1C, 0x8000) + 0x8000]
    blob += rom[lorom_to_file(0x08, 0xCB00) : lorom_to_file(0x08, 0xCB00) + 0x34FE]
    blob += rom[lorom_to_file(0x1F, 0xA5D7) : lorom_to_file(0x1F, 0xA5D7) + 0x2B02]
    while len(blob) % 64:
        blob.append(0)
    tiles = [decode_8bpp_tile(bytes(blob[i * 64 : i * 64 + 64])) for i in range(len(blob) // 64)]

    cols, rows = 32, 28
    img = Image.new("RGB", (cols * 8, rows * 8), (0, 0, 0))
    px = img.load()
    for t in range(min(len(tiles), cols * rows)):
        ox, oy = (t % cols) * 8, (t // cols) * 8
        tile = tiles[t]
        for y in range(8):
            for x in range(8):
                px[ox + x, oy + y] = pal[tile[y][x]]
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"boot title -> {out} ({img.width}x{img.height})")


def _face_row_tables(rom: bytes) -> tuple[list[int], list[int]]:
    d403 = [
        struct.unpack_from("<H", rom, lorom_to_file(0, 0xD403) + i * 2)[0] for i in range(32)
    ]
    d439 = [
        struct.unpack_from("<H", rom, lorom_to_file(0, 0xD439) + i * 2)[0] for i in range(32)
    ]
    return d403, d439


def blit_face_table(
    rom: bytes,
    table_bank: int,
    table_addr: int,
    n_panels: int,
    d403: list[int],
    d439: list[int],
    sheet_w: int = 20,
    sheet_h: int = 16,
) -> bytearray:
    """Blit [x,y,w,h]+tiles panels from a 3-byte ptr table into a WRAM-style sheet."""
    sheet = bytearray(sheet_w * sheet_h * 32)
    fo = lorom_to_file(table_bank, table_addr)
    for i in range(n_panels):
        b0, b1, b2 = rom[fo + i * 3], rom[fo + i * 3 + 1], rom[fo + i * 3 + 2]
        addr, bank = b0 | (b1 << 8), b2
        if bank < 0x0E or bank > 0x1F:
            continue
        pfo = lorom_to_file(bank, addr)
        x, y, w, h = rom[pfo], rom[pfo + 1], rom[pfo + 2], rom[pfo + 3]
        if not (1 <= w <= sheet_w and 1 <= h <= sheet_h and x + w <= sheet_w and y + h <= sheet_h):
            continue
        src = pfo + 4
        for row in range(h):
            dest = d403[y + row] + d439[x]
            for _col in range(w):
                sheet[dest : dest + 32] = rom[src : src + 32]
                src += 32
                dest += 32
    return sheet


def sheet_to_png(
    sheet: bytearray,
    pal: list[tuple[int, int, int]],
    path: Path,
    *,
    sheet_w: int = 20,
    sheet_h: int = 16,
    scale: int = 3,
    bg: tuple[int, int, int] = (0, 0, 0),
    pal_hi: list[tuple[int, int, int]] | None = None,
    hi_idxs: set[int] | None = None,
) -> None:
    """Render sheet. If pal_hi/hi_idxs set, those color indices use CGRAM $40 slot."""
    img = Image.new("RGB", (sheet_w * 8, sheet_h * 8), bg)
    px = img.load()
    use_dual = pal_hi is not None and hi_idxs is not None
    for ty in range(sheet_h):
        for tx in range(sheet_w):
            off = ty * 640 + tx * 32
            tile = decode_4bpp_tile(bytes(sheet[off : off + 32]))
            for y in range(8):
                for x in range(8):
                    i = tile[y][x]
                    if use_dual and i in hi_idxs:
                        px[tx * 8 + x, ty * 8 + y] = pal_hi[i]
                    else:
                        px[tx * 8 + x, ty * 8 + y] = pal[i]
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def compose_wall_faces(rom: bytes, outdir: Path, scale: int = 3) -> None:
    """Compose facing-dependent wall/door panels via $00EDF3 / $00EF9D.

    $008E7A loads TWO day subpals for A=0/1 and A=2/5:
      town/cavern: $13:8000 → CGRAM $30 (warm dirt), $13:8034 → $40 (masonry+wood)
      castle:      $15:8138 → $30 (warm wood/torch), $13:8014 → $40 (grey masonry)
    Wall tilemaps use attr $08 → BG subpal 2 → CGRAM $40, so indoor faces render
    from the $40 tables. Night $40: town/cavern $1D:808A, castle $15:8000.
    """
    outdir.mkdir(parents=True, exist_ok=True)
    d403, d439 = _face_row_tables(rom)
    pal_wall = read_pal(rom, lorom_to_file(0x13, 0x8034), 16)  # town/cavern $40
    pal_night = read_pal(rom, lorom_to_file(0x1D, 0x808A), 16)  # town/cavern night $40
    pal_castle = read_pal(rom, lorom_to_file(0x13, 0x8014), 16)  # castle $40
    pal_castle_night = read_pal(rom, lorom_to_file(0x15, 0x8000), 16)  # castle night $40
    bg = (0x58, 0x70, 0x58)

    # $40 tables already embed door wood / torch accents — single-pal faces.
    for name, addr, pal in [
        ("town_cavern_wall", 0xFD6C, pal_wall),
        ("town_cavern_wall_night", 0xFD6C, pal_night),
        ("town_cavern_door", 0xFDA8, pal_wall),
        ("town_cavern_door_night", 0xFDA8, pal_night),
        ("castle_wall", 0xFCB8, pal_castle),
        ("castle_wall_night", 0xFCB8, pal_castle_night),
        ("castle_door", 0xFCF4, pal_castle),
        ("wall_faces_A2_0", 0xFD6C, pal_wall),
        ("wall_faces_A2_2", 0xFCB8, pal_castle),
        ("wall_faces_A2_other", 0xFE20, read_pal(rom, lorom_to_file(0x13, 0x8034), 16)),
    ]:
        sheet = blit_face_table(rom, 0x11, addr, 20, d403, d439)
        sheet_to_png(sheet, pal, outdir / f"{name}.png", scale=scale, bg=bg)
        print(f"wall faces {name} -> {outdir / name}.png")


def compose_outdoor_faces(rom: bytes, outdir: Path, scale: int = 3) -> None:
    """Compose outdoor terrain faces via $00EE8D (when $C4==1).

    15-panel tables, stride $2D from base $11:FE20.
    Outdoor day ($008F7A): $13:8054 → CGRAM $40 (trees/foliage/ground moss).
    Mountains (terrain 1/2) are grey rock — $13:8034 (same $40 greys as town
    masonry). Trees/floors use outdoor $13:8054.
    """
    outdir.mkdir(parents=True, exist_ok=True)
    d403, d439 = _face_row_tables(rom)
    pal_mtn = read_pal(rom, lorom_to_file(0x13, 0x8034), 16)  # grey rock
    pal_outdoor = read_pal(rom, lorom_to_file(0x13, 0x8054), 16)  # day $40
    bg = (0x58, 0x70, 0x58)

    sets = [
        ("outdoor_mountains", 0xFE20, 15, pal_mtn),
        ("outdoor_trees_A", 0xFE7A, 15, pal_outdoor),
        ("outdoor_trees_B", 0xFEA7, 15, pal_outdoor),
    ]
    for name, addr, n, pal in sets:
        sheet = blit_face_table(rom, 0x11, addr, n, d403, d439)
        path = outdir / f"{name}.png"
        sheet_to_png(sheet, pal, path, scale=scale, bg=bg)
        print(f"outdoor faces {name} -> {path}")

    for terr in range(5, 13):
        addr = 0xFED4 + (terr - 5) * 0x24
        sheet = blit_face_table(rom, 0x11, addr, 12, d403, d439)
        path = outdir / f"outdoor_floor_terr{terr}.png"
        sheet_to_png(sheet, pal_outdoor, path, scale=scale, bg=bg)
        print(f"outdoor floor terr{terr} -> {path}")


def _floor_pal_no_torch(
    rom: bytes, bank: int, addr: int, torch_idxs: set[int]
) -> list[tuple[int, int, int]]:
    """Wall CHR shares torch accent indices; floor sheets remap them to greys."""
    pal = list(read_pal(rom, lorom_to_file(bank, addr), 16))
    # Map torch slots onto the grey ramp (indices 1..12 are masonry greys).
    for i in torch_idxs:
        if 0 <= i < 16:
            pal[i] = pal[max(1, min(12, i - 9))]
    return pal


def compose_indoor_floors(rom: bytes, outdir: Path, scale: int = 3) -> None:
    """Indoor floor = wall CHR 20x7 sheet ($0D:BFE0 / $0D:D160), not separate art.

    Town/cavern day $13:8034 / night $1D:808A; castle day $13:8014 / night $15:8000.
    Castle $13:8014 indices 13–15 are wall-torch reds/yellow — remapped to greys
    for floor-only sheets (floor tiles still use those indices as stone).
    """
    outdir.mkdir(parents=True, exist_ok=True)
    wall_a = load_chr(rom, lorom_to_file(0x0D, 0xBFE0), 140 * 32)
    wall_b = load_chr(rom, lorom_to_file(0x0D, 0xD160), 140 * 32)
    pals = [
        ("floor_town_cavern_day", wall_a, read_pal(rom, lorom_to_file(0x13, 0x8034), 16)),
        ("floor_town_cavern_night", wall_a, read_pal(rom, lorom_to_file(0x1D, 0x808A), 16)),
        (
            "floor_castle_day",
            wall_b,
            _floor_pal_no_torch(rom, 0x13, 0x8014, {13, 14, 15}),
        ),
        ("floor_castle_night", wall_b, read_pal(rom, lorom_to_file(0x15, 0x8000), 16)),
    ]
    for name, tiles, pal in pals:
        path = outdir / f"{name}.png"
        render_chr_grid(tiles, pal, 20, 7, scale).save(path)
        print(f"indoor floor {name} -> {path}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("--outdir", type=Path, default=Path("EXTRACTED/snes/gfx/scenes"))
    ap.add_argument("--scale", type=int, default=3)
    args = ap.parse_args()
    if Image is None:
        raise SystemExit("pip install pillow")

    rom = args.rom.read_bytes()
    args.outdir.mkdir(parents=True, exist_ok=True)

    compose_title(rom, args.outdir / "title_screen.png", scale=args.scale)
    compose_boot_title(rom, args.outdir / "boot_title_screen.png", scale=2)
    compose_walls(rom, args.outdir / "walls", scale=args.scale)
    compose_wall_faces(rom, args.outdir / "walls", scale=args.scale)
    compose_outdoor_faces(rom, args.outdir / "walls", scale=args.scale)
    compose_indoor_floors(rom, args.outdir / "walls", scale=args.scale)
    for a2 in (0, 2):
        compose_explore(rom, args.outdir / f"explore_A2_{a2}.png", scene=a2, scale=args.scale)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

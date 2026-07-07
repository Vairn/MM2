#!/usr/bin/env python3
"""Decode Might and Magic II PC CGA (.4) / EGA (.16) graphics.

Formats (GOG MM2.EXE + on-disk round-trip):

Wall / sprite sheets (THROW, TOWNF, SKY, TOWN, CAVE, …)
  u32 LE uncompressed_size
  LZW bitstream from file offset 4 (MM2.EXE 0x2A42)
  Decompressed payload (MM2.EXE 0x5C5A / 0x7018 / CGA.DRV 0x34C):
    u8 frame_count (dec[0] & 0x3F; top 2 bits = format class)
    u8 flags @ 1 (0x42EA: low nibble, bits 4–5)
    u16 or u32 LE[frame_count] frame offsets @ 2 (auto-scored; THROW/TOWN=u32, DESERT=u16)
    Per frame @ offset:
      u16 LE width, u16 LE height
      CGA (.4): (width+3)//4 bytes/row, 2bpp MSB-first (4 pixels/byte),
                CGA palette 1 black/cyan/magenta/white (CGA.DRV drv_init BX=0x0101)
      EGA (.16): (width+1)//2 bytes/row, 4bpp linear, hi nibble = left pixel

Monster combat atlas (MONSTERS.4 / MONSTERS.16)
  Header: u32 LE[75] blob file offsets (u32[0]=300 = table size); picture id N -> entry N-1
  Per blob: u32 LE size + LZW; frame 0 base + delta patches (prelude clear before blit)
  Missing PC slot: caller advances to next non-empty picture (see export_monster_variants.py)
  Docs: EXTRACTED/docs/54-pc-dos-graphics-formats.md

Usage:
  python tools/decode_pc_gfx.py THROW.4 --info
  python tools/decode_pc_gfx.py THROW.4 --frame 0 -o throw0.png
  python tools/decode_pc_gfx.py THROW.4 --extract-all
  python tools/decode_pc_gfx.py MONSTERS.4 --monsters --extract-all
  python tools/decode_pc_gfx.py "C:/.../Might and Magic 2" --batch
  python tools/pc_gfx_export.py --gog "C:/.../Might and Magic 2"
"""
from __future__ import annotations

import argparse
import shutil
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PC_GFX_OUT = ROOT / "EXTRACTED" / "pc_gfx"

if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))
# Canonical impl lives in mm2_lzw.py; re-exported here since several tools do
# `from decode_pc_gfx import lzw_decompress`.
from mm2_lzw import lzw_decompress  # noqa: E402

# IBM CGA 320x200x4 BIOS palettes. MM2's CGA.DRV drv_init (@0x2A8) issues
# INT 10h AH=0Bh with BX=0x0101 -> selects *palette 1* (black/cyan/magenta/white).
# High-intensity variant so index 3 is pure white (e.g. TOWNF checkerboard floor
# is black/white squares using only pixel values 0 and 3).
# Index order matches CGA.DRV plot @ 0x34C (2bpp, MSB-first, 4 pixels/byte).
_CGA_PALETTE_0 = [(0, 0, 0), (0, 170, 0), (170, 0, 0), (170, 85, 0)]
_CGA_PALETTE_1 = [(0, 0, 0), (85, 255, 255), (255, 85, 255), (255, 255, 255)]
_CGA_RGB = _CGA_PALETTE_1
_EGA_RGB = [
    (0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
    (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
    (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
    (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255),
]


def row_bytes(width: int, bpp: int) -> int:
    if bpp == 2:
        return (width + 3) // 4
    # .16 stores 16-colour pixels as 4bpp linear (2 pixels/byte), not EGA planes.
    return (width + 1) // 2


def bpp_for_ext(ext: str) -> int:
    return 2 if ext.lower() == ".4" else 4


@dataclass
class WallFrame:
    index: int  # logical frame index (0..N-1)
    offset: int
    width: int
    height: int
    header_size: int
    pixels: bytes
    table_slot: int = -1  # offset-table slot (grouped u16 uses even slots only)


def _computed_frame_end(dec: bytes, off: int, bpp: int, end_limit: int) -> int:
    """Byte offset immediately after frame pixels @ off (u16 LE w/h + packed rows)."""
    if off + 4 > end_limit or off + 4 > len(dec):
        return 0
    width, height = struct.unpack_from("<HH", dec, off)
    if not _valid_frame_dims(width, height, bpp):
        return 0
    rb = row_bytes(width, bpp)
    pix_end = off + 4 + rb * height
    if pix_end > end_limit or pix_end > len(dec):
        return 0
    return pix_end


def _frame_end(offsets: list[int], index: int, dec_len: int) -> int:
    start = offsets[index]
    for off in offsets[index + 1 :]:
        if off > start:
            return off
    return dec_len


def _valid_frame_dims(width: int, height: int, bpp: int) -> bool:
    if width < 4 or height < 1:
        return False
    if bpp == 2:
        return width <= 320 and height <= 200
    return width <= 640 and height <= 200


def _frame_header_size(
    dec: bytes, off: int, width: int, height: int, bpp: int, end: int
) -> int:
    """Wall frames: u16 LE width, u16 LE height, then raw pixels (4-byte header)."""
    rb = row_bytes(width, bpp)
    pix_len = rb * height
    if off + 4 + pix_len <= end:
        return 4
    return 0


def _offset_table_u32(dec: bytes, frame_count: int) -> list[int]:
    if 2 + frame_count * 4 > len(dec):
        return []
    return [struct.unpack_from("<I", dec, 2 + i * 4)[0] for i in range(frame_count)]


def _extract_packed_u32_frames(dec: bytes, frame_count: int, bpp: int) -> list[WallFrame]:
    """Extract frames from a u32 table where each entry is (end<<16)|start.

    Used by MASTER.16 (title screen + UI sprites) and several grouped outdoor
    sheets (DESERT, CASTLE, TOWN, …). MM2.EXE indexes with index*4 @ 0x7018.
    """
    frames: list[WallFrame] = []
    for i in range(frame_count):
        pos = 2 + i * 4
        if pos + 4 > len(dec):
            break
        u32 = struct.unpack_from("<I", dec, pos)[0]
        start = u32 & 0xFFFF
        end = u32 >> 16
        if start < 2 or start >= len(dec):
            continue
        if end <= start:
            end = len(dec)
        if start + 4 > end:
            continue
        width, height = struct.unpack_from("<HH", dec, start)
        if not _valid_frame_dims(width, height, bpp):
            continue
        hdr = _frame_header_size(dec, start, width, height, bpp, end)
        if hdr == 0:
            continue
        rb = row_bytes(width, bpp)
        pix_off = start + hdr
        pix_len = rb * height
        frames.append(
            WallFrame(
                index=len(frames),
                offset=start,
                width=width,
                height=height,
                header_size=hdr,
                pixels=dec[pix_off : pix_off + pix_len],
                table_slot=i,
            )
        )
    return frames


def _packed_u32_end_matches(dec: bytes, frame_count: int, bpp: int) -> tuple[int, int]:
    """Return (end_matches, valid_entries) for packed-u32 scoring."""
    matches = 0
    valid = 0
    for i in range(frame_count):
        pos = 2 + i * 4
        if pos + 4 > len(dec):
            break
        u32 = struct.unpack_from("<I", dec, pos)[0]
        start = u32 & 0xFFFF
        end = u32 >> 16
        if start < 2 or start + 4 > len(dec):
            continue
        width, height = struct.unpack_from("<HH", dec, start)
        if not _valid_frame_dims(width, height, bpp):
            continue
        rb = row_bytes(width, bpp)
        expected = start + 4 + rb * height
        if expected > len(dec):
            continue
        valid += 1
        if end == expected or (end == 0 and expected <= len(dec)):
            matches += 1
    return matches, valid


def _offset_table_u16(dec: bytes, frame_count: int) -> list[int]:
    if 2 + frame_count * 2 > len(dec):
        return []
    return [struct.unpack_from("<H", dec, 2 + i * 2)[0] for i in range(frame_count)]


def _pick_offset_table(dec: bytes, frame_count: int, bpp: int) -> tuple[str, list[int]]:
    """Pick packed u32 vs plain u32 vs u16 table layout."""
    if frame_count == 0:
        return "u16", []
    u16 = _offset_table_u16(dec, frame_count)
    u32 = _offset_table_u32(dec, frame_count)
    packed_frames = _extract_packed_u32_frames(dec, frame_count, bpp)
    plain_frames = _extract_wall_frames(dec, u32, bpp) if u32 else []
    u16_frames = _extract_wall_frames(dec, u16, bpp) if u16 else []

    scored: list[tuple[int, int, str, list[int]]] = []
    if u32:
        scored.append((len(plain_frames), 1 if len(plain_frames) == frame_count else 0, "u32", u32))
    if packed_frames:
        end_m, end_v = _packed_u32_end_matches(dec, frame_count, bpp)
        end_bonus = 1 if end_v > 0 and end_m >= max(2, end_v * 2 // 3) else 0
        scored.append((len(packed_frames), end_bonus, "packed_u32", u32))
    if u16:
        scored.append((len(u16_frames), 0, "u16", u16))

    if not scored:
        return "u16", u16
    scored.sort(key=lambda item: (item[0], item[1], 1 if item[2] == "u32" else 0), reverse=True)
    _, _, kind, table = scored[0]
    return kind, table


def _is_grouped_u16_table(dec: bytes, offsets: list[int], bpp: int) -> bool:
    """True when odd slots store the end offset of the preceding even-slot frame.

    MM2.EXE pick_frame_from_sheet @ 0x5D4A indexes the u16 table at logical_index*2
    (see shl ax,1 @ 0x5E60). On-disk grouped sheets (MASTER, CASTLE.4, …) interleave
    [frame_start, frame_end] pairs; only the even slots are frame headers.
    """
    if len(offsets) < 4:
        return False
    pairs = 0
    matches = 0
    for i in range(0, len(offsets) - 1, 2):
        start = offsets[i]
        if start < 2 or start + 4 > len(dec):
            continue
        width, height = struct.unpack_from("<HH", dec, start)
        if not _valid_frame_dims(width, height, bpp):
            continue
        end_limit = _frame_end(offsets, i, len(dec))
        expected = _computed_frame_end(dec, start, bpp, end_limit)
        if expected <= start:
            continue
        pairs += 1
        if offsets[i + 1] == expected:
            matches += 1
    return pairs >= 2 and matches >= max(2, pairs * 2 // 3)


def _extract_grouped_u16_frames(dec: bytes, offsets: list[int], bpp: int) -> list[WallFrame]:
    """Extract logical frames from an interleaved u16 start/end offset table."""
    frames: list[WallFrame] = []
    logical = 0
    for i in range(0, len(offsets), 2):
        off = offsets[i]
        if off < 2 or off >= len(dec):
            continue
        end = offsets[i + 1] if i + 1 < len(offsets) else len(dec)
        if end <= off:
            end = len(dec)
        if off + 4 > end:
            continue
        width, height = struct.unpack_from("<HH", dec, off)
        if not _valid_frame_dims(width, height, bpp):
            continue
        hdr = _frame_header_size(dec, off, width, height, bpp, end)
        if hdr == 0:
            continue
        rb = row_bytes(width, bpp)
        pix_off = off + hdr
        pix_len = rb * height
        frames.append(
            WallFrame(
                index=logical,
                offset=off,
                width=width,
                height=height,
                header_size=hdr,
                pixels=dec[pix_off : pix_off + pix_len],
                table_slot=i,
            )
        )
        logical += 1
    return frames


def _extract_wall_frames(dec: bytes, offsets: list[int], bpp: int) -> list[WallFrame]:
    if _is_grouped_u16_table(dec, offsets, bpp):
        return _extract_grouped_u16_frames(dec, offsets, bpp)
    frames: list[WallFrame] = []
    logical = 0
    for i, off in enumerate(offsets):
        if off < 2 or off >= len(dec):
            continue
        end = _frame_end(offsets, i, len(dec))
        if off + 4 > end:
            continue
        width, height = struct.unpack_from("<HH", dec, off)
        if not _valid_frame_dims(width, height, bpp):
            continue
        hdr = _frame_header_size(dec, off, width, height, bpp, end)
        if hdr == 0:
            continue
        rb = row_bytes(width, bpp)
        pix_off = off + hdr
        pix_len = rb * height
        frames.append(
            WallFrame(
                index=logical,
                offset=off,
                width=width,
                height=height,
                header_size=hdr,
                pixels=dec[pix_off : pix_off + pix_len],
                table_slot=i,
            )
        )
        logical += 1
    return frames


def parse_wall_sheet(path: Path) -> dict:
    raw = path.read_bytes()
    if len(raw) < 8:
        raise ValueError(f"{path}: too small for wall sheet header")
    dec_size = struct.unpack_from("<I", raw, 0)[0]
    if dec_size < 4:
        raise ValueError(f"{path}: invalid uncompressed_size {dec_size}")
    bpp = bpp_for_ext(path.suffix)
    dec = lzw_decompress(raw[4:], dec_size)
    if len(dec) < dec_size:
        raise ValueError(f"{path}: LZW got {len(dec)} bytes, expected {dec_size}")
    table_slot_count = dec[0] & 0x3F
    if table_slot_count == 0:
        raise ValueError(f"{path}: invalid frame_count {table_slot_count}")
    offset_kind, offsets = _pick_offset_table(dec, table_slot_count, bpp)
    grouped_u16 = offset_kind == "u16" and _is_grouped_u16_table(dec, offsets, bpp)
    if offset_kind == "packed_u32":
        frames = _extract_packed_u32_frames(dec, table_slot_count, bpp)
    else:
        frames = _extract_wall_frames(dec, offsets, bpp)
    if not frames:
        raise ValueError(f"{path}: no decodable frames ({offset_kind} table)")
    return {
        "kind": "wall",
        "path": str(path),
        "ext": path.suffix.lower(),
        "bpp": bpp,
        "uncompressed_size": dec_size,
        "frame_count": len(frames),
        "table_slot_count": table_slot_count,
        "grouped_u16": grouped_u16,
        "offset_kind": offset_kind,
        "frames": frames,
        "compressed_bytes": len(raw) - 4,
        "decompressed": dec,
    }


def decode_wall_frame_rgb(
    width: int, height: int, pixels: bytes, bpp: int, cga_palette: int = 0
) -> list[tuple[int, int, int]]:
    """Decode one wall/sprite frame to RGB (CGA.DRV 2bpp MSB-first @ 0x34C)."""
    rb = row_bytes(width, bpp)
    if bpp == 2:
        pal = _CGA_PALETTE_0 if cga_palette == 0 else _CGA_PALETTE_1
    else:
        pal = _EGA_RGB
    out: list[tuple[int, int, int]] = []
    for y in range(height):
        row = pixels[y * rb : (y + 1) * rb]
        x = 0
        if bpp == 2:
            for b in row:
                for shift in (6, 4, 2, 0):
                    if x >= width:
                        break
                    idx = (b >> shift) & 3
                    out.append(pal[idx])
                    x += 1
        else:
            x = 0
            for b in row:
                for v in ((b >> 4) & 0xF, b & 0xF):
                    if x >= width:
                        break
                    out.append(pal[v])
                    x += 1
    return out


def decode_wall_frame_indices(width: int, height: int, pixels: bytes, bpp: int) -> list[int]:
    """Flat palette indices for one wall frame (row-major)."""
    rb = row_bytes(width, bpp)
    out: list[int] = []
    for y in range(height):
        row = pixels[y * rb : (y + 1) * rb]
        x = 0
        if bpp == 2:
            for b in row:
                for shift in (6, 4, 2, 0):
                    if x >= width:
                        break
                    out.append((b >> shift) & 3)
                    x += 1
        else:
            for b in row:
                for v in ((b >> 4) & 0xF, b & 0xF):
                    if x >= width:
                        break
                    out.append(v)
                    x += 1
    return out


def resample_amiga_mask(
    mask: list[list[bool]], width: int, height: int
) -> list[list[bool]]:
    """Scale an Amiga pen-0 mask to PC frame dimensions (nearest-neighbour).

    PC wall blobs are often 1–3 px taller than the matching ``.32`` frame; using
    the Amiga mask without scaling leaves the extra scanlines fully opaque.
    """
    src_h = len(mask)
    if src_h == 0:
        return [[True] * width for _ in range(height)]
    src_w = len(mask[0])
    if src_w == width and src_h == height:
        return mask
    out: list[list[bool]] = []
    for y in range(height):
        sy = min(src_h - 1, (y * src_h) // height) if height else 0
        src_row = mask[sy]
        row: list[bool] = []
        for x in range(width):
            sx = min(src_w - 1, (x * src_w) // width) if width else 0
            row.append(src_row[sx])
        out.append(row)
    return out


def resample_amiga_rgba(
    img, width: int, height: int
) -> list[tuple[int, int, int, int]]:
    """Nearest-neighbour resample of a decoded ``.32`` RGBA frame to PC dimensions."""
    src_w, src_h = img.size
    px = img.load()
    out: list[tuple[int, int, int, int]] = []
    for y in range(height):
        sy = min(src_h - 1, (y * src_h) // height) if height else 0
        for x in range(width):
            sx = min(src_w - 1, (x * src_w) // width) if width else 0
            out.append(px[sx, sy])
    return out


def wall_transparent_indices(bpp: int, frame: int | None = None) -> tuple[int, ...]:
    """Colour-key indices for PC wall blits when no Amiga mask is available.

    CGA.DRV / EGA.DRV compare each pixel against resource byte +7 (runtime ``[0x342]``)
    before ``drv_blit``. Side-wall cones (frames 4..11) were converted with EGA pen 8 /
    CGA pen 1 in void areas while pen 0 remains real black detail elsewhere — so a
    global ``index == 0`` mask is wrong for those frames.
    """
    if frame is not None and 4 <= frame <= 11:
        return (1,) if bpp == 2 else (8,)
    return (0,)


def render_overlay_frame_rgba(
    width: int,
    height: int,
    pixels: bytes,
    bpp: int,
    *,
    cga_palette: int = 1,
    cga_silhouette: bytes | None = None,
) -> list[tuple[int, int, int, int]]:
    """Decode townt/cavet/castlet overlay frames for the walker.

    These Amiga sheets have no pen-0 void (full 32-colour art), so the Amiga
    mask path cannot key them. CGA uses pens 0/1 as void; EGA reuses the CGA
    pixel grid at each location (same frame geometry) for the alpha silhouette.
    """
    pal = _palette_for_bpp(bpp, cga_palette)
    idxs = decode_wall_frame_indices(width, height, pixels, bpp)
    cga_idxs: list[int] | None = None
    if bpp == 4 and cga_silhouette is not None:
        cga_idxs = decode_wall_frame_indices(width, height, cga_silhouette, 2)
    out: list[tuple[int, int, int, int]] = []
    for i, idx in enumerate(idxs):
        r, g, b = pal[idx]
        if bpp == 2:
            key = idx in (0, 1)
        elif cga_idxs is not None:
            key = cga_idxs[i] < 2
        else:
            key = idx in (0, 1, 9, 10, 11)
        out.append((r, g, b, 0 if key else 255))
    return out


def render_wall_frame_rgba(
    width: int,
    height: int,
    pixels: bytes,
    bpp: int,
    *,
    cga_palette: int = 1,
    transparent_indices: tuple[int, ...] | None = None,
    amiga_mask: list[list[bool]] | None = None,
    amiga_rgba: list[tuple[int, int, int, int]] | None = None,
    frame: int | None = None,
) -> list[tuple[int, int, int, int]]:
    """Decode wall frame to RGBA with PC driver-style transparency.

    Prefer ``amiga_mask`` (pen 0 from the matching ``.32`` frame) when available —
    that is the authoritative silhouette, resampled when PC ``width``/``height``
    differ. Side-wall void pens (frames 4..11) are also keyed when the mask
    misses driver void colour. Otherwise use ``transparent_indices`` or
    ``wall_transparent_indices``.

    When ``amiga_rgba`` is supplied, PC palette index **0** inside the opaque
    silhouette is replaced with the resampled Amiga pixel — side-wall cones
    (frames 4..11) use EGA pen 8 / CGA pen 1 for void but still encode real
    wall detail as index 0 on PC where Amiga uses non-zero pens.
    """
    pal = _palette_for_bpp(bpp, cga_palette)
    idxs = decode_wall_frame_indices(width, height, pixels, bpp)
    void_idxs = wall_transparent_indices(bpp, frame)
    if transparent_indices is None and amiga_mask is None:
        transparent_indices = void_idxs
    if amiga_mask is not None:
        src_h = len(amiga_mask)
        src_w = len(amiga_mask[0]) if src_h else 0
        if src_w != width or src_h != height:
            amiga_mask = resample_amiga_mask(amiga_mask, width, height)
    out: list[tuple[int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            i = y * width + x
            idx = idxs[i]
            r, g, b = pal[idx]
            if amiga_mask is not None:
                key = amiga_mask[y][x]
                if not key and idx in void_idxs:
                    key = True
            else:
                key = idx in transparent_indices  # type: ignore[operator]
            if not key and idx == 0 and amiga_rgba is not None:
                ar, ag, ab, aa = amiga_rgba[i]
                if aa > 0:
                    r, g, b = ar, ag, ab
            out.append((r, g, b, 0 if key else 255))
    return out


# Combat sprite off-screen buffer size (CGA.DRV 0xA17: CX=0x900 = 96 rows × 24 bytes;
# EGA.DRV uses 0x1200 = 96 rows × 48 bytes). Sprites are positioned with u8 x,y in frame hdr.
COMBAT_CANVAS_W = 96
COMBAT_CANVAS_H = 96


@dataclass
class MonsterPicture:
    picture_id: int
    blob_offset: int
    frame_count: int
    flags: int
    frames: list[MonsterFrame]
    scripts: list[list[tuple[int, int]]]


def parse_monster_scripts(dec: bytes, frame_count: int) -> list[list[tuple[int, int]]]:
    """Parse combat animation script sequences between the offset table and first frame.

    Each sequence is a list of (frame_index, delay) byte pairs; 0xFF ends one sequence.
    A double 0xFF marks end of all scripts (MM2.EXE combat overlay, traced from blob layout).
    """
    table_end = 2 + frame_count * 2
    inner = [struct.unpack_from("<H", dec, 2 + i * 2)[0] for i in range(frame_count)]
    first = min((o for o in inner if o > 0), default=table_end)
    data = dec[table_end:first]
    scripts: list[list[tuple[int, int]]] = []
    current: list[tuple[int, int]] = []
    i = 0
    while i < len(data):
        b = data[i]
        if b == 0xFF:
            if i + 1 < len(data) and data[i + 1] == 0xFF:
                if current:
                    scripts.append(current)
                break
            if current:
                scripts.append(current)
                current = []
            i += 1
            continue
        if i + 1 < len(data):
            current.append((data[i], data[i + 1]))
            i += 2
        else:
            i += 1
    if current:
        scripts.append(current)
    return scripts


def _palette_for_bpp(bpp: int, cga_palette: int) -> list[tuple[int, int, int]]:
    if bpp == 2:
        return _CGA_PALETTE_0 if cga_palette == 0 else _CGA_PALETTE_1
    return _EGA_RGB


def blit_monster_grid(
    canvas: list[list[int | None]],
    grid: list[list[int | None]],
    ox: int,
    oy: int,
) -> None:
    """Alpha-blit one decoded sprite grid onto a combat canvas (CGA.DRV 0xAF2 semantics)."""
    h = len(grid)
    w = len(grid[0]) if h else 0
    ch = len(canvas)
    cw = len(canvas[0]) if ch else 0
    for r in range(h):
        for c in range(w):
            v = grid[r][c]
            if v is None:
                continue
            y, x = oy + r, ox + c
            if 0 <= y < ch and 0 <= x < cw:
                canvas[y][x] = v


def canvas_to_rgba(
    canvas: list[list[int | None]],
    bpp: int,
    cga_palette: int = 1,
) -> list[tuple[int, int, int, int]]:
    pal = _palette_for_bpp(bpp, cga_palette)
    out: list[tuple[int, int, int, int]] = []
    for row in canvas:
        for v in row:
            if v is None:
                out.append((0, 0, 0, 0))
            else:
                out.append((*pal[v], 255))
    return out


def clear_monster_rect(
    canvas: list[list[int | None]], x: int, y: int, w: int, h: int
) -> None:
    """Clear a combat-canvas rectangle (Amiga TV-prelude equivalent)."""
    ch = len(canvas)
    cw = len(canvas[0]) if ch else 0
    for r in range(h):
        for c in range(w):
            yy, xx = y + r, x + c
            if 0 <= yy < ch and 0 <= xx < cw:
                canvas[yy][xx] = None


def composite_monster_layers(
    frames_by_idx: dict[int, MonsterFrame],
    layer_indices: list[int],
    cga_palette: int = 1,
) -> list[tuple[int, int, int, int]]:
    """Composite combat sprite layers onto the 96×96 buffer (base frame 0 + overlays).

    Delta overlays clear their header rectangle before blit (same prelude semantics
    as Amiga ``composite_anm_frame``); transparent RLE nibbles stay transparent.
    """
    if not frames_by_idx:
        return canvas_to_rgba(
            [[None] * COMBAT_CANVAS_W for _ in range(COMBAT_CANVAS_H)], 2, cga_palette
        )
    bpp = next(iter(frames_by_idx.values())).bpp
    canvas: list[list[int | None]] = [[None] * COMBAT_CANVAS_W for _ in range(COMBAT_CANVAS_H)]
    for idx in layer_indices:
        fr = frames_by_idx.get(idx)
        if fr is None:
            continue
        if idx != 0:
            clear_monster_rect(canvas, fr.x, fr.y, fr.width, fr.height)
        grid = decode_monster_sprite(fr.width, fr.height, fr.stream, bpp)
        blit_monster_grid(canvas, grid, fr.x, fr.y)
    return canvas_to_rgba(canvas, bpp, cga_palette)


def composite_pc_combat_frame(
    frames_by_idx: dict[int, MonsterFrame],
    frame_idx: int,
    cga_palette: int = 1,
) -> list[tuple[int, int, int, int]]:
    """One combat animation step: base frame 0, or base + cleared delta patch."""
    if frame_idx == 0 or frame_idx not in frames_by_idx:
        return composite_monster_layers(frames_by_idx, [0], cga_palette=cga_palette)
    return composite_monster_layers(frames_by_idx, [0, frame_idx], cga_palette=cga_palette)


def parse_monster_picture_blob(
    raw: bytes, file_off: int, picture_id: int, ext: str
) -> MonsterPicture | None:
    dec = _decompress_monster_blob(raw, file_off)
    if dec is None:
        return None
    frame_count = dec[0] & 0x3F
    if frame_count == 0:
        return None
    hit = monster_picture_file_offset(raw, picture_id)
    table_index = hit[1] if hit else picture_table_index(picture_id)
    frames = _parse_monster_picture(raw, file_off, picture_id, table_index, ext)
    scripts = parse_monster_scripts(dec, frame_count)
    return MonsterPicture(
        picture_id=picture_id,
        blob_offset=file_off,
        frame_count=frame_count,
        flags=dec[1],
        frames=frames,
        scripts=scripts,
    )


# EGA.DRV combat-sprite low-nibble -> colour translation (xlat @0x121B).
# Low nibble 5 is the transparent marker (handled separately, never xlat'd).
_EGA_MONSTER_XLAT = [0, 1, 2, 9, 6, 8, 10, 3, 4, 5, 7, 11, 12, 13, 14, 15]


@dataclass
class MonsterFrame:
    picture_id: int
    frame_index: int
    blob_offset: int
    frame_offset: int
    x: int
    y: int
    width: int
    height: int
    stream: bytes
    ext: str = ".4"

    @property
    def bpp(self) -> int:
        return bpp_for_ext(self.ext)


def decode_monster_sprite(width: int, height: int, stream: bytes, bpp: int) -> list[list[int | None]]:
    """Decode the CGA.DRV (0xAF2) / EGA.DRV (0x1422) nibble-RLE masked sprite.

    Each token byte: high nibble = run count (run length = count+1),
    low nibble = code. CGA (.4): code 0-3 = literal colour, code 4-15 =
    transparent. EGA (.16): code 5 = transparent, else colour = xlat[code].
    Runs wrap across rows; decode stops after `height` rows. Returns a
    height x width grid of colour indices, with None marking transparent.
    """
    grid: list[list[int | None]] = [[None] * width for _ in range(height)]
    row = -1
    remaining = 0  # pixels left in the current row (ASM 'ah')
    col = 0
    done = False
    i = 0
    while i < len(stream) and not done:
        b = stream[i]
        i += 1
        count = (b >> 4) & 0xF
        low = b & 0xF
        if bpp == 2:
            transparent = low >= 4
            color = low & 3
        else:
            transparent = low == 5
            color = _EGA_MONSTER_XLAT[low]
        for _ in range(count + 1):
            if remaining == 0:
                row += 1
                if row >= height:
                    done = True
                    break
                col = 0
                remaining = width
            if not transparent and col < width:
                grid[row][col] = color
            col += 1
            remaining -= 1
    return grid


@dataclass
class MonsterSprite:
    picture_id: int
    table_index: int
    file_offset: int
    tag: int
    x: int
    y: int
    width: int
    height: int
    stream: bytes
    ext: str = ".4"

    @property
    def bpp(self) -> int:
        return bpp_for_ext(self.ext)


def _monster_table_end(raw: bytes) -> int:
    """Header size = flat table of u32 LE blob offsets.

    The table is ``entry[0..count-1]`` of u32 file offsets; the first blob sits
    immediately after it, so ``entry[0]`` equals the table byte-size (300 on GOG,
    i.e. 75 entries). Blobs must therefore start at or after this offset.
    """
    return struct.unpack_from("<I", raw, 0)[0]


def _monster_index_count(raw: bytes) -> int:
    return _monster_table_end(raw) // 4


def _monster_header_bytes(raw: bytes) -> int:
    return _monster_table_end(raw)


def _monster_seek_from_header(raw: bytes, entry: int, header_bytes: int | None = None) -> int:
    """u32 LE blob file offset @ byte ``entry * 4`` within the header table.

    ``entry`` is the 0-based table index. A ``monsters.dat`` picture id ``N`` maps
    to ``entry = N - 1`` (ids are 1-based; the table is 0-based).
    """
    limit = header_bytes if header_bytes is not None else _monster_header_bytes(raw)
    if entry < 0 or entry * 4 + 4 > limit:
        return 0
    return struct.unpack_from("<I", raw, entry * 4)[0]


def _decompress_monster_blob(raw: bytes, off: int) -> bytes | None:
    """Per-picture LZW blob: u32 LE size @ off, bitstream @ off+4 (load @ 0x690E)."""
    if off + 8 > len(raw):
        return None
    dec_size = struct.unpack_from("<I", raw, off)[0]
    if dec_size < 4 or dec_size > 512 * 1024:
        return None
    try:
        dec = lzw_decompress(raw[off + 4 :], dec_size)
    except Exception:
        return None
    if len(dec) < dec_size:
        return None
    return dec


def _monster_blob_valid(raw: bytes, file_off: int, table_end: int) -> bool:
    if file_off < table_end:
        return False
    dec = _decompress_monster_blob(raw, file_off)
    return dec is not None and (dec[0] & 0x3F) > 0


def _monster_blob_for_index(raw: bytes, index: int) -> int | None:
    """Resolve picture id ``N`` (== ``NN.anm`` number) -> blob file offset.

    Picture ids are 1-based; the header table is 0-based, so id ``N`` reads the
    u32 offset at table entry ``N - 1``. A value of 0 means the blob is absent
    from this ``MONSTERS.*`` file.
    """
    table_end = _monster_table_end(raw)
    fo = _monster_seek_from_header(raw, index - 1, table_end)
    if fo and _monster_blob_valid(raw, fo, table_end):
        return fo
    return None


def iter_monster_anim_indices(
    raw: bytes, anm_dir: Path | None = None
) -> list[tuple[int, int]]:
    """One entry per repo ``NN.anm`` that resolves in this ``MONSTERS.*`` file."""
    root = anm_dir or ROOT
    out: list[tuple[int, int]] = []
    for path in sorted(root.glob("[0-9][0-9].anm")):
        index = int(path.stem)
        fo = _monster_blob_for_index(raw, index)
        if fo is not None:
            out.append((index, fo))
    return out


def iter_monster_header_slots(
    raw: bytes, *, dedupe: bool = True
) -> list[tuple[int, int]]:
    """Unique blobs via header u32 seeks (diagnostics). Use ``iter_monster_anim_indices`` to export.

    Returns ``(picture_id, file_offset)``; table entry ``e`` maps to picture id ``e + 1``.
    """
    table_end = _monster_table_end(raw)
    entry_count = table_end // 4
    seen: set[int] = set()
    out: list[tuple[int, int]] = []
    for entry in range(entry_count):
        fo = _monster_seek_from_header(raw, entry, table_end)
        if fo == 0 or not _monster_blob_valid(raw, fo, table_end):
            continue
        if dedupe and fo in seen:
            continue
        seen.add(fo)
        out.append((entry + 1, fo))
    return out


def iter_monster_sprite_indices(
    raw: bytes, max_sprite: int = 74
) -> list[tuple[int, int, int, str]]:
    return [(i, fo, i, "anm") for i, fo in iter_monster_anim_indices(raw) if i <= max_sprite]


def iter_monster_picture_ids(
    raw: bytes, max_id: int = 256
) -> list[tuple[int, int, int]]:
    return [(s, fo, s) for s, fo in iter_monster_header_slots(raw) if s < max_id]


def monster_picture_file_offset(
    raw: bytes, index: int
) -> tuple[int, int, str] | None:
    fo = _monster_blob_for_index(raw, index)
    if fo is None:
        return None
    return fo, index, "anm"


def picture_table_index(slot: int) -> int:
    return slot


def _parse_monster_frame(
    dec: bytes, frame_off: int, frame_end: int
) -> tuple[int, int, int, int, bytes] | None:
    """Combat sprite frame: u8 x,y,w,h @ +0..+3, then nibble-RLE stream @ +4.

    The stream runs to the next frame offset (frame_end); the codec itself
    stops after `height` rows (CGA.DRV 0xAF2 / EGA.DRV 0x1422).
    """
    if frame_off + 4 > len(dec) or frame_end > len(dec) or frame_end <= frame_off + 4:
        return None
    x, y, width, height = (
        dec[frame_off], dec[frame_off + 1], dec[frame_off + 2], dec[frame_off + 3]
    )
    if width < 1 or height < 1 or width > 128 or height > 128:
        return None
    stream = dec[frame_off + 4 : frame_end]
    return x, y, width, height, stream


def _parse_monster_picture(
    raw: bytes,
    file_off: int,
    picture_id: int,
    table_index: int,
    ext: str,
) -> list[MonsterFrame]:
    dec = _decompress_monster_blob(raw, file_off)
    if dec is None:
        return []
    frame_count = dec[0] & 0x3F
    if frame_count == 0 or 2 + frame_count * 2 > len(dec):
        return []
    inner = [struct.unpack_from("<H", dec, 2 + i * 2)[0] for i in range(frame_count)]
    ordered = sorted(o for o in inner if 0 < o <= len(dec))
    frames: list[MonsterFrame] = []
    for fi, inner_off in enumerate(inner):
        if inner_off <= 0 or inner_off >= len(dec):
            continue
        nxt = [o for o in ordered if o > inner_off]
        frame_end = min(nxt) if nxt else len(dec)
        parsed = _parse_monster_frame(dec, inner_off, frame_end)
        if parsed is None:
            continue
        x, y, width, height, stream = parsed
        frames.append(
            MonsterFrame(
                picture_id=picture_id,
                frame_index=fi,
                blob_offset=file_off,
                frame_offset=inner_off,
                x=x,
                y=y,
                width=width,
                height=height,
                stream=stream,
                ext=ext,
            )
        )
    return frames


def parse_monsters_atlas(path: Path) -> dict:
    raw = path.read_bytes()
    if len(raw) < 4:
        raise ValueError(f"{path}: too small")
    slot_count = struct.unpack_from("<H", raw, 0)[0]
    if slot_count < 2 or 2 + slot_count * 2 > len(raw):
        raise ValueError(f"{path}: invalid slot_count {slot_count}")
    offsets = [struct.unpack_from("<H", raw, 2 + i * 2)[0] for i in range(slot_count)]
    ext = path.suffix.lower()
    bpp = bpp_for_ext(ext)
    table_end = 2 + slot_count * 2
    all_frames: list[MonsterFrame] = []
    for index, off in iter_monster_anim_indices(raw):
        all_frames.extend(_parse_monster_picture(raw, off, index, index, ext))
    sprite_offs = [o for o in offsets if o >= table_end]
    first_off = min(sprite_offs) if sprite_offs else 0
    # Legacy single-frame view (first frame per picture).
    sprites: list[MonsterSprite] = []
    seen: set[int] = set()
    for fr in all_frames:
        if fr.picture_id in seen:
            continue
        seen.add(fr.picture_id)
        hit = monster_picture_file_offset(raw, fr.picture_id)
        sprites.append(
            MonsterSprite(
                picture_id=fr.picture_id,
                table_index=hit[1] if hit else picture_table_index(fr.picture_id),
                file_offset=fr.blob_offset,
                tag=struct.unpack_from("<I", raw, fr.blob_offset)[0],
                x=fr.x,
                y=fr.y,
                width=fr.width,
                height=fr.height,
                stream=fr.stream,
                ext=ext,
            )
        )
    return {
        "kind": "monsters",
        "path": str(path),
        "ext": ext,
        "bpp": bpp,
        "slot_count": slot_count,
        "offsets": offsets,
        "interstitial": raw[table_end:first_off] if first_off else b"",
        "frames": all_frames,
        "sprites": sprites,
    }


def _parse_sprite_blob(
    raw: bytes, off: int, picture_id: int, table_index: int, ext: str
) -> MonsterSprite | None:
    frames = _parse_monster_picture(raw, off, picture_id, table_index, ext)
    if not frames:
        return None
    fr = frames[0]
    return MonsterSprite(
        picture_id=picture_id,
        table_index=table_index,
        file_offset=off,
        tag=struct.unpack_from("<I", raw, off)[0],
        x=fr.x,
        y=fr.y,
        width=fr.width,
        height=fr.height,
        stream=fr.stream,
        ext=ext,
    )


def sprite_for_picture(atlas: dict, picture_id: int) -> MonsterSprite | None:
    raw = Path(atlas["path"]).read_bytes()
    hit = monster_picture_file_offset(raw, picture_id)
    if hit is None:
        return None
    off, idx, _tag = hit
    return _parse_sprite_blob(raw, off, picture_id, idx, atlas["ext"])


def is_monsters_file(path: Path) -> bool:
    return path.name.upper().startswith("MONSTERS.")


def wall_out_dir(base: Path, path: Path) -> Path:
    variant = "cga" if path.suffix.lower() == ".4" else "ega"
    return base / variant / path.stem.lower()


def make_wall_sheet_montage(
    info: dict, cga_palette: int = 1, cols: int = 8, pad: int = 2
) -> tuple[list[tuple[int, int, int]], int, int]:
    """Tile all wall-sheet frames into one RGB image (reconstructed sheet view)."""
    frames = info["frames"]
    if not frames:
        return [], 0, 0
    max_w = max(fr.width for fr in frames)
    max_h = max(fr.height for fr in frames)
    cell_w = max_w + pad
    cell_h = max_h + pad
    n = len(frames)
    rows = (n + cols - 1) // cols
    out_w = cols * cell_w
    out_h = rows * cell_h
    rgb: list[tuple[int, int, int]] = [(32, 32, 32)] * (out_w * out_h)
    for i, fr in enumerate(frames):
        tile = decode_wall_frame_rgb(
            fr.width, fr.height, fr.pixels, info["bpp"], cga_palette=cga_palette
        )
        col, row = i % cols, i // cols
        ox, oy = col * cell_w, row * cell_h
        for y in range(fr.height):
            for x in range(fr.width):
                dst_x, dst_y = ox + x, oy + y
                rgb[dst_y * out_w + dst_x] = tile[y * fr.width + x]
    return rgb, out_w, out_h


def extract_wall_sheet(path: Path, out_dir: Path, cga_palette: int = 1) -> dict[str, int]:
    """Write raw/frameNN.png per slot + sheet.png montage."""
    info = parse_wall_sheet(path)
    dest_root = out_dir or wall_out_dir(DEFAULT_PC_GFX_OUT, path)
    raw_dir = dest_root / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)
    # Drop legacy flat frameNN.png from older extractor layout.
    for legacy in dest_root.glob("frame*.png"):
        legacy.unlink(missing_ok=True)
    for fr in info["frames"]:
        rgb = decode_wall_frame_rgb(
            fr.width, fr.height, fr.pixels, info["bpp"], cga_palette=cga_palette
        )
        write_png_rgb(rgb, fr.width, fr.height, raw_dir / f"frame{fr.index:02d}.png")
    sheet_rgb, sw, sh = make_wall_sheet_montage(info, cga_palette=cga_palette)
    if sheet_rgb:
        write_png_rgb(sheet_rgb, sw, sh, dest_root / "sheet.png")
    return {"raw": len(info["frames"]), "sheet": 1 if sheet_rgb else 0}


def _pick_primary_script(scripts: list[list[tuple[int, int]]]) -> int | None:
    """Pick the best animation script for wiki showcase (longest walk-style cycle)."""
    best_si: int | None = None
    best_len = 1
    for si, script in enumerate(scripts):
        n = len(script)
        if n <= best_len:
            continue
        best_si, best_len = si, n
    if best_si is not None:
        return best_si
    for si, script in enumerate(scripts):
        if len(script) > 1:
            return si
    return None


def _write_monster_thumb(base_path: Path, out: Path, scale: int = 3) -> None:
    try:
        from PIL import Image
    except ImportError:
        return
    img = Image.open(base_path).convert("RGBA")
    w, h = img.size
    img.resize((w * scale, h * scale), Image.NEAREST).save(out)


def extract_monsters_atlas(path: Path, out_dir: Path, cga_palette: int = 1) -> dict[str, int]:
    """One folder per ``NN.anm`` index that exists in this MONSTERS file."""
    atlas = parse_monsters_atlas(path)
    dest_root = out_dir or (
        DEFAULT_PC_GFX_OUT / "monsters" / ("cga" if path.suffix.lower() == ".4" else "ega")
    )
    if dest_root.exists():
        shutil.rmtree(dest_root)
    dest_root.mkdir(parents=True, exist_ok=True)
    raw_n = comp_n = gif_n = primary_n = 0
    raw_bytes = Path(atlas["path"]).read_bytes()
    ext = atlas["ext"]
    pictures_exported: list[int] = []

    for index, off in iter_monster_anim_indices(raw_bytes):
        blob = parse_monster_picture_blob(raw_bytes, off, index, ext)
        if blob is None or not blob.frames:
            continue
        pictures_exported.append(index)
        key = f"{index:02d}"
        anim_dir = dest_root / key
        frames_dir = anim_dir / "frames"
        frames_dir.mkdir(parents=True, exist_ok=True)
        by_idx = {fr.frame_index: fr for fr in blob.frames}
        for fr in blob.frames:
            rgba = render_monster_frame_rgba(fr, cga_palette=cga_palette)
            write_png(
                rgba, fr.width, fr.height, frames_dir / f"f{fr.frame_index:02d}.png"
            )
            raw_n += 1
        base_path = anim_dir / "base.png"
        base = composite_monster_layers(by_idx, [0], cga_palette=cga_palette)
        write_png(base, COMBAT_CANVAS_W, COMBAT_CANVAS_H, base_path)
        comp_n += 1
        _write_monster_thumb(base_path, anim_dir / "thumb.png")

        script_gifs: dict[int, Path] = {}
        for si, script in enumerate(blob.scripts):
            frames_for_gif: list[Path] = []
            for step, (fidx, _delay) in enumerate(script):
                rgba = composite_pc_combat_frame(by_idx, fidx, cga_palette=cga_palette)
                step_path = anim_dir / f"s{si}_step{step:02d}.png"
                write_png(rgba, COMBAT_CANVAS_W, COMBAT_CANVAS_H, step_path)
                frames_for_gif.append(step_path)
                comp_n += 1
            if len(frames_for_gif) > 1:
                gif_path = anim_dir / f"s{si}.gif"
                _write_gif_from_pngs(frames_for_gif, gif_path)
                script_gifs[si] = gif_path
                gif_n += 1

        primary_si = _pick_primary_script(blob.scripts)
        primary_gif = dest_root / f"{key}.gif"
        if primary_si is not None and primary_si in script_gifs:
            shutil.copy2(script_gifs[primary_si], primary_gif)
            primary_n += 1
        elif base_path.is_file():
            _write_gif_from_pngs([base_path], primary_gif, duration_ms=400)
            primary_n += 1

    anm_total = len(list(ROOT.glob("[0-9][0-9].anm")))
    missing = [i for i in sorted(int(p.stem) for p in ROOT.glob("[0-9][0-9].anm"))
               if i not in pictures_exported]
    manifest = dest_root / "manifest.txt"
    manifest.write_text(
        "\n".join([
            f"# {Path(atlas['path']).name}: {len(pictures_exported)}/{anm_total} repo .anm indices",
            *(f"{i:02d} @{_monster_blob_for_index(raw_bytes, i):#x}" for i in pictures_exported),
            "",
            f"# no blob in this file: {missing}" if missing else "",
        ]),
        encoding="utf-8",
    )

    return {
        "raw": raw_n,
        "composite": comp_n,
        "gif": gif_n,
        "primary": primary_n,
        "pictures": len(pictures_exported),
        "picture_ids": pictures_exported,
        "missing_anm": missing,
    }


GIF_BG = (13, 3, 3)


def _flatten_rgba_image(im: "Image.Image", bg: tuple[int, int, int] = GIF_BG) -> "Image.Image":
    from PIL import Image

    flat = Image.new("RGB", im.size, bg)
    if im.mode == "RGBA":
        flat.paste(im, mask=im.split()[3])
    else:
        flat.paste(im)
    return flat


def _write_gif_from_pngs(
    paths: list[Path], out: Path, duration_ms: int = 150, bg: tuple[int, int, int] = GIF_BG
) -> None:
    try:
        from PIL import Image
    except ImportError:
        return
    imgs = [_flatten_rgba_image(Image.open(p).convert("RGBA"), bg) for p in paths]
    out.parent.mkdir(parents=True, exist_ok=True)
    dur = max(40, min(duration_ms, 2000))
    if len(imgs) == 1:
        imgs[0].save(out, optimize=True)
        return
    imgs[0].save(
        out,
        save_all=True,
        append_images=imgs[1:],
        duration=dur,
        loop=0,
        disposal=2,
        optimize=True,
    )


def batch_extract_gog(gog_dir: Path, out_dir: Path, cga_palette: int = 1) -> int:
    total = 0
    files = sorted(
        p for p in gog_dir.iterdir() if p.suffix.lower() in (".4", ".16")
    )
    for path in files:
        try:
            if is_monsters_file(path):
                stats = extract_monsters_atlas(path, out_dir / "monsters", cga_palette)
                variant = "cga" if path.suffix.lower() == ".4" else "ega"
                print(
                    f"{path.name}: {stats['pictures']} pictures, raw={stats['raw']} "
                    f"composite={stats['composite']} script_gifs={stats['gif']} "
                    f"primary_gifs={stats['primary']} -> {out_dir / 'monsters' / variant}"
                )
                total += stats["raw"]
            else:
                stats = extract_wall_sheet(path, wall_out_dir(out_dir, path), cga_palette=cga_palette)
                print(
                    f"{path.name}: raw={stats['raw']} sheet={stats['sheet']} "
                    f"-> {wall_out_dir(out_dir, path)}"
                )
                total += stats["raw"]
        except Exception as exc:
            print(f"{path.name}: FAILED {exc}", file=sys.stderr)
    print(f"Batch done: {len(files)} files, {total} raw frames")
    return 0


def render_monster_frame_rgba(fr: MonsterFrame, cga_palette: int = 1) -> list[tuple[int, int, int, int]]:
    """Decode a combat sprite frame's nibble-RLE stream to flat RGBA pixels."""
    bpp = fr.bpp
    grid = decode_monster_sprite(fr.width, fr.height, fr.stream, bpp)
    if bpp == 2:
        pal = _CGA_PALETTE_0 if cga_palette == 0 else _CGA_PALETTE_1
    else:
        pal = _EGA_RGB
    out: list[tuple[int, int, int, int]] = []
    for r in range(fr.height):
        for c in range(fr.width):
            v = grid[r][c]
            if v is None:
                out.append((0, 0, 0, 0))
            else:
                out.append((*pal[v], 255))
    return out


def decode_indexed_sprite(
    width: int, height: int, pixels: bytes, mask: bytes, bpp: int
) -> list[tuple[int, int, int, int]]:
    """Return RGBA rows (flat list length width*height*4)."""
    rb = row_bytes(width, bpp)
    pal = _CGA_PALETTE_0 if bpp == 2 else _EGA_RGB
    out: list[tuple[int, int, int, int]] = []
    bit = 0
    for y in range(height):
        row = pixels[y * rb : (y + 1) * rb]
        x = 0
        for b in row:
            if bpp == 2:
                for shift in (6, 4, 2, 0):
                    if x >= width:
                        break
                    masked = 0
                    if mask:
                        masked = (mask[bit >> 3] >> (7 - (bit & 7))) & 1
                    bit += 1
                    idx = (b >> shift) & 3
                    rgb = pal[idx]
                    a = 0 if masked else 255
                    out.append((*rgb, a))
                    x += 1
            else:
                for shift in (4, 0):
                    if x >= width:
                        break
                    masked = 0
                    if mask:
                        masked = (mask[bit >> 3] >> (7 - (bit & 7))) & 1
                    bit += 1
                    idx = (b >> shift) & 0xF
                    rgb = pal[idx] if idx < len(pal) else (idx * 17, idx * 17, idx * 17)
                    a = 0 if masked else 255
                    out.append((*rgb, a))
                    x += 1
    return out


def write_png_rgb(rgb: list[tuple[int, int, int]], width: int, height: int, out: Path) -> None:
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("PNG export requires Pillow: pip install pillow") from exc
    img = Image.new("RGB", (width, height))
    img.putdata(rgb)
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)


def write_png(rgba: list[tuple[int, int, int, int]], width: int, height: int, out: Path) -> None:
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("PNG export requires Pillow: pip install pillow") from exc
    img = Image.new("RGBA", (width, height))
    img.putdata(rgba)
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)


def report_monster_header(path: Path) -> dict:
    """Summarize which repo ``NN.anm`` indices resolve in ``MONSTERS.*``."""
    raw = path.read_bytes()
    slots = iter_monster_anim_indices(raw)
    anm_total = len(list(ROOT.glob("[0-9][0-9].anm")))
    return {
        "index_count": _monster_index_count(raw),
        "header_bytes": _monster_header_bytes(raw),
        "anm_total": anm_total,
        "blob_count": len(slots),
        "missing": [i for i in sorted(int(p.stem) for p in ROOT.glob("[0-9][0-9].anm"))
                    if i not in {s for s, _ in slots}],
        "slots": slots,
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Decode MM2 PC .4 / .16 graphics")
    ap.add_argument("input", type=Path, help=".4/.16 file or GOG game directory")
    ap.add_argument("-o", "--output", type=Path, help="Output file (.bin or .png)")
    ap.add_argument(
        "--out-dir",
        type=Path,
        help=f"Output directory (default: {DEFAULT_PC_GFX_OUT}/<cga|ega>/<stem>/)",
    )
    ap.add_argument("--info", action="store_true", help="Print header summary")
    ap.add_argument("--monsters", action="store_true", help="Parse MONSTERS.* atlas only")
    ap.add_argument(
        "--header",
        action="store_true",
        help="List MONSTERS.* header slots (needs --monsters)",
    )
    ap.add_argument("--list", action="store_true", help="List monster sprites (with --monsters)")
    ap.add_argument(
        "--extract-all",
        action="store_true",
        help="Write all frames/sprites to --out-dir",
    )
    ap.add_argument(
        "--batch",
        action="store_true",
        help="Extract all .4/.16 from a game directory (includes MONSTERS.*)",
    )
    ap.add_argument("--frame", type=int, default=-1, help="Wall sheet frame index")
    ap.add_argument(
        "--cga-palette",
        type=int,
        choices=(0, 1),
        default=1,
        help="CGA BIOS palette for .4 preview (1=cyan/magenta/white, MM2 default per CGA.DRV; 0=green/red/brown)",
    )
    ap.add_argument("--index", type=int, default=-1, help="Anim index NN (matches NN.anm)")
    args = ap.parse_args(argv)

    path = args.input
    if path.is_dir():
        if args.batch or not args.monsters:
            out = args.out_dir or DEFAULT_PC_GFX_OUT
            return batch_extract_gog(path, out, cga_palette=args.cga_palette)
        path = path / "MONSTERS.4"
    if not path.is_file():
        print(f"Not found: {path}", file=sys.stderr)
        return 1

    if args.monsters or is_monsters_file(path):
        atlas = parse_monsters_atlas(path)
        variant = "cga" if path.suffix.lower() == ".4" else "ega"
        out_dir = args.out_dir or (DEFAULT_PC_GFX_OUT / "monsters" / variant)
        if args.extract_all:
            stats = extract_monsters_atlas(path, out_dir, cga_palette=args.cga_palette)
            print(f"Wrote {stats['pictures']} pictures raw={stats['raw']} primary_gifs={stats['primary']} -> {out_dir.resolve()}")
            return 0
        if args.header:
            rep = report_monster_header(path)
            print(f"{path.name}: {rep['blob_count']}/{rep['anm_total']} .anm indices "
                  f"(header {rep['header_bytes']}B)")
            if rep["missing"]:
                print("  no blob:", rep["missing"])
            for index, fo in rep["slots"][:25]:
                print(f"  {index:02d} -> {fo:#x}")
            if len(rep["slots"]) > 25:
                print(f"  ... +{len(rep['slots']) - 25} more")
            return 0
        if args.list or args.info or args.index < 0:
            print(f"{path.name}: {atlas['slot_count']} table slots, "
                  f"{len(atlas['sprites'])} anims, {len(atlas['frames'])} frames")
            print(f"  interstitial: {len(atlas['interstitial'])} bytes "
                  f"(between offset table and first sprite)")
            for sp in atlas["sprites"][:20]:
                print(f"  {sp.picture_id:02d} @{sp.file_offset:#x} "
                      f"{sp.width}x{sp.height} tag=0x{sp.tag:x}")
            if len(atlas["sprites"]) > 20:
                print(f"  ... +{len(atlas['sprites']) - 20} more")
        if args.index >= 0:
            sp = sprite_for_picture(atlas, args.index)
            if sp is None:
                print(f"No blob for anim index {args.index:02d}", file=sys.stderr)
                return 1
            print(f"{args.index:02d}: {sp.width}x{sp.height} @{sp.file_offset:#x}")
            dest = args.output
            if dest is None:
                out_dir.mkdir(parents=True, exist_ok=True)
                dest = out_dir / f"{args.index:02d}.png"
            if dest.suffix.lower() == ".png":
                rgba = render_monster_frame_rgba(sp, cga_palette=args.cga_palette)
                write_png(rgba, sp.width, sp.height, dest)
                print(f"Wrote {dest.resolve()}")
            else:
                dest.write_bytes(sp.stream)
                print(f"Wrote raw RLE stream -> {dest.resolve()}")
        return 0

    info = parse_wall_sheet(path)
    sheet_out = args.out_dir or wall_out_dir(DEFAULT_PC_GFX_OUT, path)
    if args.extract_all:
        stats = extract_wall_sheet(path, sheet_out, cga_palette=args.cga_palette)
        print(f"Wrote raw={stats['raw']} sheet={stats['sheet']} -> {sheet_out.resolve()}")
        return 0
    if args.frame >= 0:
        if args.frame >= len(info["frames"]):
            print(f"Frame {args.frame} out of range (0..{len(info['frames']) - 1})", file=sys.stderr)
            return 1
        fr = info["frames"][args.frame]
        rgb = decode_wall_frame_rgb(
            fr.width, fr.height, fr.pixels, info["bpp"], cga_palette=args.cga_palette
        )
        dest = args.output or (sheet_out / f"frame{fr.index:02d}.png")
        write_png_rgb(rgb, fr.width, fr.height, dest)
        print(f"frame {args.frame}: {fr.width}x{fr.height} -> {dest.resolve()}")
        return 0
    if args.info or args.output is None:
        slots = info["table_slot_count"]
        grouped = info.get("grouped_u16", False)
        kind = info["offset_kind"]
        slot_note = f"{slots} entries"
        if kind == "packed_u32":
            slot_note += " (packed u32 start|end<<16)"
        elif grouped:
            slot_note += " (grouped u16: even=start, odd=end)"
        print(f"{path.name}: {len(info['frames'])} frames from {slot_note} [{kind}] "
              f"@{info['bpp']}bpp")
        print(f"  compressed payload: {info['compressed_bytes']} bytes")
        print(f"  decompressed: {len(info['decompressed'])} / {info['uncompressed_size']} bytes")
        for fr in info["frames"][:12]:
            slot = f" slot={fr.table_slot}" if fr.table_slot >= 0 else ""
            print(f"  frame {fr.index:2d} @{fr.offset:#x}{slot} hdr={fr.header_size} "
                  f"{fr.width}x{fr.height}")
        if len(info["frames"]) > 12:
            print(f"  ... +{len(info['frames']) - 12} more")
        if args.output:
            args.output.write_bytes(info["decompressed"])
            print(f"  wrote {args.output.resolve()}")
        return 0

    dest = args.output
    if dest.suffix.lower() == ".png":
        fr = info["frames"][0]
        rgb = decode_wall_frame_rgb(
            fr.width, fr.height, fr.pixels, info["bpp"], cga_palette=args.cga_palette
        )
        write_png_rgb(rgb, fr.width, fr.height, dest)
        print(f"Wrote frame 0 -> {dest.resolve()}")
    else:
        dest.write_bytes(info["decompressed"])
        print(f"Wrote {len(info['decompressed'])} bytes -> {dest.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

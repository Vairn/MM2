#!/usr/bin/env python3
"""Build TTF fonts from the MM2 Amiga bitmap strike.

Outputs:
  - editor/assets/fonts/mm2_bitmap_proportional.ttf
  - editor/assets/fonts/mm2_bitmap_monospace.ttf

Notes:
  - Printable ASCII (0x20..0x7E) maps to normal Unicode codepoints.
  - Non-printables (0x00..0x1F, 0x7F) map to Private Use Area U+E000+cp.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

HUNK_HEADER = 1011
HUNK_CODE = 1001


@dataclass
class TextFont:
    y_size: int
    x_size: int
    baseline: int
    lo_char: int
    hi_char: int
    char_data: int
    modulo: int
    char_loc: int


def be16(data: bytes, off: int) -> int:
    return struct.unpack_from(">H", data, off)[0]


def be32(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]


def extract_first_code_hunk(data: bytes) -> bytes:
    if len(data) < 8 or be32(data, 0) != HUNK_HEADER:
        raise ValueError("not an Amiga hunk file")
    for off in range(0, len(data) - 8, 4):
        if be32(data, off) != HUNK_CODE:
            continue
        words = be32(data, off + 4)
        start = off + 8
        end = start + words * 4
        if end <= len(data):
            return data[start:end]
    raise ValueError("HUNK_CODE not found")


def parse_text_font(code: bytes, off: int) -> TextFont | None:
    if off + 32 > len(code):
        return None
    y_size = be16(code, off + 0)
    x_size = be16(code, off + 4)
    baseline = be16(code, off + 6)
    lo_char = code[off + 12]
    hi_char = code[off + 13]
    char_data = be32(code, off + 14)
    modulo = be16(code, off + 18)
    char_loc = be32(code, off + 20)

    if y_size <= 0 or y_size > 64:
        return None
    if x_size <= 0 or x_size > 64:
        return None
    if hi_char < lo_char:
        return None
    if modulo <= 0 or modulo > 1024:
        return None
    if char_data >= len(code) or char_loc >= len(code):
        return None
    if char_data >= char_loc:
        return None
    count = hi_char - lo_char + 1
    if char_loc + count * 4 > len(code):
        return None
    if char_data + modulo * y_size > len(code):
        return None
    return TextFont(
        y_size=y_size,
        x_size=x_size,
        baseline=baseline,
        lo_char=lo_char,
        hi_char=hi_char,
        char_data=char_data,
        modulo=modulo,
        char_loc=char_loc,
    )


def find_text_font(code: bytes) -> TextFont:
    fallback = None
    for off in range(0, len(code) - 32):
        tf = parse_text_font(code, off)
        if not tf:
            continue
        if tf.y_size == 8:
            return tf
        if fallback is None:
            fallback = tf
    if fallback is None:
        raise ValueError("TextFont record not found")
    return fallback


def read_bit(code: bytes, row_start: int, bit_idx: int) -> int:
    byte_off = row_start + (bit_idx >> 3)
    if byte_off < 0 or byte_off >= len(code):
        return 0
    mask = 0x80 >> (bit_idx & 7)
    return 1 if (code[byte_off] & mask) else 0


def decode_glyph(code: bytes, tf: TextFont, cp: int) -> tuple[int, int, list[int]]:
    idx = cp - tf.lo_char
    loc = be32(code, tf.char_loc + idx * 4)
    bit_offset = (loc >> 16) & 0xFFFF
    width = loc & 0xFFFF
    if width <= 0 or width > 32:
        return (0, tf.y_size, [])
    out = [0] * (width * tf.y_size)
    for y in range(tf.y_size):
        row = tf.char_data + y * tf.modulo
        for x in range(width):
            out[y * width + x] = read_bit(code, row, bit_offset + x)
    return (width, tf.y_size, out)


def glyph_name_for_codepoint(cp: int) -> str:
    return f"u{cp:04X}"


def bitmap_to_glyph(bits: list[int], w: int, h: int, px: int, ascent: int):
    pen = TTGlyphPen(None)
    if w <= 0 or h <= 0:
        return pen.glyph()
    for y in range(h):
        for x in range(w):
            if bits[y * w + x] == 0:
                continue
            x0 = x * px
            x1 = x0 + px
            # TTF y-axis is upward; bitmap y=0 is top row.
            y1 = ascent - y * px
            y0 = y1 - px
            pen.moveTo((x0, y0))
            pen.lineTo((x1, y0))
            pen.lineTo((x1, y1))
            pen.lineTo((x0, y1))
            pen.closePath()
    return pen.glyph()


def build_ttf(code: bytes, tf: TextFont, out_path: Path, *, monospace: bool) -> None:
    units_per_em = 1024
    px = units_per_em // tf.y_size  # 128 for 8px source.
    ascent = tf.y_size * px
    descent = -px

    glyph_order = [".notdef"]
    glyphs = {}
    h_metrics = {}
    cmap = {}

    # .notdef simple square frame
    nd_pen = TTGlyphPen(None)
    nd_pen.moveTo((0, 0))
    nd_pen.lineTo((px * tf.x_size, 0))
    nd_pen.lineTo((px * tf.x_size, ascent))
    nd_pen.lineTo((0, ascent))
    nd_pen.closePath()
    glyphs[".notdef"] = nd_pen.glyph()
    h_metrics[".notdef"] = (px * tf.x_size, 0)

    mono_adv = px * tf.x_size
    for cp in range(tf.lo_char, tf.hi_char + 1):
        w, h, bits = decode_glyph(code, tf, cp)
        uni_cp = cp if 0x20 <= cp <= 0x7E else (0xE000 + cp)
        gname = glyph_name_for_codepoint(uni_cp)
        if gname in glyphs:
            # In case of accidental duplicate mapping, skip.
            continue
        glyph_order.append(gname)
        glyphs[gname] = bitmap_to_glyph(bits, w, h, px, ascent)
        adv = mono_adv if monospace else max(px, w * px)
        h_metrics[gname] = (adv, 0)
        cmap[uni_cp] = gname

    fb = FontBuilder(units_per_em, isTTF=True)
    fb.setupGlyphOrder(glyph_order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(h_metrics)
    fb.setupHorizontalHeader(ascent=ascent, descent=descent)
    fb.setupMaxp()
    fb.setupOS2(
        sTypoAscender=ascent,
        sTypoDescender=descent,
        sTypoLineGap=0,
        usWinAscent=ascent,
        usWinDescent=-descent,
        usWeightClass=400,
        usWidthClass=5,
        sxHeight=int(ascent * 0.6),
        sCapHeight=int(ascent * 0.8),
        usDefaultChar=ord("?"),
        usBreakChar=ord(" "),
    )
    family = "MM2 Bitmap Mono" if monospace else "MM2 Bitmap"
    subfamily = "Regular"
    ps_name = "MM2BitmapMono-Regular" if monospace else "MM2Bitmap-Regular"
    fb.setupNameTable(
        {
            "familyName": family,
            "styleName": subfamily,
            "uniqueFontIdentifier": f"{family} {subfamily}",
            "fullName": f"{family} {subfamily}",
            "psName": ps_name,
            "version": "Version 1.0",
        }
    )
    fb.setupPost(isFixedPitch=1 if monospace else 0)
    fb.save(str(out_path))


def main() -> None:
    repo = Path(__file__).resolve().parent.parent
    src = repo / "EXTRACTED" / "fonts" / "mm2" / "8"
    out_dir = repo / "editor" / "assets" / "fonts"
    out_dir.mkdir(parents=True, exist_ok=True)

    code = extract_first_code_hunk(src.read_bytes())
    tf = find_text_font(code)

    out_prop = out_dir / "mm2_bitmap_proportional.ttf"
    out_mono = out_dir / "mm2_bitmap_monospace.ttf"
    build_ttf(code, tf, out_prop, monospace=False)
    build_ttf(code, tf, out_mono, monospace=True)

    print(f"Wrote {out_prop}")
    print(f"Wrote {out_mono}")


if __name__ == "__main__":
    main()


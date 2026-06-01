#!/usr/bin/env python3
"""Export MM2 Amiga 8px font as PNG + JSON for ImGui bitmap usage.

Input:
  EXTRACTED/fonts/mm2/8

Output:
  editor/assets/fonts/mm2_8.png
  editor/assets/fonts/mm2_8.json
"""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


HUNK_HEADER = 1011
HUNK_CODE = 1001


@dataclass
class TextFont:
    y_size: int
    style: int
    flags: int
    x_size: int
    baseline: int
    bold_smear: int
    accessors: int
    lo_char: int
    hi_char: int
    char_data: int
    modulo: int
    char_loc: int
    char_space: int
    char_kern: int


def be16(data: bytes, off: int) -> int:
    return struct.unpack_from(">H", data, off)[0]


def be32(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]


def crc32_chunk(tag: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)


def png_chunk(tag: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + tag + payload + crc32_chunk(tag, payload)


def write_rgba_png(path: Path, w: int, h: int, rgba: bytes) -> None:
    assert len(rgba) == w * h * 4
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)  # RGBA8

    # PNG scanlines: 1 filter byte + row bytes.
    row_bytes = w * 4
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter type 0 (None)
        start = y * row_bytes
        raw.extend(rgba[start : start + row_bytes])
    idat = zlib.compress(bytes(raw), level=9)

    out = bytearray()
    out.extend(sig)
    out.extend(png_chunk(b"IHDR", ihdr))
    out.extend(png_chunk(b"IDAT", idat))
    out.extend(png_chunk(b"IEND", b""))
    path.write_bytes(out)


def extract_first_code_hunk(data: bytes) -> bytes:
    if len(data) < 8 or be32(data, 0) != HUNK_HEADER:
        raise ValueError("not an Amiga hunk file")
    for off in range(0, len(data) - 8, 4):
        if be32(data, off) != HUNK_CODE:
            continue
        long_words = be32(data, off + 4)
        byte_len = long_words * 4
        start = off + 8
        end = start + byte_len
        if end <= len(data):
            return data[start:end]
    raise ValueError("no HUNK_CODE found")


def parse_text_font(code: bytes, off: int) -> TextFont | None:
    if off + 32 > len(code):
        return None
    tf = TextFont(
        y_size=be16(code, off + 0),
        style=code[off + 2],
        flags=code[off + 3],
        x_size=be16(code, off + 4),
        baseline=be16(code, off + 6),
        bold_smear=be16(code, off + 8),
        accessors=be16(code, off + 10),
        lo_char=code[off + 12],
        hi_char=code[off + 13],
        char_data=be32(code, off + 14),
        modulo=be16(code, off + 18),
        char_loc=be32(code, off + 20),
        char_space=be32(code, off + 24),
        char_kern=be32(code, off + 28),
    )
    if tf.y_size <= 0 or tf.y_size > 64:
        return None
    if tf.x_size <= 0 or tf.x_size > 64:
        return None
    if tf.hi_char < tf.lo_char:
        return None
    if tf.modulo <= 0 or tf.modulo > 1024:
        return None
    if tf.char_data >= len(code) or tf.char_loc >= len(code):
        return None
    if tf.char_data >= tf.char_loc:
        return None
    char_count = tf.hi_char - tf.lo_char + 1
    if tf.char_loc + char_count * 4 > len(code):
        return None
    bitmap_bytes = tf.modulo * tf.y_size
    if tf.char_data + bitmap_bytes > len(code):
        return None
    return tf


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
        raise ValueError("no valid TextFont found")
    return fallback


def read_bit(code: bytes, row_start: int, bit_index: int) -> int:
    byte_off = row_start + (bit_index >> 3)
    if byte_off < 0 or byte_off >= len(code):
        return 0
    mask = 0x80 >> (bit_index & 7)
    return 1 if (code[byte_off] & mask) else 0


def decode_glyph(code: bytes, tf: TextFont, cp: int) -> tuple[int, int, list[int]]:
    idx = cp - tf.lo_char
    loc = be32(code, tf.char_loc + idx * 4)
    bit_offset = (loc >> 16) & 0xFFFF
    width = loc & 0xFFFF
    if width <= 0 or width > 32:
        return (0, tf.y_size, [])

    pixels = [0] * (width * tf.y_size)
    for y in range(tf.y_size):
        row_start = tf.char_data + y * tf.modulo
        for x in range(width):
            pixels[y * width + x] = read_bit(code, row_start, bit_offset + x)
    return (width, tf.y_size, pixels)


def parse_sizes_arg(raw: str) -> list[int]:
    values: list[int] = []
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        v = int(token)
        if v <= 0:
            raise ValueError(f"invalid size: {v}")
        values.append(v)
    if not values:
        raise ValueError("no sizes provided")
    # Preserve order while deduping.
    deduped: list[int] = []
    seen = set()
    for v in values:
        if v in seen:
            continue
        seen.add(v)
        deduped.append(v)
    return deduped


def scale_glyph(bits: list[int], src_w: int, src_h: int, dst_w: int, dst_h: int) -> list[int]:
    if src_w <= 0 or src_h <= 0 or dst_w <= 0 or dst_h <= 0:
        return []
    out = [0] * (dst_w * dst_h)
    sx = src_w / dst_w
    sy = src_h / dst_h
    for y in range(dst_h):
        src_y = min(src_h - 1, int(y * sy))
        for x in range(dst_w):
            src_x = min(src_w - 1, int(x * sx))
            out[y * dst_w + x] = bits[src_y * src_w + src_x]
    return out


def export_size(repo: Path, src: Path, out_dir: Path, code: bytes, tf: TextFont, out_size: int) -> None:
    # Export the full MM2 strike range so game-specific symbols (e.g. the party
    # selection tick/check glyph) are present in the atlas.
    first_cp = tf.lo_char
    last_cp = tf.hi_char
    if first_cp > last_cp:
        raise ValueError("font has no printable ASCII glyphs")

    scale = out_size / tf.y_size
    cell_w = max(1, round(tf.x_size * scale))
    cell_h = out_size
    cols = 16
    glyph_count = last_cp - first_cp + 1
    rows = (glyph_count + cols - 1) // cols
    atlas_w = cols * cell_w
    atlas_h = rows * cell_h

    rgba = bytearray(atlas_w * atlas_h * 4)
    glyphs: list[dict[str, int]] = []

    for i, cp in enumerate(range(first_cp, last_cp + 1)):
        src_w, src_h, src_bits = decode_glyph(code, tf, cp)
        gw = max(0, round(src_w * scale))
        gh = cell_h
        bits = scale_glyph(src_bits, src_w, src_h, gw, gh)
        cx = (i % cols) * cell_w
        cy = (i // cols) * cell_h
        for y in range(gh):
            for x in range(gw):
                if bits[y * gw + x] == 0:
                    continue
                px = cx + x
                py = cy + y
                off = (py * atlas_w + px) * 4
                rgba[off + 0] = 255
                rgba[off + 1] = 255
                rgba[off + 2] = 255
                rgba[off + 3] = 255

        glyphs.append(
            {
                "codepoint": cp,
                "x": cx,
                "y": cy,
                "w": gw,
                "h": gh,
                "advance": gw if gw > 0 else max(1, round(tf.x_size * scale * 0.5)),
                "xoffset": 0,
                "yoffset": 0,
            }
        )

    png_path = out_dir / f"mm2_{out_size}.png"
    json_path = out_dir / f"mm2_{out_size}.json"
    write_rgba_png(png_path, atlas_w, atlas_h, bytes(rgba))

    meta = {
        "name": f"mm2_{out_size}",
        "source": str(src.relative_to(repo)).replace("\\", "/"),
        "lineHeight": out_size,
        "baseline": round(tf.baseline * scale),
        "baseSize": tf.y_size,
        "targetSize": out_size,
        "scale": scale,
        "atlas": {
            "path": png_path.name,
            "width": atlas_w,
            "height": atlas_h,
            "cellWidth": cell_w,
            "cellHeight": cell_h,
            "channels": "rgba8",
        },
        "ranges": [{"start": first_cp, "end": last_cp}],
        "printableRange": {"start": 0x20, "end": 0x7E},
        "glyphs": glyphs,
    }
    json_path.write_text(json.dumps(meta, indent=2), encoding="utf-8")
    print(f"Wrote {png_path}")
    print(f"Wrote {json_path}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--sizes",
        default="8,12,16",
        help="Comma-separated output pixel sizes (default: 8,12,16)",
    )
    args = ap.parse_args()

    repo = Path(__file__).resolve().parent.parent
    src = repo / "EXTRACTED" / "fonts" / "mm2" / "8"
    out_dir = repo / "editor" / "assets" / "fonts"
    out_dir.mkdir(parents=True, exist_ok=True)

    code = extract_first_code_hunk(src.read_bytes())
    tf = find_text_font(code)
    sizes = parse_sizes_arg(args.sizes)
    for out_size in sizes:
        export_size(repo, src, out_dir, code, tf, out_size)


if __name__ == "__main__":
    main()


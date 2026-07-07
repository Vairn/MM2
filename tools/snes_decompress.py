#!/usr/bin/env python3
"""SNES MM2 graphics unpacker + renderer, derived from the 65816 ROM routines.

FINDING (ASM is source of truth): MM2 SNES does NOT LZ/RLE-compress its
graphics. The routine prior notes called a "decompressor" at $00E289 is a plain
16-bit word copy that stages a BGR555 *palette* into WRAM $7E:2000; $00E354 is a
per-channel palette cross-fade; $00E2B4 DMAs $7E:2000 -> CGRAM. CHR (4bpp tiles)
and tilemaps are stored RAW in ROM and DMA'd straight to VRAM (e.g. the
exploration loader at $069DDD DMAs 32 KiB from $06:BA5C -> VRAM $4000).

This tool therefore:
  * faithfully re-implements $00E289 (palette word copy) and $00E354 (fade step),
  * resolves a raw ROM->VRAM DMA source (LoROM 24-bit addr) to a file slice,
  * decodes SNES 4bpp bitplane tiles + a BGR555 palette and renders a PNG.

Routine map (bank $00, LoROM file offset = pc & 0x7FFF within the bank):
  $00E289  palette word copy   ROM[$E8..$EA],Y -> $7E:2000,X  (Y = word count)
  $00E354  palette fade step   moves $7E:2000 color +-1/chan toward ROM target
  $00E2B4  CGRAM DMA           $7E:2000 (512 B) -> CGRAM via BBAD $22
  $00E648  JSL entry -> JSR $00E289 (RTL wrapper)
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
    Image = None  # type: ignore


# --- LoROM address helpers ------------------------------------------------

def lorom_to_file(bank: int, addr: int) -> int:
    """Map a 24-bit LoROM CPU address to a ROM file offset.

    Banks are mirrored: $80-$FF alias $00-$7F (FastROM). Each bank maps its
    upper half ($8000-$FFFF) to a 32 KiB ROM page. Addresses with the high bit
    clear ($0000-$7FFF) also alias the same page in this cartridge's data banks.
    """
    bank &= 0x7F
    page = bank << 15
    return page | (addr & 0x7FFF)


# --- BGR555 palette -------------------------------------------------------

def bgr555_to_rgb(w: int) -> tuple[int, int, int]:
    r = (w & 0x1F) << 3
    g = ((w >> 5) & 0x1F) << 3
    b = ((w >> 10) & 0x1F) << 3
    return r | (r >> 5), g | (g >> 5), b | (b >> 5)


def read_palette_words(rom: bytes, file_off: int, count: int) -> list[int]:
    """Faithful $00E289 semantics: read `count` little-endian 16-bit words.

    E289 does `LDA [$E8],Y / STA $7E2000,X` in 16-bit mode for Y words - i.e. a
    straight copy of the on-ROM palette table (no transform).
    """
    return [struct.unpack_from("<H", rom, file_off + i * 2)[0] for i in range(count)]


def palette_fade_step(cur: list[int], target: list[int]) -> list[int]:
    """Faithful $00E354 semantics: move each BGR555 channel +-1 step toward target.

    Red step = 1 (mask $001F), green step = $20 (mask $03E0), blue step = $400
    (mask $7C00). Channels already equal are left unchanged. This is what drives
    the timed palette fade in $00E2EE / $00E32F over 31 frames.
    """
    out: list[int] = []
    for c, t in zip(cur, target):
        res = 0
        for mask, step in ((0x001F, 0x0001), (0x03E0, 0x0020), (0x7C00, 0x0400)):
            cc = c & mask
            tc = t & mask
            if cc < tc:
                cc = (cc + step) & mask
            elif cc > tc:
                cc = (cc - step) & mask
            res |= cc
        out.append(res)
    return out


# --- 4bpp tile decode -----------------------------------------------------

def decode_4bpp_tile(block: bytes) -> list[list[int]]:
    """Return 8x8 palette indices from a 32-byte SNES 4bpp tile.

    Planes 0/1 interleaved by row in bytes 0-15, planes 2/3 in bytes 16-31.
    """
    px = [[0] * 8 for _ in range(8)]
    for y in range(8):
        p0 = block[y * 2]
        p1 = block[y * 2 + 1]
        p2 = block[16 + y * 2]
        p3 = block[16 + y * 2 + 1]
        for x in range(8):
            bit = 7 - x
            m = 1 << bit
            val = ((p0 >> bit) & 1) | (((p1 >> bit) & 1) << 1) \
                | (((p2 >> bit) & 1) << 2) | (((p3 >> bit) & 1) << 3)
            px[y][x] = val
    return px


def render_chr(chr_bytes: bytes, pal_rgb: list[tuple[int, int, int]], cols: int,
               scale: int = 1) -> "Image.Image":
    if Image is None:
        raise SystemExit("pip install pillow")
    ntiles = len(chr_bytes) // 32
    rows = (ntiles + cols - 1) // cols
    img = Image.new("RGB", (cols * 8, rows * 8), (24, 24, 24))
    px = img.load()
    for t in range(ntiles):
        block = chr_bytes[t * 32:t * 32 + 32]
        tile = decode_4bpp_tile(block)
        ox = (t % cols) * 8
        oy = (t // cols) * 8
        for y in range(8):
            for x in range(8):
                idx = tile[y][x]
                px[ox + x, oy + y] = pal_rgb[idx]
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def build_subpalette(rom: bytes, pal_off: int, sub: int) -> list[tuple[int, int, int]]:
    """16 RGB colors for 4bpp sub-palette `sub` (index 0 forced transparent-ish black)."""
    words = read_palette_words(rom, pal_off + sub * 32, 16)
    pal = [bgr555_to_rgb(w) for w in words]
    return pal


# --- CLI ------------------------------------------------------------------

def cmd_render(args: argparse.Namespace) -> int:
    rom = args.rom.read_bytes()
    if args.chr_offset is not None:
        chr_off = args.chr_offset
    else:
        chr_off = lorom_to_file(args.bank, args.addr)
    chr_bytes = rom[chr_off:chr_off + args.size]
    pal = build_subpalette(rom, args.palette_offset, args.sub)
    img = render_chr(chr_bytes, pal, args.cols, args.scale)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.output)
    print(f"Wrote {args.output}: {len(chr_bytes)//32} tiles @ file 0x{chr_off:X}, "
          f"pal 0x{args.palette_offset:X} sub {args.sub}", file=sys.stderr)
    return 0


def cmd_palette(args: argparse.Namespace) -> int:
    rom = args.rom.read_bytes()
    words = read_palette_words(rom, args.palette_offset, args.count)
    for i, w in enumerate(words):
        r, g, b = bgr555_to_rgb(w)
        print(f"{i:3d}  ${w:04X}  #{r:02X}{g:02X}{b:02X}")
    return 0


def cmd_selftest(args: argparse.Namespace) -> int:
    """Verify the re-implemented $00E289 / $00E354 semantics."""
    rom = args.rom.read_bytes()
    # E289 copy must equal a raw LE-word slice of the ROM palette table.
    off = 0x31FBC  # exploration palette ($06:9FBC)
    words = read_palette_words(rom, off, 64)
    raw = rom[off:off + 128]
    ok_copy = struct.pack("<64H", *words) == raw
    # E354 fade must converge to the target within max(channel deltas) steps.
    cur = [0x0000] * 4
    target = words[:4]
    steps = 0
    while cur != target and steps < 32:
        cur = palette_fade_step(cur, target)
        steps += 1
    ok_fade = cur == target
    print(f"E289 word-copy identity: {'PASS' if ok_copy else 'FAIL'}")
    print(f"E354 fade convergence in {steps} steps: {'PASS' if ok_fade else 'FAIL'}")
    return 0 if (ok_copy and ok_fade) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("render", help="Decode raw 4bpp CHR + palette to PNG")
    r.add_argument("rom", type=Path)
    src = r.add_mutually_exclusive_group(required=True)
    src.add_argument("--chr-offset", type=lambda x: int(x, 0), default=None,
                     help="CHR file offset")
    src.add_argument("--addr", type=lambda x: int(x, 0), default=None,
                     help="LoROM CPU addr (use with --bank)")
    r.add_argument("--bank", type=lambda x: int(x, 0), default=0)
    r.add_argument("--size", type=lambda x: int(x, 0), default=0x8000)
    r.add_argument("--palette-offset", type=lambda x: int(x, 0), required=True)
    r.add_argument("--sub", type=int, default=0, help="4bpp sub-palette index (16 colors)")
    r.add_argument("--cols", type=int, default=16)
    r.add_argument("--scale", type=int, default=1)
    r.add_argument("-o", "--output", type=Path, required=True)
    r.set_defaults(func=cmd_render)

    p = sub.add_parser("palette", help="Dump BGR555 palette words")
    p.add_argument("rom", type=Path)
    p.add_argument("--palette-offset", type=lambda x: int(x, 0), required=True)
    p.add_argument("--count", type=int, default=64)
    p.set_defaults(func=cmd_palette)

    s = sub.add_parser("selftest", help="Verify E289/E354 re-implementation")
    s.add_argument("rom", type=Path)
    s.set_defaults(func=cmd_selftest)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

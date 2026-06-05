#!/usr/bin/env python3
"""Shared helpers for the sci-fi town reskin (decode + index->material remap).

The reskin is a PURE PALETTE SWAP: every pixel keeps its original palette
index (so silhouettes and shading are byte-exact), and only the colours of the
indices the tileset actually uses are changed. The indices the tileset does NOT
use are left byte-identical so the .anm sprite overlays composited over the 3D
view keep their reserved palette slots.

Measured usage across town.32 / townf.32 / townt.32:
  USED  = {0,1,2,3,18,19,22,23,24,25,26,27,28,29,30,31}  (16 slots)
  FREE  = {4..17, 20, 21}  (16 slots, reserved for .anm overlays)
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Palette indices the town tileset actually paints (everything else is free).
USED_INDICES = {0, 1, 2, 3, 18, 19, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}

# index -> new sci-fi RGB (Amiga 4-bit/channel grid). Only USED indices appear
# here; FREE indices are intentionally absent and stay as the original palette.
#   walls (blue-greys 22/29/30/31 + black seam 3)  -> steel-metal ramp
#   doors/floor/pole (browns 26/27/28)             -> teal tech-deck ramp
#   torch flame (1/2/18/19) + sconce (23/24/25)    -> cyan emissive light
SCIFI_REMAP: dict[int, tuple[int, int, int]] = {
    0: (0, 0, 0),        # transparency key (unchanged)
    3: (0, 0, 0),        # black panel-seam / outline (unchanged)
    # metal bulkhead ramp
    22: (34, 34, 68),    # wall dark
    29: (85, 102, 136),  # wall mid-dark
    30: (153, 170, 187), # wall mid-light
    31: (204, 221, 238), # wall highlight
    # teal tech-deck ramp (doors / floor / torch pole)
    26: (17, 34, 34),    # deck dark
    27: (34, 68, 68),    # deck mid
    28: (85, 119, 119),  # deck light
    # cyan emissive light (torch flame core -> outer)
    1: (255, 255, 255),  # light core / spec
    18: (221, 255, 255), # flame core -> light core
    19: (153, 238, 255), # flame mid  -> bright cyan
    2: (51, 204, 238),   # flame outer-> cyan
    # cyan sconce / fixture body (was green)
    25: (51, 204, 238),  # sconce bright
    24: (34, 153, 187),  # sconce mid
    23: (17, 102, 136),  # sconce dark
}


def u16be(b: bytes, off: int) -> int:
    return (b[off] << 8) | b[off + 1]


def decode_planes(data: bytes, off: int, frame_bytes: int) -> tuple[int, bytes]:
    out = bytearray()
    have = False
    pending = 0
    cur = off
    while len(out) < frame_bytes and cur < len(data):
        p = data[cur]
        cur += 1
        cmd = p & 0xF0
        if cmd in (0x00, 0xF0):
            nib = (p >> 4) & 0xF
            times = (p & 0xF) + 1
            for _ in range(times):
                if len(out) >= frame_bytes:
                    break
                if not have:
                    pending = nib
                    have = True
                else:
                    out.append((pending << 4) | nib)
                    have = False
        else:
            for nib in ((p >> 4) & 0xF, p & 0xF):
                if len(out) >= frame_bytes:
                    break
                if not have:
                    pending = nib
                    have = True
                else:
                    out.append((pending << 4) | nib)
                    have = False
    return cur, bytes(out)


def read_palette(b: bytes) -> list[tuple[int, int, int]]:
    n = u16be(b, 0)
    pal_off = 4 + n * 6
    pal = []
    for i in range(32):
        pw = u16be(b, pal_off + i * 2)
        pal.append((((pw >> 8) & 0xF) * 17, ((pw >> 4) & 0xF) * 17, (pw & 0xF) * 17))
    return pal


class Frame:
    __slots__ = ("w", "h", "flags", "idx")

    def __init__(self, w: int, h: int, flags: int, idx: list[int]):
        self.w = w
        self.h = h
        self.flags = flags
        self.idx = idx  # row-major palette indices (length w*h)


def decode_sheet_indexed(path: Path) -> tuple[list[Frame], list[tuple[int, int, int]], int]:
    """Decode a .32 sheet file to indexed frames + palette + depth field."""
    return decode_bytes_indexed(path.read_bytes())


def decode_bytes_indexed(b: bytes) -> tuple[list[Frame], list[tuple[int, int, int]], int]:
    """Decode raw .32 bytes to indexed frames + palette + depth field."""
    n = u16be(b, 0)
    depth = u16be(b, 2)
    info = 4
    pal = read_palette(b)
    cur = info + n * 6 + 64
    frames: list[Frame] = []
    for f in range(n):
        w = u16be(b, info + f * 6)
        h = u16be(b, info + f * 6 + 2)
        flags = u16be(b, info + f * 6 + 4)
        bpr = ((w + 15) >> 3) & 0xFFFE
        rs = h * bpr
        cur, planes = decode_planes(b, cur, 5 * rs)
        idx = [0] * (w * h)
        for y in range(h):
            for x in range(w):
                v = 0
                for pl in range(5):
                    bp = pl * rs + y * bpr + (x >> 3)
                    v |= ((planes[bp] >> (7 - (x & 7))) & 1) << pl
                idx[y * w + x] = v
        frames.append(Frame(w, h, flags, idx))
    return frames, pal, depth


def build_scifi_palette(orig: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    """Copy original palette, recolour only USED indices via SCIFI_REMAP."""
    pal = list(orig)
    for i, rgb in SCIFI_REMAP.items():
        pal[i] = rgb
    return pal

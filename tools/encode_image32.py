#!/usr/bin/env python3
"""Planar .32 image-sheet encoder (inverse of decode_planes / load_frame).

Container layout (see EXTRACTED/docs CLAUDE notes + Gfx.h):
  u16be frame_count
  u16be depth/mode
  frame_count * { u16be w, u16be h, u16be flags }
  32 * u16be palette   (Amiga 0x0RGB, 4 bits/channel)
  per frame: nibble-RLE stream of 5 concatenated bitplanes
             (rs = h * (((w+15)>>3) & 0xFFFE) bytes per plane)

Nibble-RLE: the decoder reads a flat nibble stream and reassembles bytes from
consecutive nibbles. A command byte `p`:
  (p & 0xF0) in {0x00, 0xF0}  -> run: nibble (p>>4) repeated (p&0xF)+1 times
                                 (so runs only encode nibble 0x0 or 0xF)
  otherwise                   -> two literal nibbles (p>>4), (p&0xF)
                                 (high nibble is data, must be 0x1..0xE)

This encoder reproduces the exact nibble stream: runs for 0x0/0xF nibbles,
literal pairs for everything else. A trailing literal pad nibble is harmless
because the decoder stops once a frame's byte budget is filled.

Usage:
  python tools/encode_image32.py --roundtrip       # validate vs all *.32
  (import encode_image32 for programmatic encoding)
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

from scifi_town_common import ROOT, Frame, decode_bytes_indexed

NON_GFX = {"globe.32", "disk.32"}  # XOR blobs, not image chunks


def pack_rgb12(r: int, g: int, b: int) -> int:
    return ((r // 17) << 8) | ((g // 17) << 4) | (b // 17)


def frame_to_planes(fr: Frame) -> bytes:
    """Pack indexed pixels into 5 concatenated Amiga bitplanes."""
    bpr = ((fr.w + 15) >> 3) & 0xFFFE
    rs = fr.h * bpr
    planes = bytearray(5 * rs)
    for y in range(fr.h):
        row = y * fr.w
        for x in range(fr.w):
            v = fr.idx[row + x]
            if v == 0:
                continue
            byte_off = y * bpr + (x >> 3)
            bit = 7 - (x & 7)
            for pl in range(5):
                if (v >> pl) & 1:
                    planes[pl * rs + byte_off] |= 1 << bit
    return bytes(planes)


def rle_encode_nibbles(stream: bytes) -> bytes:
    """Encode a byte stream to the .32 nibble-RLE format."""
    # Flatten to nibbles (hi, lo per byte).
    nibs = bytearray(len(stream) * 2)
    for i, byte in enumerate(stream):
        nibs[2 * i] = (byte >> 4) & 0xF
        nibs[2 * i + 1] = byte & 0xF
    out = bytearray()
    i = 0
    n = len(nibs)
    while i < n:
        v = nibs[i]
        if v == 0 or v == 0xF:
            j = i + 1
            while j < n and nibs[j] == v and (j - i) < 16:
                j += 1
            out.append((v << 4) | (j - i - 1))
            i = j
        else:
            lo = nibs[i + 1] if i + 1 < n else 0
            out.append((v << 4) | lo)
            i += 2
    return bytes(out)


def encode_image32(frames: list[Frame], palette: list[tuple[int, int, int]], depth: int) -> bytes:
    """Build a complete .32 sheet from indexed frames + 32-colour palette."""
    if len(palette) != 32:
        raise ValueError(f"palette must have 32 entries, got {len(palette)}")
    out = bytearray()
    out += struct.pack(">H", len(frames))
    out += struct.pack(">H", depth)
    for fr in frames:
        out += struct.pack(">HHH", fr.w, fr.h, fr.flags)
    for (r, g, b) in palette:
        out += struct.pack(">H", pack_rgb12(r, g, b))
    for fr in frames:
        out += rle_encode_nibbles(frame_to_planes(fr))
    return bytes(out)


def _frames_equal(a: list[Frame], b: list[Frame]) -> bool:
    if len(a) != len(b):
        return False
    for fa, fb in zip(a, b):
        if (fa.w, fa.h, fa.flags) != (fb.w, fb.h, fb.flags):
            return False
        if fa.idx != fb.idx:
            return False
    return True


def roundtrip() -> int:
    """Decode -> encode -> decode every real .32 sheet; assert pixel-identical."""
    sheets = sorted(p for p in ROOT.glob("*.32") if p.name not in NON_GFX)
    failures = 0
    for p in sheets:
        try:
            frames, pal, depth = decode_bytes_indexed(p.read_bytes())
        except Exception as e:  # noqa: BLE001
            print(f"SKIP  {p.name}: decode failed ({e})")
            continue
        if not frames:
            print(f"SKIP  {p.name}: no image chunk at offset 0")
            continue
        try:
            enc = encode_image32(frames, pal, depth)
            frames2, pal2, depth2 = decode_bytes_indexed(enc)
        except Exception as e:  # noqa: BLE001
            print(f"FAIL  {p.name}: encode/redecode raised {e}")
            failures += 1
            continue
        ok = _frames_equal(frames, frames2) and pal == pal2 and depth == depth2
        status = "OK  " if ok else "FAIL"
        ratio = len(enc) / max(1, p.stat().st_size)
        print(f"{status}  {p.name:14s} frames={len(frames):3d} "
              f"orig={p.stat().st_size:6d} enc={len(enc):6d} ({ratio:.2f}x)")
        if not ok:
            failures += 1
    print()
    if failures:
        print(f"ROUND-TRIP FAILED for {failures} sheet(s)")
    else:
        print("ROUND-TRIP OK for all sheets (pixel-identical)")
    return 1 if failures else 0


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--roundtrip", action="store_true", help="validate against all *.32")
    args = ap.parse_args()
    if args.roundtrip:
        sys.exit(roundtrip())
    ap.print_help()


if __name__ == "__main__":
    main()

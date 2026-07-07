#!/usr/bin/env python3
"""Genesis MM2 custom LZ decompressor / encoder (from 68000 @ 0x29954).

Codec is Haruhiko Okumura LZSS, confirmed bit-exact against
``EXTRACTED/genesis/asm/resource_29954.asm``:

- 4096-byte (0x1000) ring buffer, index mask 0xFFF (``ANDI.W #$FFF``).
- Ring pre-filled with 0x20 (spaces) for the first 0xFEE bytes; write
  pointer starts at r = 0xFEE (``MOVE.L #$FEE,D1`` @ 0x299A6).
- Control flag stream: 8 bits LSB-first, refilled via ``ORI.W #$FF00``
  (@ 0x299BE). Flag bit 1 = literal byte, flag bit 0 = match token.
- Match token = 2 bytes (b0, b1):
    offset (12-bit) = b0 | ((b1 & 0xF0) << 4)
    length          = (b1 & 0x0F) + 3
  The 68k inner loop (@ 0x299F6) copies ``d4 + 1`` bytes where
  ``d4 = (b1 & 0x0F) + 2``, i.e. length = (b1 & 0x0F) + 3 (THRESHOLD 2,
  minimum match 3). Copy source is ring[(offset + k) & 0xFFF]; each copied
  byte is also written back into the ring at the advancing write pointer,
  so overlapping (RLE-style) matches work.

Header (8 bytes, big-endian), skipped by ``ADDQ #8,A0`` @ 0x299A4:
  +0  u32 metadata (not a stream pointer for 0x29954)
  +4  u32 decompressed output byte count (the D0 loop counter)
  +8  inline LZ bitstream
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

RING_SIZE = 0x1000
RING_MASK = 0xFFF
RING_FILL = 0x20
RING_START = 0xFEE
MIN_MATCH = 3
MAX_MATCH = 18  # (0x0F) + 3
THRESHOLD = 2


def _new_ring() -> bytearray:
    ring = bytearray(RING_SIZE)
    for j in range(RING_START):
        ring[j] = RING_FILL
    return ring


def decompress_lz(src: bytes, out_size: int | None = None, skip: int = 8) -> bytes:
    """Decompress an MM2 Genesis LZ stream (mirrors 68000 @ 0x29954)."""
    i = skip
    if out_size is None:
        if skip >= 8 and len(src) >= 8:
            out_size = struct.unpack_from(">I", src, 4)[0]
        else:
            out_size = len(src) * 16

    out = bytearray()
    ring = _new_ring()
    r = RING_START  # D1

    flags = 0  # D6
    while len(out) < out_size and i < len(src):
        flags >>= 1
        if (flags & 0x100) == 0:
            if i >= len(src):
                break
            flags = src[i] | 0xFF00
            i += 1

        if flags & 1:
            # literal
            if i >= len(src):
                break
            b = src[i]
            i += 1
            out.append(b)
            ring[r] = b
            r = (r + 1) & RING_MASK
        else:
            # match token
            if i + 1 >= len(src):
                break
            b0 = src[i]
            b1 = src[i + 1]
            i += 2
            offset = b0 | ((b1 & 0xF0) << 4)
            length = (b1 & 0x0F) + MIN_MATCH
            for k in range(length):
                if len(out) >= out_size:
                    break
                b = ring[(offset + k) & RING_MASK]
                out.append(b)
                ring[r] = b
                r = (r + 1) & RING_MASK
    return bytes(out)


def compress_lz(data: bytes) -> bytes:
    """Greedy LZSS encoder that round-trips through :func:`decompress_lz`.

    Produces an inline stream *without* the 8-byte header (payload only).
    Not byte-identical to EA's original packer, but decodes to *data*
    exactly, exercising both the literal and match paths as a self-test.
    """
    ring = _new_ring()
    r = RING_START
    n = len(data)
    pos = 0

    flag_bits: list[int] = []
    tokens: list[bytes] = []

    def _emit_to_ring(b: int) -> None:
        nonlocal r
        ring[r] = b
        r = (r + 1) & RING_MASK

    while pos < n:
        best_len = 0
        best_off = 0
        max_len = min(MAX_MATCH, n - pos)
        if max_len >= MIN_MATCH:
            # Search every ring position. Verify each candidate by simulating
            # the decoder exactly (copy ring[(off+k)] then write it back to the
            # advancing write pointer) so RLE-style overlapping matches stay
            # correct.
            for off in range(RING_SIZE):
                if data[pos] != ring[off & RING_MASK]:
                    continue
                sim = bytearray(ring)
                sim_r = r
                length = 0
                while length < max_len:
                    val = sim[(off + length) & RING_MASK]
                    if val != data[pos + length]:
                        break
                    sim[sim_r] = val
                    sim_r = (sim_r + 1) & RING_MASK
                    length += 1
                if length > best_len:
                    best_len = length
                    best_off = off
                    if best_len == max_len:
                        break

        if best_len >= MIN_MATCH:
            flag_bits.append(0)
            b0 = best_off & 0xFF
            b1 = ((best_off >> 4) & 0xF0) | ((best_len - MIN_MATCH) & 0x0F)
            tokens.append(bytes((b0, b1)))
            for k in range(best_len):
                _emit_to_ring(data[pos + k])
            pos += best_len
        else:
            flag_bits.append(1)
            tokens.append(bytes((data[pos],)))
            _emit_to_ring(data[pos])
            pos += 1

    # Pack: one control byte (LSB-first) per 8 tokens, then those tokens.
    out = bytearray()
    ti = 0
    for base in range(0, len(flag_bits), 8):
        group = flag_bits[base : base + 8]
        ctrl = 0
        for bitpos, bit in enumerate(group):
            ctrl |= (bit & 1) << bitpos
        out.append(ctrl)
        for _ in group:
            out += tokens[ti]
            ti += 1
    return bytes(out)


def read_resource(rom: bytes, off: int) -> tuple[int, int, bytes]:
    """Return (metadata, out_size, decoded) for the 8-byte-header table at *off*."""
    metadata, out_size = struct.unpack_from(">II", rom, off)
    payload = rom[off + 8 :]
    decoded = decompress_lz(rom[off:], out_size=out_size, skip=8)
    return metadata, out_size, decoded


def _self_test() -> int:
    import os

    cases = [
        b"",
        b"A",
        b"AAAAAAAAAAAAAAAA",
        b"the quick brown fox the quick brown fox jumps",
        bytes(range(256)) * 3,
        os.urandom(1024),
        (b"\x20" * 40) + os.urandom(200) + (b"\x20" * 40),
    ]
    ok = True
    for idx, data in enumerate(cases):
        enc = compress_lz(data)
        dec = decompress_lz(enc, out_size=len(data), skip=0)
        status = "ok" if dec == data else "FAIL"
        if dec != data:
            ok = False
        print(
            f"[self-test {idx}] len={len(data):5d} enc={len(enc):5d} {status}",
            file=sys.stderr,
        )
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path, nargs="?")
    ap.add_argument("--offset", type=lambda x: int(x, 0))
    ap.add_argument("--size", type=lambda x: int(x, 0), default=0)
    ap.add_argument("-o", "--output", type=Path)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return _self_test()

    if args.rom is None or args.offset is None:
        ap.error("rom and --offset are required unless --self-test")

    rom = args.rom.read_bytes()
    off = args.offset
    if args.size:
        chunk = rom[off : off + args.size]
        out = decompress_lz(chunk, args.size, skip=0)
    else:
        metadata, out_size, out = read_resource(rom, off)
        print(f"header: metadata=0x{metadata:X} out_size=0x{out_size:X}", file=sys.stderr)

    print(f"Decompressed 0x{off:X} -> {len(out)} bytes", file=sys.stderr)
    if args.output:
        args.output.write_bytes(out)
    else:
        sys.stdout.buffer.write(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Trace native console gfx routines in SNES / Genesis MM2 ROMs."""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

# SNES I/O (address lo, address hi) little-endian in 65816 absolute addressing
SNES_IO = {
  0x2100: "INIDISP",
  0x2105: "BGMODE",
  0x2107: "BG1SC",
  0x2108: "BG2SC",
  0x2109: "BG3SC",
  0x210A: "BG4SC",
  0x2115: "VMAIN",
  0x2116: "VMADDL",
  0x2117: "VMADDH",
  0x2118: "VMDATAL",
  0x2119: "VMDATAH",
  0x2121: "CGADD",
  0x2122: "CGDATA",
  0x212C: "TM",
  0x4200: "NMITIMEN",
  0x420B: "MDMAEN",
  0x4300: "DMAP0",
  0x4301: "BBAD0",
  0x4302: "A1T0",
  0x4303: "A1B0",
  0x4304: "A1M0",
  0x4305: "DAS0",
  0x4306: "DAS1",
  0x4330: "DMAP4",
  0x4334: "A1T4",
}

GENESIS_VDP_REGS = list(range(0x80, 0x94))


def lorom_file(pc: int) -> int:
    bank = (pc >> 16) & 0x7F
    addr = pc & 0xFFFF
    return (bank << 15) | (addr & 0x7FFF)


def scan_snes_io(data: bytes) -> dict:
    hits: dict[str, list[str]] = {}
    for addr, name in SNES_IO.items():
        lo, hi = addr & 0xFF, (addr >> 8) & 0xFF
        pat = bytes([0x8D, lo, hi])  # STA abs
        stz = bytes([0x9C, lo, hi])  # STZ abs
        stx = bytes([0x8E, lo, hi])  # STX abs
        for label, p in (("STA", pat), ("STZ", stz), ("STX", stx)):
            offs = [i for i in range(len(data) - 3) if data[i : i + 3] == p]
            if offs:
                key = f"{label}_{name}"
                hits[key] = [hex(o) for o in offs[:24]]
    return hits


def disasm_window(data: bytes, off: int, before: int = 48, after: int = 64) -> str:
    start = max(0, off - before)
    chunk = data[start : off + after]
    lines = []
    for i in range(0, len(chunk), 16):
        addr = start + i
        row = chunk[i : i + 16]
        asc = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        mark = " <<" if start + i <= off < start + i + 16 else ""
        lines.append(f"  {addr:06X}: {row.hex():32s}  {asc}{mark}")
    return "\n".join(lines)


def trace_snes_vram_sites(data: bytes) -> list[dict]:
    sites = []
    pat = bytes([0x8D, 0x18, 0x21])  # STA $2118
    for off in range(len(data) - 3):
        if data[off : off + 3] != pat:
            continue
        ctx_start = max(0, off - 64)
        ctx = data[ctx_start:off]
        # look for immediate loads before STA: A9 xx (LDA #imm), A2 xx (LDX #imm)
        imms = []
        i = len(ctx) - 1
        while i >= 1:
            op = ctx[i - 1]
            if op in (0xA9, 0xA2, 0xA0) and i < len(ctx):
                imms.append((op, ctx[i]))
            if op in (0x20, 0x4C, 0x60, 0x6C):  # JSR/JMP/RTS/JMP indirect
                break
            i -= 1
        imms_list = []
        for op, val in imms[-6:]:
            imms_list.append(
                {"reg": {0xA9: "LDA", 0xA2: "LDX", 0xA0: "LDY"}.get(op, hex(op)), "value": hex(val)}
            )
        sites.append(
            {
                "file_offset": hex(off),
                "preceding_immediates": imms_list,
                "hex_window": disasm_window(data, off),
            }
        )
    return sites


def scan_snes_dma(data: bytes) -> list[dict]:
    """Find DMA register block writes ($4300-$437F)."""
    blocks = []
    # STA $43xx patterns
    for ch in range(8):
        base = 0x4300 + ch * 0x10
        for reg_off, name in (
            (0, "DMAP"),
            (1, "BBAD"),
            (2, "A1T"),
            (3, "A1B"),
            (4, "A1M"),
            (5, "DASL"),
            (6, "DASH"),
        ):
            addr = base + reg_off
            pat = bytes([0x8D, addr & 0xFF, 0x43])
            for off in range(len(data) - 3):
                if data[off : off + 3] == pat:
                    blocks.append({"channel": ch, "reg": name, "file_offset": hex(off)})
    return blocks[:80]


def find_snes_palette_blocks(data: bytes, limit: int = 20) -> list[dict]:
    """32-color CGRAM blocks (64 bytes LE BGR555)."""
    cands = []
    for i in range(0, len(data) - 64, 2):
        words = [struct.unpack_from("<H", data, i + j)[0] for j in range(0, 64, 2)]
        if not all(w < 0x8000 for w in words):
            continue
        uniq = len(set(words))
        if uniq < 12:
            continue
        # reject all-grey ramps that are likely not palettes
        rgbs = [(w & 0x1F, (w >> 5) & 0x1F, (w >> 10) & 0x1F) for w in words]
        spread = max(rgbs) if rgbs else (0, 0, 0)
        if max(spread) - min(spread) < 2 and uniq < 20:
            continue
        cands.append({"offset": hex(i), "unique_colors": uniq, "first8": [hex(w) for w in words[:8]]})
        if len(cands) >= limit:
            break
    return cands


def scan_genesis_vdp(data: bytes) -> dict:
    """Find move.l #imm, d0 / move.w #imm, (a5) style VDP setup."""
    routines = []
    # Common: 0x3F3C 0x0000 0xC000 -> MOVE.W #$3F3C, $C00000 (VDP control)
  # In 68k: 33FC xxxx C000 00xx
    for off in range(len(data) - 8):
        if data[off] == 0x33 and data[off + 1] == 0xFC:  # MOVE.W #imm, (xxx).L
            imm = struct.unpack_from(">H", data, off + 2)[0]
            addr = struct.unpack_from(">I", data, off + 4)[0]
            if (addr & 0xFFFF0000) in (0xC0000000, 0x00C00000):
                routines.append(
                    {
                        "file_offset": hex(off),
                        "value": hex(imm),
                        "dest": hex(addr),
                        "vdp_reg": hex(imm >> 8) if addr & 0xFFFF == 4 else "data",
                    }
                )
    return {"vdp_move_w": routines[:40]}


def find_genesis_dma(data: bytes) -> list[dict]:
    hits = []
    # MOVE.L (An)+, $C00000  or similar VDP DMA from 68k
    for off in range(len(data) - 6):
        # 20 BC 00 C0 00 00 pattern variants
        if data[off : off + 2] in (bytes([0x20, 0xBC]), bytes([0x21, 0xC0])):
            addr = struct.unpack_from(">I", data, off + 2)[0] if off + 6 <= len(data) else 0
            if addr in (0xC00000, 0x00C00000):
                hits.append({"file_offset": hex(off), "pattern": data[off : off + 6].hex()})
    return hits[:30]


def kosinski_decompress(src: bytes, max_out: int = 0x10000) -> bytes | None:
    """Minimal Kosinski decompressor; returns None on failure."""
    out = bytearray()
    i = 0
    bitbuf = 0
    bits = 0

    def refill() -> bool:
        nonlocal bitbuf, bits, i
        if i >= len(src):
            return False
        bitbuf |= src[i] << bits
        i += 1
        bits += 8
        return True

    def get_bit() -> int | None:
        nonlocal bitbuf, bits
        if bits == 0 and not refill():
            return None
        v = bitbuf & 1
        bitbuf >>= 1
        bits -= 1
        return v

    try:
        while len(out) < max_out:
            if get_bit():
                b = get_bit()
                if b is None:
                    break
                if b:
                    b2 = get_bit()
                    if b2 is None:
                        break
                    if b2:
                        b3 = get_bit()
                        if b3 is None:
                            break
                        if not b3:
                            if bits < 8 and not refill():
                                break
                            out.append(bitbuf & 0xFF)
                            bitbuf >>= 8
                            bits -= 8
                        else:
                            if bits < 12 and not refill():
                                break
                            val = bitbuf & 0xFFF
                            bitbuf >>= 12
                            bits -= 12
                            if val == 0xFFF:
                                break
                            count = (val >> 8) + 3
                            offset = ((val & 0xFF) << 4) | 0
                            if not offset:
                                continue
                            for _ in range(count):
                                out.append(out[-offset] if offset <= len(out) else 0)
                    else:
                        for _ in range(3):
                            if bits < 8 and not refill():
                                break
                            out.append(bitbuf & 0xFF)
                            bitbuf >>= 8
                            bits -= 8
                else:
                    if bits < 8 and not refill():
                        break
                    out.append(bitbuf & 0xFF)
                    bitbuf >>= 8
                    bits -= 8
            else:
                if bits < 16 and not refill():
                    break
                desc = bitbuf & 0xFFFF
                bitbuf >>= 16
                bits -= 16
                if desc == 0:
                    break
                offset = ((desc >> 8) | ((desc & 0xF0) << 4)) + 1
                length = (desc & 0xF) + 2
                if offset > len(out):
                    return None
                for _ in range(length):
                    out.append(out[-offset])
    except Exception:
        return None
    return bytes(out) if len(out) >= 32 else None


def try_genesis_decompress(data: bytes, starts: list[int]) -> list[dict]:
    results = []
    for off in starts:
        chunk = data[off : off + 0x10000]
        out = kosinski_decompress(chunk)
        if out and len(out) >= 64:
            tile_like = sum(
                1
                for t in range(0, min(len(out), 0x800) - 32, 32)
                if max((b >> 4) & 0xF for b in out[t : t + 32]) <= 0xF
            )
            results.append(
                {
                    "offset": hex(off),
                    "codec": "kosinski_trial",
                    "out_size": len(out),
                    "tile_like_32b_in_first_2k": tile_like,
                    "head": out[:32].hex(),
                }
            )
    return results


def analyze_snes(path: Path) -> dict:
    data = path.read_bytes()
    return {
        "path": str(path),
        "kind": "snes",
        "io_register_hits": scan_snes_io(data),
        "vram_upload_sites": trace_snes_vram_sites(data),
        "dma_register_writes": scan_snes_dma(data),
        "palette_candidates": find_snes_palette_blocks(data, 12),
    }


def analyze_genesis(path: Path) -> dict:
    data = path.read_bytes()
    vdp = scan_genesis_vdp(data)
    # trial kosinski at entropy chunk starts + aligned boundaries
    starts = [0x30000, 0x40000, 0x50000, 0x60000, 0x70000]
    starts += [0x40000 + i for i in range(0, 0x1000, 0x100)]
    decomp = try_genesis_decompress(data, starts)
    return {
        "path": str(path),
        "kind": "genesis",
        "vdp_setup": vdp,
        "vdp_dma_patterns": find_genesis_dma(data),
        "kosinski_trials": decomp[:20],
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    args = ap.parse_args()

    suffix = args.rom.suffix.lower()
    result = analyze_snes(args.rom) if suffix in {".sfc", ".smc"} else analyze_genesis(args.rom)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Wrote {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

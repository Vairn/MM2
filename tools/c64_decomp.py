#!/usr/bin/env python3
"""MM2-1 C64 boot RELOCATION ($03B7 / chain $0847).

CORRECTION (2026-07-07): this routine is a block MEMMOVE, not decompression.
See tools/c64_verify_relocation.py and EXTRACTED/docs/c64-graphics.md.

The routine at $03B7 (copied from $0845) copies 256-byte windows using
self-modifying absolute,X operands at $03BF-$03C0 (src) and $03C2-$03C3
(dst). Each outer pass INCs those high-byte operand bytes until $03C0 == $FF.
Because src-dst is a constant $90 across all 247 passes, the effect is a
straight relocation of the payload DOWN by 144 bytes (no codec).

Verified byte-exact against the chain (every relocated byte == chain byte+$90).
"""
from __future__ import annotations


def copy_block(mem: bytearray, src: int, dst: int, n: int) -> None:
    for i in range(n):
        mem[dst + i] = mem[src + i]


def apply_boot_init(mem: bytearray) -> None:
    """Mirror $081E: IEC stub $086B->$0400, loader $0845->$03B7, then JMP $03B7.

    The copy includes two tail bytes of the preceding ``JMP $03B7`` ($B7,$03).
    Real hardware enters at $03B7 on those bytes; offline decomp uses the
    aligned block at $0847 (SEI … JMP $080B) copied to $03B7 — same layout the
    INC $03C0 / INC $03C3 self-modify operands expect.
    """
    copy_block(mem, 0x086B, 0x0400, 0x28)
    copy_block(mem, 0x0845, 0x03B7, 0xE0)
    # Re-align decomp routine so $03BE=LDA $0893,X and $03C0/$03C3 are operands.
    copy_block(mem, 0x0847, 0x03B7, 0x22)


def run_decomp(mem: bytearray, *, max_outer: int = 300) -> int:
    """Run decompression until $03C0 reaches $FF (same as $0860 CMP #$FF)."""
    outer = 0
    while mem[0x03C0] != 0xFF:
        src_base = mem[0x03BF] | (mem[0x03C0] << 8)
        dst_base = mem[0x03C2] | (mem[0x03C3] << 8)
        for x in range(256):
            mem[(dst_base + x) & 0xFFFF] = mem[(src_base + x) & 0xFFFF]
        mem[0x03C0] = (mem[0x03C0] + 1) & 0xFF
        mem[0x03C3] = (mem[0x03C3] + 1) & 0xFF
        outer += 1
        if outer > max_outer:
            raise RuntimeError(f"decomp did not finish (03C0={mem[0x03C0]:02X})")
    return outer


def decomp_skip_pc() -> int:
    """PC after decomp: LDA #$37 / STA $01 / CLI / JMP $080B ($0864 in chain)."""
    return 0x03D4

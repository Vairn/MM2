#!/usr/bin/env python3
"""Disassemble Sega Genesis / Mega Drive ROM images (68000) with Capstone."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import capstone
from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000


def disassemble(data: bytes, base: int, out, max_insns: int | None) -> int:
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
    md.detail = True
    count = 0
    for insn in md.disasm(data, base):
        hex_bytes = insn.bytes.hex()
        out.write(f"{insn.address:06x}  {hex_bytes:<16}  {insn.mnemonic:8}  {insn.op_str}\n")
        count += 1
        if max_insns is not None and count >= max_insns:
            out.write(f"; ... truncated after {max_insns} instructions\n")
            break
    return count


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path, help="Genesis .gen / .md / .bin ROM")
    ap.add_argument("--org", type=lambda x: int(x, 0), default=0, help="Base address (default 0)")
    ap.add_argument("--offset", type=lambda x: int(x, 0), default=0, help="File offset to start")
    ap.add_argument("--length", type=lambda x: int(x, 0), default=0, help="Bytes to disassemble (0=all)")
    ap.add_argument("-o", "--output", type=Path, help="Output .asm (default stdout)")
    ap.add_argument("--max-insns", type=int, default=None)
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    end = len(rom) if args.length == 0 else min(len(rom), args.offset + args.length)
    chunk = rom[args.offset:end]
    base = args.org + args.offset

    header = (
        f"; {args.rom.name}\n"
        f"; Capstone {capstone.__version__} — Motorola 68000 BE\n"
        f"; file offset 0x{args.offset:X}, base 0x{base:X}, {len(chunk)} bytes\n\n"
    )

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w", encoding="utf-8", newline="\n") as f:
            f.write(header)
            n = disassemble(chunk, base, f, args.max_insns)
        print(f"Wrote {n} instructions to {args.output}", file=sys.stderr)
    else:
        sys.stdout.write(header)
        disassemble(chunk, base, sys.stdout, args.max_insns)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Trace MM2 spell picker/gate/dispatch from mm2.asm."""
from __future__ import annotations

import re
from pathlib import Path

from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000

ASM = Path(__file__).resolve().parents[1] / "EXTRACTED" / "mm2.asm"


def build_full_code_image() -> bytearray:
    """Merge IRA instruction bytes + DC gap bytes into one image."""
    text = ASM.read_text(encoding="utf-8", errors="replace")
    size = 0x248A6
    mem = bytearray(size)

    insn_re = re.compile(
        r"^\s+(?:[A-Z][A-Z0-9_.$+*/-]+|\t).+;\s*([0-9a-fA-F]+):\s*([0-9a-f]+)",
        re.I,
    )
    for line in text.splitlines():
        m = insn_re.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if addr >= size:
            continue
        hx = m.group(2)
        raw = bytes(int(hx[i : i + 2], 16) for i in range(0, len(hx), 2))
        for i, b in enumerate(raw):
            if addr + i < size:
                mem[addr + i] = b

    for line in text.splitlines():
        m = re.search(r";([0-9a-fA-F]+)(?::|\s*$)", line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if not (0x8000 <= addr < size):
            continue
        off = addr
        if "DC.L" in line:
            vals = re.findall(r"\$([0-9a-fA-F]+)", line.split("DC.L", 1)[1])
            for v in vals:
                raw = int(v, 16).to_bytes(4, "big")
                for i, b in enumerate(raw):
                    if off + i < size:
                        mem[off + i] = b
                off += 4
        elif "DC.W" in line:
            vals = re.findall(r"\$([0-9a-fA-F]+)", line.split("DC.W", 1)[1])
            for v in vals:
                raw = int(v, 16).to_bytes(2, "big")
                for i, b in enumerate(raw):
                    if off + i < size:
                        mem[off + i] = b
                off += 2
    return mem


def dump(addr: int, count: int = 40) -> None:
    mem = build_full_code_image()
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
    md.skipdata = True
    print(f"\n=== {addr:#06x} ===")
    n = 0
    for ins in md.disasm(bytes(mem[addr : addr + 0x240]), addr):
        print(f"{ins.address:06x}  {ins.bytes.hex():<12}  {ins.mnemonic:8}  {ins.op_str}")
        n += 1
        if n >= count:
            break


def main() -> None:
    for addr in (0x6E30, 0x11A90, 0x13730, 0xE260, 0xCDB8, 0xCFF8, 0xD266):
        dump(addr, 60)


if __name__ == "__main__":
    main()


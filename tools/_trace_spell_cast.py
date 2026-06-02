#!/usr/bin/env python3
"""Trace spell cast path from mm2.asm IRA gap bytes."""
from __future__ import annotations

import re
import struct
from pathlib import Path

from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000

ASM = Path(__file__).resolve().parents[1] / "EXTRACTED" / "mm2.asm"
SPELLS = Path(__file__).resolve().parents[1] / "tools" / "mm2_spells.py"


def build_gap_image() -> bytearray:
    text = ASM.read_text(encoding="utf-8", errors="replace")
    mem = bytearray(0x248A6 - 0x8000)
    for line in text.splitlines():
        m = re.search(r";([0-9a-fA-F]+)(?::|\s*$)", line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if addr < 0x8000 or addr >= 0x248A6:
            continue
        off = addr - 0x8000
        if "DC.L" in line:
            for v in re.findall(r"\$([0-9a-fA-F]+)", line.split("DC.L", 1)[1]):
                for i, b in enumerate(int(v, 16).to_bytes(4, "big")):
                    if off + i < len(mem):
                        mem[off + i] = b
                off += 4
        elif "DC.W" in line:
            for v in re.findall(r"\$([0-9a-fA-F]+)", line.split("DC.W", 1)[1]):
                for i, b in enumerate(int(v, 16).to_bytes(2, "big")):
                    if off + i < len(mem):
                        mem[off + i] = b
                off += 2
    return mem


def disasm_at(md: Cs, mem: bytearray, addr: int, count: int = 80) -> list:
    off = addr - 0x8000
    out = []
    for ins in md.disasm(bytes(mem[off : off + 0x400]), addr, count=count):
        out.append((ins.address, ins.mnemonic, ins.op_str, ins.bytes.hex()))
    return out


def find_jsr_a4_targets(mem: bytearray, lo: int, hi: int) -> dict[int, list[int]]:
    """Find JSR d(A4) in range; return disp -> [call sites]."""
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
    hits: dict[int, list[int]] = {}
    for addr in range(lo, hi, 2):
        off = addr - 0x8000
        if off < 0 or off + 6 > len(mem):
            continue
        w = struct.unpack_from(">H", mem, off)[0]
        # JSR d16(A4) = 4EAC xxxx
        if (w & 0xFFFF) == 0x4EAC and off + 2 < len(mem):
            disp = struct.unpack_from(">h", mem, off + 2)[0]
            hits.setdefault(disp, []).append(addr)
    return hits


def main() -> None:
    mem = build_gap_image()
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)

    # A4 disp as signed: -0x7E12 -> disp word 0x81EE
    targets = {
        "spell_cast_entry_-7E12": -0x7E12,
        "spell_precast_-7E4E": -0x7E4E,
        "read_key_-7BD2": -0x7BD2,
        "normalize_key_-7BCC": -0x7BCC,
    }
    print("=== JSR d(A4) call sites in combat/spell block 0x11000..0x14000 ===")
    for name, disp in targets.items():
        sites = []
        for addr in range(0x11000, 0x14000, 2):
            off = addr - 0x8000
            if off + 4 > len(mem):
                continue
            if mem[off : off + 2] == b"\x4e\xac":
                d = struct.unpack_from(">h", mem, off + 2)[0]
                if d == disp:
                    sites.append(addr)
        print(f"{name} (disp {disp:#06x}): {len(sites)} sites")
        for s in sites[:8]:
            print(f"  called from {s:#06x}")

    print("\n=== spell_cast_entry: resolve -$7E12 thunk (find routine body) ===")
    # Scan all JSR -7E12 in full gap; disasm first caller and callee
    disp = -0x7E12
    for addr in range(0x8000, 0x248A6, 2):
        off = addr - 0x8000
        if mem[off : off + 2] != b"\x4e\xac":
            continue
        if struct.unpack_from(">h", mem, off + 2)[0] != disp:
            continue
        if addr < 0x11000 or addr > 0x14000:
            continue
        print(f"\n--- caller @ {addr:#06x} ---")
        for a, mn, op, _ in disasm_at(md, mem, max(0x11000, addr - 0x20), 12):
            if a >= addr - 0x20:
                print(f"  {a:06x}  {mn:8}  {op}")

    # Find indirect: many thunks use JMP d(A4) or JSR via table
    print("\n=== Disasm around 0x11AAC (JSR -$7E12) ===")
    for a, mn, op, hx in disasm_at(md, mem, 0x11A80, 50):
        print(f"{a:06x}  {hx:<12}  {mn:8}  {op}")

    print("\n=== skill_spell_effect_dispatch ~0xCFF8 ===")
    for a, mn, op, hx in disasm_at(md, mem, 0xCFF0, 40):
        print(f"{a:06x}  {hx:<12}  {mn:8}  {op}")

    print("\n=== skill_spell_jump_executor ~0xD23E (search LINK nearby) ===")
    for start in range(0xD200, 0xD280, 2):
        off = start - 0x8000
        if mem[off : off + 2] == b"\x4e\x55":
            print(f"LINK at {start:#06x}")
            for a, mn, op, hx in disasm_at(md, mem, start, 25):
                print(f"  {a:06x}  {mn:8}  {op}")

    print("\n=== spell_effect_jump_table 0xD002 (first 12 entries) ===")
    for i in range(12):
        base = 0xD002 + i * 8
        off = base - 0x8000
        w0 = struct.unpack_from(">I", mem, off)[0]
        w1 = struct.unpack_from(">I", mem, off + 4)[0]
        # bra.w + jsr pc
        hi0, lo0 = (w0 >> 16) & 0xFFFF, w0 & 0xFFFF
        hi1, lo1 = (w1 >> 16) & 0xFFFF, w1 & 0xFFFF
        stub = None
        if hi1 == 0x4EBA:
            stub = base + 4 + struct.unpack(">h", lo1.to_bytes(2, "big"))[0]
        print(f"  [{i:2d}] @{base:#06x}: {w0:08x} {w1:08x}  stub~{stub:#x}" if stub else f"  [{i:2d}] @{base:#06x}: {w0:08x} {w1:08x}")

    print("\n=== DATA_spell_effect_offset_table 0xD186 (first 16 words) ===")
    for i in range(16):
        off = 0xD186 + i * 2 - 0x8000
        w = struct.unpack_from(">h", mem, off)[0]
        target = 0xD23E + w if w < 0 else None  # typical PC-rel from executor
        print(f"  idx {i:2d}: word {w:#06x} ({w})")


def scan_routine(addr: int, count: int = 40) -> None:
    mem = build_gap_image()
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
    print(f"\n=== {addr:#06x} ===")
    for a, mn, op, hx in disasm_at(md, mem, addr, count):
        print(f"{a:06x}  {hx:<12}  {mn:8}  {op}")


def find_all_jsr_a4(disp: int) -> list[int]:
    mem = build_gap_image()
    sites = []
    for addr in range(0x8000, 0x248A6, 2):
        off = addr - 0x8000
        if mem[off : off + 2] != b"\x4e\xac":
            continue
        if struct.unpack_from(">h", mem, off + 2)[0] == disp:  # disp is signed A4 offset
            sites.append(addr)
    return sites


if __name__ == "__main__":
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == "--scan":
        for label, d in [
            ("-7E12", -0x7E12),
            ("-7E4E", -0x7E4E),
            ("-7F20", -0x7F20),
            ("-6D88", -0x6D88),
            ("-702A", -0x702A),
        ]:
            sites = find_all_jsr_a4(d)
            print(f"JSR {label}: {len(sites)} sites")
            for s in sites:
                print(f"  {s:#06x}")
        for a in (0x13730, 0xCDB8, 0xD266, 0xD27E):
            scan_routine(a)
    else:
        main()

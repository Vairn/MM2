#!/usr/bin/env python3
"""Scan C64 MM2 code blobs for a graphics / VIC / codec path.

Looks for instructions that touch the VIC-II ($D000-$D02E), CIA2 serial
($DD00), colour RAM ($D800-$DBFF), or classic bitmap-RAM bases
($2000/$4000/$6000/$8000/$A000/$C000/$E000). Also flags $D011/$D016/$D018
which select hires/multicolor bitmap mode. Purely static byte-pattern based.

Run: python tools/c64_scan_gfx_routines.py
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Store/load absolute opcodes that carry a 16-bit operand (lo, hi).
ABS_OPS = {
    0x8D: "STA", 0x8E: "STX", 0x8C: "STY",
    0x9D: "STA,X", 0x99: "STA,Y",
    0xAD: "LDA", 0xAE: "LDX", 0xAC: "LDY",
    0xBD: "LDA,X", 0xB9: "LDA,Y",
    0x0D: "ORA", 0x2D: "AND", 0x4D: "EOR", 0x6D: "ADC",
    0xCD: "CMP", 0xED: "SBC", 0xEE: "INC", 0xCE: "DEC",
    0x2C: "BIT", 0x1D: "ORA,X", 0x0E: "ASL", 0x4E: "LSR",
}

VIC_NAMES = {
    0xD011: "D011 (Y-scroll/BMM/mode)",
    0xD016: "D016 (X-scroll/MCM)",
    0xD018: "D018 (screen+charset/bitmap base)",
    0xD020: "D020 (border)",
    0xD021: "D021 (bg0)",
    0xD01A: "D01A (irq enable)",
    0xD012: "D012 (raster)",
}

BITMAP_BASES = {0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xE000}


def classify(addr: int) -> str | None:
    if addr in VIC_NAMES:
        return f"VIC {VIC_NAMES[addr]}"
    if 0xD000 <= addr <= 0xD02E:
        return f"VIC $D0{addr & 0xFF:02X}"
    if 0xD400 <= addr <= 0xD418:
        return f"SID $D4{addr & 0xFF:02X}"
    if 0xD800 <= addr <= 0xDBFF:
        return f"colorRAM ${addr:04X}"
    if addr in (0xDD00, 0xDD01, 0xDD02, 0xDD03):
        return f"CIA2/IEC ${addr:04X}"
    if addr in (0xDC00, 0xDC01):
        return f"CIA1 ${addr:04X}"
    if (addr & 0x1FFF) == 0 and addr in BITMAP_BASES:
        return f"bitmap-base ${addr:04X}"
    return None


def scan(path: Path, base: int) -> list[str]:
    data = path.read_bytes()
    # PRG files start with 2-byte load addr; strip if base derived from it.
    hits: list[str] = []
    i = 0
    n = len(data)
    while i < n - 2:
        op = data[i]
        name = ABS_OPS.get(op)
        if name is not None:
            addr = data[i + 1] | (data[i + 2] << 8)
            tag = classify(addr)
            if tag is not None:
                loc = base + i
                hits.append(
                    f"    @${loc:04X} (off {i:5}): {name:6} ${addr:04X}  -> {tag}"
                )
        i += 1
    return hits


def main() -> None:
    targets = [
        ("relocated boot ($0800 base)",
         ROOT / "EXTRACTED/c64/emulator/mm2-1_relocated.bin", 0x0800),
        ("T19S0 chain (PRG body @ $0801)",
         ROOT / "EXTRACTED/c64/emulator/mm2-1_t19s0_chain.bin", 0x07FF),
        ("T22S9 chain",
         ROOT / "EXTRACTED/c64/emulator/mm2-1_t22s9_chain.bin", 0x0000),
    ]
    # Add all data-track blobs (no meaningful base; report file offset).
    for disk in sorted((ROOT / "EXTRACTED/c64/gfx/tracks").glob("mm2-*")):
        for blob in sorted(disk.glob("t*.bin")):
            targets.append((f"track {disk.name}/{blob.stem}", blob, 0x0000))

    for label, path, base in targets:
        if not path.exists():
            print(f"[missing] {label}: {path}")
            continue
        hits = scan(path, base)
        if hits:
            print(f"\n### {label}  ({len(hits)} gfx/IO refs)")
            # De-duplicate by (op,addr) but keep counts to avoid huge output.
            for line in hits[:40]:
                print(line)
            if len(hits) > 40:
                print(f"    ... {len(hits) - 40} more")


if __name__ == "__main__":
    main()

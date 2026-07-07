#!/usr/bin/env python3
"""Extract SNES DMA channel-0 setups (ROM/WRAM -> VRAM/CGRAM) from MM2 SFC."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

BBAD_NAMES = {
    0x18: "VRAM $2118 write",
    0x19: "VRAM $2119 write",
    0x22: "CGRAM $2122 write",
}

SNES_IO = {
    0x2116: "VMADDL",
    0x2117: "VMADDH",
    0x2121: "CGADD",
}


def decode_dma_cluster(data: bytes, base: int, size: int = 0x50) -> list[str]:
    chunk = data[base : base + size]
    lines: list[str] = []
    i = 0
    while i < len(chunk):
        op = chunk[i]
        if op == 0xA9 and i + 2 <= len(chunk):
            lines.append(f"LDA #{chunk[i + 1]:02X}")
            i += 2
        elif op == 0x8D and i + 3 <= len(chunk):
            addr = chunk[i + 1] | (chunk[i + 2] << 8)
            name = SNES_IO.get(addr, "")
            suffix = f" ; {name}" if name else ""
            lines.append(f"STA ${addr:04X}{suffix}")
            i += 3
        elif op == 0x9C and i + 3 <= len(chunk):
            addr = chunk[i + 1] | (chunk[i + 2] << 8)
            lines.append(f"STZ ${addr:04X}")
            i += 3
        else:
            i += 1
    return lines


def parse_cluster_values(data: bytes, base: int) -> dict[int, int]:
    """Return last immediate written to each $43xx register in cluster."""
    chunk = data[base : base + 0x50]
    regs: dict[int, int] = {}
    i = 0
    while i < len(chunk) - 4:
        if chunk[i] == 0xA9 and chunk[i + 2] == 0x8D and chunk[i + 4] == 0x43:
            regs[chunk[i + 3]] = chunk[i + 1]
            i += 2
        elif chunk[i] == 0x8D and chunk[i + 2] == 0x43:
            i += 3
        else:
            i += 1
    return regs


def parse_vram_dest(data: bytes, base: int) -> int | None:
    """Recover the VMADD word from `LDA #imm / STA $2116|$2117` in the cluster.

    The DMA setup precedes the `STA $4300` anchor, so scan a wide window before
    `base` and take the last VMADDL/VMADDH immediates.
    """
    chunk = data[max(0, base - 0x40) : base + 0x10]
    vmaddl = vmaddh = None
    i = 0
    while i < len(chunk) - 4:
        if chunk[i] == 0xA9 and chunk[i + 2] == 0x8D:
            addr = chunk[i + 3] | (chunk[i + 4] << 8)
            if addr == 0x2116:
                vmaddl = chunk[i + 1]
            elif addr == 0x2117:
                vmaddh = chunk[i + 1]
        i += 1
    if vmaddl is not None and vmaddh is not None:
        return (vmaddh << 8) | vmaddl
    return None


def resolve_dma_source(a1_low: int, a1_high: int, a1_bank: int, rom_size: int) -> dict:
    """Resolve a DMA source from A1T0L ($4302), A1T0H ($4303), A1B0 ($4304).

    24-bit source = A1B0:A1T0H:A1T0L. This ROM is LoROM: each bank maps its
    $8000-$FFFF half to a 32 KiB ROM page; $7E/$7F are WRAM; $00-$3F/$80-$BF
    with addr<$2000 alias WRAM $7E; $70-$7D:$0000-$7FFF is SRAM.
    """
    addr16 = (a1_high << 8) | a1_low
    addr24 = (a1_bank << 16) | addr16
    snes = f"${a1_bank:02X}:{addr16:04X}"

    # WRAM (direct)
    if a1_bank in (0x7E, 0x7F):
        return {
            "kind": "wram",
            "snes_addr": snes,
            "resolved_wram": f"${a1_bank:02X}:{addr16:04X}",
        }

    # WRAM low mirror ($00-$3F / $80-$BF : $0000-$1FFF -> $7E:0000-$1FFF)
    if addr16 < 0x2000 and ((a1_bank <= 0x3F) or (0x80 <= a1_bank <= 0xBF)):
        return {
            "kind": "wram",
            "snes_addr": snes,
            "wram_mirror": True,
            "resolved_wram": f"$7E:{addr16:04X}",
        }

    # LoROM ROM window: $8000-$FFFF in any bank.
    if addr16 >= 0x8000:
        bank = a1_bank & 0x7F
        file_off = (bank << 15) | (addr16 & 0x7FFF)
        if file_off < rom_size:
            return {"kind": "rom", "snes_addr": snes, "file_offset": hex(file_off)}
        return {"kind": "rom_oob", "snes_addr": snes, "addr24": hex(addr24)}

    # SRAM ($70-$7D:$0000-$7FFF) or IO/other low addresses.
    if 0x70 <= a1_bank <= 0x7D:
        return {"kind": "sram", "snes_addr": snes, "addr24": hex(addr24)}

    return {"kind": "other", "snes_addr": snes, "addr24": hex(addr24)}


def find_dma_setups(data: bytes) -> list[dict]:
    setups: list[dict] = []
    anchors = (bytes([0x8D, 0x00, 0x43]), bytes([0x9C, 0x00, 0x43]))
    for base in range(len(data) - 40):
        if data[base : base + 3] not in anchors:
            continue
        cluster_base = base - 24
        regs = parse_cluster_values(data, cluster_base)
        if not {0x02, 0x03, 0x05} <= regs.keys():
            continue
        a1_low = regs.get(0x02, 0)
        a1_high = regs.get(0x03, 0)
        a1_bank = regs.get(0x04, 0)
        size = regs.get(0x05, 0) | (regs.get(0x06, 0) << 8)
        bbad = regs.get(0x01, 0)
        src = resolve_dma_source(a1_low, a1_high, a1_bank, len(data))
        dest = parse_vram_dest(data, cluster_base)
        entry = {
            "setup_offset": hex(base),
            "bbad": hex(bbad),
            "bbad_meaning": BBAD_NAMES.get(bbad, "unknown"),
            "size_bytes": size,
            "dest_vram": hex(dest) if dest is not None else None,
            "decode": decode_dma_cluster(data, cluster_base, 0x40),
            **src,
        }
        setups.append(entry)

    seen: set[tuple] = set()
    out: list[dict] = []
    for s in setups:
        key = (
            s.get("kind"),
            s.get("snes_addr"),
            s.get("resolved_wram"),
            s.get("file_offset"),
            s["size_bytes"],
            s["dest_vram"],
            s["bbad"],
        )
        if key not in seen:
            seen.add(key)
            out.append(s)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    args = ap.parse_args()

    data = args.rom.read_bytes()
    setups = find_dma_setups(data)
    rom_setups = [s for s in setups if s["kind"] == "rom"]
    wram_setups = [s for s in setups if s["kind"] == "wram"]
    cgram_setups = [s for s in setups if s.get("bbad_meaning", "").startswith("CGRAM")]

    result = {
        "path": str(args.rom),
        "total_setups": len(setups),
        "rom_setups": rom_setups,
        "wram_setups": wram_setups,
        "cgram_setups": cgram_setups,
        "wram_setups_count": len(wram_setups),
        "all_setups": setups,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(
        f"Wrote {len(setups)} DMA setups "
        f"({len(rom_setups)} ROM, {len(wram_setups)} WRAM, {len(cgram_setups)} CGRAM) "
        f"to {args.output}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

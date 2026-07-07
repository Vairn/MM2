#!/usr/bin/env python3
"""Trace confirmed SNES MM2 gfx load paths (decompress + DMA + VRAM CPU)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def file_to_pc(off: int) -> int:
    bank = off >> 15
    return (bank << 16) | 0x8000 | (off & 0x7FFF)


def pc_to_file(pc: int) -> int:
    bank = (pc >> 16) & 0x7F
    return (bank << 15) | (pc & 0x7FFF)


def ptr_from_dp(e8: int, e9: int, ea: int) -> tuple[int, int]:
  snes = e8 | (e9 << 8) | (ea << 16)
  return snes, pc_to_file(snes) if (snes >> 16) <= 0x7F else -1


def find_jsl_e648_sites(data: bytes) -> list[dict]:
    sites: list[dict] = []
    pat = bytes([0x22, 0x48, 0xE6, 0x00])  # JSL $00E648
    for off in range(len(data) - 20):
        if data[off : off + 4] != pat:
            continue
        pre = data[max(0, off - 24) : off]
        e8 = e9 = ea = None
        x_imm = y_imm = None
        for i in range(len(pre) - 2):
            if pre[i] == 0xA9:
                if i + 1 < len(pre) and pre[i + 2] == 0x85:
                    dp = pre[i + 3] if i + 3 < len(pre) else None
                    if dp == 0xE8:
                        e8 = pre[i + 1]
                    elif dp == 0xE9:
                        e9 = pre[i + 1]
                    elif dp == 0xEA:
                        ea = pre[i + 1]
            elif pre[i] == 0xA2 and i + 1 < len(pre):
                x_imm = pre[i + 1]
            elif pre[i] == 0xA0 and i + 1 < len(pre):
                y_imm = pre[i + 1]
        rom_ptr = None
        rom_file = None
        if e8 is not None and e9 is not None and ea is not None:
            snes, fo = ptr_from_dp(e8, e9, ea)
            rom_ptr = f"${snes:06X}"
            rom_file = hex(fo) if fo >= 0 else None
        sites.append(
            {
                "file_offset": hex(off),
                "snes_pc": hex(file_to_pc(off)),
                "rom_source_ptr": rom_ptr,
                "rom_file_offset": rom_file,
                "decomp_x": hex(x_imm) if x_imm is not None else None,
                "decomp_y": hex(y_imm) if y_imm is not None else None,
            }
        )
    return sites


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    args = ap.parse_args()

    data = args.rom.read_bytes()
    result = {
        "path": str(args.rom),
        "exploration_vram_cpu_upload": {
            "routine_pc": "$069ED8",
            "file_offset": hex(pc_to_file(0x069ED8)),
            "description": "Reads 16-bit words via [$E8]/[$EB], STA $2118; counters $17BE/$17C0",
            "callers": [
                {"file_offset": "0x31D15", "snes_pc": "$069D15", "note": "JSR before second decompress"},
            ],
        },
        "exploration_init": {
            "routine_pc": "$069DDD",
            "file_offset": "0x31DDD",
            "rom_blob_ptr": "$069FBC",
            "rom_file_offset": hex(pc_to_file(0x069FBC)),
            "decompress": "JSL $00E648 (Y=$40) then DMA 32KiB to VRAM $4000",
            "bgmode": "$F1 @ STA $2105",
        },
        "decompress_entry": {
            "jsl_pc": "$00E648",
            "jsr_pc": "$00E289",
            "file_offset": hex(pc_to_file(0x00E289)),
            "reads": "[$E8],Y from 24-bit pointer in DP $E8-$EA",
        },
        "wram_to_vram_template": {
            "routine_pc": "$00E650",
            "file_offset": hex(pc_to_file(0x00E650)),
            "note": "Sequential WRAM $7E:7E00/$7E:4C80/$7E:5900 -> VRAM (3200 B chunks)",
        },
        "palette_cgram_dma": {
            "routine_pc": "$00E2B4",
            "file_offset": hex(pc_to_file(0x00E2B4)),
            "source": "WRAM $7E:7E00 (mirror $40:7E00)",
            "size_bytes": 512,
            "bbad": "$22 (CGDATA)",
        },
        "palette_inline": {
            "sites": [
                {"file_offset": "0x280A0", "cgadd": "$41", "cgdata_bytes": ["$4A", "$29"]},
                {"file_offset": "0x2841C", "note": "CGADD + CGDATA sequence"},
                {"file_offset": "0x28CB1", "note": "duplicate of 0x280A0 pattern"},
            ],
        },
        "game_state_vars": {
            "$0119": "Facing/area index; ADC #$40 on scene turn; range adjusted at $058E25",
            "$17C4": "Scene-ready flag (STA after fade)",
            "$17BE/$17C0": "VRAM upload loop counters",
        },
        "jSL_E648_sites": find_jsl_e648_sites(data),
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Wrote {args.output} ({len(result['jSL_E648_sites'])} decompress call sites)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

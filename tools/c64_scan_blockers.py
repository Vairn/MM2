#!/usr/bin/env python3
"""Scan MM2 C64 disks for post-decomp blocker addresses ($E3BF, $4546, $0100)."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "EXTRACTED/c64/analysis/blockers_scan.json"

import sys

sys.path.insert(0, str(ROOT / "tools"))
from c64_d64_layout import SPT, offset_to_track_sector, sector_payload  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", type=Path, default=OUT)
    args = ap.parse_args()

    report: dict = {"scan": "blockers_4546_0100_e3bf"}

    report["bit_e3bf"] = {
        "kernal_label": "Initialize BASIC RAM (entry $E3BF, first opcode $A9)",
        "disk_hits": [],
        "note": (
            "With $01=$37, BIT reads KERNAL ROM (not cartridge). "
            "INC $01 after BIT: $37->$38 (CHAREN=0, charset ROM at $D000). "
            "Post-$0100 loop at $0827 checks ZP $32==$08."
        ),
    }
    for d64 in sorted((ROOT / "EXTRACTED/c64").glob("MM2-*.D64")):
        data = d64.read_bytes()
        i = data.find(bytes([0x2C, 0xBF, 0xE3]))
        if i >= 0:
            report["bit_e3bf"]["disk_hits"].append(
                {
                    "disk": d64.name,
                    "offset": i,
                    "track_sector": offset_to_track_sector(len(data), i),
                }
            )

    chain = (ROOT / "EXTRACTED/c64/emulator/mm2-1_t19s0_chain.bin").read_bytes()
    report["4546"] = {
        "decomp_pass_61": {"src": "$4593", "dst": "$4503", "src_head": "0000000000000000"},
        "chain_end": f"${0x801 + len(chain):04X}",
        "addr_4593_in_chain": 0x4593 < 0x801 + len(chain),
        "disk_fixed_blob": "none on any of six D64s",
        "runtime_fill": "required before copy at $0816",
    }

    candidates: list[dict] = []
    for d64 in sorted((ROOT / "EXTRACTED/c64").glob("MM2-*.D64")):
        data = d64.read_bytes()
        for track in range(1, 36):
            for sector in range(SPT[track - 1]):
                payload = sector_payload(data, track, sector)
                pairs = 0
                for i in range(0, len(payload) - 1, 2):
                    t, s = payload[i], payload[i + 1]
                    if 1 <= t <= 35 and s < SPT[t - 1]:
                        pairs += 1
                if pairs >= 20:
                    candidates.append(
                        {
                            "disk": d64.name,
                            "track": track,
                            "sector": sector,
                            "pairs": pairs,
                            "head": payload[:16].hex(),
                        }
                    )
    report["track_sector_heuristic"] = sorted(candidates, key=lambda x: -x["pairs"])[:15]

    p16 = sector_payload((ROOT / "EXTRACTED/c64/MM2-1.D64").read_bytes(), 18, 16)
    report["t18s16"] = {
        "vector_02a7_le": f"${p16[0] | (p16[1] << 8):04X}",
        "setnam_name": p16[4:19].decode("ascii", errors="replace"),
        "setnam_ptr": "$02A9",
        "jmp_after_load": "JMP ($02A7)",
        "ptr_02b8_repeat_count": sum(
            1 for i in range(len(p16) - 1) if p16[i] == 0xB8 and p16[i + 1] == 0x02
        ),
        "jsr_0334": "T18S16 @ $0421 — JSR $0334 (init before SETLFS); $0334 is $00 in offline prepared RAM",
    }

    prg_0100: list[list] = []
    for d64 in sorted((ROOT / "EXTRACTED/c64").glob("MM2-*.D64")):
        data = d64.read_bytes()
        for track in range(1, 36):
            for sector in range(SPT[track - 1]):
                payload = sector_payload(data, track, sector)
                if len(payload) >= 4 and payload[0] == 0x01 and payload[1] == 0x08:
                    load_addr = payload[2] | (payload[3] << 8)
                    if load_addr == 0x0100:
                        prg_0100.append([d64.name, track, sector])
    report["stub_0100"] = {
        "prg_load_0100_sectors": prg_0100,
        "vice_live_pattern": (
            "PETSCII '38911'/'0000' at $0100 after goto $080B — "
            "BASIC/KERNAL low RAM pattern, not game IEC stub"
        ),
        "offline_prepared": "page $01 never written by decomp (dst pages $08–$FE)",
    }

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

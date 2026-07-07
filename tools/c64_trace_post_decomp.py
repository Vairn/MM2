#!/usr/bin/env python3
"""Offline trace from post-decomp $080B using prepared RAM + minimal 6502 core.

Documents the $4546->$00CB copy and $0100 entry without VICE.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREPARED = ROOT / "EXTRACTED/c64/emulator/ram/mm2-1_prepared_0800.bin"
T18 = ROOT / "EXTRACTED/c64/asm/t18_loader_mm2-1.bin"
OUT = ROOT / "EXTRACTED/c64/analysis/post_decomp_trace.json"

import sys

sys.path.insert(0, str(ROOT / "tools"))
from c64_6502_min import Cpu, step  # noqa: E402
from disasm_6502 import disasm  # noqa: E402


def load_prepared(path: Path, *, iec: bool) -> bytearray:
    ram = path.read_bytes()
    if len(ram) != 0xF800:
        raise SystemExit(f"expected 63488 bytes, got {len(ram)}")
    mem = bytearray(65536)
    mem[0x0800:0x10000] = ram
    if iec and T18.exists():
        iec_blob = T18.read_bytes()[:254]
        for i, b in enumerate(iec_blob):
            mem[0x0400 + i] = b
    mem[0x01] = 0x37
    return mem


def trace_from(mem: bytearray, start: int, max_steps: int) -> dict:
    cpu = Cpu(pc=start, mem=mem, p=0x25)
    log: list[dict] = []
    last_pc = -1

    for _ in range(max_steps):
        pc = cpu.pc
        if pc != last_pc:
            last_pc = pc
            if len(log) < 500 and (
                pc in (0x080B, 0x0810, 0x0816, 0x081F, 0x0100)
                or pc & 0xFF00 in (0x0400, 0xDD00)
            ):
                log.append({"pc": f"${pc:04X}", "a": cpu.a, "x": cpu.x, "y": cpu.y})

        try:
            step(cpu)
        except NotImplementedError as e:
            return {
                "stopped": f"unsupported: {e}",
                "steps": cpu.steps,
                "pc": f"${cpu.pc:04X}",
                "log": log,
            }

        if cpu.stopped or cpu.pc == 0x0100:
            break

    region_4546 = bytes(mem[0x4546 : 0x4546 + 256])
    region_00cb = bytes(mem[0x00CB : 0x00CB + 256])
    region_0100 = bytes(mem[0x0100 : 0x0100 + 64])

    return {
        "steps": cpu.steps,
        "final_pc": f"${cpu.pc:04X}",
        "4546_nonzero": sum(1 for b in region_4546 if b),
        "00cb_nonzero": sum(1 for b in region_00cb if b),
        "4546_head": region_4546[:32].hex(),
        "00cb_head": region_00cb[:32].hex(),
        "0100_head": region_0100[:32].hex(),
        "0100_disasm": disasm(region_0100, 0x0100, 16),
        "log": log,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--prepared", type=Path, default=PREPARED)
    ap.add_argument("--start", type=lambda x: int(x, 0), default=0x080B)
    ap.add_argument("--max-steps", type=int, default=50_000)
    ap.add_argument("--no-iec", action="store_true")
    ap.add_argument("-o", "--out", type=Path, default=OUT)
    args = ap.parse_args()

    mem = load_prepared(args.prepared, iec=not args.no_iec)
    report = trace_from(mem, args.start, args.max_steps)
    report["start"] = f"${args.start:04X}"
    report["iec_at_0400"] = not args.no_iec

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

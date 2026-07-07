#!/usr/bin/env python3
"""Prepare post-init C64 memory from MM2-1 T19 chain (offline boot simulation).

Applies the copy loops at $081E without VICE, optional decompression at $03B7,
writes RAM image $0800-$FFFF for inspection or VICE `load`.

Usage:
  python tools/c64_prepare_memory.py
  python tools/c64_prepare_memory.py --skip-decomp --goto 080b
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHAIN = ROOT / "EXTRACTED/c64/emulator/mm2-1_t19s0_chain.bin"
OUT = ROOT / "EXTRACTED/c64/emulator/ram"

import sys

sys.path.insert(0, str(ROOT / "tools"))
from c64_decomp import apply_boot_init, run_decomp  # noqa: E402


def load_chain(mem: bytearray, chain: bytes, base: int = 0x0801) -> None:
    for i, b in enumerate(chain):
        mem[base + i] = b


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--skip-decomp", action="store_true")
    ap.add_argument("--out", type=Path, default=OUT / "mm2-1_prepared_0800.bin")
    args = ap.parse_args()

    chain = CHAIN.read_bytes()
    mem = bytearray(65536)
    load_chain(mem, chain)
    apply_boot_init(mem)

    iters = 0
    if not args.skip_decomp:
        iters = run_decomp(mem)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(bytes(mem[0x0800:0x10000]))

    # Sidecar: key regions
    meta = args.out.with_suffix(".json")
    import json

    meta.write_text(
        json.dumps(
            {
                "decomp_iters": iters,
                "080b_head": mem[0x080B : 0x080B + 16].hex(),
                "0400_head": mem[0x0400 : 0x0400 + 16].hex(),
                "03b7_head": mem[0x03B7 : 0x03B7 + 16].hex(),
                "03c0": mem[0x03C0],
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"Wrote {args.out} ({args.out.stat().st_size} bytes), decomp_iters={iters}")
    print(f"Meta: {meta}")


if __name__ == "__main__":
    main()

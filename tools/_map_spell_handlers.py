#!/usr/bin/env python3
"""Map MM2 sparse spell/skill dispatch tables."""
from __future__ import annotations

import struct
from pathlib import Path
import importlib.util


def _load_picker():
    here = Path(__file__).resolve().parent
    path = here / "_trace_spell_picker.py"
    spec = importlib.util.spec_from_file_location("_trace_spell_picker", path)
    mod = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(mod)
    return mod


def parse_dense_pairs(mem: bytearray, base: int, limit: int = 0x200) -> list[tuple[int, int]]:
    pairs: list[tuple[int, int]] = []
    off = base
    end = base + limit
    while off + 8 <= end:
        w0 = struct.unpack_from(">I", mem, off)[0]
        w1 = struct.unpack_from(">I", mem, off + 4)[0]
        hi0, lo0 = (w0 >> 16) & 0xFFFF, w0 & 0xFFFF
        hi1 = (w1 >> 16) & 0xFFFF
        if hi0 != 0x4EBA or hi1 != 0x6000:
            break
        disp = struct.unpack(">h", lo0.to_bytes(2, "big"))[0]
        pairs.append((off, off + disp))
        off += 8
    return pairs


def parse_sparse(mem: bytearray, table_base: int, count: int, jmp_base: int) -> list[tuple[int, int, int]]:
    rows: list[tuple[int, int, int]] = []
    for code in range(count):
        w = struct.unpack_from(">h", mem, table_base + code * 2)[0]
        tgt = (jmp_base + w) & 0xFFFFFFFF
        rows.append((code, w, tgt))
    return rows


def main() -> None:
    picker = _load_picker()
    mem = picker.build_full_code_image()

    cler_pairs = parse_dense_pairs(mem, 0xCDC6)
    sorc_pairs = parse_dense_pairs(mem, 0xD006)
    extra_pairs = parse_dense_pairs(mem, 0xD186, 0x80)

    print("=== Dense JSR/BRA blocks ===")
    print(f"CDB8 block @0xCDC6: {len(cler_pairs)} entries")
    print(f"D002 block @0xD006: {len(sorc_pairs)} entries")
    print(f"D186 extra @0xD186: {len(extra_pairs)} entries")

    print("\nCDB8 first entries:")
    for i, (tbl, stub) in enumerate(cler_pairs[:10]):
        print(f"  {i:2d}: table {tbl:#06x} -> stub {stub:#06x}")

    print("\nD002 first entries:")
    for i, (tbl, stub) in enumerate(sorc_pairs[:10]):
        print(f"  {i:2d}: table {tbl:#06x} -> stub {stub:#06x}")

    print("\nD186 extras:")
    for i, (tbl, stub) in enumerate(extra_pairs):
        print(f"  {i:2d}: table {tbl:#06x} -> stub {stub:#06x}")

    cler_sparse = parse_sparse(mem, 0xCF1E, 0x60, 0xCFF2)
    sorc_sparse = parse_sparse(mem, 0xD1AE, 0x5C, 0xD27C)
    cler_live = [r for r in cler_sparse if r[1] != 2]
    sorc_live = [r for r in sorc_sparse if r[1] != 2]

    print("\n=== Sparse executors ===")
    print(f"CDB8 executor @0xCFDE: {len(cler_live)} live codes / 96")
    print(f"CFF8 executor @0xD266: {len(sorc_live)} live codes / 92")

    print("\nCDB8 live code sample:")
    for code, w, tgt in cler_live[:20]:
        print(f"  code {code:02d}: off {w:6d} -> {tgt:#06x}")

    print("\nCFF8 live code sample:")
    for code, w, tgt in sorc_live[:20]:
        print(f"  code {code:02d}: off {w:6d} -> {tgt:#06x}")

    cset = {c for c, _, _ in cler_live}
    sset = {c for c, _, _ in sorc_live}
    print("\nCode overlap:")
    print(f"  union={len(cset | sset)} intersection={len(cset & sset)}")
    print(f"  cler-only sample={sorted(cset - sset)[:12]}")
    print(f"  sorc-only sample={sorted(sset - cset)[:12]}")


if __name__ == "__main__":
    main()


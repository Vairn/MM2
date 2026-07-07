#!/usr/bin/env python3
"""Build Genesis MM2 gfx analysis JSON: A5 vectors, resource tables, $2DBA uploads."""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

try:
    import capstone
    from capstone import CS_ARCH_M68K, CS_MODE_M68K_000
except ImportError:
    capstone = None

A5_BASE = 0x312
UPLOAD_ROUTINE = 0x2DBA


def read_u32_be(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]


def parse_resource_table(rom: bytes, off: int) -> dict:
    if off + 8 > len(rom):
        return {"offset": hex(off), "error": "truncated"}
    w0 = read_u32_be(rom, off)
    w1 = read_u32_be(rom, off + 4)
    payload = rom[off + 8 : off + 8 + min(w1, 64)]
    return {
        "offset": hex(off),
        "word0": hex(w0),
        "word1_out_size": hex(w1),
        "word1_decimal": w1,
        "payload_head": payload[:32].hex(),
        "inline_payload_offset": hex(off + 8),
    }


def a5_vectors(rom: bytes) -> list[dict]:
    rows = []
    for disp in range(0, 0x80, 6):
        off = A5_BASE + disp
        if rom[off : off + 2] != b"\x4E\xF9":
            break
        dest = read_u32_be(rom, off + 2)
        rows.append({"a5_disp": hex(disp), "vector_offset": hex(off), "target": hex(dest)})
    return rows


def find_bsr_targets(rom: bytes, target: int) -> list[int]:
    hits = []
    for off in range(len(rom) - 2):
        if rom[off] != 0x61:
            continue
        rel = struct.unpack_from(">h", rom, off + 1)[0]
        if off + 2 + rel == target:
            hits.append(off)
    return hits


def decode_upload_call(rom: bytes, call_off: int) -> dict:
    if capstone is None:
        return {"call_offset": hex(call_off)}
    md = capstone.Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
    insns = list(md.disasm(rom[max(0, call_off - 120) : call_off + 2], max(0, call_off - 120)))
    stack: list[int] = []
    src = None
    for ins in reversed(insns):
        if ins.address >= call_off:
            continue
        op = ins.op_str.lower()
        if ins.mnemonic == "pea":
            if "pc" in op:
                disp = int(op.split("(")[0].strip(), 16)
                stack.insert(0, ins.address + ins.size + disp)
            elif ".l" in op:
                stack.insert(0, int(op.split("(")[1].split(")")[0], 16))
        elif ins.mnemonic == "move.l" and "#" in op and "(a7)" in op:
            stack.insert(0, int(ins.op_str.split("#")[1].split(",")[0], 16))
        elif ins.mnemonic == "movea.l" and "(a7)" in op:
            src = ins.op_str.split(",")[1].strip()
        elif ins.mnemonic in ("bsr", "jsr", "jmp") and ins.address < call_off - 80:
            break
    # $2DBA stack (last pushed = $8(a6)): dest_type, vdp_addr, length, src
    args = {}
    if len(stack) >= 4:
        args = {
            "dest_type": hex(stack[0]),
            "vdp_addr": hex(stack[1]),
            "byte_length": stack[2],
            "src_ptr": hex(stack[3]) if stack[3] else None,
        }
    elif stack:
        args = {"stack": [hex(x) for x in stack], "src_reg": src}
    return {"call_offset": hex(call_off), **args}


def boot_uploads(rom: bytes) -> list[dict]:
    """Known boot uploads from vdp_init_0706.asm (confirmed)."""
    return [
        {
            "call_offset": "0x776",
            "resource_table": "0xC52",
            "path": "$0C(A5) resolve + $2DBA",
            "dest_type": "VRAM",
            "vdp_addr": "0x4080",
            "byte_length": 0x940,
            "codec": "LZ inline @ table+8",
        },
        {
            "call_offset": "0x7A0",
            "resource_table": "0x13D8",
            "path": "$0C(A5) + $06(A5) DMA",
            "dest_type": "VRAM",
            "vdp_addr": "0x49C0",
            "byte_length": 0x1340,
            "dma_source_ram": "0xFF0002",
            "codec": "LZ inline @ table+8",
        },
        {
            "call_offset": "0x7DA",
            "resource_table": "0x1262",
            "path": "$0C(A5) + $06(A5) to RAM",
            "dest_type": "RAM",
            "ram_addr": "0xFF1000",
            "byte_length": 0x2000,
            "codec": "LZ inline @ table+8",
        },
        {
            "call_offset": "0x83E",
            "resource_table": None,
            "path": "$2DBA from RAM",
            "dest_type": "VRAM",
            "vdp_addr": "0x2000",
            "byte_length": 0x2000,
            "src_ram": "0xFF1000",
            "note": "nametable patch loop @ 0x800-0x826 before upload",
        },
        {
            "call_offset": "0x87E",
            "resource_table": "0xBD2",
            "path": "$2DBA",
            "dest_type": "VRAM",
            "vdp_addr": "0x0",
            "byte_length": 0x80,
        },
        {
            "call_offset": "0x8AA",
            "resource_table": "0xB12",
            "path": "$0C(A5) + $2DBA CRAM",
            "dest_type": "CRAM",
            "vdp_addr": "0xC000",
            "byte_length": 0x180,
            "codec": "LZ inline @ table+8",
        },
        {
            "call_offset": "0x924",
            "resource_table": "0x1FDC",
            "path": "$0C(A5) + $2DBA",
            "dest_type": "VRAM",
            "vdp_addr": "0x5D20",
            "byte_length": 0x12E0,
        },
        {
            "call_offset": "0x976",
            "resource_table": None,
            "path": "$2DBA from RAM",
            "dest_type": "VRAM",
            "vdp_addr": "0xF00",
            "byte_length": 0x380,
            "src_ram": "0xFFFF0004",
        },
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    tables = [0xC52, 0x13D8, 0x1262, 0xBB2, 0xB12, 0xBD2, 0x1FDC]

    uploads = [decode_upload_call(rom, off) for off in find_bsr_targets(rom, UPLOAD_ROUTINE)]

    result = {
        "rom": str(args.rom),
        "md5_note": "3238047522caa4e27be2720da924777e",
        "a5_base": hex(A5_BASE),
        "a5_vectors": a5_vectors(rom),
        "key_vectors": {
            "resource_lookup_0C": "0x29954",
            "vdp_dma_06": "0x29F7E",
            "vdp_upload_2DBA": hex(UPLOAD_ROUTINE),
        },
        "resource_tables": {hex(t): parse_resource_table(rom, t) for t in tables},
        "boot_vdp_uploads_confirmed": boot_uploads(rom),
        "vdp_upload_calls_all": uploads[:120],
        "compression": {
            "codec": "custom_lz",
            "decompressor_rom": "0x29954",
            "variant_rom": "0x29A22",
            "ring_size": 0x1000,
            "header_bytes": 8,
            "header_layout": "BE u32 word0 (metadata), BE u32 word1 (output_size); LZ stream at table+8",
            "kosinski": "not used (trial failed)",
            "status": "algorithm traced; Python port partial — 1262/C52 plausible, not VRAM-verified",
        },
        "emulator_validation": {
            "status": "not_run",
            "note": "BlastEm / Genesis Plus GX not invoked in this session",
        },
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Wrote {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

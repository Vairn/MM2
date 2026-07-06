#!/usr/bin/env python3
"""Disassemble Might and Magic II PC binaries (8086 / 16-bit real mode).

Targets:
  MM2.EXE   — DOS MZ executable (GOG)
  CGA.DRV   — flat driver blob with near jmp dispatch table @ 0
  EGA.DRV   — same layout as CGA.DRV

Output: EXTRACTED/pc/<name>.capstone.asm (+ optional .meta.txt)

Usage:
  python tools/disasm_pc_x86.py "C:/.../Might and Magic 2/MM2.EXE"
  python tools/disasm_pc_x86.py "C:/.../Might and Magic 2" --all-drivers
  python tools/disasm_pc_x86.py MM2.EXE CGA.DRV EGA.DRV -o EXTRACTED/pc
"""
from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "EXTRACTED" / "pc"

# Known MM2 PC addresses (file-offset == linear ORG for this EXE build).
PC_LABELS: dict[int, str] = {
    0x114A: "rect_clip_or_fill",
    0x1416: "draw_rect_driver",
    0x1522: "driver_dispatch_num",
    0x2158: "debug_print_xy",
    0x24DA: "video_driver_call",
    0x29B6: "lzw_load_resource",
    0x2A42: "lzw_decompress",
    0x2F34: "blit_sprite_driver",
    0x31DE: "resource_name_lookup",
    0x326A: "load_gfx_file",
    0x366E: "parse_resource_token",
    0x3A92: "viewport_blit_setup",
    0x3FB6: "resource_record_ptr",
    0x42EA: "frame_index_scale",
    0x47A0: "set_viewport_mode",
    0x5234: "load_wall_gfx_sheet",
    0x54D0: "video_error_print",
    0x555C: "parse_resource_flags",
    0x55AA: "open_resource_bundle",
    0x5C5A: "gfx_format_class_check",
    0x5CAE: "gfx_needs_decompress_check",
    0x5CEE: "gfx_frame_size_check",
    0x5D4A: "pick_frame_from_sheet",
    0x5D28: "gfx_frame_count_check",
    0x60B0: "dispatch_gfx_by_type",
    0x6544: "load_combat_monster_gfx",
    0x6964: "copy_picture_block",
    0x6971: "copy_picture_block_impl",
    0x6BB4: "draw_globe_gfx",
    0x6E16: "draw_book_gfx",
    0x78C6: "decompress_if_needed",
    0x8815: "filename_table",
}

# CGA.DRV dispatch slot names (NWC driver API order, index = offset/3).
CGA_DRV_SLOTS = [
    "init", "shutdown", "set_palette", "set_mode", "plot", "read_pixel",
    "draw_line", "fill_rect", "blit", "text", "scroll", "unknown_11",
    "unknown_12", "unknown_13", "unknown_14", "unknown_15", "unknown_16",
    "unknown_17", "unknown_18", "unknown_19", "unknown_20", "unknown_21",
    "unknown_22", "unknown_23", "unknown_24", "unknown_25", "unknown_26",
    "unknown_27", "unknown_28", "unknown_29", "unknown_30", "unknown_31",
]


@dataclass
class ImageInfo:
    path: Path
    kind: str  # mz | flat_drv
    size: int
    org: int
    code_off: int
    code_size: int
    entry: int | None
    dispatch: list[tuple[int, int]]  # (table_off, target_off)


def parse_mz(raw: bytes) -> tuple[int, int, int]:
    """Return (code_file_offset, entry_linear, header_bytes)."""
    if raw[:2] != b"MZ":
        raise ValueError("not a DOS MZ executable")
    e_cparhdr = struct.unpack_from("<H", raw, 8)[0]
    e_ip = struct.unpack_from("<H", raw, 20)[0]
    e_cs = struct.unpack_from("<H", raw, 22)[0]
    code_off = e_cparhdr * 16
    entry = (e_cs << 4) + e_ip
    return code_off, entry, code_off


def parse_drv_dispatch(raw: bytes) -> list[tuple[int, int]]:
    """Near jmp (E9 rel16) table at file start."""
    entries: list[tuple[int, int]] = []
    off = 0
    while off + 3 <= len(raw) and raw[off] == 0xE9:
        rel = struct.unpack_from("<h", raw, off + 1)[0]
        target = (off + 3 + rel) & 0xFFFF
        entries.append((off, target))
        off += 3
    return entries


def load_image(path: Path) -> tuple[bytes, ImageInfo]:
    raw = path.read_bytes()
    if raw[:2] == b"MZ":
        code_off, entry, _ = parse_mz(raw)
        info = ImageInfo(
            path=path,
            kind="mz",
            size=len(raw),
            org=0,
            code_off=0,
            code_size=len(raw),
            entry=entry,
            dispatch=[],
        )
        return raw, info
    dispatch = parse_drv_dispatch(raw)
    info = ImageInfo(
        path=path,
        kind="flat_drv",
        size=len(raw),
        org=0,
        code_off=0,
        code_size=len(raw),
        entry=dispatch[0][1] if dispatch else None,
        dispatch=dispatch,
    )
    return raw, info


def format_insn(insn, labels: dict[int, str]) -> str:
    addr = insn.address
    label = labels.get(addr)
    prefix = f"{label}:\n" if label else ""
    hex_bytes = insn.bytes.hex()
    return f"{prefix}{addr:05X}  {hex_bytes:<12}  {insn.mnemonic:<8}  {insn.op_str}\n"


def disassemble(
    raw: bytes,
    info: ImageInfo,
    out_path: Path,
    meta_path: Path,
    max_insns: int | None,
) -> int:
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    md.detail = True
    md.skipdata = True

    labels = dict(PC_LABELS) if info.path.name.upper() == "MM2.EXE" else {}
    for i, (_tab, target) in enumerate(info.dispatch):
        name = CGA_DRV_SLOTS[i] if i < len(CGA_DRV_SLOTS) else f"api_{i}"
        labels.setdefault(target, f"drv_{name}")

    count = 0
    with out_path.open("w", encoding="utf-8") as out:
        out.write(f"; Capstone {capstone.__version__} — 8086 / 16-bit real mode\n")
        out.write(f"; File: {info.path.resolve()}\n")
        out.write(f"; Kind: {info.kind}\n")
        out.write(f"; Size: {info.size} bytes (0x{info.size:X})\n")
        if info.entry is not None:
            out.write(f"; Entry: 0x{info.entry:05X}\n")
        out.write(";\n")
        if info.dispatch:
            out.write("; --- driver dispatch (near jmp table) ---\n")
            for i, (tab, target) in enumerate(info.dispatch):
                name = CGA_DRV_SLOTS[i] if i < len(CGA_DRV_SLOTS) else f"api_{i}"
                out.write(f";   [{i:2d}] @{tab:04X} -> @{target:04X}  {name}\n")
            out.write(";\n")

        for insn in md.disasm(raw, info.org):
            out.write(format_insn(insn, labels if insn.address in labels else {}))
            count += 1
            if max_insns is not None and count >= max_insns:
                out.write(f"; ... truncated after {max_insns} instructions\n")
                break

    meta_lines = [
        f"file={info.path.name}",
        f"kind={info.kind}",
        f"bytes={info.size}",
        f"entry=0x{info.entry:X}" if info.entry is not None else "entry=none",
    ]
    if info.kind == "mz":
        code_off, entry, _ = parse_mz(raw)
        meta_lines.append(f"mz_header_paragraphs=0x{code_off // 16:X}")
        meta_lines.append(f"mz_code_offset=0x{code_off:X}")
    for i, (tab, target) in enumerate(info.dispatch):
        name = CGA_DRV_SLOTS[i] if i < len(CGA_DRV_SLOTS) else f"api_{i}"
        meta_lines.append(f"dispatch[{i}] @{tab:X} -> @{target:X} {name}")
    meta_path.write_text("\n".join(meta_lines) + "\n", encoding="utf-8")
    return count


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Disassemble MM2 PC x86 binaries")
    ap.add_argument("inputs", nargs="*", type=Path, help="MM2.EXE, *.DRV, or game dir")
    ap.add_argument("-o", "--output-dir", type=Path, default=DEFAULT_OUT)
    ap.add_argument(
        "--all-drivers",
        action="store_true",
        help="With a game directory, also disassemble CGA.DRV and EGA.DRV",
    )
    ap.add_argument("--max-insns", type=int, default=None)
    args = ap.parse_args(argv)

    paths: list[Path] = []
    for inp in args.inputs or [Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")]:
        if inp.is_dir():
            for name in ["MM2.EXE"]:
                p = inp / name
                if p.is_file():
                    paths.append(p)
            if args.all_drivers:
                for name in ["CGA.DRV", "EGA.DRV", "HGA.DRV", "MCGA.DRV", "TGA.DRV"]:
                    p = inp / name
                    if p.is_file():
                        paths.append(p)
        elif inp.is_file():
            paths.append(inp)

    if not paths:
        print("No input files found", file=sys.stderr)
        return 1

    args.output_dir.mkdir(parents=True, exist_ok=True)
    total = 0
    for path in paths:
        raw, info = load_image(path)
        stem = path.stem.lower().replace(".", "_")
        out_asm = args.output_dir / f"{stem}.capstone.asm"
        out_meta = args.output_dir / f"{stem}.meta.txt"
        n = disassemble(raw, info, out_asm, out_meta, args.max_insns)
        total += n
        print(f"{path.name}: {info.kind}, {n} insns -> {out_asm.relative_to(ROOT)}")

    print(f"Done: {len(paths)} file(s), {total} instructions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

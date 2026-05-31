#!/usr/bin/env python3
"""Disassemble Amiga hunk executables (68000) with Capstone."""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

from amitools.binfmt.hunk import Hunk
from amitools.binfmt.hunk.HunkReader import HunkReader
import capstone
from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000

HUNK_HEADER_MAGIC = 1011  # 0x3F3


def load_code_hunks(path: Path) -> list[dict]:
    reader = HunkReader()
    result = reader.read_file(str(path))
    if result != 0:
        raise SystemExit(f"{path}: hunk parse failed ({reader.error_string})")

    segments: list[dict] = []
    for h in reader.hunks:
        if h.get("type") != Hunk.HUNK_CODE:
            continue
        data = h.get("data", b"")
        segments.append(
            {
                "file_offset": h.get("hunk_file_offset", 0),
                "size": len(data),
                "data": data,
            }
        )
    return segments


def disassemble_segment(md: Cs, data: bytes, base: int, out, max_insns: int | None) -> int:
    count = 0
    for insn in md.disasm(data, base):
        hex_bytes = insn.bytes.hex()
        out.write(f"{insn.address:06x}  {hex_bytes:<16}  {insn.mnemonic:8}  {insn.op_str}\n")
        count += 1
        if max_insns is not None and count >= max_insns:
            out.write(f"; ... truncated after {max_insns} instructions\n")
            break
    return count


def disassemble_file(path: Path, out_path: Path, max_insns: int | None) -> None:
    code_hunks = load_code_hunks(path)
    if not code_hunks:
        raise SystemExit(f"{path}: no CODE hunks found")

    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
    md.skipdata = True  # continue through embedded strings/tables
    load_addr = 0
    total_insns = 0

    with out_path.open("w", encoding="utf-8") as out:
        out.write(f"; Capstone {capstone.__version__} disassembly\n")
        out.write(f"; File: {path.resolve()}\n")
        out.write("; Architecture: Motorola 68000 (Amiga hunk executable)\n\n")

        for idx, seg in enumerate(code_hunks):
            out.write(
                f"; --- CODE segment {idx} "
                f"({seg['size']} bytes, file offset 0x{seg['file_offset']:x}) ---\n"
            )
            total_insns += disassemble_segment(md, seg["data"], load_addr, out, max_insns)
            out.write("\n")
            load_addr += seg["size"]

    code_bytes = sum(s["size"] for s in code_hunks)
    print(
        f"{path.name}: {len(code_hunks)} code segment(s), "
        f"{code_bytes} bytes, {total_insns} instructions -> {out_path}"
    )


def describe_non_binary(path: Path) -> None:
    data = path.read_bytes()
    try:
        text = data.decode("latin-1")
    except UnicodeDecodeError:
        print(f"{path.name}: not a hunk executable (binary data)", file=sys.stderr)
        return
    if all(32 <= ord(c) < 127 or c in "\r\n\t" for c in text):
        print(f"{path.name}: AmigaDOS script (not machine code):", file=sys.stderr)
        for line in text.splitlines():
            print(f"  {line}", file=sys.stderr)
    else:
        print(f"{path.name}: not a hunk executable", file=sys.stderr)


def main() -> None:
    ap = argparse.ArgumentParser(description="Disassemble Amiga m68k hunk executables")
    ap.add_argument("inputs", nargs="+", type=Path)
    ap.add_argument("-o", "--output-dir", type=Path, default=Path("EXTRACTED"))
    ap.add_argument("--max-insns", type=int, default=None, help="limit per code segment")
    args = ap.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for path in args.inputs:
        if not path.is_file():
            print(f"skip {path}: not found", file=sys.stderr)
            continue

        magic = struct.unpack(">I", path.read_bytes()[:4])[0]
        if magic != HUNK_HEADER_MAGIC:
            describe_non_binary(path)
            continue

        out_path = args.output_dir / f"{path.stem}.capstone.asm"
        disassemble_file(path, out_path, args.max_insns)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Extract CODE/DATA hunks from Amiga executables for Ghidra/IDA raw import."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

from amitools.binfmt.hunk import Hunk
from amitools.binfmt.hunk.HunkReader import HunkReader

HUNK_HEADER_MAGIC = 1011


def extract(path: Path, out_dir: Path) -> None:
    reader = HunkReader()
    if reader.read_file(str(path)) != 0:
        raise SystemExit(reader.error_string or "hunk parse failed")

    out_dir.mkdir(parents=True, exist_ok=True)
    load_addr = 0
    manifest: dict = {
        "source": str(path.resolve()),
        "cpu": "68000",
        "endian": "big",
        "segments": [],
        "entry_points": [],
        "notes": [
            "Import each .bin in Ghidra as Motorola 68000, big-endian.",
            "Use the load_addr from this manifest as the image base.",
            "Relocations are not applied; prefer EXTRACTED/mm2.asm for annotated listing.",
        ],
    }

    code_idx = 0
    data_idx = 0
    for h in reader.hunks:
        htype = h.get("type")
        file_off = h.get("hunk_file_offset", 0)

        if htype == Hunk.HUNK_CODE:
            data = h.get("data", b"")
            name = f"{path.stem}_code_{code_idx:02d}.bin"
            (out_dir / name).write_bytes(data)
            seg = {
                "name": name,
                "kind": "code",
                "load_addr": load_addr,
                "size": len(data),
                "file_offset": file_off,
            }
            manifest["segments"].append(seg)
            if code_idx == 0 and len(data) >= 0x1C:
                # bra.w stub then jmp main — common MM2 entry layout
                w0 = struct.unpack_from(">H", data, 0)[0]
                if w0 >> 12 == 0x6:  # bra
                    manifest["entry_points"].append(
                        {"addr": load_addr + 0x18, "label": "start_after_stub"}
                    )
            code_idx += 1
            load_addr += len(data)

        elif htype == Hunk.HUNK_DATA:
            data = h.get("data", b"")
            name = f"{path.stem}_data_{data_idx:02d}.bin"
            (out_dir / name).write_bytes(data)
            manifest["segments"].append(
                {
                    "name": name,
                    "kind": "data",
                    "load_addr": load_addr,
                    "size": len(data),
                    "file_offset": file_off,
                }
            )
            data_idx += 1
            load_addr += len(data)

        elif htype == Hunk.HUNK_BSS:
            size = h.get("size", 0)
            manifest["segments"].append(
                {
                    "name": None,
                    "kind": "bss",
                    "load_addr": load_addr,
                    "size": size,
                    "file_offset": file_off,
                }
            )
            load_addr += size

    manifest_path = out_dir / f"{path.stem}_segments.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Wrote {len(manifest['segments'])} segment(s) + {manifest_path}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("executable", type=Path)
    ap.add_argument("-o", "--output-dir", type=Path, default=Path("EXTRACTED/ghidra"))
    args = ap.parse_args()

    magic = struct.unpack(">I", args.executable.read_bytes()[:4])[0]
    if magic != HUNK_HEADER_MAGIC:
        raise SystemExit(f"{args.executable}: not an Amiga hunk file")

    extract(args.executable, args.output_dir)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Convert Apple II WOZ2 images to DSK and extract/catalog what we can.

MM2 Apple II disks use copy-protected / non-catalog layouts on some sides; this tool
always produces a 140K DSK (6-and-2 decode) and a manifest with catalog attempts,
string hits for Amiga-equivalent gfx stems, and optional file extraction when DOS 3.3
catalog parses cleanly.

Requires: pip install git+https://github.com/BrentRector/orchard.git  (nibbler)

Usage:
  python tools/extract_apple2_woz.py EXTRACTED/apple -o EXTRACTED/apple2
  python tools/extract_apple2_woz.py "path/to/disk A.woz" -o EXTRACTED/apple2/extracted/disk_a
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_GFX_STEMS = [
    "CASTLE", "CAVE", "TOWN", "TOWNB", "TOWNF", "TOWNT", "THROW", "SKY",
    "DESERT", "SWAMP", "OCEAN", "TUNDRA", "OUTDOOR", "OUTB", "OUTF",
    "NWCP", "MONSTER", "INTRO", "ENDGAME", "GLOBE", "CASTLEF", "CASTLET",
    "CAVEF", "CAVET",
]


def _side_letter(path: Path, index: int) -> str:
    m = re.search(r"disk\s*([A-Fa-f])\b", path.stem)
    if m:
        return m.group(1).lower()
    return chr(ord("a") + index)


def _scan_stems(data: bytes) -> list[dict]:
    hits: list[dict] = []
    for stem in _GFX_STEMS:
        for pat in (stem.encode(), stem.lower().encode()):
            off = 0
            while True:
                i = data.find(pat, off)
                if i < 0:
                    break
                hits.append({"stem": stem, "offset": i, "hex": hex(i)})
                off = i + 1
    return hits


def _try_catalog(dsk: bytes) -> dict:
    try:
        from nibbler.dsk import read_catalog, read_file_data, read_vtoc
    except ImportError:
        return {"error": "nibbler not installed"}

    try:
        vtoc = read_vtoc(dsk)
        files = read_catalog(dsk)
    except Exception as exc:  # noqa: BLE001 — catalog often fails on protected sides
        return {"error": str(exc), "vtoc_partial": True}

    out_files = []
    extracted: list[dict] = []
    return {
        "vtoc": {k: v for k, v in vtoc.items() if k != "raw"},
        "files": files,
        "extracted": extracted,
    }


def process_woz(woz: Path, out_root: Path, index: int) -> dict:
    side = _side_letter(woz, index)
    disks_dir = out_root / "disks"
    extracted_dir = out_root / "extracted" / f"disk_{side}"
    disks_dir.mkdir(parents=True, exist_ok=True)
    extracted_dir.mkdir(parents=True, exist_ok=True)

    dsk_path = disks_dir / f"disk_{side}.dsk"
    subprocess.run(
        [sys.executable, "-m", "nibbler", "dsk", str(woz), "-o", str(dsk_path)],
        check=True,
        capture_output=True,
        text=True,
    )
    dsk = dsk_path.read_bytes()
    manifest: dict = {
        "source_woz": str(woz.relative_to(ROOT)) if woz.is_relative_to(ROOT) else str(woz),
        "dsk": str(dsk_path.relative_to(ROOT)) if dsk_path.is_relative_to(ROOT) else str(dsk_path),
        "size": len(dsk),
        "stem_hits": _scan_stems(dsk),
        "catalog": _try_catalog(dsk),
    }

    # Extract catalog files when readable
    cat = manifest.get("catalog") or {}
    if isinstance(cat, dict) and cat.get("files"):
        try:
            from nibbler.dsk import read_file_data

            for ent in cat["files"]:
                data = read_file_data(dsk, ent)
                safe = re.sub(r"[^A-Za-z0-9._-]+", "_", ent["name"].strip())
                out_path = extracted_dir / f"{safe}.{ent['type'].lower()}"
                out_path.write_bytes(data)
                ent["extracted_path"] = str(out_path.relative_to(ROOT))
        except Exception as exc:  # noqa: BLE001
            manifest["extract_error"] = str(exc)

    (extracted_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return manifest


def main() -> None:
    ap = argparse.ArgumentParser(description="Extract Apple II WOZ2 disks for MM2 RE")
    ap.add_argument("input", type=Path, help="WOZ file or directory (searched recursively)")
    ap.add_argument("-o", "--out", type=Path, default=ROOT / "EXTRACTED" / "apple2")
    args = ap.parse_args()

    if args.input.is_file():
        woz_files = [args.input]
    else:
        woz_files = sorted(args.input.rglob("*.woz"))

    if not woz_files:
        raise SystemExit(f"No .woz files under {args.input}")

    summary = []
    for i, woz in enumerate(woz_files):
        print(f"Processing {woz.name}...")
        summary.append(process_woz(woz, args.out, i))

    (args.out / "manifest_summary.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    print(f"Done: {len(summary)} disk(s) -> {args.out}")


if __name__ == "__main__":
    main()

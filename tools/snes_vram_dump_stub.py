#!/usr/bin/env python3
"""Emulator-assisted SNES VRAM / CGRAM validation for MM2 SNES port.

Supports:
  - Raw 64 KiB VRAM dumps (Mesen / bsnes export)
  - ZSNES .zst save states (VRAM @ file offset 0x20C13 — see snes_zst_extract.py)
  - WRAM staging compare vs VRAM ($7E:7E00 -> $2D40, etc. from gfx_paths.json)

Typical ZSNES workflow (manual F2 — keyboard automation failed on Win11 2026-07):

  powershell tools\\snes_zsnes_dump.ps1 -Seconds 60
  # Press F2 at title or field view while script waits
  python tools\\snes_zst_extract.py EXTRACTED\\snes\\emulator\\mm2.zst -o EXTRACTED\\snes\\emulator\\dumps --compare-staging

Mesen (recommended for direct VRAM export):

  1. Load EXTRACTED/SNES/Might and Magic II (Europe).sfc
  2. Run to first-person field view
  3. Tools -> Memory Tools -> open Memory Viewer -> dropdown **Video RAM**
  4. File -> Export -> save as ``field.vram`` (exactly **65536 bytes**, no extension required)
  5. python tools/snes_vram_dump_stub.py --dump field.vram --compare-staging

  powershell tools\\mesen_launch.ps1

Mesen **16 MiB** ``.dmp`` files (16777216 bytes) are the full **CPU address map**
(256 x 64 KiB pages: page ``N`` = ``$NN:0000``). PPU VRAM is **not** in that map.
The tool scans pages for WRAM->VRAM DMA staging matches and extracts a 64 KiB slice
only when staging validates (>=80%%). Re-export via Memory Viewer -> Video RAM if
you see a 16 MiB file.

Installed Mesen: tools/emulators/Mesen2/Mesen.exe (winget SourMesen.Mesen2)
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
MESEN_EXE = _REPO / "tools" / "emulators" / "Mesen2" / "Mesen.exe"
MESEN_WINGET = (
    Path.home()
    / "AppData"
    / "Local"
    / "Microsoft"
    / "WinGet"
    / "Links"
    / "Mesen.exe"
)
ZSNES_DIR = Path(
    r"C:\Users\Adam Templeton\Downloads\SNES Gems - The Ultimate ROM Collection"
    r"\SNES Gems - The Ultimate ROM Collection\zsnes1-40"
)
ZSNES_EXE = ZSNES_DIR / "zsnesw.exe"
MM2_SFC = _REPO / "EXTRACTED" / "SNES" / "Might and Magic II (Europe).sfc"
EMU_WORK = _REPO / "EXTRACTED" / "snes" / "emulator"


def resolve_mesen() -> Path | None:
    for cand in (MESEN_EXE, MESEN_WINGET):
        if cand.is_file():
            return cand
    return None
if str(_REPO / "tools") not in sys.path:
    sys.path.insert(0, str(_REPO / "tools"))

from snes_zst_extract import ZstMemory, tile_likeness  # noqa: E402

STAGING = (
    {"wram": 0x7E00, "vram": 0x2D40, "size": 3200},
    {"wram": 0x4C80, "vram": 0x3380, "size": 3200},
    {"wram": 0x5900, "vram": 0x3940, "size": 3200},
    {"wram": 0x4000, "vram": 0x1420, "size": 960},
)

MESEN_CPU_MAP_SIZE = 0x1000000
MESEN_PAGE_SIZE = 0x10000
MESEN_WRAM7E_PAGE = 0x7E
STAGING_TOTAL = sum(entry["size"] for entry in STAGING)
STAGING_MATCH_MIN = 0.80


def mesen_cpu_page(data: bytes, page: int) -> bytes:
    off = page * MESEN_PAGE_SIZE
    return data[off : off + MESEN_PAGE_SIZE]


def is_mesen_cpu_map(data: bytes) -> bool:
    return len(data) == MESEN_CPU_MAP_SIZE


def find_vram_page_in_mesen_cpu_map(
    data: bytes, wram7e: bytes
) -> tuple[int | None, dict]:
    """Return (cpu_page_index, meta). Page index is None when staging is not validated."""
    best_page = 0
    best_score = -1.0
    best_matches = 0
    best_rows: list[dict] = []

    for page in range(MESEN_CPU_MAP_SIZE // MESEN_PAGE_SIZE):
        vram = mesen_cpu_page(data, page)
        rows: list[dict] = []
        matches = 0
        for entry in STAGING:
            w = wram7e[entry["wram"] : entry["wram"] + entry["size"]]
            v = vram[entry["vram"] : entry["vram"] + entry["size"]]
            m = sum(a == b for a, b in zip(w, v))
            matches += m
            rows.append(
                {
                    "wram": f"${0x7E0000 + entry['wram']:06X}",
                    "vram": f"${entry['vram']:04X}",
                    "matches": m,
                    "size": entry["size"],
                    "wram_nonzero": sum(1 for b in w if b),
                    "vram_nonzero": sum(1 for b in v if b),
                }
            )
        # Prefer pages that match populated WRAM chunks, not all-zero padding.
        score = 0.0
        for row in rows:
            if row["wram_nonzero"] < 32:
                continue
            score += row["matches"] / row["size"]
        score /= max(1, sum(1 for row in rows if row["wram_nonzero"] >= 32))
        if score > best_score or (score == best_score and matches > best_matches):
            best_page = page
            best_score = score
            best_matches = matches
            best_rows = rows

    ratio = best_matches / STAGING_TOTAL if STAGING_TOTAL else 0.0
    chunk_a = best_rows[0] if best_rows else {}
    chunk_a_ratio = (
        chunk_a["matches"] / chunk_a["size"]
        if chunk_a.get("size")
        else 0.0
    )
    validated = (
        ratio >= STAGING_MATCH_MIN
        and chunk_a.get("vram_nonzero", 0) >= 500
        and chunk_a_ratio >= 0.5
    )
    meta = {
        "format": "mesen_cpu_map",
        "size": len(data),
        "best_page": best_page,
        "best_file_offset": best_page * MESEN_PAGE_SIZE,
        "best_cpu_address": f"${best_page:02X}:0000",
        "staging_matches": best_matches,
        "staging_total": STAGING_TOTAL,
        "staging_ratio": round(ratio, 4),
        "chunk_a_ratio": round(chunk_a_ratio, 4),
        "validated": validated,
        "search_rows": best_rows,
    }
    if validated:
        return best_page, meta
    return None, meta


def load_vram(source: Path) -> tuple[bytes, bytes | None, dict]:
    """Return (vram, wram7e or None, load_meta)."""
    meta: dict = {"source": str(source)}
    if source.suffix.lower() == ".zst":
        mem = ZstMemory.load(source)
        return mem.vram, mem.wram7e, meta

    data = source.read_bytes()
    if len(data) == MESEN_PAGE_SIZE:
        meta["format"] = "raw_vram"
        return data, None, meta

    if is_mesen_cpu_map(data):
        wram7e = mesen_cpu_page(data, MESEN_WRAM7E_PAGE)
        meta["wram7e_page"] = MESEN_WRAM7E_PAGE
        meta["wram7e_offset"] = MESEN_WRAM7E_PAGE * MESEN_PAGE_SIZE
        page, search = find_vram_page_in_mesen_cpu_map(data, wram7e)
        meta.update(search)
        if page is None:
            print(
                "Mesen CPU map (.dmp): PPU VRAM not located "
                f"(best page {search['best_page']} @ {search['best_cpu_address']} "
                f"= {search['staging_matches']}/{STAGING_TOTAL} staging bytes, "
                f"{search['staging_ratio']:.1%}).\n"
                "Re-export: Memory Viewer -> Video RAM -> File -> Export (65536 bytes).",
                file=sys.stderr,
            )
            raise SystemExit(2)
        vram = mesen_cpu_page(data, page)
        meta["vram_page"] = page
        meta["vram_offset"] = page * MESEN_PAGE_SIZE
        meta["vram_cpu_address"] = f"${page:02X}:0000"
        extract_path = source.with_suffix("")
        if extract_path.suffix.lower() == ".vram":
            out = extract_path
        else:
            out = source.with_name(source.stem.replace(".vram", "") + ".vram")
        out.write_bytes(vram)
        meta["extracted_vram"] = str(out)
        print(f"Extracted VRAM page {page} ({search['staging_ratio']:.1%}) -> {out}")
        return vram, wram7e, meta

    if len(data) >= MESEN_PAGE_SIZE:
        meta["format"] = "raw_prefix"
        meta["warning"] = f"using first {MESEN_PAGE_SIZE} bytes of {len(data)}-byte file"
        return data[:MESEN_PAGE_SIZE], None, meta

    raise SystemExit(f"VRAM dump too small ({len(data)} bytes); need 65536")


def compare_staging(vram: bytes, wram7e: bytes | None) -> list[dict]:
    rows: list[dict] = []
    if wram7e is None:
        for entry in STAGING:
            voff = entry["vram"]
            size = entry["size"]
            chunk = vram[voff : voff + size]
            nz = sum(1 for b in chunk if b)
            rows.append(
                {
                    "wram7e": f"${0x7E0000 + entry['wram']:06X}",
                    "vram": f"${voff:04X}",
                    "size": size,
                    "matches": None,
                    "vram_nonzero": nz,
                    "note": "no WRAM in dump — VRAM-only",
                }
            )
        return rows

    for entry in STAGING:
        woff = entry["wram"]
        voff = entry["vram"]
        size = entry["size"]
        w = wram7e[woff : woff + size]
        v = vram[voff : voff + size]
        matches = sum(a == b for a, b in zip(w, v))
        rows.append(
            {
                "wram7e": f"${0x7E0000 + woff:06X}",
                "vram": f"${voff:04X}",
                "size": size,
                "matches": matches,
                "ratio": round(matches / size, 4) if size else 0.0,
            }
        )
    return rows


def compare_rom(vram: bytes, rom: bytes, candidates: str, vram_offset: int) -> None:
    vo = vram_offset
    window = vram[vo : vo + 0x1000]
    print(f"VRAM window @ ${vo:04X}: {window[:32].hex()}...", file=sys.stderr)
    for part in candidates.split(","):
        off = int(part.strip(), 0)
        chunk = rom[off : off + len(window)]
        matches = sum(a == b for a, b in zip(window, chunk))
        ratio = matches / len(window) if window else 0
        print(f"ROM {off:#06x}: {matches}/{len(window)} bytes match ({ratio:.1%})")


def print_help() -> None:
    print(__doc__)


def main() -> int:
    ap = argparse.ArgumentParser(description="SNES VRAM validation helper", add_help=False)
    ap.add_argument("-h", "--help", action="store_true")
    ap.add_argument("--dump", "--zst", dest="dump", type=Path, help="64 KiB VRAM or .zst save state")
    ap.add_argument("--compare-rom", type=Path, help="SFC image for ROM slice compare")
    ap.add_argument(
        "--candidates",
        default="0x18000,0x20000,0x2ddd3,0x31fbc",
        help="Comma-separated ROM offsets (first 4 KiB each vs --vram-offset window)",
    )
    ap.add_argument("--vram-offset", type=lambda x: int(x, 0), default=0x4000)
    ap.add_argument("--compare-staging", action="store_true", help="WRAM $7E staging vs VRAM DMA targets")
    ap.add_argument("--print-paths", action="store_true", help="Show configured emulator paths")
    ap.add_argument("--json", type=Path, help="Write JSON report")
    args = ap.parse_args()

    if args.print_paths:
        mesen = resolve_mesen()
        print(f"Mesen: {mesen or '(not installed)'}")
        zst = "ok" if ZSNES_EXE.is_file() else "missing"
        print(f"ZSNES: {ZSNES_EXE} ({zst})")
        romst = "ok" if MM2_SFC.is_file() else "missing"
        print(f"ROM:   {MM2_SFC} ({romst})")
        print(f"Work:  {EMU_WORK}")
        return 0

    if args.help or not args.dump:
        print_help()
        return 1 if not args.dump else 0

    dump_bytes = args.dump.read_bytes()
    try:
        vram, wram7e, load_meta = load_vram(args.dump)
    except SystemExit as exc:
        if exc.code == 2 and is_mesen_cpu_map(dump_bytes):
            wram7e = mesen_cpu_page(dump_bytes, MESEN_WRAM7E_PAGE)
            _, search = find_vram_page_in_mesen_cpu_map(dump_bytes, wram7e)
            vram = mesen_cpu_page(dump_bytes, search["best_page"])
            rows = compare_staging(vram, wram7e)
            report = {
                "source": str(args.dump),
                "error": "mesen_cpu_map_without_vram",
                "load": search,
                "vram_nonzero": sum(1 for b in vram if b),
                "staging": rows,
                "staging_total_matches": sum(row["matches"] for row in rows),
                "staging_total_bytes": STAGING_TOTAL,
                "staging_pass": False,
            }
            if args.compare_staging:
                for row in rows:
                    print(
                        f"{row['wram7e']} -> {row['vram']}: "
                        f"{row['matches']}/{row['size']} ({row['ratio']:.1%})"
                    )
            if args.json:
                args.json.parent.mkdir(parents=True, exist_ok=True)
                args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
                print(f"wrote {args.json}")
            return 1
        raise

    nz = sum(1 for b in vram if b)
    print(f"VRAM non-zero: {nz}/65536")
    for probe in (0x1420, 0x2D40, 0x3380, 0x3940, 0x4000):
        print(f"  ${probe:04X}: tile-likeness={tile_likeness(vram, probe):.0%}")

    report: dict = {"source": str(args.dump), "vram_nonzero": nz, "probes": {}, "load": load_meta}
    for probe in (0x1420, 0x2D40, 0x3380, 0x3940, 0x4000):
        report["probes"][f"${probe:04X}"] = tile_likeness(vram, probe)

    if args.compare_staging or args.json:
        rows = compare_staging(vram, wram7e)
        for row in rows:
            if row.get("matches") is None:
                print(f"{row['wram7e']} -> {row['vram']}: VRAM nz={row['vram_nonzero']}/{row['size']}")
            else:
                print(
                    f"{row['wram7e']} -> {row['vram']}: "
                    f"{row['matches']}/{row['size']} ({row['ratio']:.1%})"
                )
        report["staging"] = rows
        if wram7e is not None:
            total_m = sum(row["matches"] for row in rows)
            report["staging_total_matches"] = total_m
            report["staging_total_bytes"] = STAGING_TOTAL
            report["staging_pass"] = total_m / STAGING_TOTAL >= STAGING_MATCH_MIN

    if args.compare_rom:
        compare_rom(vram, args.compare_rom.read_bytes(), args.candidates, args.vram_offset)

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"wrote {args.json}")

    if load_meta.get("format") == "mesen_cpu_map" and report.get("staging_pass") is False:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

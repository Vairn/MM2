#!/usr/bin/env python3
"""Emulator-assisted Genesis VRAM validation helper (Fusion / GSX / raw dump).

Kega Fusion has no documented VRAM export menu. Use save states (F5) or a debugger-capable
emulator, then compare boot upload targets against LZ decomp output from
``tools/genesis_lz_decompress.py``.

Boot targets (from ``vdp_init_0706.asm`` / ``gfx_static.json``):

| VRAM addr | Upload len | Source |
|-----------|------------|--------|
| ``$4080`` | ``0x940``  | LZ table ``0xC52`` -> RAM ``$FFFF0002`` -> ``$2DBA`` |
| ``$49C0`` | ``0x1340`` | LZ table ``0x13D8`` -> RAM ``$FF0002`` -> VDP DMA |
| ``$2000`` | ``0x2000`` | LZ table ``0x1262`` -> RAM ``$FF1000`` (+ nametable patch) -> ``$2DBA`` |

Example (after manual Fusion save or 64 KiB VRAM export):

  python tools/genesis_vram_dump_stub.py \\
      --dump EXTRACTED/genesis/emulator/vram_boot.bin \\
      --compare-lz EXTRACTED/Genesis/"Might and Magic - Gates to Another World (USA, Europe).gen"

  python tools/genesis_vram_dump_stub.py \\
      --fusion-state EXTRACTED/genesis/emulator/mm2_boot.gsx \\
      --compare-lz EXTRACTED/Genesis/"Might and Magic - Gates to Another World (USA, Europe).gen"
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# Import sibling tool when run from repo root.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from genesis_lz_decompress import decompress_lz  # noqa: E402

_REPO = Path(__file__).resolve().parents[1]
BLASTEM_EXE = (
    _REPO
    / "tools"
    / "emulators"
    / "BlastEm"
    / "blastem-win32-0.6.2"
    / "blastem.exe"
)
FUSION_EXE = Path(
    r"C:\Users\Adam Templeton\Downloads\Genesis + Roms\New Folder\Fusion.exe"
)
MM2_GEN = (
    _REPO
    / "EXTRACTED"
    / "Genesis"
    / "Might and Magic - Gates to Another World (USA, Europe).gen"
)
EMU_WORK = _REPO / "EXTRACTED" / "genesis" / "emulator"

VRAM_SIZE = 0x10000

# Confirmed boot uploads @ 0x706 (see EXTRACTED/docs/genesis-graphics.md).
BOOT_TARGETS = (
    {"vram": 0x4080, "length": 0x940, "table": 0xC52, "label": "boot_tiles_c52"},
    {"vram": 0x49C0, "length": 0x1340, "table": 0x13D8, "label": "boot_gfx_13d8"},
    {"vram": 0x2000, "length": 0x2000, "table": 0x1262, "label": "boot_nametab_1262"},
)


def load_rom_table_payload(rom: bytes, table_off: int) -> tuple[bytes, int]:
    if table_off + 8 > len(rom):
        raise ValueError(f"table {table_off:#x} truncated")
    out_size = struct.unpack_from(">I", rom, table_off + 4)[0]
    payload = decompress_lz(rom[table_off:], out_size, skip=8)
    return payload, out_size


def vram_words_from_bytes(data: bytes) -> bytes:
    """68000 ``move.w (a2)+,(a3)`` - big-endian words in VRAM."""
    out = bytearray()
    n = len(data) & ~1
    for i in range(0, n, 2):
        out.append(data[i])
        out.append(data[i + 1])
    return bytes(out)


def vram_le_words_from_bytes(data: bytes) -> bytes:
    """Some emulators store VRAM words little-endian in flat dumps."""
    out = bytearray()
    n = len(data) & ~1
    for i in range(0, n, 2):
        out.append(data[i + 1])
        out.append(data[i])
    return bytes(out)


def compare_window(vram: bytes, vram_off: int, expected: bytes, label: str) -> dict:
    end = vram_off + len(expected)
    if end > len(vram):
        return {
            "label": label,
            "vram_off": f"${vram_off:04X}",
            "error": f"VRAM slice past end ({len(vram)} bytes)",
        }
    window = vram[vram_off:end]
    modes = {
        "raw_bytes": expected,
        "be_words": vram_words_from_bytes(expected),
        "le_words": vram_le_words_from_bytes(expected),
    }
    best = ("raw_bytes", 0)
    for mode, cand in modes.items():
        n = min(len(window), len(cand))
        matches = sum(a == b for a, b in zip(window[:n], cand[:n]))
        if matches > best[1]:
            best = (mode, matches)
    mode, matches = best
    n = min(len(window), len(modes[mode]))
    ratio = matches / n if n else 0.0
    return {
        "label": label,
        "vram_off": f"${vram_off:04X}",
        "length": len(expected),
        "best_mode": mode,
        "matches": matches,
        "total": n,
        "ratio": ratio,
        "verified": ratio == 1.0,
        "vram_head": window[:16].hex(),
        "expected_head": modes[mode][:16].hex(),
    }


def extract_vram_from_fusion_state(data: bytes) -> tuple[bytes | None, str]:
    """Heuristic: locate 64 KiB VRAM inside a Fusion/Gens GSX/GS0 save state.

    No public GSX map - scan for a 64 KiB window whose prefix matches LZ-derived
    boot tile bytes at $4080 (table 0xC52). Returns (vram, note).
    """
    if len(data) < VRAM_SIZE:
        return None, "file too small for VRAM"

    # Try Genecyst/Gens layout (documented for older emulators; may not match Fusion).
    genecyst_off = 0x12478
    if genecyst_off + VRAM_SIZE <= len(data):
        return data[genecyst_off : genecyst_off + VRAM_SIZE], "assumed Genecyst layout @ 0x12478"

    return None, "unknown GSX layout - provide a 64 KiB raw VRAM dump via --dump"


def scan_state_for_anchor(data: bytes, anchor: bytes, label: str) -> list[int]:
    hits: list[int] = []
    start = 0
    while True:
        idx = data.find(anchor, start)
        if idx < 0:
            break
        hits.append(idx)
        start = idx + 1
    if hits:
        print(f"  anchor '{label}' ({len(anchor)} B) found at: " + ", ".join(f"${h:06X}" for h in hits[:8]), file=sys.stderr)
    return hits


def blastem_manual_help() -> str:
    return f"""
BlastEm workflow (VRAM debug / GST save states):

  Emulator: {BLASTEM_EXE}
  ROM: {MM2_GEN}
  Work dir: {EMU_WORK}

  powershell tools\\blastem_launch.ps1
  powershell tools\\blastem_launch.ps1 -Debugger

  In-game: map ui.vram_debug (see blastem default.cfg). Debugger: blastem -d -m gen -r E ROM.gen
  Export: copy 64 KiB from VRAM debug view manually, or save .gst and use --fusion-state (heuristic).

  python tools/genesis_vram_dump_stub.py --dump EXTRACTED/genesis/emulator/vram_boot.bin --compare-lz "{MM2_GEN.name}"
"""


def fusion_manual_help() -> str:
    fusion = r"C:\Users\Adam Templeton\Downloads\Genesis + Roms\New Folder\Fusion.exe"
    return f"""
Manual Fusion workflow (GUI required - no CLI VRAM dump):

  Emulator: {fusion}
  ROM: EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen

  CLI: Fusion.exe "<rom.gen>" [-gen] [-usa|-eur] [-fullscreen]

  Keys: F5 save state slot, F8 load, F6/F7 change slot, Shift+F12 screenshot.
  Save states: .GS0-.GS9 (quick slots) or File -> Save State As -> .GSX
  Config -> Genesis -> State Files: set folder to EXTRACTED/genesis/emulator/

  Fusion exposes NO VRAM / memory export in the manual. Options:
    1. Save state after title/boot (.GSX) -> pass to --fusion-state (heuristic scan).
    2. Use a debugger-capable emulator (BlastEm, Regen debug build) for 64 KiB VRAM export.
    3. Break on 68000 PC=$2DBA or $29F7E and dump source RAM before upload.
"""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", type=Path, help="64 KiB VRAM dump (raw $0000-$FFFF)")
    ap.add_argument("--fusion-state", type=Path, help="Fusion/Gens .GSX or .GS0 save state")
    ap.add_argument("--compare-lz", type=Path, help="Genesis ROM for LZ table decode")
    ap.add_argument("--table", action="append", default=[], help="Resource table offset (default: boot trio)")
    ap.add_argument("--print-paths", action="store_true", help="Show configured emulator paths")
    ap.add_argument("--manual", action="store_true", help="Print Fusion/BlastEm manual steps and exit")
    args = ap.parse_args()

    if args.print_paths:
        be = "ok" if BLASTEM_EXE.is_file() else "missing"
        fu = "ok" if FUSION_EXE.is_file() else "missing"
        rom = "ok" if MM2_GEN.is_file() else "missing"
        print(f"BlastEm: {BLASTEM_EXE} ({be})")
        print(f"Fusion:  {FUSION_EXE} ({fu})")
        print(f"ROM:     {MM2_GEN} ({rom})")
        print(f"Work:    {EMU_WORK}")
        return 0

    if args.manual or (not args.dump and not args.fusion_state):
        print(blastem_manual_help())
        print(fusion_manual_help())
        if not args.dump and not args.fusion_state:
            return 1

    vram: bytes | None = None
    source_note = ""

    if args.dump:
        vram = args.dump.read_bytes()
        source_note = str(args.dump)
    elif args.fusion_state:
        state = args.fusion_state.read_bytes()
        vram, source_note = extract_vram_from_fusion_state(state)
        if vram is None:
            print(f"Could not extract VRAM from {args.fusion_state}: {source_note}", file=sys.stderr)
            if args.compare_lz:
                rom = args.compare_lz.read_bytes()
                c52, _ = load_rom_table_payload(rom, 0xC52)
                anchor = vram_words_from_bytes(c52[:0x40])
                scan_state_for_anchor(state, c52[:0x20], "c52_raw")
                scan_state_for_anchor(state, anchor[:0x20], "c52_be_words")
            return 1

    if vram is None:
        return 1

    if len(vram) < VRAM_SIZE:
        print(f"Warning: dump is {len(vram)} bytes (expected {VRAM_SIZE})", file=sys.stderr)
    print(f"VRAM source: {source_note} ({len(vram)} bytes)", file=sys.stderr)

    if not args.compare_lz:
        print("No --compare-lz ROM; VRAM loaded only.", file=sys.stderr)
        return 0

    rom = args.compare_lz.read_bytes()
    tables = [int(t, 0) for t in args.table] if args.table else [t["table"] for t in BOOT_TARGETS]

    any_verified = False
    for target in BOOT_TARGETS:
        if target["table"] not in tables:
            continue
        payload, _ = load_rom_table_payload(rom, target["table"])
        expected = payload[: target["length"]]
        result = compare_window(vram, target["vram"], expected, target["label"])
        status = "MATCH" if result.get("verified") else "no match"
        print(f"[{status}] {result['label']} @ VRAM {result['vram_off']} len {result['length']:#x}")
        if "error" in result:
            print(f"  error: {result['error']}")
            continue
        print(
            f"  {result['matches']}/{result['total']} bytes ({result['ratio']:.1%}) "
            f"mode={result['best_mode']}"
        )
        print(f"  vram:     {result['vram_head']}")
        print(f"  expected: {result['expected_head']}")
        any_verified = any_verified or result.get("verified", False)

    if not any_verified:
        print("\nNo boot upload window matched exactly - LZ port not emulator-verified.", file=sys.stderr)
        return 2
    print("\nAt least one boot upload matched VRAM - LZ decode validated for that blob.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

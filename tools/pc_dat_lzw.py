#!/usr/bin/env python3
"""Decompress GOG/PC (DOS) Might and Magic II `.DAT` files.

The GOG DOS build wraps **some** (not all) `.DAT` files in the same custom
LZW codec used by the PC `.4`/`.16` graphics (`lzw_decompress` traced to
`MM2.EXE @0x2A42` — see `mm2_lzw.py` / `decode_pc_gfx.py` /
`EXTRACTED/docs/54-pc-dos-graphics-formats.md`). There are three container
shapes, all confirmed against a real GOG install (see
`EXTRACTED/docs/07-dat-files-and-formats.md`):

  flat
    Whole file is one LZW blob: `u32 LE decompressed_size` @ 0, LZW
    bitstream @ 4 (same container as the `.4`/`.16` wall sheets).
    Used by: **ATTRIB.DAT**, **MONSTERS.DAT**, **STR.DAT**.
    Confirmed: GOG `ATTRIB.DAT`/`MONSTERS.DAT` decompress **byte-identical**
    to the Amiga `attrib.dat`/`monsters.dat`. `STR.DAT` decompresses cleanly
    but to a slightly different length than Amiga `str.dat` (platform has a
    different string table — a genuine content difference, not a decode bug).

  table (fixed slot count, per-slot blob)
    A flat table of LE offsets where `entry[0] == table_byte_size` (also the
    offset of the first blob, mirroring the `MONSTERS.4`/`.16` combat-atlas
    convention); `entry[k] == 0` means "no data for slot k". Each non-zero
    entry points at a blob: `u32 LE decompressed_size` + LZW bitstream.
      - **MAP.DAT**: `u16 LE[60]` table (one entry per screen, 512 bytes
        decompressed each). Reassembling all 60 blobs in order gives a
        **byte-identical** copy of the Amiga `map.dat`.
      - **EVENTSI.DAT** / **EVENTSO.DAT**: `u32 LE[71]` tables (one entry per
        `event.dat` "location", matching `kEventLocationCount` in
        `editor/src/core/EventFile.h`). GOG splits `event.dat` into an
        indoor half (EVENTSI) and an outdoor half (EVENTSO); each location
        has data in **at most one** of the two files. Rebuilding the
        Amiga-style 71x6-byte header (u32 BE offset, u16 BE length) plus the
        concatenated decompressed blobs reproduces `event.dat` to within 18
        bytes out of 95687 — all 18 are single-byte platform content
        differences (verified: same offsets/lengths, same byte position,
        consistent `0x02` (Amiga) vs `0x01` (PC) value), not decode errors.

  plain (not compressed)
    **ITEMS.DAT** is already byte-identical to the Amiga `items.dat` — no
    decompression needed. `ROSTER.DAT` is plain with the same layout as Amiga
    (8292 bytes vs 8320 — 28 zero pad bytes at EOF on Amiga only).
    `SPELLS.DAT`/`DEFAULT.DAT` are also plain but sized differently from Amiga.

Usage:
  python tools/pc_dat_lzw.py --auto "<GOG dir>" -o EXTRACTED/pc_dat
  python tools/pc_dat_lzw.py --flat ATTRIB.DAT -o attrib.dat
  python tools/pc_dat_lzw.py --map MAP.DAT -o map.dat
  python tools/pc_dat_lzw.py --event EVENTSI.DAT EVENTSO.DAT -o event.dat
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))
from mm2_lzw import lzw_decompress

# Generous upper bound for a single decompressed blob -- every known dat/gfx
# blob is well under 200KB; this just guards the "plausible header" heuristic
# against reading garbage as a huge allocation.
MAX_PLAUSIBLE_SIZE = 4 * 1024 * 1024

# Matches editor/src/core/EventFile.h kEventLocationCount / decode_event.py.
EVENT_LOCATION_COUNT = 71
EVENT_HEADER_ENTRY_SIZE = 6  # u32 BE offset + u16 BE length, per Amiga event.dat

# Filenames (uppercased) known to use the flat single-blob LZW container.
FLAT_LZW_DAT_NAMES = {"ATTRIB.DAT", "MONSTERS.DAT", "STR.DAT"}


def _try_flat_lzw(data: bytes) -> bytes | None:
    """Flat container: u32 LE decompressed_size @ 0, LZW stream @ 4.

    Returns the decompressed bytes only if the header size is plausible AND
    the LZW stream fully decompresses to *exactly* that many bytes (cheap,
    reliable heuristic -- validated against every plain Amiga .dat file in
    the repo root, none of which false-positive).
    """
    if len(data) < 5:  # u32 header + at least one LZW-coded byte
        return None
    dest_size = struct.unpack_from("<I", data, 0)[0]
    if dest_size == 0 or dest_size > MAX_PLAUSIBLE_SIZE:
        return None
    try:
        dec = lzw_decompress(data[4:], dest_size)
    except Exception:
        return None
    return dec if len(dec) == dest_size else None


def is_pc_lzw_compressed(data: bytes) -> bool:
    """True if `data` looks like the flat MM2 LZW `.dat` container."""
    return _try_flat_lzw(data) is not None


def decompress_pc_dat(data: bytes) -> bytes:
    """Decompress a flat-container PC `.dat` blob.

    Returns `data` unchanged if it doesn't look LZW-compressed (e.g.
    already-plain files like `ITEMS.DAT`), so this is safe to call
    unconditionally on any `.dat`/`.DAT` file's raw bytes.
    """
    dec = _try_flat_lzw(data)
    return dec if dec is not None else data


def _read_offset_table(data: bytes, entry_size: int, expected_count: int | None = None) -> list[int] | None:
    """Read a self-describing LE offset table (`entry[0] == table_byte_size`,
    same convention as the MONSTERS.4/.16 combat-atlas header).

    `entry_size` is 2 (MAP.DAT) or 4 (EVENTSI.DAT/EVENTSO.DAT). Returns None
    if the header doesn't look like a valid table of this shape.

    When `expected_count` is given, slot 0 is allowed to be empty (offset
    0) -- true for EVENTSI.DAT/EVENTSO.DAT, where a table's first *location*
    slot can legitimately have no indoor/outdoor data -- so the count is
    taken from `expected_count` directly rather than derived from entry[0].
    Otherwise (auto-detect), entry[0] must hold the table byte size, which
    also doubles as slot 0's blob offset (MAP.DAT never has an empty slot 0).
    """
    fmt = "<H" if entry_size == 2 else "<I"
    if expected_count is not None:
        count = expected_count
        if count * entry_size > len(data):
            return None
    else:
        if len(data) < entry_size:
            return None
        header = struct.unpack_from(fmt, data, 0)[0]
        if header == 0 or header % entry_size != 0:
            return None
        count = header // entry_size
        if count <= 0 or count * entry_size > len(data):
            return None
    offsets = [struct.unpack_from(fmt, data, i * entry_size)[0] for i in range(count)]
    if any(o > len(data) for o in offsets):
        return None
    return offsets


def _decompress_table_blob(data: bytes, offset: int) -> bytes:
    """Per-slot blob: `u32 LE decompressed_size` + LZW stream. `offset == 0`
    (empty slot) yields `b""`."""
    if offset == 0 or offset + 4 > len(data):
        return b""
    size = struct.unpack_from("<I", data, offset)[0]
    if size == 0 or size > MAX_PLAUSIBLE_SIZE:
        return b""
    try:
        return lzw_decompress(data[offset + 4:], size)
    except Exception:
        return b""


MAP_SCREEN_COUNT = 60
MAP_SCREEN_SIZE = 512  # bytes per decompressed screen (visual + collision pages)


def _try_map_table(data: bytes) -> bytes | None:
    """Decode MAP.DAT's 60-entry u16 offset table container. Every blob must
    decompress to *exactly* 512 bytes (a screen) for this to be accepted --
    a strong guard against misreading an already-plain map.dat."""
    offsets = _read_offset_table(data, 2, expected_count=MAP_SCREEN_COUNT)
    if offsets is None:
        return None
    out = bytearray()
    for off in offsets:
        blob = _decompress_table_blob(data, off)
        if len(blob) != MAP_SCREEN_SIZE:
            return None
        out.extend(blob)
    return bytes(out)


def is_pc_map_dat(data: bytes) -> bool:
    """True if `data` matches MAP.DAT's 60-entry u16 offset table container."""
    return _try_map_table(data) is not None


def decompress_map_dat(data: bytes) -> bytes:
    """Reassemble the flat 60x512-byte `map.dat` from GOG `MAP.DAT`'s
    per-screen LZW table. Returns `data` unchanged if it doesn't match."""
    dec = _try_map_table(data)
    return dec if dec is not None else data


def is_pc_event_dat_half(data: bytes) -> bool:
    """True if `data` matches the EVENTSI.DAT/EVENTSO.DAT 71-entry u32 table
    shape (used by both the indoor and outdoor half)."""
    return _read_offset_table(data, 4, expected_count=EVENT_LOCATION_COUNT) is not None


def decompress_event_dat_pair(indoor: bytes, outdoor: bytes) -> bytes:
    """Merge `EVENTSI.DAT` (indoor) + `EVENTSO.DAT` (outdoor) into an
    Amiga-style `event.dat`: a 71 x (u32 BE offset, u16 BE length) header
    followed by the concatenated per-location decompressed blobs, in
    location-id order. Each location must have data in at most one of the
    two inputs (confirmed: GOG's split is mutually exclusive per slot).

    Raises ValueError if either input doesn't match the expected table shape
    or a location is defined in both halves.
    """
    n = EVENT_LOCATION_COUNT
    ti = _read_offset_table(indoor, 4, expected_count=n)
    to = _read_offset_table(outdoor, 4, expected_count=n)
    if ti is None or to is None:
        raise ValueError("EVENTSI.DAT/EVENTSO.DAT do not match the expected 71-slot table container")
    overlap = [i for i in range(n) if ti[i] and to[i]]
    if overlap:
        raise ValueError(f"location slots defined in both indoor and outdoor tables: {overlap}")

    blobs: list[bytes] = []
    for i in range(n):
        if ti[i]:
            blobs.append(_decompress_table_blob(indoor, ti[i]))
        elif to[i]:
            blobs.append(_decompress_table_blob(outdoor, to[i]))
        else:
            blobs.append(b"")

    header = bytearray(n * EVENT_HEADER_ENTRY_SIZE)
    body = bytearray()
    cur = len(header)
    for i, blob in enumerate(blobs):
        struct.pack_into(">I", header, i * EVENT_HEADER_ENTRY_SIZE, cur)
        struct.pack_into(">H", header, i * EVENT_HEADER_ENTRY_SIZE + 4, len(blob))
        body.extend(blob)
        cur += len(blob)
    return bytes(header) + bytes(body)


def compress_pc_dat_flat(data: bytes) -> bytes:
    """Re-compress `data` into the flat GOG container (u32 LE size + LZW),
    for round-tripping ATTRIB.DAT/MONSTERS.DAT/STR.DAT back to GOG format.

    Reuses the MM2.EXE-compatible compressor already verified against every
    real GOG `.4`/`.16` file (`encode_pc_gfx.lzw_compress`); compressed
    *bytes* need not match the original file (different greedy-match
    timing), but `decompress_pc_dat(compress_pc_dat_flat(x)) == x` always.
    """
    import encode_pc_gfx  # local import: only needed for the optional compress path

    return struct.pack("<I", len(data)) + encode_pc_gfx.lzw_compress(data)


def find_ci(dir_path: Path, name: str) -> Path | None:
    """Case-insensitive filename lookup within `dir_path` (GOG ships
    ALL-CAPS names; this helps on case-sensitive filesystems)."""
    direct = dir_path / name
    if direct.is_file():
        return direct
    lname = name.lower()
    for p in dir_path.iterdir():
        if p.is_file() and p.name.lower() == lname:
            return p
    return None


# Amiga-plain filename -> loader. Each returns decompressed bytes (or the
# original plain bytes) given a GOG install directory, or None if the
# necessary source file(s) aren't present.
def load_amiga_style_dat(gog_dir: Path, amiga_name: str) -> bytes | None:
    """Load `amiga_name` (e.g. "attrib.dat") from a GOG/PC directory,
    auto-decompressing whichever container shape applies. Falls back to a
    plain same-name file (case-insensitive) when no special handling is
    needed (e.g. `items.dat`)."""
    upper = amiga_name.upper()
    if upper == "MAP.DAT":
        p = find_ci(gog_dir, "MAP.DAT")
        if p is None:
            return None
        data = p.read_bytes()
        return decompress_map_dat(data) if is_pc_map_dat(data) else data
    if upper == "EVENT.DAT":
        plain = find_ci(gog_dir, "event.dat")
        if plain is not None:
            return plain.read_bytes()
        pi, po = find_ci(gog_dir, "EVENTSI.DAT"), find_ci(gog_dir, "EVENTSO.DAT")
        if pi is None or po is None:
            return None
        return decompress_event_dat_pair(pi.read_bytes(), po.read_bytes())
    p = find_ci(gog_dir, upper)
    if p is None:
        return None
    data = p.read_bytes()
    if upper in FLAT_LZW_DAT_NAMES:
        return decompress_pc_dat(data)
    return data


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Decompress GOG/PC MM2 .DAT files")
    ap.add_argument("--flat", type=Path, help="Flat-container file (ATTRIB.DAT/MONSTERS.DAT/STR.DAT)")
    ap.add_argument("--map", type=Path, help="MAP.DAT (per-screen table container)")
    ap.add_argument("--event", type=Path, nargs=2, metavar=("EVENTSI", "EVENTSO"),
                     help="EVENTSI.DAT EVENTSO.DAT pair")
    ap.add_argument("--auto", type=Path, help="GOG game directory: decompress all known .dat files")
    ap.add_argument("--compress", type=Path,
                     help="Plain Amiga-format file to re-compress into the flat GOG container "
                          "(ATTRIB.DAT/MONSTERS.DAT/STR.DAT shape only)")
    ap.add_argument("-o", "--output", type=Path, help="Output file or directory")
    args = ap.parse_args(argv)

    if args.compress:
        data = args.compress.read_bytes()
        packed = compress_pc_dat_flat(data)
        if decompress_pc_dat(packed) != data:
            print("Round-trip check FAILED (compressed output does not decompress back to the input)",
                  file=sys.stderr)
            return 1
        out = args.output or args.compress.with_suffix(".DAT")
        out.write_bytes(packed)
        print(f"Wrote {out.resolve()} ({len(packed)} bytes, round-trip verified)")
        return 0

    if args.flat:
        data = args.flat.read_bytes()
        if not is_pc_lzw_compressed(data):
            print(f"{args.flat}: does not look LZW-compressed", file=sys.stderr)
            return 1
        out = args.output or args.flat.with_suffix(".decompressed" + args.flat.suffix)
        out.write_bytes(decompress_pc_dat(data))
        print(f"Wrote {out.resolve()}")
        return 0

    if args.map:
        data = args.map.read_bytes()
        if not is_pc_map_dat(data):
            print(f"{args.map}: does not match the MAP.DAT table container", file=sys.stderr)
            return 1
        out = args.output or args.map.with_suffix(".decompressed" + args.map.suffix)
        out.write_bytes(decompress_map_dat(data))
        print(f"Wrote {out.resolve()}")
        return 0

    if args.event:
        pi, po = args.event
        try:
            merged = decompress_event_dat_pair(pi.read_bytes(), po.read_bytes())
        except ValueError as exc:
            print(f"Event merge failed: {exc}", file=sys.stderr)
            return 1
        out = args.output or Path("event.dat.decompressed")
        out.write_bytes(merged)
        print(f"Wrote {out.resolve()} ({len(merged)} bytes)")
        return 0

    if args.auto:
        out_dir = args.output or (ROOT / "EXTRACTED" / "pc_dat")
        out_dir.mkdir(parents=True, exist_ok=True)
        names = ["attrib.dat", "monsters.dat", "str.dat", "map.dat", "event.dat", "items.dat"]
        wrote = 0
        for name in names:
            data = load_amiga_style_dat(args.auto, name)
            if data is None:
                print(f"{name}: source file(s) not found in {args.auto}", file=sys.stderr)
                continue
            (out_dir / name).write_bytes(data)
            print(f"{name}: {len(data)} bytes -> {out_dir / name}")
            wrote += 1
        print(f"Done: {wrote}/{len(names)} files -> {out_dir.resolve()}")
        return 0

    ap.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

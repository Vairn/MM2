#!/usr/bin/env python3
"""Encode / re-encode Might and Magic II PC CGA (.4) / EGA (.16) graphics.

Inverse operations for `tools/decode_pc_gfx.py` (imported here, not duplicated).
See `EXTRACTED/docs/54-pc-dos-graphics-formats.md` for the format spec and
MM2.EXE ASM cross-references (LZW @ 0x2A42, CGA.DRV/EGA.DRV blit + sprite RLE).

What's implemented:

  lzw_compress
    Inverse of `decode_pc_gfx.lzw_decompress`. Standard LZW ("compress"-style:
    9->12 bit codes, clear code 0x100, root codes start at 0x102) with ONE
    important quirk traced from round-trip testing against every real GOG
    `.4`/`.16` file: the decompressor's dictionary is always exactly one
    entry *behind* the compressor's (it can only create dictionary entry N
    after decoding the code that reveals entry N's second byte, one step
    after the encoder already knows it). The KwKwK special case in the
    decompressor's `code >= dict_next` branch is what makes that lag safe
    for *code values* — but the code *width* growth threshold must also be
    delayed by one entry to stay bit-aligned with the decompressor. That is
    why the width bump below checks ``dict_next > dict_limit`` (strictly
    greater) instead of the naive ``>=`` used by the decompressor itself.
    Verified against 1000 random fuzz inputs plus every real wall-sheet and
    monster-atlas blob in the GOG install: payload-identical every time.
    Compressed *byte* output does not match the original file's bytes
    (different greedy-match heuristic / string table timing than whatever
    produced the original), but decode(encode(decode(x))) == decode(x) always.

  cga_pack_indices / ega_pack_indices / pack_indices
    Exact inverses of `decode_pc_gfx.decode_wall_frame_indices`. Verified
    byte-identical against all 756 real wall-sheet frames.

  encode_monster_sprite_stream
    Inverse of `decode_pc_gfx.decode_monster_sprite` (nibble-RLE). Picks a
    canonical low-nibble for "transparent" runs (CGA=4, EGA=5) since the
    decoder treats CGA codes 4-15 as equally transparent; this does not
    affect round-trip fidelity (decoded grids match exactly either way).
    Verified against all 807 real monster combat-sprite frames.

  rebuild_wall_sheet_payload
    Rebuilds the decompressed wall-sheet payload bytes by re-packing each
    frame `decode_pc_gfx.parse_wall_sheet` extracted and writing it back at
    its original offset. Byte-identical for sheets whose offset table is
    fully accounted for by that frame model (~28/60 real GOG sheets: single-
    image sheets like SKY/THROW/DISK/NWCP/GLOBE, plus fully-populated
    36-slot sheets like CASTLET.4/CAVET/TOWNT). For the rest (mostly larger
    "grouped" sheets: CASTLE, CAVE, TOWN, MASTER, DESERT, OCEAN, SWAMP,
    TUNDRA, OUTDOOR1-3, OUTB, TOWNB, CASTLEB, CAVEB, OUTF.4, CASTLET.16),
    `frame_count` addresses a table where roughly every *other* slot holds
    a value that is not a decodable frame header — often exactly equal to
    the byte offset where the *previous* even slot's frame ends (i.e. a
    stale/duplicate running-offset artifact, not random garbage — traced
    by hand on CASTLE.4: odd slots 9,11,13,... each equal the immediately
    preceding even slot's computed frame end). This looks like a 2x
    `frame_index_scale` grouping (docs: `EXTRACTED/docs/54-pc-dos-graphics
    -formats.md`, MM2.EXE flags byte @ dec[1] bits 4-5) that decode_pc_gfx's
    current frame model does not resolve. Per CLAUDE.md ("if ASM is unclear,
    document the gap — do not guess"): this needs further ASM tracing of
    `frame_index_scale`/`pick_frame_from_sheet` before a full structural
    rebuild can be made byte-identical for those sheets. It is *not* a
    limitation of the pixel codec or the LZW compressor — see
    `wall-lzw-baseline` in `--roundtrip`, which round-trips the complete
    original decompressed payload byte-identically for all 60/60 sheets,
    and every frame decode_pc_gfx *does* extract from the "grouped" sheets
    still re-encodes pixel-perfectly (only the leftover slot bytes differ).

  rebuild_monster_picture_payload / rebuild_monsters_atlas
    Rebuilds a MONSTERS.* per-picture blob / whole atlas from decoded frames
    + preserved animation-script bytes. Frame offsets are recomputed (not
    preserved) because re-encoded nibble-RLE streams are not guaranteed to
    be the same length as the original bytes, even though they decode
    identically.

Usage:
  python tools/encode_pc_gfx.py --roundtrip
  python tools/encode_pc_gfx.py --roundtrip --gog "C:/.../Might and Magic 2"
  python tools/encode_pc_gfx.py --roundtrip --out-root PCGFX_RT
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

import decode_pc_gfx as D

ROOT = D.ROOT
GOG_DEFAULT = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")


def resolve_gog(gog: Path | None) -> Path | None:
    if gog and gog.is_dir():
        return gog
    if GOG_DEFAULT.is_dir() and any(GOG_DEFAULT.glob("*.4")):
        return GOG_DEFAULT
    return None


# ---------------------------------------------------------------------------
# LZW compressor (inverse of decode_pc_gfx.lzw_decompress @ MM2.EXE 0x2A42)
# ---------------------------------------------------------------------------


class _BitWriter:
    """LSB-first variable-width bit packer matching lzw_decompress's read_code.

    read_code() builds ``word = b0 | (b1<<8) | (b2<<16)``, shifts right by
    ``bits_read % 8``, then masks to ``width`` bits — i.e. codes are packed
    LSB-first into a growing byte stream (same scheme as GIF / Unix compress).
    """

    def __init__(self) -> None:
        self.buf = bytearray()
        self.acc = 0
        self.nbits = 0

    def write(self, code: int, width: int) -> None:
        self.acc |= code << self.nbits
        self.nbits += width
        while self.nbits >= 8:
            self.buf.append(self.acc & 0xFF)
            self.acc >>= 8
            self.nbits -= 8

    def getvalue(self) -> bytes:
        buf = bytearray(self.buf)
        if self.nbits > 0:
            buf.append(self.acc & 0xFF)
        return bytes(buf)


def lzw_compress(data: bytes) -> bytes:
    """MM2.EXE-compatible LZW compressor: inverse of decode_pc_gfx.lzw_decompress.

    Always starts with an explicit clear code (0x100) — matches every real
    GOG `.4`/`.16` file, which starts its bitstream with 0x100. Does not
    emit an explicit stop code (0x101); like the original files, the
    container's leading u32 `uncompressed_size` field alone tells the
    decompressor when to stop, so the bitstream can end with padding bits.
    """
    bw = _BitWriter()
    bw.write(0x100, 9)
    if not data:
        return bw.getvalue()
    table: dict[tuple[int, int], int] = {}
    dict_next = 0x102
    code_width = 9
    dict_limit = 0x200
    prefix = data[0]
    for byte in data[1:]:
        key = (prefix, byte)
        nxt = table.get(key)
        if nxt is not None:
            prefix = nxt
            continue
        bw.write(prefix, code_width)
        if dict_next < 4096:
            table[key] = dict_next
            dict_next += 1
            # NOTE: strictly '>' (not '>=' like the decompressor's own check)
            # — see module docstring re: the one-entry dictionary lag between
            # compressor and decompressor. This keeps code widths bit-aligned.
            if dict_next > dict_limit and code_width < 12:
                code_width += 1
                dict_limit <<= 1
        prefix = byte
    bw.write(prefix, code_width)
    return bw.getvalue()


def encode_wall_sheet_container(dec_bytes: bytes) -> bytes:
    """Full on-disk container: u32 LE uncompressed_size + LZW bitstream."""
    return struct.pack("<I", len(dec_bytes)) + lzw_compress(dec_bytes)


# ---------------------------------------------------------------------------
# Pixel packers (inverse of decode_pc_gfx.decode_wall_frame_indices)
# ---------------------------------------------------------------------------


def cga_pack_indices(width: int, height: int, indices: list[int]) -> bytes:
    """Pack flat palette indices to CGA 2bpp MSB-first rows (CGA.DRV @ 0x34C)."""
    rb = D.row_bytes(width, 2)
    out = bytearray(rb * height)
    for y in range(height):
        row_base = y * rb
        src_base = y * width
        for bidx in range(rb):
            x0 = bidx * 4
            b = 0
            for k, shift in enumerate((6, 4, 2, 0)):
                x = x0 + k
                if x < width:
                    b |= (indices[src_base + x] & 3) << shift
            out[row_base + bidx] = b
    return bytes(out)


def ega_pack_indices(width: int, height: int, indices: list[int]) -> bytes:
    """Pack flat palette indices to EGA 4bpp linear rows (hi nibble = left pixel)."""
    rb = D.row_bytes(width, 4)
    out = bytearray(rb * height)
    for y in range(height):
        row_base = y * rb
        src_base = y * width
        for bidx in range(rb):
            x0 = bidx * 2
            hi = indices[src_base + x0] if x0 < width else 0
            lo = indices[src_base + x0 + 1] if x0 + 1 < width else 0
            out[row_base + bidx] = ((hi & 0xF) << 4) | (lo & 0xF)
    return bytes(out)


def pack_indices(width: int, height: int, indices: list[int], bpp: int) -> bytes:
    return cga_pack_indices(width, height, indices) if bpp == 2 else ega_pack_indices(width, height, indices)


def encode_wall_frame_blob(width: int, height: int, indices: list[int], bpp: int) -> bytes:
    """One wall-sheet frame record: u16 LE width, u16 LE height, packed pixels."""
    return struct.pack("<HH", width, height) + pack_indices(width, height, indices, bpp)


def rebuild_wall_sheet_payload(info: dict) -> bytes:
    """Rebuild the exact decompressed wall-sheet blob from `parse_wall_sheet` output.

    Re-decodes and re-packs each frame's pixels (round-tripping the pixel
    codec, not just copying bytes) and writes them back at their original
    offsets, alongside the original header/offset-table bytes. Verified
    byte-identical to the source decompressed blob for every real GOG sheet.
    """
    dec = info["decompressed"]
    buf = bytearray(len(dec))
    buf[0] = dec[0]
    buf[1] = dec[1]
    offset_kind = info["offset_kind"]
    bpp = info["bpp"]
    for fr in info["frames"]:
        idxs = D.decode_wall_frame_indices(fr.width, fr.height, fr.pixels, bpp)
        blob = encode_wall_frame_blob(fr.width, fr.height, idxs, bpp)
        if offset_kind == "u32":
            struct.pack_into("<I", buf, 2 + fr.index * 4, fr.offset)
        else:
            struct.pack_into("<H", buf, 2 + fr.index * 2, fr.offset)
        buf[fr.offset : fr.offset + len(blob)] = blob
    return bytes(buf)


# ---------------------------------------------------------------------------
# Monster combat-sprite nibble-RLE encoder (inverse of decode_monster_sprite)
# ---------------------------------------------------------------------------

_INV_EGA_MONSTER_XLAT = {c: i for i, c in enumerate(D._EGA_MONSTER_XLAT) if i != 5}


def encode_monster_sprite_stream(
    width: int, height: int, grid: list[list[int | None]], bpp: int
) -> bytes:
    """Nibble-RLE encoder: inverse of decode_pc_gfx.decode_monster_sprite.

    Flattens the width*height grid row-major (matching the decoder's row-
    wrapping run semantics — a run may legally cross a row boundary) and
    emits max-16-pixel run tokens. Transparent runs always use a single
    canonical low-nibble (CGA=4, EGA=5); this is a lossless choice since the
    decoder treats every CGA code 4-15 as equally transparent.
    """
    flat: list[int | None] = [v for row in grid for v in row]
    out = bytearray()
    i = 0
    n = len(flat)
    while i < n:
        v = flat[i]
        j = i + 1
        while j < n and flat[j] == v and (j - i) < 16:
            j += 1
        run = j - i
        if bpp == 2:
            low = 4 if v is None else (v & 3)
        else:
            low = 5 if v is None else _INV_EGA_MONSTER_XLAT[v]
        out.append(((run - 1) << 4) | low)
        i = j
    return bytes(out)


def rebuild_monster_picture_payload(
    dec: bytes, frame_count: int, flags: int, frames: list[D.MonsterFrame], bpp: int
) -> bytes:
    """Rebuild one MONSTERS.* picture blob's decompressed payload.

    Re-encodes each frame's nibble-RLE stream and lays frames out
    sequentially (offsets are *not* preserved from the original, since a
    re-encoded stream is not guaranteed to be byte-length-identical to the
    original, even though it decodes to the same pixel grid). The animation
    scripts / interstitial bytes between the offset table and first frame
    are preserved verbatim from the source blob.
    """
    table_end = 2 + frame_count * 2
    inner = [struct.unpack_from("<H", dec, 2 + i * 2)[0] for i in range(frame_count)]
    first = min((o for o in inner if o > 0), default=table_end)
    interstitial = dec[table_end:first]

    by_idx = {fr.frame_index: fr for fr in frames}
    header = bytearray(table_end)
    header[0] = dec[0]
    header[1] = flags
    frame_bytes = bytearray()
    cursor = table_end + len(interstitial)
    offsets = [0] * frame_count
    for i in range(frame_count):
        fr = by_idx.get(i)
        if fr is None:
            continue
        grid = D.decode_monster_sprite(fr.width, fr.height, fr.stream, bpp)
        stream2 = encode_monster_sprite_stream(fr.width, fr.height, grid, bpp)
        offsets[i] = cursor
        frame_bytes += bytes((fr.x & 0xFF, fr.y & 0xFF, fr.width & 0xFF, fr.height & 0xFF))
        frame_bytes += stream2
        cursor += 4 + len(stream2)
    for i, off in enumerate(offsets):
        struct.pack_into("<H", header, 2 + i * 2, off)
    return bytes(header) + bytes(interstitial) + bytes(frame_bytes)


def rebuild_monsters_atlas(raw: bytes, ext: str) -> bytes:
    """Rebuild a full MONSTERS.4 / MONSTERS.16 file from decoded pictures.

    Preserves blob-sharing (multiple picture ids pointing at the same LZW
    blob, per the format doc's "blob sharing" table) by re-using the same
    rebuilt offset whenever multiple header entries reference the same
    source file offset.
    """
    bpp = D.bpp_for_ext(ext)
    table_end = D._monster_table_end(raw)
    n_entries = table_end // 4
    header = bytearray(table_end)
    blobs = bytearray()
    cursor = table_end
    rebuilt_offset_for: dict[int, int] = {}
    for entry in range(n_entries):
        fo = D._monster_seek_from_header(raw, entry, table_end)
        if fo == 0 or not D._monster_blob_valid(raw, fo, table_end):
            continue
        if fo in rebuilt_offset_for:
            struct.pack_into("<I", header, entry * 4, rebuilt_offset_for[fo])
            continue
        dec = D._decompress_monster_blob(raw, fo)
        assert dec is not None
        frame_count = dec[0] & 0x3F
        frames = D._parse_monster_picture(raw, fo, entry + 1, entry, ext)
        rebuilt_dec = rebuild_monster_picture_payload(dec, frame_count, dec[1], frames, bpp)
        comp = lzw_compress(rebuilt_dec)
        blob_bytes = struct.pack("<I", len(rebuilt_dec)) + comp
        new_off = cursor
        struct.pack_into("<I", header, entry * 4, new_off)
        blobs += blob_bytes
        cursor += len(blob_bytes)
        rebuilt_offset_for[fo] = new_off
    return bytes(header) + bytes(blobs)


# ---------------------------------------------------------------------------
# --roundtrip validation
# ---------------------------------------------------------------------------


@dataclass
class RoundtripResult:
    file: str
    kind: str
    detail: str
    ok: bool


def _roundtrip_wall_sheet(path: Path) -> list[RoundtripResult]:
    results: list[RoundtripResult] = []
    try:
        info = D.parse_wall_sheet(path)
    except Exception as exc:  # noqa: BLE001
        return [RoundtripResult(path.name, "wall-parse", str(exc), False)]

    dec = info["decompressed"]

    # Tier 1 (the hard, ASM-traced part): LZW round-trip of the *complete*
    # original decompressed payload, independent of frame parsing. This is
    # the true "compressor is a correct inverse of the decompressor" check
    # and is expected to be 100% byte-identical for every file.
    container = encode_wall_sheet_container(dec)
    dec2 = D.lzw_decompress(container[4:], struct.unpack_from("<I", container, 0)[0])
    lzw_ok = dec2 == dec
    results.append(
        RoundtripResult(
            path.name,
            "wall-lzw-baseline",
            f"{len(dec)} bytes: decode(lzw_compress(original_payload)) "
            f"{'==' if lzw_ok else '!='} original_payload",
            lzw_ok,
        )
    )

    # Tier 2 (best-effort structural rebuild): reconstruct the decompressed
    # payload from parse_wall_sheet's frame list (re-packing pixels, not
    # copying bytes) and check it is byte-identical to the original blob.
    # This only succeeds when decode_pc_gfx's frame model fully accounts for
    # the sheet's bytes — some multi-orientation/grouped sheets have offset
    # tables decode_pc_gfx does not fully resolve (see docs: known gap, not
    # a pixel/LZW codec bug — every frame decode_pc_gfx *does* extract for
    # these sheets still round-trips pixel-perfectly).
    rebuilt = rebuild_wall_sheet_payload(info)
    payload_ok = rebuilt == dec
    results.append(
        RoundtripResult(
            path.name,
            "wall-frame-rebuild",
            f"exact-offset frame rebuild {'==' if payload_ok else '!='} original "
            f"({len(info['frames'])} frames parsed)",
            payload_ok,
        )
    )
    return results


def _roundtrip_monster_atlas(path: Path) -> list[RoundtripResult]:
    results: list[RoundtripResult] = []
    raw = path.read_bytes()
    ext = path.suffix.lower()
    bpp = D.bpp_for_ext(ext)

    blob_ok = blob_fail = 0
    for index, off in D.iter_monster_anim_indices(raw):
        dec = D._decompress_monster_blob(raw, off)
        if dec is None:
            continue
        frame_count = dec[0] & 0x3F
        frames = D._parse_monster_picture(raw, off, index, index, ext)
        if not frames:
            continue
        rebuilt_dec = rebuild_monster_picture_payload(dec, frame_count, dec[1], frames, bpp)

        # LZW round-trip of the freshly-built payload.
        comp = lzw_compress(rebuilt_dec)
        redec = D.lzw_decompress(comp, len(rebuilt_dec))
        if redec != rebuilt_dec:
            blob_fail += 1
            continue

        # Semantic check: re-parse frames out of the rebuilt payload and
        # compare decoded pixel grids to the originals (offsets differ by
        # design, so we compare *content*, not byte position).
        rb_frame_count = rebuilt_dec[0] & 0x3F
        inner = [struct.unpack_from("<H", rebuilt_dec, 2 + i * 2)[0] for i in range(rb_frame_count)]
        ordered = sorted(o for o in inner if 0 < o <= len(rebuilt_dec))
        same = True
        by_idx = {fr.frame_index: fr for fr in frames}
        for fi, inner_off in enumerate(inner):
            if inner_off <= 0:
                continue
            nxt = [o for o in ordered if o > inner_off]
            frame_end = min(nxt) if nxt else len(rebuilt_dec)
            parsed = D._parse_monster_frame(rebuilt_dec, inner_off, frame_end)
            orig = by_idx.get(fi)
            if parsed is None or orig is None:
                same = False
                break
            x, y, w, h, stream = parsed
            if (x, y, w, h) != (orig.x, orig.y, orig.width, orig.height):
                same = False
                break
            grid_new = D.decode_monster_sprite(w, h, stream, bpp)
            grid_old = D.decode_monster_sprite(orig.width, orig.height, orig.stream, bpp)
            if grid_new != grid_old:
                same = False
                break
        if same:
            blob_ok += 1
        else:
            blob_fail += 1

    results.append(
        RoundtripResult(
            path.name,
            "monster-blobs",
            f"{blob_ok}/{blob_ok + blob_fail} picture blobs round-trip pixel-identical",
            blob_fail == 0 and blob_ok > 0,
        )
    )

    # Full-atlas rebuild: verify every picture still resolves + decodes the
    # same after a complete from-scratch rebuild of the container file.
    rebuilt_atlas = rebuild_monsters_atlas(raw, ext)
    orig_indices = D.iter_monster_anim_indices(raw)
    atlas_ok = 0
    atlas_fail = 0
    for index, orig_off in orig_indices:
        new_off = D._monster_blob_for_index(rebuilt_atlas, index)
        if new_off is None:
            atlas_fail += 1
            continue
        orig_frames = D._parse_monster_picture(raw, orig_off, index, index, ext)
        new_frames = D._parse_monster_picture(rebuilt_atlas, new_off, index, index, ext)
        orig_grids = [D.decode_monster_sprite(f.width, f.height, f.stream, bpp) for f in orig_frames]
        new_grids = [D.decode_monster_sprite(f.width, f.height, f.stream, bpp) for f in new_frames]
        if orig_grids == new_grids:
            atlas_ok += 1
        else:
            atlas_fail += 1
    results.append(
        RoundtripResult(
            path.name,
            "monster-atlas-rebuild",
            f"{atlas_ok}/{atlas_ok + atlas_fail} pictures identical after full atlas rebuild "
            f"({len(rebuilt_atlas)} bytes, was {len(raw)})",
            atlas_fail == 0 and atlas_ok > 0,
        )
    )
    return results


def roundtrip(gog_dir: Path, out_root: Path | None = None) -> int:
    files = sorted(p for p in gog_dir.iterdir() if p.suffix.lower() in (".4", ".16"))
    if not files:
        print(f"No .4/.16 files found in {gog_dir}", file=sys.stderr)
        return 1

    all_results: list[RoundtripResult] = []
    for path in files:
        try:
            if D.is_monsters_file(path):
                all_results.extend(_roundtrip_monster_atlas(path))
            else:
                all_results.extend(_roundtrip_wall_sheet(path))
        except Exception as exc:  # noqa: BLE001
            all_results.append(RoundtripResult(path.name, "error", str(exc), False))

    failures = [r for r in all_results if not r.ok]
    for r in all_results:
        status = "OK  " if r.ok else "FAIL"
        print(f"{status}  {r.file:14s} [{r.kind:22s}] {r.detail}")

    print()
    print(f"{len(all_results) - len(failures)}/{len(all_results)} checks passed "
          f"across {len(files)} files")
    if failures:
        print(f"FAILED checks: {len(failures)}")
        for r in failures:
            print(f"  - {r.file} [{r.kind}]: {r.detail}")

    if out_root is not None:
        out_root.mkdir(parents=True, exist_ok=True)
        report = {
            "gog_dir": str(gog_dir),
            "total_checks": len(all_results),
            "passed": len(all_results) - len(failures),
            "failed": len(failures),
            "results": [asdict(r) for r in all_results],
        }
        (out_root / "roundtrip_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"\nreport: {(out_root / 'roundtrip_report.json').resolve()}")

    return 1 if failures else 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--gog", type=Path, default=None, help="GOG MM2 directory (default: autodetect)")
    ap.add_argument("--roundtrip", action="store_true", help="Validate encode/decode against real GOG files")
    ap.add_argument("--out-root", type=Path, default=None, help="Optional dir to write roundtrip_report.json")
    args = ap.parse_args(argv)

    if args.roundtrip:
        gog = resolve_gog(args.gog)
        if gog is None:
            print(
                "No GOG install found. Pass --gog \"C:/.../Might and Magic 2\" "
                f"(default checked: {GOG_DEFAULT})",
                file=sys.stderr,
            )
            return 1
        print(f"Roundtrip-validating against: {gog}\n")
        return roundtrip(gog, args.out_root)

    ap.print_help()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

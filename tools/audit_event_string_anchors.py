#!/usr/bin/env python3
"""Audit event.dat string anchors vs ASM EventRuntime rules for all 71 locs.

Truth:
  - Map init @ 0x1754A: walk triplets to 00 00 00; anchor = pos + LE u16 rel
  - Queued overlay @ 0x176B6: anchor = LE u16 at work_buf[0..1]; scripts @ 2
  - Castle blobs (no terminator): init leaves anchor $FFFF; overlay uses LE@0
  - locs 60..70 are overlay banks (never map-entered)
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from decode_event import decode_location, decode_strings, display_string  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
data = (ROOT / "event.dat").read_bytes()


def find_terminator(blob: bytes) -> int | None:
    """Return index AFTER 00 00 00, or None. Matches ASM 0x1754A / C++ initParsed."""
    pos = 0
    while pos + 2 < len(blob):
        a, b, c = blob[pos], blob[pos + 1], blob[pos + 2]
        pos += 3
        if a == 0 and b == 0 and c == 0:
            return pos
    return None


def asm_map_anchor(blob: bytes) -> tuple[int | None, int | None]:
    """(string_anchor, script_start) for map init path, or (None, None)."""
    pos = find_terminator(blob)
    if pos is None or pos + 1 >= len(blob):
        return None, None
    str_rel = blob[pos] | (blob[pos + 1] << 8)
    return pos + str_rel, pos + 2


def asm_overlay_anchor(blob: bytes) -> int | None:
    if len(blob) < 2:
        return None
    return blob[0] | (blob[1] << 8)


def textiness(strs: list[str]) -> tuple[float, int]:
    """Fraction of non-empty strings that look printable; count of letter-hits."""
    if not strs:
        return 0.0, 0
    ok = 0
    letters = 0
    nonempty = 0
    for s in strs:
        if s in ("", "z", "\n"):
            continue
        nonempty += 1
        printable = sum(1 for c in s if 32 <= ord(c) <= 126 or c == "\n")
        letters += sum(1 for c in s if c.isalpha())
        if len(s) > 0 and printable / len(s) >= 0.75:
            ok += 1
    return (ok / nonempty if nonempty else 0.0), letters


def main() -> int:
    fails = 0
    print(f"{'loc':>3} {'kind':<14} {'codec':>5} {'mapA':>5} {'ovlA':>5} "
          f"{'nStr':>4} {'txt%':>5}  note")
    print("-" * 88)

    for loc in range(71):
        off, length = struct.unpack_from(">IH", data, loc * 6)
        blob = data[off : off + length]
        meta = decode_location(blob, loc)
        codec_off = meta["string_table_offset"]
        map_a, map_script = asm_map_anchor(blob)
        ovl_a = asm_overlay_anchor(blob)

        # Expected anchor for editor/codec (how strings should be decoded):
        if 60 <= loc <= 70:
            expected = ovl_a
            path = "overlay"
        elif map_a is None:
            # castle blob — strings only meaningful via overlay LE@0
            expected = ovl_a
            path = "castle/ovl"
        else:
            expected = map_a
            path = "map"

        strs = meta["strings"]
        # Also decode from expected for comparison
        exp_strs = decode_strings(blob, expected) if expected is not None and expected < len(blob) else []
        txt, letters = textiness(strs)
        exp_txt, exp_letters = textiness(exp_strs)

        notes = []
        if expected is not None and codec_off != expected:
            notes.append(f"CODEC_MISMATCH want={expected}")
            fails += 1
        if path == "map" and map_a is not None:
            # Map path: codec must match map anchor; overlay LE@0 is unrelated
            if txt < 0.5 and len([s for s in strs if s not in ("", "z", "\n")]) > 3:
                notes.append(f"LOW_TEXT map={txt:.2f}")
                fails += 1
        if path in ("overlay", "castle/ovl"):
            if exp_txt < 0.5 and len(exp_strs) > 2:
                notes.append(f"OVL_LOW_TEXT {exp_txt:.2f}")
                fails += 1
            # If we used map path wrongly, map strings would be garbage
            if map_a is not None and map_a != ovl_a:
                map_strs = decode_strings(blob, map_a) if map_a < len(blob) else []
                mt, _ = textiness(map_strs)
                if mt < exp_txt - 0.2:
                    notes.append("map-scan-would-poison")  # informational

        # Spot-check known strings
        joined = "\n".join(strs)
        if loc == 0 and "Middlegate Inn" not in joined:
            notes.append("MISSING Middlegate Inn")
            fails += 1
        if loc == 60 and "Corak proclaims" not in joined:
            notes.append("MISSING Corak")
            fails += 1
        if loc == 60 and "Nordon asks" not in joined:
            notes.append("MISSING Nordon")
            fails += 1
        if loc == 66 and "Murray" not in joined:
            notes.append("MISSING Murray")
            fails += 1

        kind = meta["record_kind"]
        note = "; ".join(notes) if notes else "OK"
        print(
            f"{loc:3d} {kind:<14} {codec_off:5d} "
            f"{map_a if map_a is not None else -1:5d} "
            f"{ovl_a if ovl_a is not None else -1:5d} "
            f"{len(strs):4d} {txt:5.2f}  {note}"
        )

        # Cross-check: first printable string preview for failures / overlays
        if notes and notes != ["OK"]:
            for i, s in enumerate(strs[:3]):
                t = display_string(s).replace("\n", "/")[:50]
                print(f"       codec[{i}] {t}")
            for i, s in enumerate(exp_strs[:3]):
                t = display_string(s).replace("\n", "/")[:50]
                print(f"       expect[{i}] {t}")

    print("-" * 88)
    print(f"failures: {fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(main())

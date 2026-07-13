#!/usr/bin/env python3
"""Cross-check editor BytecodeParse rules vs ASM anchors for all 71 locs.
Also verifies C codec offsets if a quick ctypes load isn't available —
reimplements the C overlay/map split identically.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from decode_event import decode_location, decode_strings  # noqa: E402

data = (ROOT / "event.dat").read_bytes()


def c_codec_offsets(blob: bytes, loc_id: int) -> tuple[int, int, str]:
    """Mirror EXTRACTED/decomp/mm2_event_codec.c after overlay fix."""
    if 60 <= loc_id <= 70 and len(blob) >= 2:
        anchor = blob[0] | (blob[1] << 8)
        return 2, anchor, "overlay_bank"
    pos = 0
    terminated = False
    while pos + 2 < len(blob):
        a, b, c = blob[pos], blob[pos + 1], blob[pos + 2]
        pos += 3
        if a == b == c == 0:
            terminated = True
            break
    str_rel = (blob[pos] | (blob[pos + 1] << 8)) if pos + 1 < len(blob) else 0
    script_off = pos + 2
    str_off = pos + str_rel
    kind = "castle_blob" if not terminated else "standard"
    return script_off, str_off, kind


def main() -> int:
    fails = 0
    for loc in range(71):
        off, length = struct.unpack_from(">IH", data, loc * 6)
        blob = data[off : off + length]
        py = decode_location(blob, loc)
        c_script, c_str, c_kind = c_codec_offsets(blob, loc)

        if py["string_table_offset"] != c_str:
            print(f"loc {loc}: editor/py str_off {py['string_table_offset']} != C {c_str}")
            fails += 1
        if py["script_offset"] != c_script:
            print(f"loc {loc}: editor/py script_off {py['script_offset']} != C {c_script}")
            fails += 1

        # ASM resolve path for this loc
        if 60 <= loc <= 70:
            asm_anchor = blob[0] | (blob[1] << 8)  # 0x176C2 LE
        else:
            pos = 0
            found = False
            while pos + 2 < len(blob):
                a, b, c = blob[pos], blob[pos + 1], blob[pos + 2]
                pos += 3
                if a == b == c == 0:
                    found = True
                    break
            if not found:
                asm_anchor = blob[0] | (blob[1] << 8)  # castle → overlay LE
            else:
                asm_anchor = pos + (blob[pos] | (blob[pos + 1] << 8))

        if py["string_table_offset"] != asm_anchor:
            print(f"loc {loc}: codec {py['string_table_offset']} != ASM anchor {asm_anchor}")
            fails += 1

        strs = decode_strings(blob, asm_anchor) if asm_anchor < len(blob) else []
        # Every loc with strings should have mostly printable content
        real = [s for s in strs if s not in ("", "z", "\n")]
        if real:
            bad = 0
            for s in real:
                pr = sum(1 for ch in s if 32 <= ord(ch) <= 126 or ch == "\n")
                if pr / max(1, len(s)) < 0.7:
                    bad += 1
            if bad > len(real) * 0.25:
                print(f"loc {loc}: {bad}/{len(real)} strings look binary")
                fails += 1

    # Spot checks vs known FAQ/docs strings (indices are LE-anchor based)
    checks = {
        0: (1, "Middlegate Inn"),
        60: (1, "Corak proclaims"),
        60: (2, "Nordon asks"),  # noqa: overwrites — fix below
        66: (5, "Murray"),  # may need adjust
    }
    # redo properly
    spot = [
        (0, "Middlegate Inn"),
        (60, "Corak proclaims"),
        (60, "Nordon asks"),
        (60, "Feldecarb Fountain"),
        (61, "brownie"),
        (62, "Chris will teach"),
        (66, "Murray"),
        (70, "Hall of Spells"),
    ]
    for loc, needle in spot:
        off, length = struct.unpack_from(">IH", data, loc * 6)
        blob = data[off : off + length]
        py = decode_location(blob, loc)
        joined = "\n".join(py["strings"])
        if needle not in joined:
            print(f"loc {loc}: missing {needle!r}")
            fails += 1
        else:
            print(f"loc {loc}: OK has {needle!r}")

    print(f"cross-check failures: {fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(main())

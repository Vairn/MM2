#!/usr/bin/env python3
"""Audit event.dat for triplet-reachable fight segments that contain OP_12/13
with NO OP_2B before the fight in the linear byte stream."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from decode_event import (  # noqa: E402
    decode_location,
    parse_segment_stream_nodes,
    read_header,
    split_script_segments,
)


def main() -> None:
    data = (Path(__file__).resolve().parents[1] / "EXTRACTED" / "event.dat").read_bytes()
    header = read_header(data)

    flagged = []
    guarded_total = 0
    for idx, (off, length) in enumerate(header):
        blob = data[off : off + length]
        loc = decode_location(blob, idx)
        script = blob[loc["script_offset"] : loc["string_table_offset"]]
        segments = split_script_segments(script)
        for pos, evt, cond in loc["triplets"]:
            if evt >= len(segments) or not segments[evt]:
                continue
            seg = segments[evt]
            nodes = parse_segment_stream_nodes(seg)
            ops = [int(n["op"]) for n in nodes]
            fight_idx = next((i for i, o in enumerate(ops) if o in (0x12, 0x13)), None)
            if fight_idx is None:
                continue
            guarded = any(o == 0x2B for o in ops[:fight_idx])
            if guarded:
                guarded_total += 1
            else:
                flagged.append((idx, pos, evt, cond, seg.hex()[:56]))

    print(f"Guarded fight segments (OP_2B before fight): {guarded_total}")
    print(f"UNGUARDED fight segments: {len(flagged)}\n")
    for idx, pos, evt, cond, h in flagged:
        print(f"  loc {idx:2d} tile({pos&0xF},{pos>>4}) evt={evt} cond=0x{cond:02X} seg={h}")


if __name__ == "__main__":
    main()

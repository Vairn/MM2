#!/usr/bin/env python3
"""One-off scanner: OP_1F party_effect spell grants in event.dat."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from decode_event import (
    read_header,
    decode_location,
    split_script_segments,
    parse_segment_stream_nodes,
    decompile_op,
    try_load_default_items,
)
from mm2_spells import CLER_FLAT, SORC_FLAT

data = open("event.dat", "rb").read()
header = read_header(data)
items = try_load_default_items()

names = {}
for i in range(1, 49):
    lv, n, nm = SORC_FLAT[i]
    names[i - 1] = f"S{lv}/{n} {nm}"
for i in range(1, 49):
    lv, n, nm = CLER_FLAT[i]
    names[48 + i - 1] = f"C{lv}/{n} {nm}"


def triplet_label(loc_id: int, loc: dict) -> dict[int, list[str]]:
    out: dict[int, list[str]] = {}
    for pos, evt, flags in loc["triplets"]:
        y, x = (pos >> 4) & 0xF, pos & 0xF
        out.setdefault(evt, []).append(f"({y},{x})")
    return out


for loc_id, (off, length) in enumerate(header):
    blob = data[off : off + length]
    loc = decode_location(blob, loc_id)
    script = blob[loc["script_offset"] : loc["string_table_offset"]]
    segments = split_script_segments(script)
    triggers = triplet_label(loc_id, loc)

    for evt, seg in enumerate(segments):
        if not seg:
            continue
        nodes = parse_segment_stream_nodes(seg)
        context = []
        for node in nodes:
            op = node["op"]
            args = list(node["args"])
            pseudo = decompile_op(op, args, loc["strings"], items)
            if "show_text" in pseudo or pseudo.startswith("set_service"):
                context.append(pseudo[:100])
            if "party_effect" not in pseudo:
                continue
            # parse args after sel=
            import re

            m = re.search(r"party_effect(?:_b)?\(sel=0x[0-9A-Fa-f]+,\s*(.+)\)", pseudo)
            arg_bytes = []
            if m:
                arg_bytes = [int(x, 16) for x in m.group(1).split()]
            trig = ", ".join(triggers.get(evt, ["?"]))
            ctx = context[-2:] if context else []
            print(f"loc={loc_id:02d} evt={evt:02d} tile={trig}")
            print(f"  {pseudo}")
            if ctx:
                for c in ctx:
                    print(f"  ctx: {c}")
            print()

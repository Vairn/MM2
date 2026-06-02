#!/usr/bin/env python3
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
    looks_like_text_record,
)

loc_ids = [int(x) for x in sys.argv[1:]]
data = open("event.dat", "rb").read()
header = read_header(data)
items = try_load_default_items()

for loc_id in loc_ids:
    off, length = header[loc_id]
    blob = data[off : off + length]
    loc = decode_location(blob, loc_id)
    print(f"=== LOC {loc_id:02d} ===")
    for i, s in enumerate(loc["strings"]):
        t = s.replace("\n", " / ")
        print(f"  [{i}] {t[:110]}")
    script = blob[loc["script_offset"] : loc["string_table_offset"]]
    segs = split_script_segments(script)
    for evt, seg in enumerate(segs):
        if not seg:
            continue
        trigs = [(p >> 4 & 0xF, p & 0xF) for p, e, f in loc["triplets"] if e == evt]
        if looks_like_text_record(seg):
            txt = seg.decode("ascii", errors="replace").replace("@", " / ")
            print(f"  evt{evt:02d} {trigs} TEXT: {txt[:100]}")
            continue
        nodes = parse_segment_stream_nodes(seg)
        ops = [decompile_op(n["op"], list(n["args"]), loc["strings"], items) for n in nodes]
        if not any(
            k in o
            for o in ops
            for k in ("party_effect", "set_party_attr", "exec_selector", "show_text", "apply_party")
        ):
            continue
        print(f"  evt{evt:02d} tiles {trigs}:")
        for o in ops:
            print(f"    {o[:130]}")

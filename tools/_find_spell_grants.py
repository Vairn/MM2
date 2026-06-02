#!/usr/bin/env python3
"""Find spell grant events in event.dat."""
import re
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
from mm2_spells import CLER_FLAT, SORC_FLAT

SPELL_NAMES = {}
for i in range(1, 49):
    lv, n, nm = SORC_FLAT[i]
    SPELL_NAMES[i - 1] = f"S{lv}/{n} {nm}"
for i in range(1, 49):
    lv, n, nm = CLER_FLAT[i]
    SPELL_NAMES[48 + i - 1] = f"C{lv}/{n} {nm}"

TARGET_SPELLS = {
    57: "C2/3 Nature's Gate",
    69: "C4/2 Air Transmutation",
    74: "C5/1 Air Encasement",
    79: "C6/1 Earth Transmutation",
    82: "C6/4 Water Encasement",
    83: "C6/5 Water Transmutation",
    84: "C7/1 Earth Encasement",
    88: "C8/1 Fire Encasement",
    89: "C8/2 Fire Transmutation",
    92: "C9/1 Divine Intervention",
}

data = open("event.dat", "rb").read()
header = read_header(data)
items = try_load_default_items()

print("=== set_party_attr spell grants ===")
for loc_id, (off, length) in enumerate(header):
    blob = data[off : off + length]
    loc = decode_location(blob, loc_id)
    script = blob[loc["script_offset"] : loc["string_table_offset"]]
    segments = split_script_segments(script)
    by_evt = {}
    for pos, evt, _ in loc["triplets"]:
        y, x = (pos >> 4) & 0xF, pos & 0xF
        by_evt.setdefault(evt, []).append((y, x))
    for evt, seg in enumerate(segments):
        if not seg or looks_like_text_record(seg):
            continue
        for node in parse_segment_stream_nodes(seg):
            op = node["op"]
            args = list(node["args"])
            pseudo = decompile_op(op, args, loc["strings"], items)
            if "set_party_attr" not in pseudo:
                continue
            ctx = ""
            for s in loc["strings"]:
                if any(k in s for k in ("Encasement", "Transmutation", "Nature", "Divine")):
                    if len(s) > 20:
                        ctx = s.split("\n")[0][:80]
                        break
            trigs = by_evt.get(evt, [])
            print(f"loc={loc_id:02d} evt={evt:02d} tile={trigs} {pseudo}")
            for i, s in enumerate(loc["strings"]):
                if "Encasement" in s or "Transmutation" in s or "Nature" in s or "Divine" in s:
                    print(f"  str[{i}]: {s[:100].replace(chr(10), ' / ')}")

print("\n=== party_effect sel=0x00 spell grants (31 04 ...) ===")
for loc_id, (off, length) in enumerate(header):
    blob = data[off : off + length]
    loc = decode_location(blob, loc_id)
    script = blob[loc["script_offset"] : loc["string_table_offset"]]
    segments = split_script_segments(script)
    by_evt = {}
    for pos, evt, _ in loc["triplets"]:
        y, x = (pos >> 4) & 0xF, pos & 0xF
        by_evt.setdefault(evt, []).append((y, x))
    for evt, seg in enumerate(segments):
        if not seg or looks_like_text_record(seg):
            continue
        for node in parse_segment_stream_nodes(seg):
            pseudo = decompile_op(node["op"], list(node["args"]), loc["strings"], items)
            m = re.search(r"party_effect\(sel=0x00,\s*31 04 ([0-9A-Fa-f]{2}) ([0-9A-Fa-f]{2})", pseudo)
            if not m:
                continue
            b2, b3 = int(m.group(1), 16), int(m.group(2), 16)
            idx = b3  # spellbook index often in 3rd byte for 31 04 grants
            name = SPELL_NAMES.get(idx, f"idx={idx}")
            trigs = by_evt.get(evt, [])
            print(f"loc={loc_id:02d} evt={evt:02d} tile={trigs} -> {name} | {pseudo}")

print("\n=== strings: Nature / Divine / Elder ===")
for loc_id, (off, length) in enumerate(header):
    blob = data[off : off + length]
    loc = decode_location(blob, loc_id)
    for i, s in enumerate(loc["strings"]):
        if any(k in s for k in ("Nature's Gate", "Divine Intervention", "Elder Druid", "Red Hot Wolf")):
            print(f"loc={loc_id:02d} str[{i}]: {s[:120].replace(chr(10), ' / ')}")

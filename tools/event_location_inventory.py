#!/usr/bin/env python3
"""Build machine-readable inventory of all 71 event.dat location records."""

from __future__ import annotations

import json
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from decode_event import (  # noqa: E402
    OPCODES,
    decode_location,
    parse_segment_stream_nodes,
    read_header,
    split_script_segments,
)

ROOT = Path(__file__).resolve().parents[1]

# Canonical names for locations 0-59 (mirrors editor AreaNames.h).
AREA_NAMES: dict[int, str] = {
    0: "Middlegate",
    1: "Atlantium",
    2: "Tundara",
    3: "Vulcania",
    4: "Sandsobar",
    5: "A1",
    6: "B1",
    7: "C1",
    8: "D1",
    9: "A2",
    10: "B2",
    11: "C2",
    12: "A3",
    13: "B3",
    14: "C3",
    15: "A4",
    16: "B4",
    17: "Middlegate Cavern",
    18: "Atlantium Cavern",
    19: "Tundara Cavern",
    20: "Vulcania Cavern",
    21: "Sandsobar Cavern",
    22: "Corak's Cave",
    23: "Square Lake Cave",
    24: "Ice Cavern",
    25: "Sarakin's Mine",
    26: "Murray's Cave",
    27: "Druid's Cave",
    28: "Forbidden Forest Cavern",
    29: "Dragon's Dominion",
    30: "Dawn's Mist Bog",
    31: "Gemmaker's Cave",
    32: "Nomadic Rift",
    33: "E1",
    34: "D2",
    35: "E2",
    36: "D3",
    37: "E3",
    38: "C4",
    39: "D4",
    40: "E4",
    41: "Elemental Plane of Air",
    42: "Elemental Plane of Fire",
    43: "Elemental Plane of Earth",
    44: "Elemental Plane of Water",
    45: "Hillstone Dungeon L1",
    46: "Hillstone Dungeon L2",
    47: "Woodhaven Dungeon L1",
    48: "Woodhaven Dungeon L2",
    49: "Pinehurst Dungeon L1",
    50: "Pinehurst Dungeon L2",
    51: "Luxus Palace Dungeon L1",
    52: "Luxus Palace Dungeon L2",
    53: "Ancients (Good)",
    54: "Ancients (Evil)",
    55: "Hillstone",
    56: "Woodhaven",
    57: "Pinehurst",
    58: "Luxus Palace Royale",
    59: "Castle Xabran",
}

EXTRA_NAMES: dict[int, str] = {
    60: "Quest bank: Nordon/Nordonna/Corak intro",
    61: "Encoded tables (spell/hireling indices)",
    62: "Side quests: Chris cartography, Gertrude/Rat Fink",
    63: "Castle blob (non-triplet)",
    64: "Lord Haart heirloom / castle flavor",
    65: "Castle blob (non-triplet)",
    66: "Endgame: Corak soul, Murray/Dawn, Horvath",
    67: "Hall of Spells pool (mixed text/scripts)",
    68: "Castle blob (non-triplet)",
    69: "Queen Lamanda (Luxus storyline)",
    70: "Meta bank: HoS, Hireling Hall, bishops, puzzles",
}


def _readable(s: str) -> bool:
    if not s or len(s) < 4:
        return False
    letters = sum(1 for c in s if c.isalpha())
    return letters >= 4 and letters / len(s) >= 0.3


def classify_record(loc: dict, segments: list[bytes]) -> str:
    if not loc["terminated"]:
        return "castle_blob"
    script_len = max(0, loc["string_table_offset"] - loc["script_offset"])
    if script_len == 0 and loc["strings"]:
        return "string_bank"
    if script_len > 0 and any(
        len(seg) >= 4 and sum(1 for b in seg if 0x41 <= b <= 0x7A) >= 3
        for seg in segments
    ):
        if script_len > 500:
            return "mixed_pool"
        return "standard"
    if loc["triplets"] and script_len > 0:
        return "standard"
    return "unknown"


def opcode_histogram(segments: list[bytes]) -> dict[str, int]:
    hist: Counter[str] = Counter()
    for seg in segments:
        for node in parse_segment_stream_nodes(seg):
            op = int(node["op"])
            name = OPCODES.get(op, {}).get("name", f"OP_{op:02X}")
            hist[str(name)] += 1
    return dict(sorted(hist.items()))


def sample_strings(strings: list[str], limit: int = 5) -> list[str]:
    out: list[str] = []
    for s in strings:
        if _readable(s.replace("@", " ")):
            out.append(re.sub(r"\s+", " ", s.replace("@", " "))[:120])
        if len(out) >= limit:
            break
    return out


def build_inventory(data: bytes) -> dict:
    header = read_header(data)
    locations = []
    for loc_id, (off, length) in enumerate(header):
        blob = data[off : off + length]
        loc = decode_location(blob, loc_id)
        script = blob[loc["script_offset"] : loc["string_table_offset"]]
        segments = split_script_segments(script) if script else []
        kind = classify_record(loc, segments)
        name = AREA_NAMES.get(loc_id) or EXTRA_NAMES.get(loc_id, f"Location {loc_id}")
        locations.append(
            {
                "id": loc_id,
                "name": name,
                "offset": off,
                "length": length,
                "record_kind": kind,
                "triplet_count": len(loc["triplets"]),
                "terminated": loc["terminated"],
                "script_bytes": len(script),
                "segment_count": len(segments),
                "string_count": len(loc["strings"]),
                "sample_strings": sample_strings(loc["strings"]),
                "opcode_histogram": opcode_histogram(segments) if script else {},
            }
        )
    return {
        "file_size": len(data),
        "location_count": len(locations),
        "locations": locations,
    }


def main() -> None:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "event.dat"
    out_path = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "EXTRACTED" / "event_location_inventory.json"
    data = path.read_bytes()
    inv = build_inventory(data)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(inv, indent=2), encoding="utf-8")
    print(f"Wrote {out_path} ({inv['location_count']} locations)")


if __name__ == "__main__":
    main()

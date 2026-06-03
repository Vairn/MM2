#!/usr/bin/env python3
"""Shared helpers for event.dat location markdown docs."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from decode_event import (  # noqa: E402
    FLAG_NAMES,
    OPCODES,
    _skip_n_tokens,
    decompile_op,
    decode_location,
    decode_strings,
    display_string,
    looks_like_text_record,
    parse_segment_stream_nodes,
    read_header,
    split_script_segments,
    try_load_default_items,
)

AREA_NAMES: dict[int, str] = {
    0: "Middlegate",
    1: "Atlantium",
    2: "Tundara",
    3: "Vulcania",
    4: "Sandsobar",
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
    41: "Elemental Plane of Air",
    42: "Elemental Plane of Fire",
    43: "Elemental Plane of Earth",
    44: "Elemental Plane of Water",
    45: "Hillstone Dungeon Level 1",
    46: "Hillstone Dungeon Level 2",
    47: "Woodhaven Dungeon Level 1",
    48: "Woodhaven Dungeon Level 2",
    49: "Pinehurst Dungeon Level 1",
    50: "Pinehurst Dungeon Level 2",
    51: "Luxus Palace Dungeon Level 1",
    52: "Luxus Palace Dungeon Level 2",
    53: "Ancients (Good)",
    54: "Ancients (Evil)",
    55: "Hillstone",
    56: "Woodhaven",
    57: "Pinehurst",
    58: "Luxus Palace Royale",
    59: "Castle Xabran",
}

OUTDOOR_SECTOR: dict[int, str] = {
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
    33: "E1",
    34: "D2",
    35: "E2",
    36: "D3",
    37: "E3",
    38: "C4",
    39: "D4",
    40: "E4",
}

EXTRA_EVENT_LOC: dict[int, str] = {
    60: "Quest: Nordon/Nordonna/Corak",
    61: "Spell/hireling index tables",
    62: "Side quests (Chris, Gertrude)",
    63: "Castle blob A",
    64: "Lord Haart heirloom",
    65: "Castle blob B",
    66: "Endgame Corak/Murray/Horvath",
    67: "Hall of Spells pool",
    68: "Castle blob C",
    69: "Queen Lamanda (Luxus)",
    70: "Meta bank (HoS, bishops, puzzles)",
}

TEXT_OPS = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0B}


def default_event_dat() -> Path:
    for p in (ROOT / "event.dat", ROOT / "EXTRACTED" / "event.dat"):
        if p.exists():
            return p
    raise FileNotFoundError("event.dat not found in repo root or EXTRACTED/")


def location_label(loc_id: int) -> str:
    if loc_id in AREA_NAMES:
        return AREA_NAMES[loc_id]
    if loc_id in OUTDOOR_SECTOR:
        return OUTDOOR_SECTOR[loc_id]
    if loc_id in EXTRA_EVENT_LOC:
        return EXTRA_EVENT_LOC[loc_id]
    if loc_id < 60:
        return f"Area {loc_id}"
    return f"extra {loc_id}"


def location_slug(loc_id: int) -> str:
    name = location_label(loc_id)
    slug = re.sub(r"[^a-zA-Z0-9]+", "_", name).strip("_").lower()
    if not slug:
        slug = "area"
    return f"loc_{loc_id:02d}_{slug}"


def map_screen_note(loc_id: int) -> str:
    if loc_id < 60:
        parts = [f"map screen **{loc_id}**"]
        if loc_id in OUTDOOR_SECTOR:
            parts.append(f"overland sector **{OUTDOOR_SECTOR[loc_id]}**")
        elif loc_id in AREA_NAMES:
            parts.append(AREA_NAMES[loc_id])
        return "; ".join(parts)
    return EXTRA_EVENT_LOC.get(loc_id, "event.dat overlay (not a map screen)")


def load_location(data: bytes, loc_id: int) -> tuple[int, int, bytes, dict]:
    header = read_header(data)
    off, length = header[loc_id]
    blob = data[off : off + length]
    loc = decode_location(blob, loc_id)
    return off, length, blob, loc


def triplet_table_md(triplets: list[tuple[int, int, int]]) -> list[str]:
    lines = [
        "| Tile (y,x) | pos | Event | Condition |",
        "|------------|-----|-------|-----------|",
    ]
    for pos, evt, flags in triplets:
        y = (pos >> 4) & 0xF
        x = pos & 0xF
        cond = FLAG_NAMES.get(flags, f"0x{flags:02X}")
        lines.append(f"| ({y},{x}) | `0x{pos:02X}` | **{evt}** | {cond} |")
    return lines


def string_indices_in_segment(seg: bytes) -> set[int]:
    idxs: set[int] = set()
    for node in parse_segment_stream_nodes(seg):
        op = int(node["op"])
        args = [int(x) for x in node["args"]]
        if op in TEXT_OPS and args:
            idxs.add(args[0])
    return idxs


def format_event_segment(
    evt: int,
    seg: bytes,
    strings: list[str],
    items: list[str],
    triggers: list[tuple[int, int, int]] | None = None,
) -> list[str]:
    lines: list[str] = []
    if triggers:
        parts = []
        for pos, flags, _ in triggers:
            y = (pos >> 4) & 0xF
            x = pos & 0xF
            cond = FLAG_NAMES.get(flags, f"0x{flags:02X}")
            parts.append(f"({y},{x})/{cond}")
        lines.append(f"**Event {evt:02d}** — triggers: {', '.join(parts)}")
    else:
        lines.append(f"**Event {evt:02d}**")

    lines.append("")
    if not seg:
        lines.append("*(empty segment)*")
        lines.append("")
        return lines

    if looks_like_text_record(seg):
        txt = seg.decode("ascii", errors="replace").replace("@", " / ")
        lines.append("```")
        lines.append(txt)
        lines.append("```")
        lines.append("")
        lines.append("*Plain-text record in the 0xFF pool (not an opcode script).*")
        lines.append("")
        return lines

    lines.append("```hex")
    lines.append(seg.hex(" "))
    lines.append("```")
    lines.append("")
    lines.append("```")
    parsed = parse_segment_stream_nodes(seg)
    for idx, node in enumerate(parsed):
        op = int(node["op"])
        args = [int(x) for x in node["args"]]
        pseudo = decompile_op(op, args, strings, items)
        lines.append(f"{idx:02d}: {pseudo}")
        if op in (0x10, 0x11, 0x2B) and args:
            base = int(node["off"]) + 2
            target = _skip_n_tokens(seg, base, args[0])
            if target is not None and target < len(seg):
                t_op = seg[target]
                spec = OPCODES.get(t_op)
                if spec is not None and spec["argc"] is not None:
                    argc_i = int(spec["argc"])
                    targs = list(seg[target + 1 : target + 1 + argc_i])
                    lines.append(
                        f"    # skip -> {decompile_op(t_op, targs, strings, items)}"
                    )
    lines.append("```")
    lines.append("")
    return lines


def referenced_strings_md(strings: list[str], indices: Iterable[int]) -> list[str]:
    used = sorted(set(indices))
    if not used:
        return []
    lines = ["#### Strings used", ""]
    for i in used:
        if 0 <= i < len(strings):
            txt = display_string(strings[i]).replace("\n", " / ")
            lines.append(f"- `[{i:02d}]` {txt}")
        else:
            lines.append(f"- `[{i:02d}]` *(out of range)*")
    lines.append("")
    return lines


def render_location_markdown(
    data: bytes,
    loc_id: int,
    *,
    event_filter: set[int] | None = None,
    compact: bool = False,
) -> str:
    off, length, blob, loc = load_location(data, loc_id)
    items = try_load_default_items()
    script = blob[loc["script_offset"] : loc["string_table_offset"]]
    segments = split_script_segments(script)

    by_evt: dict[int, list[tuple[int, int, int]]] = {}
    for pos, evt, flags in loc["triplets"]:
        by_evt.setdefault(evt, []).append((pos, flags, pos))

    label = location_label(loc_id)
    lines = [
        f"# Location {loc_id:02d} — {label}",
        "",
        f"- **event.dat** offset `0x{off:06X}`, length **{length}** bytes",
        f"- **Map:** {map_screen_note(loc_id)}",
        f"- **Record kind:** `{loc.get('record_kind', 'standard')}`",
        f"- **Triggers:** {len(loc['triplets'])}; **script segments:** {len(segments)}; "
        f"**strings:** {len(loc['strings'])}",
        "",
    ]
    if not loc.get("terminated", True):
        lines += [
            "> Non-standard record (no `00 00 00` triplet terminator). "
            "Tile triggers may be unreliable; treat as string/script pool.",
            "",
        ]

    if event_filter is None:
        lines.append("## Tile triggers")
        lines.append("")
        if loc["triplets"]:
            lines.extend(triplet_table_md(loc["triplets"]))
        else:
            lines.append("*(no tile-event triplets)*")
        lines.append("")
    else:
        filt = sorted(event_filter)
        sub = [(p, e, f) for p, e, f in loc["triplets"] if e in event_filter]
        lines.append("## Class-quest triggers (this doc)")
        lines.append("")
        if sub:
            lines.extend(triplet_table_md(sub))
        else:
            lines.append("*(no triggers for filtered events)*")
        lines.append("")

    evt_ids = sorted(by_evt.keys()) if event_filter is None else sorted(event_filter)
    str_used: set[int] = set()

    lines.append("## Events")
    lines.append("")
    for evt in evt_ids:
        triggers = by_evt.get(evt, [])
        if evt >= len(segments):
            lines.append(f"**Event {evt:02d}** — *(missing script segment)*")
            lines.append("")
            continue
        seg = segments[evt]
        lines.extend(
            format_event_segment(evt, seg, loc["strings"], items, triggers or None)
        )
        str_used |= string_indices_in_segment(seg)

    if loc["strings"]:
        if compact or (event_filter is not None):
            lines.extend(referenced_strings_md(loc["strings"], str_used))
        else:
            lines.append("## String table")
            lines.append("")
            for i, s in enumerate(loc["strings"]):
                txt = display_string(s).replace("\n", " / ")
                lines.append(f"- `[{i:02d}]` {txt}")
            lines.append("")

    if not compact:
        lines.append("---")
        lines.append("")
        lines.append(
            "*Generated by `tools/build_event_location_docs.py` from `event.dat`. "
            "Decoder: `tools/decode_event.py`.*"
        )
        lines.append("")
    return "\n".join(lines)


# Doc 37: door-code / popup events — hex + key ops only (full dump in loc_*.md).
_DOC37_MINIMAL_EVENTS: set[tuple[int, int]] = {
    (22, 6),
    (53, 4),
    (53, 5),
    (54, 4),
    (54, 5),
}


def render_class_quest_compact(
    data: bytes,
    loc_id: int,
    event_ids: set[int],
    event_notes: dict[int, str] | None = None,
) -> str:
    """Minimal markdown block for doc 37 (one location, selected events)."""
    off, length, blob, loc = load_location(data, loc_id)
    items = try_load_default_items()
    script = blob[loc["script_offset"] : loc["string_table_offset"]]
    segments = split_script_segments(script)
    by_evt: dict[int, list[tuple[int, int, int]]] = {}
    for pos, evt, flags in loc["triplets"]:
        if evt in event_ids:
            by_evt.setdefault(evt, []).append((pos, flags, pos))

    label = location_label(loc_id)
    sector = OUTDOOR_SECTOR.get(loc_id)
    map_bit = f"map **{loc_id}**" + (f", sector **{sector}**" if sector else "")
    lines = [f"### Location {loc_id:02d} — {label} ({map_bit})", ""]
    str_used: set[int] = set()

    for evt in sorted(event_ids):
        note = (event_notes or {}).get(evt)
        if note:
            lines.append(f"**Event {evt:02d}** — {note}")
        else:
            lines.append(f"**Event {evt:02d}**")
        triggers = by_evt.get(evt, [])
        if triggers:
            rows = []
            for pos, flags, _ in triggers:
                y = (pos >> 4) & 0xF
                x = pos & 0xF
                cond = FLAG_NAMES.get(flags, f"0x{flags:02X}")
                rows.append(f"({y},{x}) `0x{pos:02X}` {cond}")
            lines.append(f"- Triggers: {', '.join(rows)}")
        if evt >= len(segments) or not segments[evt]:
            lines.append("- *(empty / missing segment)*")
            lines.append("")
            continue
        seg = segments[evt]
        if looks_like_text_record(seg):
            txt = seg.decode("ascii", errors="replace").replace("@", " / ")
            lines.append(f"- Text: {txt[:100]}")
            lines.append("")
            continue
        lines.append(f"- Hex: `{seg.hex(' ')}`")
        minimal = (loc_id, evt) in _DOC37_MINIMAL_EVENTS
        nodes = parse_segment_stream_nodes(seg)
        if minimal:
            picked = []
            for node in nodes:
                op = int(node["op"])
                args = [int(x) for x in node["args"]]
                if op in (0x30, 0x18, 0x12, 0x0E, 0x03, 0x06, 0x05):
                    picked.append(decompile_op(op, args, loc["strings"], items))
                if op in TEXT_OPS and args:
                    str_used.add(args[0])
            lines.append(f"- Key ops: {'; '.join(picked) if picked else '(see loc doc)'}")
        else:
            lines.append("- Pseudo:")
            for node in nodes:
                op = int(node["op"])
                args = [int(x) for x in node["args"]]
                lines.append(f"  - {decompile_op(op, args, loc['strings'], items)}")
                if op in TEXT_OPS and args:
                    str_used.add(args[0])
        lines.append("")

    if str_used and loc["strings"]:
        parts = []
        for i in sorted(str_used):
            if 0 <= i < len(loc["strings"]):
                txt = display_string(loc["strings"][i]).replace("\n", " / ")[:90]
                parts.append(f"[{i}] {txt}")
        lines.append(f"Strings: {'; '.join(parts)}")
        lines.append("")
    return "\n".join(lines)

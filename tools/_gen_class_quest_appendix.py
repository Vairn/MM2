#!/usr/bin/env python3
"""Generate class-quest event appendix for doc 37."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import decode_event as de  # noqa: E402

QUEST: dict[int, dict] = {
    34: {"label": "34  D2 (Mount Farview Juror plaque)", "events": [12]},
    13: {"label": "13  B3 (Jouster's Way / Knight quest)", "events": [8]},
    28: {"label": "28  Forbidden Forest Cavern (Paladin quest)", "events": [3]},
    10: {"label": "10  B2 (Baron Wilfrey / Archer quest)", "events": [17]},
    38: {"label": "38  C4 (Brutal Bruno / Barbarian quest)", "events": [12]},
    22: {"label": "22  Corak's Cave (Cleric quest)", "events": [4, 5]},
    30: {"label": "30  Dawn's Mist Bog (Ninja quest)", "events": [4]},
    53: {"label": "53  Ancients (Evil) — Ybmug / Sorcerer", "events": [4, 5, 6]},
    54: {"label": "54  Ancients (Good) — Yekop / Sorcerer", "events": [4, 5, 6]},
    61: {"label": "61  Spell/hireling index tables (HoS class-quest hints)", "events": "hints"},
}

TEXT_OPS = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0B}
HINT_KEYS = (
    "juror",
    "dread knight",
    "wilfrey",
    "bruno",
    "dawn",
    "yekop",
    "ybmug",
    "corak",
    "paladin",
    "robber",
    "class",
    "forbidden forest",
    "ancient",
)


def str_refs(seg: bytes) -> list[int]:
    refs: set[int] = set()
    for node in de.parse_segment_stream_nodes(seg):
        op = int(node["op"])
        args = [int(x) for x in node["args"]]
        if op in TEXT_OPS and args:
            refs.add(args[0])
    return sorted(refs)


def fmt_seg(seg: bytes, strings: list[str], items: list[str]):
    rows = []
    for node in de.parse_segment_stream_nodes(seg):
        op = int(node["op"])
        args = [int(x) for x in node["args"]]
        off = int(node["off"])
        hexb = f"{op:02X}" + (" " + " ".join(f"{a:02X}" for a in args) if args else "")
        pseudo = de.decompile_op(op, args, strings, items)
        rows.append((off, hexb, pseudo))
    return rows


def main() -> None:
    event_path = ROOT / "event.dat"
    if not event_path.exists():
        event_path = ROOT / "EXTRACTED" / "event.dat"
    data = event_path.read_bytes()
    items = de.try_load_default_items()
    header = de.read_header(data)

    out: list[str] = []
    out.append("## Appendix — full event.dat decode (class quest chain)")
    out.append("")
    out.append(
        "Generated from `tools/decode_event.py` (`event.dat <loc>` segment tables) "
        "plus `parse_segment_stream_nodes` / `decompile_op` for complete opcode bytes. "
        "`--decompile` dumps all 71 locations and has no location filter."
    )
    out.append("")

    for loc_id, meta in QUEST.items():
        off, length = header[loc_id]
        blob = data[off : off + length]
        loc = de.decode_location(blob, loc_id)
        script = blob[loc["script_offset"] : loc["string_table_offset"]]
        segments = de.split_script_segments(script)
        strings = loc["strings"]

        out.append(f"### Location {meta['label']}")
        out.append("")
        out.append(
            f"- **event.dat offset:** `0x{off:06X}`  **length:** {length} bytes"
        )
        out.append(
            f"- **Script:** offset `0x{loc['script_offset']:04X}`, "
            f"{loc['script_length']} bytes  |  **Strings:** offset "
            f"`0x{loc['string_table_offset']:04X}`, {len(strings)} entries"
        )
        out.append("")
        out.append("#### All triggers")
        out.append("")
        out.append("| Tile (y,x) | pos | cond | event |")
        out.append("|------------|-----|------|-------|")
        for pos, evt, flags in loc["triplets"]:
            y, x = (pos >> 4) & 0xF, pos & 0xF
            cond = de.FLAG_NAMES.get(flags, f"0x{flags:02X}")
            out.append(f"| ({y},{x}) | `0x{pos:02X}` | {cond} | **{evt}** |")
        out.append("")

        if meta["events"] == "hints":
            out.append("#### HoS / hint strings mentioning class quests")
            out.append("")
            for i, s in enumerate(strings):
                txt = de.display_string(s)
                if not txt.strip():
                    continue
                low = txt.lower()
                if any(k in low for k in HINT_KEYS):
                    one = txt.replace("\n", " / ")
                    out.append(f"- **[{i}]** {one}")
            out.append("")
            continue

        all_refs: set[int] = set()
        for evt in meta["events"]:
            if evt >= len(segments):
                out.append(f"#### Event **{evt}** — missing segment")
                out.append("")
                continue
            seg = segments[evt]
            out.append(f"#### Event **{evt}** — quest segment")
            out.append("")
            out.append(
                f"Raw bytes ({len(seg)}): `{' '.join(f'{b:02X}' for b in seg)}`"
            )
            out.append("")
            out.append("| Off | Bytes | Pseudo-code |")
            out.append("|-----|-------|-------------|")
            for off, hexb, pseudo in fmt_seg(seg, strings, items):
                out.append(f"| +0x{off:02X} | `{hexb}` | {pseudo} |")
            out.append("")
            refs = str_refs(seg)
            all_refs.update(refs)
            if refs:
                out.append("**Strings referenced by this event:**")
                out.append("")
                for i in refs:
                    txt = de.display_string(strings[i]) if i < len(strings) else "<missing>"
                    out.append(f"- **[{i}]** {txt.replace(chr(10), ' / ')}")
                out.append("")

        if all_refs:
            out.append("#### Combined string table (quest events only)")
            out.append("")
            for i in sorted(all_refs):
                txt = de.display_string(strings[i]) if i < len(strings) else "<missing>"
                out.append(f"**[{i}]**")
                out.append("")
                out.append("```")
                out.append(txt if txt else "<EMPTY>")
                out.append("```")
                out.append("")

    dest = ROOT / "EXTRACTED" / "docs" / "_37_appendix.md"
    dest.write_text("\n".join(out), encoding="utf-8")
    print(f"Wrote {dest} ({len(out)} lines)")


if __name__ == "__main__":
    main()

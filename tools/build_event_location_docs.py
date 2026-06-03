#!/usr/bin/env python3
"""Generate per-location event.dat reference docs (71 locations).

Usage:
  python tools/build_event_location_docs.py
  python tools/build_event_location_docs.py --event-dat path/to/event.dat
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from event_doc_common import (  # noqa: E402
    AREA_NAMES,
    OUTDOOR_SECTOR,
    default_event_dat,
    location_label,
    location_slug,
    render_location_markdown,
)
from decode_event import read_header  # noqa: E402

OUT_DIR = ROOT / "EXTRACTED" / "docs" / "events"
README = OUT_DIR / "README.md"

# Grouped index sections (loc id ranges / lists).
_INDEX_GROUPS: list[tuple[str, list[int]]] = [
    ("Towns (map 0–4)", list(range(0, 5))),
    ("Overland sectors (map 5–16)", list(range(5, 17))),
    ("Wilderness & dungeons (map 17–32)", list(range(17, 33))),
    ("Overland sectors (map 33–40)", list(range(33, 41))),
    ("Elemental planes (map 41–44)", list(range(41, 45))),
    ("Castle basements (map 45–52)", list(range(45, 53))),
    ("Ancients & castles (map 53–59)", list(range(53, 60))),
    ("Overlay banks (loc 60–70, not map screens)", list(range(60, 71))),
]


def map_screen_cell(loc_id: int) -> str:
    if loc_id >= 60:
        return "—"
    if loc_id in OUTDOOR_SECTOR:
        return f"{loc_id} · {OUTDOOR_SECTOR[loc_id]}"
    if loc_id in AREA_NAMES:
        return str(loc_id)
    return str(loc_id)


def build_index(rows: list[tuple[int, str, str]]) -> str:
    by_loc = {loc_id: (label, fname) for loc_id, label, fname in rows}
    lines = [
        "# event.dat — per-location reference",
        "",
        "Decoded script pages for all **71** `event.dat` locations (indices **0..70**).",
        "Each file lists tile triggers, bytecode segments, and string table entries.",
        "",
        "| Resource | Doc |",
        "|----------|-----|",
        "| `event.dat` container | [`06-event-dat-format.md`](../06-event-dat-format.md) |",
        "| Opcode reference | [`07-event-script-opcodes.md`](../07-event-script-opcodes.md) |",
        "| Runtime / collision `0x80` | [`08-event-runtime.md`](../08-event-runtime.md) |",
        "| 60 `map.dat` screens | [`21-map-dat-format.md`](../21-map-dat-format.md) |",
        "| Wiki hub (numbered) | [`40-events-by-location.md`](../40-events-by-location.md) |",
        "| Class quests | [`37-mount-farview-class-quest-event.md`](../37-mount-farview-class-quest-event.md) |",
        "",
        "Map screen names follow `editor/src/core/AreaNames.h`. Locations **60..70** are",
        "overlay banks (quests, HoS, castle blobs, meta) — not `map.dat` screens.",
        "",
        "## Regenerate",
        "",
        "```powershell",
        "python tools\\build_event_location_docs.py",
        "```",
        "",
        "Decoder: `tools/decode_event.py` · Editor: [`editor/README.md`](../../editor/README.md) (Events section)",
        "",
    ]
    for heading, loc_ids in _INDEX_GROUPS:
        lines += ["", f"### {heading}", "", "| Loc | Map | Area | Doc |", "|-----|-----|------|-----|"]
        for loc_id in loc_ids:
            if loc_id not in by_loc:
                continue
            label, fname = by_loc[loc_id]
            lines.append(
                f"| {loc_id:02d} | {map_screen_cell(loc_id)} | {label} | [{fname}]({fname}) |"
            )
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--event-dat", type=Path, default=None)
    args = ap.parse_args()

    event_path = args.event_dat or default_event_dat()
    data = event_path.read_bytes()
    header = read_header(data)
    if len(header) != 71:
        print(f"warning: expected 71 locations, got {len(header)}", file=sys.stderr)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    index_rows: list[tuple[int, str, str]] = []

    for loc_id in range(len(header)):
        label = location_label(loc_id)
        fname = f"{location_slug(loc_id)}.md"
        md = render_location_markdown(data, loc_id)
        (OUT_DIR / fname).write_text(md, encoding="utf-8", newline="\n")
        index_rows.append((loc_id, label, fname))
        print(f"  wrote {fname}")

    README.write_text(build_index(index_rows), encoding="utf-8", newline="\n")
    print(f"wrote {README.relative_to(ROOT)}")
    print(f"done: {len(index_rows)} location docs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

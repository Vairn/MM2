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
    default_event_dat,
    location_label,
    location_slug,
    render_location_markdown,
)
from decode_event import read_header  # noqa: E402

OUT_DIR = ROOT / "EXTRACTED" / "docs" / "events"
README = OUT_DIR / "README.md"


def build_index(rows: list[tuple[int, str, str]]) -> str:
    lines = [
        "# event.dat — per-location reference",
        "",
        "71 **event.dat** location records (indices **0..70**). Each file lists tile triggers,",
        "event script hex + pseudo-code (`tools/decode_event.py`), and strings referenced by those events.",
        "",
        "**Class quests only:** [`37-mount-farview-class-quest-event.md`](../37-mount-farview-class-quest-event.md)",
        "",
        "Regenerate all location files:",
        "",
        "```bash",
        "python tools/build_event_location_docs.py",
        "```",
        "",
        "| Loc | Area | Doc |",
        "|-----|------|-----|",
    ]
    for loc_id, label, fname in rows:
        lines.append(f"| {loc_id:02d} | {label} | [{fname}]({fname}) |")
    lines += [
        "",
        "Map screen names follow `editor/src/core/AreaNames.h`. Locations **60..70** are",
        "overlay banks (quests, HoS, castle blobs, meta) — not the 60 `map.dat` screens.",
        "",
    ]
    return "\n".join(lines)


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

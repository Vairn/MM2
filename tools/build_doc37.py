#!/usr/bin/env python3
"""Build slim class-quest doc 37 from _doc37_front.md + compact event decodes.

Usage:
  python tools/build_doc37.py
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from event_doc_common import (  # noqa: E402
    default_event_dat,
    render_class_quest_compact,
)

FRONT = ROOT / "EXTRACTED" / "_doc37_front.md"
OUT = ROOT / "EXTRACTED" / "docs" / "37-mount-farview-class-quest-event.md"

# (loc, event_id, short note) — class-quest scripts only
CLASS_QUEST_EVENTS: list[tuple[int, int, str]] = [
    (34, 12, "Juror plaque + OP_0E 0x97 → 0x9D76"),
    (13, 8, "Knight completion (Jouster's Way)"),
    (28, 3, "Paladin arena"),
    (10, 17, "Archer vs Wilfrey"),
    (38, 12, "Barbarian vs Bruno"),
    (7, 9, "Cleric — Corak's Soul (C1 ghosts)"),
    (22, 3, "Cleric — Admit 8 Pass gate"),
    (22, 4, "Cleric — guide opens Hero's Tomb"),
    (22, 5, "Cleric — crypt reunion OP_0E 0x65"),
    (22, 6, "Cleric prep — Holy Word popup"),
    (30, 4, "Ninja — Dawn's throne"),
    (53, 4, "Sorcerer Evil — access code (46)"),
    (53, 5, "Sorcerer Evil — access code (23)"),
    (53, 6, "Sorcerer Evil — free Ybmug / completion"),
    (54, 4, "Sorcerer Good — access code (64)"),
    (54, 5, "Sorcerer Good — access code (32)"),
    (54, 6, "Sorcerer Good — free Yekop / completion"),
]


def events_by_loc() -> dict[int, set[int]]:
    out: dict[int, set[int]] = {}
    for loc, evt, _ in CLASS_QUEST_EVENTS:
        out.setdefault(loc, set()).add(evt)
    return out


def build_decode_section(data: bytes) -> str:
    by_loc = events_by_loc()
    lines = [
        "## Event decode (class quests only)",
        "",
        "Completion / turn-in scripts only. Full triggers and all events per map:",
        "[`EXTRACTED/docs/events/`](events/README.md).",
        "",
    ]
    for loc in sorted(by_loc.keys()):
        evts = by_loc[loc]
        notes = {e: n for l, e, n in CLASS_QUEST_EVENTS if l == loc}
        lines.append(render_class_quest_compact(data, loc, evts, notes))
    return "\n".join(lines)


def patch_front(front: str) -> str:
    front = front.replace(
        "Regenerate full doc: `python tools/_build_doc37.py` (merges `EXTRACTED/_doc37_front.md` + `_doc37_appendix.md`).",
        "Regenerate: `python tools/build_doc37.py` (merges `EXTRACTED/_doc37_front.md` + class-quest decodes).",
    )
    front = front.replace(
        "Quest guides + opcode/string appendix are inline below (locations **34, 13, 28, 10, 38, 22, 30, 53, 54, 7, 61**).",
        "Quest guides below; per-location event reference in [`EXTRACTED/docs/events/`](events/README.md).",
    )
    # Trim regenerate block that referenced removed scripts
    old_regen = """## Regenerate

```bash
python tools/decode_event.py event.dat 34
python tools/_decompile_locs.py 34 13 28 10 38 22 30 53 54 7
python tools/_build_doc37.py
```

---

## Full event decode appendix

*All triggers, string tables, per-event raw hex and `decode_event.py` pseudo-code for locations **34, 13, 28, 10, 38, 22, 30, 53, 54, 7, 61**.*
"""
    new_regen = """## Regenerate

```bash
python tools/build_doc37.py
python tools/build_event_location_docs.py   # all 71 location docs
python tools/decode_event.py event.dat 34    # CLI spot-check
```

"""
    if old_regen in front:
        front = front.replace(old_regen, new_regen)
    elif "## Regenerate" in front:
        # Already partially edited — ensure build command present
        if "build_doc37.py" not in front:
            front = front.rstrip() + "\n\n" + new_regen
    else:
        front = front.rstrip() + "\n\n" + new_regen
    return front.rstrip() + "\n\n"


def main() -> int:
    if not FRONT.is_file():
        print(f"missing {FRONT}", file=sys.stderr)
        return 1
    data = default_event_dat().read_bytes()
    front = patch_front(FRONT.read_text(encoding="utf-8"))
    doc = front + build_decode_section(data)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(doc, encoding="utf-8", newline="\n")
    nlines = doc.count("\n") + (0 if doc.endswith("\n") else 1)
    print(f"wrote {OUT.relative_to(ROOT)} ({nlines} lines)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

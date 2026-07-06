#!/usr/bin/env python3
"""Emit wiki/maze-walker/eventFieldMap.js from game/include/mm2/events/EventFieldMap.h."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "game" / "include" / "mm2" / "events" / "EventFieldMap.h"
OUT = ROOT / "wiki" / "maze-walker" / "eventFieldMap.js"

CELL_RE = re.compile(r"\{0x([0-9A-Fa-f]{2}),\s*\d+\}")


def main() -> int:
    text = HEADER.read_text(encoding="utf-8")
    start = text.index("kEventFieldMap[")
    chunk = text[start:]
    cells = CELL_RE.findall(chunk)
    if len(cells) != 0x80:
        raise SystemExit(f"expected 128 field-map cells, got {len(cells)}")

    body = "\n".join(
        [
            "/** Auto-generated — run: python tools/gen_walker_event_field_map.py */",
            '"use strict";',
            "",
            "/** Selector -> record byte offset; 0xFF = computed / non-writable (EventFieldMap.h). */",
            f"export const EVENT_FIELD_OFFSET = new Uint8Array([{', '.join(f'0x{c}' for c in cells)}]);",
            "",
            "export function resolveMemberFieldOffset(selector) {",
            "  const sel = selector & 0x7f;",
            "  const off = EVENT_FIELD_OFFSET[sel];",
            "  return off === 0xff ? null : off;",
            "}",
            "",
        ]
    )
    OUT.write_text(body, encoding="utf-8")
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

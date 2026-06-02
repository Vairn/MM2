#!/usr/bin/env python3
"""Build event VM token-length table (A4-$6CC8).

The skip helper @ 0x157FC indexes word_table[token_byte * 2] and adds that
delta to event_parse_pos. For fixed-arg opcodes the delta equals 1 + argc.

Output: EXTRACTED/event_token_len_table.json
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from decode_event import OPCODES  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "EXTRACTED" / "event_token_len_table.json"


def build_table() -> list[dict]:
    rows: list[dict] = []
    for b in range(256):
        spec = OPCODES.get(b)
        if spec is None:
            delta = 1  # unknown byte: advance one (safe minimum)
            note = "unknown"
        else:
            argc = spec["argc"]
            if argc is None:
                # OP_10/11/2B: selector byte only in static skip; stream is variable
                delta = 2
                note = "variable_skip_tok"
            elif b == 0xFF:
                delta = 1
                note = "record_delim"
            else:
                delta = 1 + int(argc)
                note = str(spec["name"])
        rows.append({"byte": b, "delta": delta, "note": note})
    return rows


def main() -> None:
    rows = build_table()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(
        json.dumps({"word_indexed": True, "entries": rows}, indent=2),
        encoding="utf-8",
    )
    print(f"Wrote {OUT} ({len(rows)} entries)")


if __name__ == "__main__":
    main()

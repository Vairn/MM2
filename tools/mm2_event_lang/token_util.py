"""Token skip helpers using event_token_len_table.json."""

from __future__ import annotations

import json
from pathlib import Path

from decode_event import OPCODES

ROOT = Path(__file__).resolve().parents[2]
TOKEN_TABLE_PATH = ROOT / "EXTRACTED" / "event_token_len_table.json"


def load_token_deltas() -> list[int]:
    if TOKEN_TABLE_PATH.is_file():
        data = json.loads(TOKEN_TABLE_PATH.read_text(encoding="utf-8"))
        return [int(e["delta"]) for e in data["entries"]]
    return [1] * 256


def advance_one_token(seg: bytes, pos: int, token_deltas: list[int] | None = None) -> int | None:
    if pos >= len(seg):
        return None
    op = seg[pos]
    spec = OPCODES.get(op)
    if spec is None:
        return None
    argc = spec["argc"]
    if argc is None:
        return pos + 2 if pos + 1 < len(seg) else None
    end = pos + 1 + int(argc)
    return end if end <= len(seg) else None


def skip_n_tokens(
    seg: bytes, pos: int, n: int, token_deltas: list[int] | None = None
) -> int | None:
    cur = pos
    for _ in range(n):
        nxt = advance_one_token(seg, cur, token_deltas)
        if nxt is None:
            return None
        cur = nxt
    return cur


def count_tokens(seg: bytes, start: int, end: int) -> int:
    """Count complete opcode tokens from start up to (not including) end byte offset."""
    n = 0
    pos = start
    while pos < end:
        nxt = advance_one_token(seg, pos)
        if nxt is None or nxt > end:
            break
        n += 1
        pos = nxt
    return n


def count_tokens_in_ops(ops: list, start_idx: int, end_idx: int) -> int:
    return max(0, end_idx - start_idx)

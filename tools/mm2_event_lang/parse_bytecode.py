"""Parse location records into lowered opcode streams."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from decode_event import (  # noqa: E402
    decode_location,
    looks_like_text_record,
    parse_segment_stream_nodes,
    split_script_segments,
)

from .ast import LoweredOp, RecordKind


def classify_record_kind(loc: dict) -> RecordKind:
    kind = loc.get("record_kind", "unknown")
    mapping = {
        "standard": RecordKind.STANDARD,
        "string_bank": RecordKind.STRING_BANK,
        "mixed_pool": RecordKind.MIXED_POOL,
        "castle_blob": RecordKind.CASTLE_BLOB,
    }
    return mapping.get(kind, RecordKind.UNKNOWN)


def parse_segment(seg: bytes) -> list[LoweredOp]:
    if not seg:
        return []
    nodes = parse_segment_stream_nodes(seg)
    return [
        LoweredOp(op=int(n["op"]), args=[int(x) for x in n["args"]], off=int(n["off"]))
        for n in nodes
    ]


def parse_location_record(blob: bytes, loc_id: int) -> tuple[dict, list[bytes], list[list[LoweredOp]]]:
    loc = decode_location(blob, loc_id)
    script = blob[loc["script_offset"] : loc["string_table_offset"]]
    segments_raw = split_script_segments(script)
    segments_ops = [parse_segment(s) for s in segments_raw]
    return loc, segments_raw, segments_ops


def is_plain_text_segment(seg: bytes) -> bool:
    return looks_like_text_record(seg)

"""Encode lifted AST / lowered ops into event.dat location records."""

from __future__ import annotations

import struct

from .ast import EventFile, Location, RecordKind, Script, StringDef
from .lower import lower_script, trigger_cond_byte


def _encode_strings(strings: list[StringDef]) -> bytes:
    out = bytearray()
    for s in strings:
        text = s.text.replace("\n", "@").encode("ascii", errors="replace")
        out.extend(text)
        out.append(0xFF)
    return bytes(out)


def _ops_to_bytes(ops) -> bytes:
    out = bytearray()
    for op in ops:
        out.append(op.op)
        out.extend(op.args)
    return bytes(out)


def _build_string_index(strings: list[StringDef]) -> dict[str, int]:
    idx: dict[str, int] = {}
    for s in strings:
        idx[s.name] = s.index
    return idx


def encode_location(loc: Location) -> bytes:
    if loc.raw_blob is not None and not loc.modified:
        return bytes(loc.raw_blob)

    strings = sorted(loc.strings, key=lambda s: s.index)
    for i, s in enumerate(strings):
        if s.index < 0:
            s.index = i

    string_index = _build_string_index(strings)
    str_table = _encode_strings(strings)

    max_event = 0
    for sc in loc.scripts:
        max_event = max(max_event, sc.event_id)
    for t in loc.triggers:
        max_event = max(max_event, t.event_id)

    script_by_id: dict[int, Script] = {s.event_id: s for s in loc.scripts}
    segments: list[bytes] = []
    for eid in range(max_event + 1):
        sc = script_by_id.get(eid)
        if sc is None:
            segments.append(b"")
        elif sc.is_plain_text:
            text = sc.body[0].data.get("text", "") if sc.body else ""
            segments.append(text.replace("\n", "@").encode("ascii", errors="replace"))
        else:
            from .lower import lower_stmts

            ops = lower_stmts(sc.body, string_index) if sc.body else []
            segments.append(_ops_to_bytes(ops))

    script_pool = bytearray()
    for seg in segments:
        script_pool.extend(seg)
        script_pool.append(0xFF)

    triplets = bytearray()
    for t in loc.triggers:
        pos = ((t.y & 0xF) << 4) | (t.x & 0xF)
        cond = t.cond_raw if t.cond.value == "raw" else trigger_cond_byte(t.cond.value, t.cond_raw)
        triplets.extend([pos, t.event_id & 0xFF, cond & 0xFF])
    if loc.terminated:
        triplets.extend([0, 0, 0])

    str_rel = 2 + len(script_pool)
    header = struct.pack("<H", str_rel)
    record = bytes(triplets) + header + bytes(script_pool) + str_table
    return record


def encode_file(event: EventFile) -> bytes:
    from decode_event import encode_event_dat

    records = [encode_location(loc) for loc in event.locations]
    if event.header and len(event.header) == len(records):
        return encode_event_dat(records, event.header)
    return encode_event_dat(records)

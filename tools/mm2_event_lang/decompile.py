"""Decompile event.dat records into lifted AST."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from decode_event import load_event_records, read_header  # noqa: E402

from .ast import EventFile, Location, RecordKind, Script, Stmt, StringDef, Trigger, TriggerCond
from .cfg_lift import lift_segment
from .parse_bytecode import (
    classify_record_kind,
    is_plain_text_segment,
    parse_location_record,
)
from .semantic_tables import (
    TRIGGER_COND_BY_BYTE,
    auto_script_name,
    auto_string_name,
    load_items_table,
)


def _trigger_cond(raw: int) -> TriggerCond:
    label = TRIGGER_COND_BY_BYTE.get(raw)
    if label == "always":
        return TriggerCond.ALWAYS
    if label == "enter":
        return TriggerCond.ENTER
    if label == "from north":
        return TriggerCond.FROM_NORTH
    if label == "dir special":
        return TriggerCond.DIR_SPECIAL
    if label == "any_direction":
        return TriggerCond.ANY_DIRECTION
    if label == "facing ns":
        return TriggerCond.FACING_NS
    if label == "enter special":
        return TriggerCond.ENTER_SPECIAL
    return TriggerCond.RAW


def decompile_location(
    blob: bytes,
    loc_id: int,
    items: list[str] | None = None,
) -> Location:
    items = items if items is not None else load_items_table()
    kind = RecordKind.UNKNOWN

    if not blob:
        return Location(id=loc_id, record_kind=kind, raw_blob=b"")

    loc_dict, segments_raw, segments_ops = parse_location_record(blob, loc_id)
    kind = classify_record_kind(loc_dict)
    strings_raw: list[str] = loc_dict["strings"]

    triplets_end = loc_dict["script_offset"] - 2
    raw_triplets = bytes(blob[:triplets_end]) if triplets_end > 0 else b""

    if kind == RecordKind.CASTLE_BLOB:
        return Location(
            id=loc_id,
            record_kind=kind,
            terminated=bool(loc_dict.get("terminated")),
            raw_blob=bytes(blob),
            raw_triplets=raw_triplets,
            comment="castle_blob — raw record preserved",
        )

    string_defs: list[StringDef] = []
    name_by_index: dict[int, str] = {}
    for i, text in enumerate(strings_raw):
        name = auto_string_name(i, text)
        name_by_index[i] = name
        string_defs.append(StringDef(name=name, text=text, index=i))

    triggers: list[Trigger] = []
    by_event: dict[int, list] = {}
    for pos, evt, cond_raw in loc_dict["triplets"]:
        y, x = (pos >> 4) & 0xF, pos & 0xF
        tc = _trigger_cond(cond_raw)
        script_name = auto_script_name(evt)
        triggers.append(
            Trigger(
                y=y,
                x=x,
                cond=tc,
                cond_raw=cond_raw,
                script_name=script_name,
                event_id=evt,
            )
        )
        by_event.setdefault(evt, []).append(triggers[-1])

    scripts: list[Script] = []
    for eid, seg_raw in enumerate(segments_raw):
        name = auto_script_name(eid)

        if is_plain_text_segment(seg_raw):
            text = seg_raw.decode("ascii", errors="replace").replace("@", "\n")
            scripts.append(
                Script(
                    name=name,
                    event_id=eid,
                    body=[Stmt.plain_text(text)],
                    is_plain_text=True,
                    raw_segment=bytes(seg_raw),
                )
            )
            continue

        ops = segments_ops[eid] if eid < len(segments_ops) else []
        body = lift_segment(ops, seg_raw, strings_raw, items)

        for t in by_event.get(eid, []):
            t.script_name = name

        for stmt in _walk_stmts(body):
            if stmt.kind in ("say", "service_title") and isinstance(stmt.data.get("string"), int):
                idx = stmt.data["string"]
                stmt.data["string"] = name_by_index.get(idx, f"s{idx}")

        scripts.append(
            Script(
                name=name,
                event_id=eid,
                body=body,
                is_plain_text=False,
                raw_segment=bytes(seg_raw),
            )
        )

    return Location(
        id=loc_id,
        record_kind=kind,
        terminated=bool(loc_dict.get("terminated")),
        triggers=triggers,
        strings=string_defs,
        scripts=scripts,
        raw_blob=bytes(blob),
        raw_triplets=raw_triplets,
    )


def _walk_stmts(stmts):
    from .ast import Stmt

    for s in stmts:
        yield s
        if s.kind == "if":
            yield from _walk_stmts(s.data.get("then") or [])
            yield from _walk_stmts(s.data.get("else") or [])
        elif s.kind == "unlifted":
            yield from _walk_stmts(s.data.get("body") or [])


def decompile_file(path: Path, items: list[str] | None = None) -> EventFile:
    data = path.read_bytes()
    header, records = load_event_records(data)
    locations = [decompile_location(rec, i, items) for i, rec in enumerate(records)]
    return EventFile(locations=locations, raw_records=list(records), header=header)

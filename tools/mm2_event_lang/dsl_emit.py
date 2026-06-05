"""Emit readable .mm2evt from lifted AST."""

from __future__ import annotations

import textwrap

from .ast import EventFile, Expr, Location, RecordKind, Script, Stmt, TriggerCond


def _indent(lines: list[str], level: int = 1) -> list[str]:
    pad = "  " * level
    return [f"{pad}{ln}" if ln else "" for ln in lines]


def _format_string_block(name: str, text: str) -> list[str]:
    if "\n" in text or len(text) > 72:
        escaped = text.replace('"""', '\\"\\"\\"')
        return [f"  {name}:", f'    """{escaped}"""']
    shown = text.replace("\n", "@")
    return [f'  {name}: "{shown}"']


def _format_expr(expr: Expr) -> str:
    k, d = expr.kind, expr.data
    if k == "class_is":
        return f"class {d['class']}"
    if k == "gold_at_least":
        return f"gold >= {d['amount']}"
    if k == "answer_eq":
        return f'answer == "{d["text"]}"'
    if k == "has_item":
        probe = d.get("probe", 0)
        if probe:
            return f'has_item "{d["name"]}" probe={probe}'
        return f'has_item "{d["name"]}"'
    if k == "code16":
        return f"code16 == 0x{d['value']:04X}"
    if k == "era_in":
        return f"era in {d['lo']}..{d['hi']}"
    if k == "day_in":
        return f"day in {d['lo']}..{d['hi']}"
    if k == "monster_present":
        return f"monster_present 0x{d['a']:02X} 0x{d['b']:02X}"
    if k == "yes_no":
        return "yes_no" if not d.get("mode") else "yes_no mode=1"
    if k == "threshold":
        return f"cond >= 0x{d['value']:02X}"
    if k == "raw_op":
        args = " ".join(f"0x{x:02X}" for x in d["args"])
        return f"@op 0x{d['op']:02X} {args}".strip()
    return f"@cond {k}"


def _say_verb(variant: str) -> str:
    if variant == "door":
        return "say_door"
    if variant == "block":
        return "say_block"
    if variant in ("popup_a", "popup_b"):
        return "say_popup"
    if variant == "basic":
        return "say"
    return "say"


def _format_stmt(stmt: Stmt, depth: int = 1, next_stmt: Stmt | None = None) -> list[str]:
    k, d = stmt.kind, stmt.data
    pad = "  " * depth

    if k == "say":
        verb = _say_verb(str(d.get("variant", "")))
        ref = d["string"]
        return [f"{pad}{verb} {ref}"]

    if k == "service_title":
        ref = d["string"]
        mode = int(d.get("mode", 0))
        return [f"{pad}service_title {ref} mode={mode}"]

    if k == "wait":
        return [f"{pad}wait {d['kind']}"]

    if k == "ask_yes_no":
        if d.get("mode"):
            return [f"{pad}ask yes_no mode=1"]
        return [f"{pad}ask yes_no"]

    if k == "if":
        cond = _format_expr(d["cond"])
        lines = [f"{pad}if {cond}:"]
        then_body = d.get("then") or []
        for j, s in enumerate(then_body):
            nxt = then_body[j + 1] if j + 1 < len(then_body) else None
            lines.extend(_format_stmt(s, depth + 1, nxt))
        else_body = d.get("else") or []
        if else_body:
            lines.append(f"{pad}else:")
            for j, s in enumerate(else_body):
                nxt = else_body[j + 1] if j + 1 < len(else_body) else None
                lines.extend(_format_stmt(s, depth + 1, nxt))
        return lines

    if k == "set_quest_complete":
        return [f"{pad}set quest_complete"]
    if k == "set_quest_flag":
        return [f"{pad}set quest_flag {d['name']} {d.get('value', 'ok')}"]
    if k == "apply_party":
        return [
            f"{pad}@apply_party count=0x{d['count']:02X} op=0x{d['op']:02X} val=0x{d['val']:02X}"
        ]
    if k == "apply_party_masked":
        return [
            f"{pad}@apply_party_masked set=0x{d['set']:02X} and=0x{d['and']:02X} or=0x{d['or']:02X}"
        ]
    if k == "selector":
        return [f"{pad}selector 0x{d['value']:02X}"]
    if k == "shop":
        return [f"{pad}selector 0x{d.get('selector', 0):02X}"]
    if k == "quest":
        return [f"{pad}selector 0x{d.get('selector', 0):02X}"]
    if k == "engine_call":
        return [f"{pad}@engine_call 0x{d['code']:02X}"]
    if k == "go_to":
        return [f"{pad}go_to screen {d['screen']} pos 0x{d['pos']:02X}"]
    if k == "fight":
        m = " ".join(f"0x{x:02X}" for x in d["monsters"])
        f = " ".join(f"0x{x:02X}" for x in d["flags"])
        return [f"{pad}fight monsters {m} flags {f}"]
    if k == "fight_b":
        data = " ".join(f"0x{x:02X}" for x in d["data"])
        return [f"{pad}fight_b {data}"]
    if k == "set_tile":
        return [f"{pad}set_tile ({d['y']},{d['x']}) 0x{d['a']:02X} 0x{d['b']:02X}"]
    if k == "clear_tile_event":
        return [f"{pad}clear_tile_event"]
    if k == "clear_input":
        return [f"{pad}clear_input"]
    if k == "abort":
        return [f"{pad}abort"]
    if k == "end":
        return [f"{pad}end"]
    if k == "delay":
        return [f"{pad}delay {d['ticks']}"]
    if k == "plain_text":
        txt = d["text"].replace("\n", "@")
        return [f"{pad}| {txt} |"]
    if k == "unlifted":
        lines = [f"{pad}@unlifted {{"]
        for s in d.get("body") or []:
            lines.extend(_format_stmt(s, depth + 1))
        lines.append(f"{pad}}}")
        return lines
    if k == "raw_op":
        args = " ".join(f"0x{x:02X}" for x in d["args"])
        return [f"{pad}@op 0x{d['op']:02X} {args}".strip()]

    return [f"{pad}@{k}"]


def _format_trigger_cond(t) -> str:
    if t.cond == TriggerCond.RAW:
        return f"when 0x{t.cond_raw:02X}"
    return t.cond.value


def emit_location(loc: Location, area_comment: str = "") -> str:
    lines: list[str] = []
    if area_comment:
        lines.append(f"# {area_comment}")
    lines.append(f"location {loc.id}")
    lines.append(f"record {loc.record_kind.value}")

    if loc.record_kind == RecordKind.CASTLE_BLOB and loc.raw_blob is not None:
        lines.append("raw_record:")
        hex_bytes = loc.raw_blob.hex(" ")
        for chunk in textwrap.wrap(hex_bytes, 72):
            lines.append(f"  {chunk}")
        return "\n".join(lines) + "\n"

    if loc.strings:
        lines.append("")
        lines.append("strings:")
        for s in loc.strings:
            lines.extend(_format_string_block(s.name, s.text))

    if loc.triggers:
        lines.append("")
        seen: set[tuple] = set()
        for t in loc.triggers:
            key = (t.y, t.x, t.cond_raw, t.event_id)
            if key in seen:
                continue
            seen.add(key)
            cond = _format_trigger_cond(t)
            lines.append(f"on tile ({t.y}, {t.x}) {cond} -> {t.script_name}  @event {t.event_id}")

    for sc in loc.scripts:
        if not sc.body and not sc.is_plain_text:
            continue
        lines.append("")
        lines.append(f"script {sc.name}:  @event {sc.event_id}")
        if sc.is_plain_text and sc.body:
            lines.extend(_format_stmt(sc.body[0], 1))
        else:
            for j, stmt in enumerate(sc.body):
                nxt = sc.body[j + 1] if j + 1 < len(sc.body) else None
                lines.extend(_format_stmt(stmt, 1, nxt))

    return "\n".join(lines) + "\n"


def emit_file(event: EventFile) -> str:
    parts = []
    for loc in event.locations:
        parts.append(emit_location(loc))
    return "\n".join(parts)

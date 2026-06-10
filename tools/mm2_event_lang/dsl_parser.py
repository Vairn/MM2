"""Parse .mm2evt text into lifted AST."""

from __future__ import annotations

import re
from pathlib import Path

from .ast import EventFile, Expr, Location, RecordKind, Script, StringDef, Stmt, Trigger, TriggerCond

RE_LOCATION = re.compile(r"^location\s+(\d+)", re.I)
RE_RECORD = re.compile(r"^record\s+(\w+)", re.I)
RE_STRING_LINE = re.compile(r'^(\w+):\s*"(.*)"\s*$')
RE_STRING_ALIAS = re.compile(r"^(\w+):\s*@(\w+)\s*$")
RE_STRING_BLOCK = re.compile(r"^(\w+):\s*$")
RE_TRIGGER = re.compile(
    r"^on\s+tile\s+\((\d+),\s*(\d+)\)\s+(.+?)\s+->\s+(\w+)(?:\s+@event\s+(\d+))?",
    re.I,
)
RE_SCRIPT = re.compile(r"^script\s+(\w+):\s*(?:@event\s+(\d+))?", re.I)
RE_EVENT_META = re.compile(r"@event\s+(\d+)", re.I)


def _parse_cond(text: str) -> tuple[TriggerCond, int]:
    t = text.strip().lower()
    if t.startswith("when 0x"):
        return TriggerCond.RAW, int(t.split("0x")[1], 16)
    mapping = {
        "always": TriggerCond.ALWAYS,
        "enter": TriggerCond.ENTER,
        "from north": TriggerCond.FROM_NORTH,
        "dir special": TriggerCond.DIR_SPECIAL,
        "any_direction": TriggerCond.ANY_DIRECTION,
        "facing ns": TriggerCond.FACING_NS,
        "enter special": TriggerCond.ENTER_SPECIAL,
    }
    return mapping.get(t, TriggerCond.RAW), 0


def _parse_expr(text: str) -> Expr:
    t = text.strip()
    if t.startswith("class_field "):
        m = re.match(r"class_field\s+0x([0-9A-Fa-f]+)(?:\s+mask=0x([0-9A-Fa-f]+))?", t)
        if m:
            field = int(m.group(1), 16)
            mask = int(m.group(2), 16) if m.group(2) else 0x05
            return Expr.class_field(field, mask)
    if t.startswith("class "):
        return Expr.class_is(t[6:].strip())
    if re.match(r"has_item\s+0x", t, re.I):
        m = re.match(r"has_item\s+0x([0-9A-Fa-f]+)(?:\s+probe=(\d+))?", t, re.I)
        if m:
            return Expr.has_item_id(int(m.group(1), 16), int(m.group(2) or 0))
    if t.startswith("gold >="):
        return Expr.gold_at_least(int(t.split(">=")[1].strip()))
    if t.startswith('answer == "'):
        inner = t.split('"')[1]
        return Expr.answer_eq(inner)
    if t.startswith('has_item "'):
        parts = t.split('"')
        name = parts[1]
        probe = 0
        if "probe=" in t:
            probe = int(re.search(r"probe=(\d+)", t).group(1))
        return Expr.has_item(name, probe)
    if t.startswith("code16"):
        v = int(t.split("0x")[1], 16)
        return Expr.code16(v)
    if t.startswith("yes_no"):
        mode = 1 if "mode=1" in t else 0
        return Expr.yes_no(mode)
    if t.startswith("load_var8 "):
        m = re.match(
            r"load_var8\s+group=0x([0-9A-Fa-f]+)(?:\s+index=0x([0-9A-Fa-f]+))?",
            t,
            re.I,
        )
        if m:
            idx = int(m.group(2), 16) if m.group(2) else 0
            return Expr("load_var8", {"group": int(m.group(1), 16), "index": idx})
    return Expr("unknown", {"text": t})


def _code_part(line: str) -> str:
    """Strip trailing inline comment (emitter uses '  # ...')."""
    return line.strip().split("  #", 1)[0].strip()


def _parse_stmt_line(line: str) -> Stmt | None:
    s = _code_part(line)
    if not s or s.startswith("#"):
        return None
    if s.startswith("| ") and s.endswith(" |"):
        return Stmt.plain_text(s[2:-2].replace("@", "\n"))
    if s == "wait space":
        return Stmt.wait("space")
    if s == "wait key":
        return Stmt.wait("key")
    if s == "ask yes_no":
        return Stmt.ask_yes_no(0)
    if s == "ask yes_no mode=1":
        return Stmt.ask_yes_no(1)
    if s == "clear_input":
        return Stmt.clear_input()
    if s == "clear_tile_event":
        return Stmt.clear_tile_event()
    if s == "abort":
        return Stmt.abort()
    if s == "end":
        return Stmt.end()
    if s == "set quest_complete":
        return Stmt.set_quest_complete()
    if s.startswith("set quest_flag "):
        parts = s.split()
        return Stmt.set_quest_flag(parts[2], parts[3] if len(parts) > 3 else "ok")
    if s.startswith("selector 0x") or s.startswith("selector 0X"):
        val = int(s.split("0x", 1)[1].split()[0], 16)
        return Stmt.selector(val)
    if s.startswith("shop "):
        name = s[5:].strip().split("#", 1)[0].strip()
        return Stmt.shop(name, 0)
    if s.startswith("quest "):
        name = s[6:].strip().split("#", 1)[0].strip()
        return Stmt.quest(name, 0)
    if s.startswith("service_title "):
        m = re.match(r"service_title\s+(\w+)\s+mode=(\d+)", s.split("#", 1)[0].strip())
        if m:
            return Stmt.service_title(m.group(1), int(m.group(2)))
    if s.startswith("say_service "):
        m = re.match(r"say_service\s+(\w+)\s+mode=(\d+)", s.split("#", 1)[0].strip())
        if m:
            return Stmt.service_title(m.group(1), int(m.group(2)))
    if s.startswith("say_door "):
        return Stmt.say("door", s[9:].strip())
    if s.startswith("say_block "):
        return Stmt.say("block", s[10:].strip())
    if s.startswith("say_popup "):
        return Stmt.say("popup_a", s[10:].strip())
    if s.startswith("say "):
        return Stmt.say("", s[4:].strip())
    if s.startswith("go_to "):
        m = re.search(r"screen\s+(\d+)\s+pos\s+0x([0-9A-Fa-f]+)", s)
        if m:
            return Stmt.go_to(int(m.group(1)), int(m.group(2), 16))
    if s.startswith("delay "):
        return Stmt.delay(int(s[6:].strip()))
    if s.startswith("load_var8 "):
        m = re.match(
            r"load_var8\s+group=0x([0-9A-Fa-f]+)(?:\s+index=0x([0-9A-Fa-f]+))?",
            s,
            re.I,
        )
        if m:
            idx = int(m.group(2), 16) if m.group(2) else 0
            return Stmt.load_var8(int(m.group(1), 16), idx)
    if s.startswith("store_var8 "):
        m = re.match(r"store_var8\s+group=0x([0-9A-Fa-f]+)\s+value=0x([0-9A-Fa-f]+)", s, re.I)
        if m:
            return Stmt.store_var8(int(m.group(1), 16), int(m.group(2), 16))
    if s.startswith("@op "):
        parts = s.split()
        op = int(parts[1], 16)
        args = [int(x, 16) for x in parts[2:]]
        return Stmt.raw_op(op, args)
    if s.startswith("if "):
        return None
    if s.startswith("else"):
        return None
    return Stmt("unknown_line", {"text": s})


def _indent_level(line: str) -> int:
    stripped = line.lstrip(" ")
    return (len(line) - len(stripped)) // 2


def _parse_body(lines: list[str], start: int) -> tuple[list[Stmt], int]:
    body: list[Stmt] = []
    i = start
    while i < len(lines):
        raw = lines[i]
        if not raw.strip():
            i += 1
            continue
        lvl = _indent_level(raw)
        if lvl == 0 and not raw.startswith("  "):
            break
        if lvl == 0 and start > 0:
            break
        rel = raw.strip()
        if rel.startswith("if "):
            cond_text = rel[3:].rstrip(":")
            cond = _parse_expr(cond_text)
            then_body, i = _parse_body(lines, i + 1)
            else_body: list[Stmt] = []
            if i < len(lines) and lines[i].strip() == "else:":
                else_body, i = _parse_body(lines, i + 1)
            body.append(Stmt.if_stmt(cond, then_body, else_body))
            continue
        if rel == "else:":
            break
        stmt = _parse_stmt_line(raw.strip())
        if stmt:
            body.append(stmt)
        i += 1
    return body, i


def parse_location(text: str) -> Location:
    lines = text.splitlines()
    loc_id = 0
    record_kind = RecordKind.STANDARD
    strings: list[StringDef] = []
    triggers: list[Trigger] = []
    scripts: list[Script] = []
    raw_blob: bytes | None = None
    terminated = True
    script_map: dict[str, int] = {}

    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            i += 1
            continue

        m = RE_LOCATION.match(stripped)
        if m:
            loc_id = int(m.group(1))
            i += 1
            continue

        m = RE_RECORD.match(stripped)
        if m:
            try:
                record_kind = RecordKind(m.group(1).lower())
            except ValueError:
                record_kind = RecordKind.UNKNOWN
            i += 1
            continue

        if stripped == "raw_record:":
            hex_parts: list[str] = []
            i += 1
            while i < len(lines) and (lines[i].startswith("  ") or not lines[i].strip()):
                hex_parts.extend(lines[i].split())
                i += 1
            raw_blob = bytes(int(b, 16) for b in hex_parts if b)
            continue

        if stripped == "strings:":
            i += 1
            idx = 0
            while i < len(lines):
                sl = lines[i].strip()
                if not sl or sl.startswith("on ") or sl.startswith("script "):
                    break
                m = RE_STRING_LINE.match(sl)
                if m:
                    strings.append(StringDef(m.group(1), m.group(2).replace("@", "\n"), idx))
                    idx += 1
                    i += 1
                    continue
                m = RE_STRING_BLOCK.match(sl)
                if m and i + 1 < len(lines) and '"""' in lines[i + 1]:
                    name = m.group(1)
                    i += 2
                    buf: list[str] = []
                    while i < len(lines) and '"""' not in lines[i]:
                        buf.append(lines[i].strip())
                        i += 1
                    if i < len(lines):
                        last = lines[i].split('"""')[0]
                        if last.strip():
                            buf.append(last.strip())
                    strings.append(StringDef(name, "\n".join(buf), idx))
                    idx += 1
                    i += 1
                    continue
                break
            continue

        m = RE_TRIGGER.match(stripped)
        if m:
            y, x = int(m.group(1)), int(m.group(2))
            cond, raw = _parse_cond(m.group(3))
            name = m.group(4)
            eid = int(m.group(5)) if m.group(5) else script_map.get(name, 0)
            script_map[name] = eid
            triggers.append(
                Trigger(y=y, x=x, cond=cond, cond_raw=raw, script_name=name, event_id=eid)
            )
            i += 1
            continue

        m = RE_SCRIPT.match(stripped)
        if m:
            name = m.group(1)
            eid = int(m.group(2)) if m.group(2) else script_map.get(name, len(scripts))
            script_map[name] = eid
            body, i = _parse_body(lines, i + 1)
            is_plain = bool(body) and body[0].kind == "plain_text"
            scripts.append(Script(name=name, event_id=eid, body=body, is_plain_text=is_plain))
            continue

        i += 1

    if record_kind == RecordKind.CASTLE_BLOB and raw_blob is None:
        raw_blob = b""

    return Location(
        id=loc_id,
        record_kind=record_kind,
        terminated=terminated,
        triggers=triggers,
        strings=strings,
        scripts=scripts,
        raw_blob=raw_blob,
        modified=True,
    )


def parse_file(text: str) -> EventFile:
    chunks = re.split(r"(?=^location\s+\d+)", text, flags=re.M)
    locations = [parse_location(c) for c in chunks if c.strip()]
    return EventFile(locations=locations)


def parse_file_path(path: Path) -> EventFile:
    return parse_file(path.read_text(encoding="utf-8"))

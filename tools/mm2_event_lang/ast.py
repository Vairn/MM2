"""Event script AST — lifted (human) and lowered (bytecode) layers."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any


class RecordKind(str, Enum):
    STANDARD = "standard"
    STRING_BANK = "string_bank"
    MIXED_POOL = "mixed_pool"
    CASTLE_BLOB = "castle_blob"
    OVERLAY_BANK = "overlay_bank"
    UNKNOWN = "unknown"


class TriggerCond(str, Enum):
    ALWAYS = "always"
    ENTER = "enter"
    FROM_NORTH = "from north"
    DIR_SPECIAL = "dir special"
    ANY_DIRECTION = "any_direction"
    FACING_NS = "facing ns"
    ENTER_SPECIAL = "enter special"
    RAW = "raw"


@dataclass
class Trigger:
    y: int
    x: int
    cond: TriggerCond
    cond_raw: int = 0
    script_name: str = ""
    event_id: int = 0


@dataclass
class StringDef:
    name: str
    text: str
    index: int = -1


@dataclass
class LoweredOp:
    op: int
    args: list[int]
    off: int = 0


@dataclass
class Expr:
    """Lifted condition / value expression."""

    kind: str
    data: dict[str, Any] = field(default_factory=dict)

    @staticmethod
    def class_field(field: int, mask: int = 0x05) -> Expr:
        return Expr("class_field", {"field": field, "mask": mask})

    @staticmethod
    def has_item_id(item_id: int, probe: int = 0) -> Expr:
        return Expr("has_item_id", {"item": item_id, "probe": probe})

    @staticmethod
    def class_is(name: str) -> Expr:
        return Expr("class_is", {"class": name})

    @staticmethod
    def gold_at_least(amount: int) -> Expr:
        return Expr("gold_at_least", {"amount": amount})

    @staticmethod
    def answer_eq(text: str) -> Expr:
        return Expr("answer_eq", {"text": text})

    @staticmethod
    def has_item(name: str, probe: int = 0) -> Expr:
        return Expr("has_item", {"name": name, "probe": probe})

    @staticmethod
    def code16(value: int) -> Expr:
        return Expr("code16", {"value": value})

    @staticmethod
    def era_in(lo: int, hi: int) -> Expr:
        return Expr("era_in", {"lo": lo, "hi": hi})

    @staticmethod
    def day_in(lo: int, hi: int) -> Expr:
        return Expr("day_in", {"lo": lo, "hi": hi})

    @staticmethod
    def monster_present(a: int, b: int) -> Expr:
        return Expr("monster_present", {"a": a, "b": b})

    @staticmethod
    def yes_no(mode: int = 0) -> Expr:
        return Expr("yes_no", {"mode": mode})

    @staticmethod
    def threshold(value: int) -> Expr:
        return Expr("threshold", {"value": value})

    @staticmethod
    def raw_op(op: int, args: list[int]) -> Expr:
        return Expr("raw_op", {"op": op, "args": args})


@dataclass
class Stmt:
    """Lifted statement."""

    kind: str
    data: dict[str, Any] = field(default_factory=dict)

    @staticmethod
    def say(variant: str, string_ref: str | int, mode: int = 0) -> Stmt:
        return Stmt("say", {"variant": variant, "string": string_ref, "mode": mode})

    @staticmethod
    def service_title(string_ref: str | int, mode: int = 0) -> Stmt:
        """OP_0B: building sign / service banner (not spoken dialog)."""
        return Stmt("service_title", {"string": string_ref, "mode": mode})

    @staticmethod
    def wait(kind: str) -> Stmt:
        return Stmt("wait", {"kind": kind})

    @staticmethod
    def ask_yes_no(mode: int = 0) -> Stmt:
        return Stmt("ask_yes_no", {"mode": mode})

    @staticmethod
    def if_stmt(cond: Expr, then_body: list[Stmt], else_body: list[Stmt] | None = None) -> Stmt:
        return Stmt("if", {"cond": cond, "then": then_body, "else": else_body or []})

    @staticmethod
    def set_quest_complete() -> Stmt:
        return Stmt("set_quest_complete", {})

    @staticmethod
    def set_quest_flag(name: str, value: str = "ok") -> Stmt:
        return Stmt("set_quest_flag", {"name": name, "value": value})

    @staticmethod
    def apply_party(count: int, op_byte: int, val: int) -> Stmt:
        return Stmt("apply_party", {"count": count, "op": op_byte, "val": val})

    @staticmethod
    def apply_party_masked(count: int, set_b: int, and_m: int, or_m: int) -> Stmt:
        return Stmt(
            "apply_party_masked",
            {"count": count, "set": set_b, "and": and_m, "or": or_m},
        )

    @staticmethod
    def selector(value: int) -> Stmt:
        """OP_0E: engine service dispatch byte."""
        return Stmt("selector", {"value": value})

    @staticmethod
    def shop(name: str, selector: int) -> Stmt:
        return Stmt("shop", {"name": name, "selector": selector})

    @staticmethod
    def quest(name: str, selector: int) -> Stmt:
        return Stmt("quest", {"name": name, "selector": selector})

    @staticmethod
    def engine_call(code: int) -> Stmt:
        return Stmt("engine_call", {"code": code})

    @staticmethod
    def go_to(screen: int, pos_byte: int, label: str = "") -> Stmt:
        return Stmt("go_to", {"screen": screen, "pos": pos_byte, "label": label})

    @staticmethod
    def fight(monsters: list[int], flags: list[int]) -> Stmt:
        return Stmt("fight", {"monsters": monsters, "flags": flags})

    @staticmethod
    def fight_b(data: list[int]) -> Stmt:
        return Stmt("fight_b", {"data": data})

    @staticmethod
    def set_tile(y: int, x: int, a: int, b: int) -> Stmt:
        return Stmt("set_tile", {"y": y, "x": x, "a": a, "b": b})

    @staticmethod
    def clear_tile_event() -> Stmt:
        return Stmt("clear_tile_event", {})

    @staticmethod
    def clear_input() -> Stmt:
        return Stmt("clear_input", {})

    @staticmethod
    def abort() -> Stmt:
        return Stmt("abort", {})

    @staticmethod
    def end() -> Stmt:
        return Stmt("end", {})

    @staticmethod
    def delay(ticks: int) -> Stmt:
        return Stmt("delay", {"ticks": ticks})

    @staticmethod
    def load_var8(group: int, index: int = 0) -> Stmt:
        return Stmt("load_var8", {"group": group, "index": index})

    @staticmethod
    def store_var8(group: int, value: int) -> Stmt:
        return Stmt("store_var8", {"group": group, "value": value})

    @staticmethod
    def plain_text(text: str) -> Stmt:
        return Stmt("plain_text", {"text": text})

    @staticmethod
    def unlifted(stmts: list[Stmt]) -> Stmt:
        return Stmt("unlifted", {"body": stmts})

    @staticmethod
    def raw_op(op: int, args: list[int]) -> Stmt:
        return Stmt("raw_op", {"op": op, "args": args})


@dataclass
class Script:
    name: str
    event_id: int
    body: list[Stmt]
    is_plain_text: bool = False
    raw_segment: bytes | None = None


@dataclass
class Location:
    id: int
    record_kind: RecordKind
    terminated: bool = True
    triggers: list[Trigger] = field(default_factory=list)
    strings: list[StringDef] = field(default_factory=list)
    scripts: list[Script] = field(default_factory=list)
    raw_blob: bytes | None = None
    raw_triplets: bytes | None = None
    modified: bool = False
    comment: str = ""


@dataclass
class EventFile:
    locations: list[Location] = field(default_factory=list)
    raw_records: list[bytes] = field(default_factory=list)
    header: list[tuple[int, int]] = field(default_factory=list)

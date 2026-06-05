"""Lower lifted AST back to opcode streams for encoding."""

from __future__ import annotations

from .ast import Expr, LoweredOp, Script, Stmt
from .semantic_tables import (
    CLASS_FIELD_BY_NAME,
    QUEST_FLAG_PATTERNS,
    SAY_OP_BY_VARIANT,
    SELECTOR_BY_NAME,
    TRIGGER_BYTE_BY_COND,
)


def _encode_answer(text: str) -> list[int]:
    out = [0xFA] * 10
    for i, ch in enumerate(text[:10]):
        out[i] = (0x11A - ord(ch)) & 0xFF
    return out


def expr_to_cond_ops(expr: Expr) -> list[LoweredOp]:
    k = expr.kind
    d = expr.data
    if k == "class_is":
        field = CLASS_FIELD_BY_NAME.get(str(d["class"]).lower(), 0)
        return [LoweredOp(0x2D, [field, 0x05])]
    if k == "gold_at_least":
        v = int(d["amount"])
        return [LoweredOp(0x24, [v & 0xFF, (v >> 8) & 0xFF])]
    if k == "answer_eq":
        return [LoweredOp(0x30, _encode_answer(str(d["text"])))]
    if k == "has_item":
        return [LoweredOp(0x28, [int(d.get("probe", 0)), 0])]
    if k == "code16":
        v = int(d["value"])
        return [LoweredOp(0x25, [(v >> 8) & 0xFF, v & 0xFF])]
    if k == "era_in":
        return [LoweredOp(0x22, [int(d["lo"]), int(d["hi"])])]
    if k == "day_in":
        return [LoweredOp(0x23, [int(d["lo"]), int(d["hi"])])]
    if k == "monster_present":
        return [LoweredOp(0x16, [int(d["a"]), int(d["b"])])]
    if k == "yes_no":
        return [LoweredOp(0x0A if int(d.get("mode", 0)) else 0x09, [])]
    if k == "threshold":
        return [LoweredOp(0x1B, [int(d["value"])])]
    if k == "raw_op":
        return [LoweredOp(int(d["op"]), list(d["args"]))]
    return [LoweredOp(0x1C, [0])]


def stmt_to_ops(stmt: Stmt, string_index: dict[str, int]) -> list[LoweredOp]:
    k = stmt.kind
    d = stmt.data

    def sref(key: str) -> int:
        ref = d[key]
        if isinstance(ref, int):
            return ref
        return string_index.get(str(ref), 0)

    if k == "say":
        variant = str(d.get("variant", ""))
        op = SAY_OP_BY_VARIANT.get(variant, 0x03)
        if variant == "service":
            return [LoweredOp(0x0B, [sref("string"), int(d.get("mode", 0))])]
        return [LoweredOp(op, [sref("string")])]
    if k == "service_title":
        return [LoweredOp(0x0B, [sref("string"), int(d.get("mode", 0))])]
    if k == "selector":
        return [LoweredOp(0x0E, [int(d["value"])])]
    if k == "wait":
        return [LoweredOp(0x08 if d["kind"] == "key" else 0x07, [])]
    if k == "ask_yes_no":
        return [LoweredOp(0x0A if int(d.get("mode", 0)) else 0x09, [])]
    if k == "if":
        cond_ops = expr_to_cond_ops(d["cond"])
        then_ops = lower_stmts(d["then"], string_index)
        else_ops = lower_stmts(d.get("else") or [], string_index)
        n_skip = len(else_ops)
        branch = LoweredOp(0x10, [n_skip if else_ops else 0])
        return cond_ops + [branch] + else_ops + then_ops
    if k == "set_quest_complete":
        return [LoweredOp(0x18, [0x00, 0x75, 0xFE, 0x01])]
    if k == "set_quest_flag":
        name = str(d["name"])
        for (set_b, and_m, or_m), (flag, val) in QUEST_FLAG_PATTERNS.items():
            if flag == name and str(d.get("value", "ok")) == val:
                return [LoweredOp(0x18, [0x00, set_b, and_m, or_m])]
        return [LoweredOp(0x18, [0x00, 0x75, 0x00, 0x01])]
    if k == "apply_party":
        return [LoweredOp(0x15, [int(d["count"]), int(d["op"]), int(d["val"])])]
    if k == "apply_party_masked":
        return [
            LoweredOp(
                0x18,
                [int(d["count"]), int(d["set"]), int(d["and"]), int(d["or"])],
            )
        ]
    if k == "shop":
        sel = int(d.get("selector", 0))
        if not sel:
            sel = SELECTOR_BY_NAME.get(("shop", str(d["name"])), 0x01)
        return [LoweredOp(0x0E, [sel])]
    if k == "quest":
        sel = int(d.get("selector", 0))
        if not sel:
            sel = SELECTOR_BY_NAME.get(("quest", str(d["name"])), 0x97)
        return [LoweredOp(0x0E, [sel])]
    if k == "engine_call":
        return [LoweredOp(0x0D, [int(d["code"])])]
    if k == "go_to":
        return [LoweredOp(0x0C, [int(d["screen"]), int(d["pos"])])]
    if k == "fight":
        m = list(d["monsters"])
        f = list(d["flags"])
        return [LoweredOp(0x12, m + f)]
    if k == "fight_b":
        return [LoweredOp(0x13, list(d["data"]))]
    if k == "set_tile":
        pos = ((int(d["y"]) & 0xF) << 4) | (int(d["x"]) & 0xF)
        return [LoweredOp(0x21, [pos, int(d["a"]), int(d["b"])])]
    if k == "clear_tile_event":
        return [LoweredOp(0x14, [])]
    if k == "clear_input":
        return [LoweredOp(0x2F, [])]
    if k == "abort":
        return [LoweredOp(0x29, [])]
    if k == "end":
        return [LoweredOp(0x0F, [])]
    if k == "delay":
        return [LoweredOp(0x1E, [int(d["ticks"])])]
    if k == "plain_text":
        return []
    if k == "unlifted":
        return lower_stmts(d["body"], string_index)
    if k == "raw_op":
        return [LoweredOp(int(d["op"]), list(d["args"]))]
    return []


def lower_stmts(stmts: list[Stmt], string_index: dict[str, int]) -> list[LoweredOp]:
    out: list[LoweredOp] = []
    for s in stmts:
        out.extend(stmt_to_ops(s, string_index))
    return out


def lower_script(script: Script, string_index: dict[str, int]) -> list[LoweredOp]:
    if script.is_plain_text:
        return []
    return lower_stmts(script.body, string_index)


def trigger_cond_byte(cond: str, raw: int = 0) -> int:
    return TRIGGER_BYTE_BY_COND.get(cond.lower(), raw)

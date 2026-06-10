"""Lift lowered opcode streams to readable structured statements."""

from __future__ import annotations

from decode_event import decode_u16_arg

from .ast import Expr, LoweredOp, Stmt
from .semantic_tables import (
    COND_SET_OPS,
    TRANSITION_NAMES,
)
from .token_util import skip_n_tokens


def _index_at_offset(ops: list[LoweredOp], off: int) -> int:
    for i, op in enumerate(ops):
        if op.off >= off:
            return i
    return len(ops)


def _op_to_cond_expr(op: LoweredOp, strings: list[str], items: list[str]) -> Expr:
    o, a = op.op, op.args
    if o == 0x09:
        return Expr.yes_no(0)
    if o == 0x0A:
        return Expr.yes_no(1)
    if o == 0x16 and len(a) >= 2:
        return Expr.monster_present(a[0], a[1])
    if o == 0x1B and a:
        return Expr.threshold(a[0])
    if o == 0x22 and len(a) >= 2:
        return Expr.era_in(a[0], a[1])
    if o == 0x23 and len(a) >= 2:
        return Expr.day_in(a[0], a[1])
    if o == 0x24:
        v = decode_u16_arg(o, a)
        return Expr.gold_at_least(v if v is not None else 0)
    if o == 0x25:
        v = decode_u16_arg(o, a)
        return Expr.code16(v if v is not None else 0)
    if o == 0x28 and len(a) >= 2:
        return Expr.has_item_id(a[1], a[0])
    if o == 0x2D and len(a) >= 2:
        return Expr.class_field(a[0], a[1])
    if o == 0x17 and len(a) >= 2:
        return Expr("load_var8", {"group": a[0], "index": a[1]})
    if o == 0x30:
        decoded = "".join(
            chr((0x11A - b) & 0xFF) if 0x20 <= ((0x11A - b) & 0xFF) <= 0x7E else "?"
            for b in a
        ).rstrip()
        return Expr.answer_eq(decoded)
    return Expr.raw_op(o, a)


def _op_to_stmt(op: LoweredOp, strings: list[str], items: list[str]) -> Stmt | None:
    o, a = op.op, op.args
    if o in COND_SET_OPS:
        return None
    if o in (0x10, 0x11, 0x2B):
        return None
    if o in (0x01, 0x02, 0x03, 0x04, 0x05, 0x06) and a:
        variants = {0x01: "basic", 0x02: "block", 0x03: "", 0x04: "door", 0x05: "popup_a", 0x06: "popup_b"}
        return Stmt.say(variants[o], a[0])
    if o == 0x07:
        return Stmt.wait("space")
    if o == 0x08:
        return Stmt.wait("key")
    if o == 0x09:
        return Stmt.ask_yes_no(0)
    if o == 0x0A:
        return Stmt.ask_yes_no(1)
    if o == 0x0B and len(a) >= 2:
        return Stmt.service_title(a[0], a[1])
    if o == 0x0C and len(a) >= 2:
        label = TRANSITION_NAMES.get((a[0], a[1]), "")
        return Stmt.go_to(a[0], a[1], label)
    if o == 0x0D and a:
        return Stmt.engine_call(a[0])
    if o == 0x0E and a:
        return Stmt.selector(a[0])
    if o == 0x0F:
        return Stmt.end()
    if o == 0x12 and len(a) >= 12:
        return Stmt.fight(a[:10], a[10:12])
    if o == 0x13 and len(a) >= 10:
        return Stmt.fight_b(a[:10])
    if o == 0x14:
        return Stmt.clear_tile_event()
    if o == 0x15 and len(a) >= 3:
        return Stmt.apply_party(a[0], a[1], a[2])
    if o == 0x18 and len(a) >= 4:
        return Stmt.apply_party_masked(a[0], a[1], a[2], a[3])
    if o == 0x17 and len(a) >= 2:
        return Stmt.load_var8(a[0], a[1])
    if o == 0x1A and len(a) >= 2:
        return Stmt.store_var8(a[0], a[1])
    if o == 0x1E and a:
        return Stmt.delay(a[0])
    if o == 0x21 and len(a) >= 3:
        y, x = (a[0] >> 4) & 0xF, a[0] & 0xF
        return Stmt.set_tile(y, x, a[1], a[2])
    if o == 0x29:
        return Stmt.abort()
    if o == 0x2F:
        return Stmt.clear_input()
    return Stmt.raw_op(o, a)


def lift_range(
    ops: list[LoweredOp],
    seg: bytes,
    start: int,
    end: int,
    strings: list[str],
    items: list[str],
) -> list[Stmt]:
    body: list[Stmt] = []
    i = start
    pending_cond: Expr | None = None

    while i < end:
        op = ops[i]

        if op.op in COND_SET_OPS:
            pending_cond = _op_to_cond_expr(op, strings, items)
            i += 1
            continue

        if op.op in (0x10, 0x11) and op.args:
            n = op.args[0]
            skip_start = op.off + 2
            target_off = skip_n_tokens(seg, skip_start, n)
            if target_off is None:
                body.append(_op_to_stmt(op, strings, items) or Stmt.raw_op(op.op, op.args))
                i += 1
                pending_cond = None
                continue
            skip_idx = _index_at_offset(ops, target_off)
            skip_idx = min(skip_idx, end)

            if pending_cond is None:
                pending_cond = Expr("unknown", {})

            if op.op == 0x10:
                else_body = lift_range(ops, seg, i + 1, skip_idx, strings, items)
                then_body = lift_range(ops, seg, skip_idx, end, strings, items)
            else:
                then_body = lift_range(ops, seg, i + 1, skip_idx, strings, items)
                else_body = lift_range(ops, seg, skip_idx, end, strings, items)

            body.append(Stmt.if_stmt(pending_cond, then_body, else_body))
            return body

        if op.op == 0x2B and op.args and pending_cond is None:
            n = op.args[0]
            skip_start = op.off + 2
            target_off = skip_n_tokens(seg, skip_start, n)
            if target_off is not None:
                skip_idx = min(_index_at_offset(ops, target_off), end)
                skipped = lift_range(ops, seg, i + 1, skip_idx, strings, items)
                rest = lift_range(ops, seg, skip_idx, end, strings, items)
                if skipped and rest:
                    body.extend(skipped)
                    body.extend(rest)
                    return body

        stmt = _op_to_stmt(op, strings, items)
        if stmt:
            body.append(stmt)
        pending_cond = None
        i += 1

    return body


def lift_segment(
    ops: list[LoweredOp],
    seg: bytes,
    strings: list[str],
    items: list[str],
) -> list[Stmt]:
    if not ops:
        return []
    try:
        lifted = lift_range(ops, seg, 0, len(ops), strings, items)
        return lifted if lifted else [_op_to_stmt(o, strings, items) or Stmt.raw_op(o.op, o.args) for o in ops]
    except Exception:
        fallback = [_op_to_stmt(o, strings, items) or Stmt.raw_op(o.op, o.args) for o in ops]
        return [Stmt.unlifted(fallback)]

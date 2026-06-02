"""Opcode handlers for event VM (tier A subset)."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from decode_event import OPCODES, decompile_op, decode_u16_arg  # noqa: E402

from .memory import EventSimState


class ScriptRunner:
    def __init__(self, state: EventSimState, strings: list[str]):
        self.state = state
        self.strings = strings
        self.abort = False
        self.done = False

    def read_u8(self) -> int:
        pos = self.state.gs.event_parse_pos
        if pos >= len(self.state.work_buf):
            return 0xFF
        b = self.state.work_buf[pos]
        self.state.gs.event_parse_pos = pos + 1
        return b

    def resolve_string(self, idx: int) -> str:
        if 0 <= idx < len(self.strings):
            return self.strings[idx].replace("@", "\n")
        return f"<str[{idx}]>"

    def skip_tokens(self, n: int) -> None:
        for _ in range(n):
            pos = self.state.gs.event_parse_pos
            if pos >= len(self.state.work_buf):
                break
            tok = self.state.work_buf[pos]
            delta = self.state.token_deltas[tok] if tok < len(self.state.token_deltas) else 1
            self.state.gs.event_parse_pos = pos + delta

    def run_from(self, start: int, end: int | None = None) -> None:
        self.state.gs.event_parse_pos = start
        self.state.gs.script_abort = 0
        self.abort = False
        self.done = False
        while not self.done and not self.abort:
            pos = self.state.gs.event_parse_pos
            if end is not None and pos >= end:
                break
            op = self.read_u8()
            if op == 0xFF:
                self.done = True
                break
            if not self._dispatch(op):
                break

    def _dispatch(self, op: int) -> bool:
        st = self.state
        gs = st.gs
        pos_before = gs.event_parse_pos - 1

        spec = OPCODES.get(op)
        if spec is None:
            gs.script_abort = 1
            st.append_log(f"INVALID op 0x{op:02X}")
            return False

        argc = spec["argc"]
        args: list[int] = []
        if argc is None:
            if gs.event_parse_pos < len(st.work_buf):
                args = [st.work_buf[gs.event_parse_pos]]
                gs.event_parse_pos += 1
        elif argc > 0:
            args = list(st.work_buf[gs.event_parse_pos : gs.event_parse_pos + int(argc)])
            gs.event_parse_pos += len(args)

        pseudo = decompile_op(op, args, self.strings, [])
        st.append_log(pseudo)

        if op in (0x01, 0x02, 0x03, 0x04, 0x05, 0x06) and args:
            st.append_log(f'  TEXT: "{self.resolve_string(args[0])[:100]}"')
        elif op == 0x07:
            st.append_log("  [wait space — auto-continue]")
        elif op == 0x09 or op == 0x0A:
            gs.cond_flag = 1
            st.append_log("  [yn prompt — cond=1]")
        elif op == 0x0F:
            self.done = True
        elif op == 0x10:
            if gs.cond_flag and args:
                self.skip_tokens(args[0])
        elif op == 0x11:
            if not gs.cond_flag and args:
                self.skip_tokens(args[0])
        elif op == 0x14:
            gs.script_abort = 1
            self.abort = True
        elif op == 0x22 and len(args) >= 2:
            gs.cond_flag = 1 if args[0] <= gs.era_low <= args[1] else 0
        elif op == 0x24:
            val = decode_u16_arg(op, args)
            gs.cond_flag = 1 if val is not None and st.party_gold >= val else 0
        elif op == 0x25:
            val = decode_u16_arg(op, args)
            gs.cond_flag = 1 if val == 0 else 0
        elif op == 0x28 and len(args) >= 2:
            gs.cond_flag = 0
            st.append_log(f"  [consume item {args[1]} — cond=0]")
        elif op == 0x29:
            gs.script_abort = 1
            self.abort = True
        elif op == 0x30 and len(args) >= 10:
            ans = "".join(chr((0x11A - b) & 0xFF) for b in args).rstrip()
            st.append_log(f'  [password check "{ans}"]')
            gs.cond_flag = 0
        elif op == 0x32 and args:
            gs.cond_flag = 0
        elif op == 0x0E and args:
            st.append_log(f"  [selector thunk 0x{args[0]:02X}]")

        if gs.event_parse_pos <= pos_before:
            gs.event_parse_pos = pos_before + 1
        return True

"""Event simulator memory model (A4 workspace subset)."""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mm2_gamestate import ANCHOR, EVENT_WORK_SIZE, GameState  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
TOKEN_TABLE_PATH = ROOT / "EXTRACTED" / "event_token_len_table.json"

# Facing index 0/2/4/6 → context mask (defaults until RAM dump available).
DEFAULT_CONTEXT_MASK = [0xF0, 0xF0, 0xF0, 0xF0]

FACING_FROM_KEY = {"N": 0, "E": 2, "S": 4, "W": 6}


@dataclass
class EventSimState:
    gs: GameState
    event_data: bytes = b""
    map_data: bytes = b""
    attrib_data: bytes = b""
    work_buf: bytearray = field(default_factory=lambda: bytearray(EVENT_WORK_SIZE))
    token_deltas: list[int] = field(default_factory=list)
    party_gold: int = 1000
    items: dict[int, str] = field(default_factory=dict)
    log: list[str] = field(default_factory=list)

    @classmethod
    def create(cls, image_size: int = ANCHOR + 0x8000) -> "EventSimState":
        mem = bytearray(image_size)
        gs = GameState(mem)
        st = cls(gs=gs)
        st.load_token_table()
        return st

    def load_token_table(self) -> None:
        if TOKEN_TABLE_PATH.is_file():
            data = json.loads(TOKEN_TABLE_PATH.read_text(encoding="utf-8"))
            self.token_deltas = [e["delta"] for e in data["entries"]]
        else:
            self.token_deltas = [1] * 256

    def sync_scalars_to_gs(self) -> None:
        off = -0x47C8
        ea = ANCHOR + off
        n = min(len(self.work_buf), len(self.gs._mem) - ea)
        self.gs._mem[ea : ea + n] = self.work_buf[:n]

    def party_tile(self) -> int:
        y = self.gs.coord_a & 0xF
        x = self.gs.coord_b & 0xF
        return (y << 4) | x

    def set_party(self, x: int, y: int) -> None:
        self.gs.coord_a = y & 0xF
        self.gs.coord_b = x & 0xF

    def context_mask(self) -> int:
        fi = self.gs.facing_index if hasattr(self.gs, "facing_index") else 0
        if fi < len(DEFAULT_CONTEXT_MASK):
            return DEFAULT_CONTEXT_MASK[fi]
        return 0xF0

    def attrib_era_gate(self) -> int:
        screen = self.gs.screen_mode_id
        if screen >= 60 or len(self.attrib_data) < (screen + 1) * 64:
            return self.gs.era_low
        return self.attrib_data[screen * 64 + 0x0F]

    def append_log(self, msg: str) -> None:
        self.log.append(msg)
        if len(self.log) > 500:
            self.log = self.log[-400:]

    def memory_rows(self) -> list[tuple[str, str, str]]:
        rows: list[tuple[str, str, str]] = []
        fields = [
            ("screen_mode_id", -0x79F2, "b"),
            ("coord_a (Y)", -0x79F0, "b"),
            ("coord_b (X)", -0x79F1, "b"),
            ("era_low", -0x79B5, "b"),
            ("event_parse_pos", -0x7956, "w"),
            ("event_script_anchor", -0x7954, "w"),
            ("pending_event_latch", -0x7952, "b"),
            ("cond_flag", -0x7951, "b"),
            ("script_abort", -0x79EA, "b"),
            ("facing_index", -0x55D7, "b"),
            ("queued_event_id", -0x5D46, "b"),
            ("party_gold (sim)", None, "i"),
        ]
        for name, off, kind in fields:
            if kind == "i":
                val = self.party_gold
                hx = f"{val}"
            elif kind == "b":
                val = self.gs.u8(off)
                hx = f"0x{val:02X}"
            else:
                val = self.gs.u16(off)
                hx = f"0x{val:04X}"
            rows.append((name, hx, str(val)))
        rows.append(("party_tile", f"0x{self.party_tile():02X}", f"({self.gs.coord_a},{self.gs.coord_b})"))
        return rows

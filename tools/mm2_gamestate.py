#!/usr/bin/env python3
"""MM2 global game-state model (A4 workspace).

All mutable runtime state lives in one RAM block anchored by A4 = $7FFE
(``LEA.L $7FFE,A4`` at 0x24920). Each field is a fixed *signed* 16-bit
displacement from A4. See ``EXTRACTED/docs/14-game-state-struct.md`` and the
C mirror ``EXTRACTED/decomp/mm2_gamestate.h``.

Notation: the canonical key is the true signed offset (e.g. ``-0x7951``); the
legacy docs name fields by the raw displacement word (``word = offset & 0xFFFF``
e.g. ``0x86AF``). Effective address ``EA = ANCHOR + offset``.

68000 is big-endian; all word/long access here is big-endian.

Usage::

    gs = GameState.load("snapshot.bin")   # raw memory image, byte0 == addr 0
    print(gs.era, gs.day(gs.era), gs.year(gs.era))
    gs.cond_flag = 1
    gs.save("snapshot.bin")
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

ANCHOR = 0x7FFE
ERA_COUNT = 10
PARTY_SIZE = 8
ROSTER_STRIDE = 0x82
EVENT_WORK_SIZE = 2220


@dataclass(frozen=True)
class FieldDef:
    name: str
    offset: int           # true signed displacement from A4
    size: str             # 'b' | 'w' | 'l' | 'arr'
    note: str = ""

    @property
    def word(self) -> int:
        """Legacy raw displacement word (how older docs name it)."""
        return self.offset & 0xFFFF

    @property
    def ea(self) -> int:
        return (ANCHOR + self.offset) & 0xFFFFFFFF


# Confirmed, ASM-verified fields.
FIELDS: list[FieldDef] = [
    # session / control
    FieldDef("screen_mode_id",   -0x79F2, "b", "$860E mode selector"),
    FieldDef("coord_b",          -0x79F1, "b", "$860F player/map coord"),
    FieldDef("coord_a",          -0x79F0, "b", "$8610 player/map coord (nibble)"),
    FieldDef("script_abort",     -0x79EA, "b", "$8616 event abort flag"),
    FieldDef("first_time_flag",  -0x79E9, "b", "$8617 first-time/modal"),
    FieldDef("screen_mode_prev", -0x79E6, "b", "$861A previous mode id"),
    FieldDef("busy_status",      -0x79E5, "b", "$861B busy/modal latch"),
    FieldDef("era",              -0x79B6, "w", "$864A timeline 0..9"),
    FieldDef("era_low",          -0x79B5, "b", "$864B era index for event gating"),
    FieldDef("new_game_flag",    -0x79B2, "b", "$864E new-game vs load"),
    FieldDef("last_move_key",    -0x79B1, "b", "$864F N/S/E/W"),
    FieldDef("event_parse_pos",  -0x7956, "w", "$86AA event-script cursor"),
    FieldDef("event_script_anchor", -0x7954, "w", "$86AC string anchor / $FFFF=init"),
    FieldDef("pending_event_latch", -0x7952, "b", "$86AE movement pending scan"),
    FieldDef("cond_flag",        -0x7951, "b", "$86AF event predicate result"),
    FieldDef("exit_flags",       -0x7950, "b", "$86B0 ESC/exit bit flags"),
    FieldDef("event_script_start", -0x5C44, "w", "$A3BC saved script PC base"),
    FieldDef("queued_event_id",  -0x5D46, "b", "$A2BA $FF=none"),
    FieldDef("saved_cond_flag",  -0x5D42, "b", "$A2BE party-select save"),
    FieldDef("string_walk_index", -0x5D44, "w", "$A2BC string resolver"),
    FieldDef("facing_index",     -0x55D7, "b", "$AA29 0/2/4/6 N/E/S/W"),
    FieldDef("event_busy_sentinel", -0x55C8, "b", "$AA38 $FF during script"),
    FieldDef("attrib_era_gate_cache", -0x560B, "b", "$A9F5 attrib+0x0F cache"),
    FieldDef("input_state_end",  -0x7999, "b", "$8667 end of input-state span"),
    # calendar / era
    FieldDef("time_subday",      -0x79B4, "w", "$864C 256 = 1 day"),
    FieldDef("period_flag_a",    -0x798C, "b", "$8674 day 60/120/180 reset"),
    FieldDef("period_flag_b",    -0x798D, "b", "$8673 day 60/120/180 reset"),
    # pointers
    FieldDef("draw_ctx",         -0x7A1A, "l", "$85E6 draw context ptr"),
    FieldDef("manx_pool",        -0x5E62, "l", "$A19E MANX arena base"),
    FieldDef("map_blob",         -0x110C, "l", "$EEF4 map.dat ptr"),
    FieldDef("event_blob",       -0x1108, "l", "$EEF8 event.dat ptr"),
]

# Array / table bases (accessed via lea; element typing is per-table).
ARRAY_BASES: list[FieldDef] = [
    FieldDef("day",              -0x79DE, "arr", "$8622 word[10] day-of-year"),
    FieldDef("year",             -0x79CA, "arr", "$8636 word[10] year"),
    FieldDef("input_state",      -0x799D, "arr", "$8663 input-state bytes"),
    FieldDef("roster_index_tbl", -0x796A, "arr", "$8696 party->roster map"),
    FieldDef("class_name_tbl",   -0x7928, "arr", "$86D8 class strings"),
    FieldDef("race_name_tbl",    -0x7908, "arr", "$86F8 race strings"),
    FieldDef("align_name_tbl",   -0x78F4, "arr", "$870C alignment strings"),
    FieldDef("month_tbl",        -0x711C, "arr", "$8EE4 word[13] months"),
    FieldDef("season_tbl_a",     -0x7102, "arr", "$8EFE season derivation"),
    FieldDef("season_tbl_b",     -0x70F5, "arr", "$8F0B season derivation"),
    FieldDef("opcode_len_tbl",   -0x6CC8, "arr", "$9338 event token lengths"),
    FieldDef("context_mask_tbl", -0x6BE6, "arr", "$941A facing->context"),
    FieldDef("party_slots",      -0x5E5E, "arr", "$A1A2 byte[8] party roster ids"),
    FieldDef("tile_runtime_flags", -0x55D6, "arr", "$AA2A per-tile flags"),
    FieldDef("tile_table_a",     -0x55BA, "arr", "$AA46 tile source table"),
    FieldDef("tile_visited",     -0x54BA, "arr", "$AB46 visited/event bits"),
    FieldDef("event_work_buf",   -0x47C8, "arr", "$B838 byte[2220] event buffer"),
    FieldDef("roster_base",      -0x2A3E, "arr", "$D5C2 roster records, stride 0x82"),
]

_SCALARS = {f.name: f for f in FIELDS}


class GameState:
    """Wraps a raw memory image (byte 0 == absolute address 0).

    Scalar confirmed fields are exposed as read/write attributes. Arrays use
    explicit helper methods so element typing stays explicit.
    """

    def __init__(self, image: bytearray):
        self._mem = image

    # ---- construction / serialization (loader/saver) ----
    @classmethod
    def load(cls, path: str | Path) -> "GameState":
        return cls(bytearray(Path(path).read_bytes()))

    @classmethod
    def from_bytes(cls, data: bytes) -> "GameState":
        return cls(bytearray(data))

    def save(self, path: str | Path) -> None:
        Path(path).write_bytes(bytes(self._mem))

    def to_bytes(self) -> bytes:
        return bytes(self._mem)

    # ---- primitive big-endian access by true signed offset ----
    def _ea(self, off: int) -> int:
        ea = ANCHOR + off
        if ea < 0 or ea + 4 > len(self._mem):
            # still allow byte access near the edge; callers size-check
            pass
        return ea

    def u8(self, off: int) -> int:
        return self._mem[self._ea(off)]

    def set_u8(self, off: int, v: int) -> None:
        self._mem[self._ea(off)] = v & 0xFF

    def u16(self, off: int) -> int:
        return struct.unpack_from(">H", self._mem, self._ea(off))[0]

    def set_u16(self, off: int, v: int) -> None:
        struct.pack_into(">H", self._mem, self._ea(off), v & 0xFFFF)

    def u32(self, off: int) -> int:
        return struct.unpack_from(">I", self._mem, self._ea(off))[0]

    def set_u32(self, off: int, v: int) -> None:
        struct.pack_into(">I", self._mem, self._ea(off), v & 0xFFFFFFFF)

    # ---- typed scalar attributes ----
    def __getattr__(self, name: str):
        f = _SCALARS.get(name)
        if f is None:
            raise AttributeError(name)
        if f.size == "b":
            return self.u8(f.offset)
        if f.size == "w":
            return self.u16(f.offset)
        return self.u32(f.offset)

    def __setattr__(self, name: str, value) -> None:
        if name == "_mem":
            super().__setattr__(name, value)
            return
        f = _SCALARS.get(name)
        if f is None:
            super().__setattr__(name, value)
            return
        if f.size == "b":
            self.set_u8(f.offset, value)
        elif f.size == "w":
            self.set_u16(f.offset, value)
        else:
            self.set_u32(f.offset, value)

    # ---- calendar arrays ----
    def day(self, era: int) -> int:
        return self.u16(-0x79DE + era * 2)

    def set_day(self, era: int, v: int) -> None:
        self.set_u16(-0x79DE + era * 2, v)

    def year(self, era: int) -> int:
        return self.u16(-0x79CA + era * 2)

    def set_year(self, era: int, v: int) -> None:
        self.set_u16(-0x79CA + era * 2, v)

    def party_slots(self) -> list[int]:
        base = -0x5E5E
        return [self.u8(base + i) for i in range(PARTY_SIZE)]


def _print_layout() -> None:
    print(f"# MM2 game-state layout (A4 = ${ANCHOR:04X})\n")
    rows = sorted(FIELDS + ARRAY_BASES, key=lambda f: f.offset)
    print(f"{'field':<20} {'offset':>9} {'word':>6} {'EA':>6} {'sz':>4}  note")
    print("-" * 72)
    for f in rows:
        print(f"{f.name:<20} {('-0x%X' % -f.offset):>9} "
              f"{f.word:6X} {f.ea:6X} {f.size:>4}  {f.note}")


if __name__ == "__main__":
    _print_layout()

"""Event loader, init, tile scanner, and pool seek."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from decode_event import decode_location, read_header, split_script_segments  # noqa: E402

from .memory import EventSimState, FACING_FROM_KEY
from .opcodes import ScriptRunner

ROOT = Path(__file__).resolve().parents[2]


class EventRuntime:
    def __init__(self, state: EventSimState):
        self.state = state
        self.loc_id = 0
        self.loc_meta: dict = {}
        self.segments: list[bytes] = []
        self.strings: list[str] = []

    def load_files(
        self,
        event_path: Path | None = None,
        map_path: Path | None = None,
        attrib_path: Path | None = None,
    ) -> None:
        event_path = event_path or ROOT / "event.dat"
        map_path = map_path or ROOT / "map.dat"
        attrib_path = attrib_path or ROOT / "attrib.dat"
        self.state.event_data = event_path.read_bytes()
        self.state.map_data = map_path.read_bytes() if map_path.is_file() else b""
        self.state.attrib_data = attrib_path.read_bytes() if attrib_path.is_file() else b""

    def load_location(self, loc_id: int) -> None:
        header = read_header(self.state.event_data)
        if loc_id < 0 or loc_id >= len(header):
            raise ValueError(f"location {loc_id} out of range")
        off, length = header[loc_id]
        blob = self.state.event_data[off : off + length]
        clamp = min(length, len(blob), 0x8AC)
        self.state.work_buf = bytearray(blob[:clamp].ljust(0x8AC, b"\x00"))
        self.loc_id = loc_id
        self.loc_meta = decode_location(blob, loc_id)
        script = blob[self.loc_meta["script_offset"] : self.loc_meta["string_table_offset"]]
        self.segments = split_script_segments(script) if script else []
        self.strings = self.loc_meta["strings"]
        self.state.gs.screen_mode_id = loc_id & 0xFF
        self.state.gs.event_script_anchor = 0xFFFF
        self.state.gs.pending_event_latch = 0
        self.state.gs.queued_event_id = 0xFF
        if loc_id < 60 and len(self.state.attrib_data) >= (loc_id + 1) * 64:
            gate = self.state.attrib_data[loc_id * 64 + 0x0F]
            self.state.gs.era_low = gate
            self.state.gs.attrib_era_gate_cache = gate
        self._init_parsed()
        self.state.append_log(f"Loaded location {loc_id} ({self.loc_meta.get('record_kind', '?')})")

    def _init_parsed(self) -> None:
        """Port of 0x1754A triplet parse."""
        gs = self.state.gs
        buf = self.state.work_buf
        pos = 0
        while pos + 2 < len(buf):
            a, b, c = buf[pos], buf[pos + 1], buf[pos + 2]
            pos += 3
            if a == 0 and b == 0 and c == 0:
                break
        if pos + 1 < len(buf):
            str_rel = buf[pos] | (buf[pos + 1] << 8)
            gs.event_script_anchor = pos + str_rel
        else:
            gs.event_script_anchor = pos
        gs.event_script_start = pos + 2 if pos + 2 <= len(buf) else pos
        gs.event_parse_pos = gs.event_script_start

    def pool_seek(self, event_id: int) -> int | None:
        """Return script start offset for event_id, or None."""
        start = self.loc_meta.get("script_offset", 0)
        end = self.loc_meta.get("string_table_offset", len(self.state.work_buf))
        script = bytes(self.state.work_buf[start:end])
        record = 0
        seg_start = 0
        for i, b in enumerate(script):
            if b == 0xFF:
                if record == event_id:
                    return start + seg_start
                record += 1
                seg_start = i + 1
        if record == event_id:
            return start + seg_start
        return None

    def move(self, key: str) -> None:
        key = key.upper()
        if key not in FACING_FROM_KEY:
            return
        gs = self.state.gs
        gs.last_move_key = ord(key)
        gs.facing_index = FACING_FROM_KEY[key]
        x, y = gs.coord_b & 0xF, gs.coord_a & 0xF
        if key == "N" and y > 0:
            y -= 1
        elif key == "S" and y < 15:
            y += 1
        elif key == "W" and x > 0:
            x -= 1
        elif key == "E" and x < 15:
            x += 1
        self.state.set_party(x, y)
        gs.pending_event_latch = 1

    def scan_and_run(self) -> bool:
        """Port of 0x175E2 tile scanner + interpreter entry."""
        st = self.state
        gs = st.gs
        if gs.event_script_anchor == 0xFFFF:
            self._init_parsed()

        gs.event_parse_pos = gs.event_script_start
        gs.script_abort = 0
        gs.exit_flags = 0
        party_tile = st.party_tile()
        ctx = st.context_mask()
        fired = False

        pos = 0
        while pos + 2 < len(st.work_buf):
            a, b, c = st.work_buf[pos], st.work_buf[pos + 1], st.work_buf[pos + 2]
            if a == 0 and b == 0 and c == 0:
                break
            if a == party_tile and (c & ctx) != 0:
                script_off = self.pool_seek(b)
                if script_off is None:
                    st.append_log(f"No script for event {b} at tile 0x{a:02X}")
                else:
                    if gs.era_low != st.attrib_era_gate():
                        st.append_log("Era gate blocked script")
                    else:
                        st.append_log(f"FIRE event {b} @ tile ({gs.coord_a},{gs.coord_b})")
                        runner = ScriptRunner(st, self.strings)
                        end = self.loc_meta.get("string_table_offset", len(st.work_buf))
                        runner.run_from(script_off, end)
                        fired = True
                st.work_buf[pos + 1] = 0
                st.work_buf[pos + 2] = 0
            pos += 3

        gs.pending_event_latch = 0
        st.sync_scalars_to_gs()
        return fired

    def trigger_event(self, event_id: int) -> None:
        off = self.pool_seek(event_id)
        if off is None:
            self.state.append_log(f"Event {event_id} not found")
            return
        runner = ScriptRunner(self.state, self.strings)
        end = self.loc_meta.get("string_table_offset", len(self.state.work_buf))
        runner.run_from(off, end)
        self.state.sync_scalars_to_gs()

    def step(self) -> bool:
        if self.state.gs.pending_event_latch:
            return self.scan_and_run()
        return False

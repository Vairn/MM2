"""Split TUI for MM2 event simulator (textual)."""

from __future__ import annotations

from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Horizontal, Vertical
from textual.widgets import Footer, Header, Input, Static, DataTable, RichLog

from .map_view import render_map
from .memory import EventSimState
from .runtime import EventRuntime


class MapPanel(Static):
    def update_map(self, rt: EventRuntime) -> None:
        gs = rt.state.gs
        text = render_map(
            rt.state.map_data,
            rt.loc_id if rt.loc_id < 60 else min(rt.loc_id, 59),
            gs.coord_b & 0xF,
            gs.coord_a & 0xF,
            rt.loc_meta.get("triplets"),
        )
        self.update(text)


class MemoryPanel(DataTable):
    def on_mount(self) -> None:
        self.add_columns("field", "hex", "value")

    def refresh_memory(self, st: EventSimState) -> None:
        self.clear()
        for name, hx, val in st.memory_rows():
            self.add_row(name, hx, val)


class EventSimApp(App):
    CSS = """
    Screen { layout: vertical; }
    #top { height: 1fr; }
    #map { width: 1fr; border: solid green; }
    #log { width: 2fr; border: solid cyan; }
    #mem { height: 12; border: solid yellow; }
    #cmd { height: 3; }
    """

    BINDINGS = [
        Binding("q", "quit", "Quit"),
        Binding("n", "move_n", "North"),
        Binding("s", "move_s", "South"),
        Binding("e", "move_e", "East"),
        Binding("w", "move_w", "West"),
        Binding("space", "step", "Step event"),
    ]

    def __init__(self, loc: int = 0, x: int = 8, y: int = 8, gold: int = 1000, era: int = 0) -> None:
        super().__init__()
        self.start_loc = loc
        self.start_x = x
        self.start_y = y
        self.start_gold = gold
        self.start_era = era
        self.state = EventSimState.create()
        self.rt = EventRuntime(self.state)

    def compose(self) -> ComposeResult:
        yield Header(show_clock=False)
        with Horizontal(id="top"):
            yield Static(id="map")
            yield RichLog(id="log", highlight=True, markup=False)
        yield DataTable(id="mem")
        yield Input(placeholder="load 0 | move N | step | gold 500 | trigger 1", id="cmd")
        yield Footer()

    def on_mount(self) -> None:
        mem = self.query_one("#mem", DataTable)
        mem.add_columns("field", "hex", "value")
        self.rt.load_files()
        self.rt.load_location(self.start_loc)
        self.state.party_gold = self.start_gold
        self.state.gs.era_low = self.start_era & 0xFF
        self.state.set_party(self.start_x, self.start_y)
        self.refresh_all()
        self.query_one("#log", RichLog).write("MM2 event simulator — N/S/E/W move, Space step, commands below")

    def refresh_all(self) -> None:
        self.query_one("#map", Static).update(
            render_map(
                self.rt.state.map_data,
                self.rt.loc_id,
                self.rt.state.gs.coord_b & 0xF,
                self.rt.state.gs.coord_a & 0xF,
                self.rt.loc_meta.get("triplets"),
            )
        )
        mem = self.query_one("#mem", DataTable)
        mem.clear(columns=False)
        for name, hx, val in self.state.memory_rows():
            mem.add_row(name, hx, val)

    def write_log(self, msg: str) -> None:
        self.query_one("#log", RichLog).write(msg)

    def action_move_n(self) -> None:
        self.rt.move("N")
        self.write_log("move N")
        self.refresh_all()

    def action_move_s(self) -> None:
        self.rt.move("S")
        self.write_log("move S")
        self.refresh_all()

    def action_move_e(self) -> None:
        self.rt.move("E")
        self.write_log("move E")
        self.refresh_all()

    def action_move_w(self) -> None:
        self.rt.move("W")
        self.write_log("move W")
        self.refresh_all()

    def action_step(self) -> None:
        self.rt.step()
        for line in self.state.log[-15:]:
            self.write_log(line)
        self.refresh_all()

    def on_input_submitted(self, event: Input.Submitted) -> None:
        cmd = event.value.strip()
        event.input.value = ""
        if not cmd:
            return
        parts = cmd.split()
        op = parts[0].lower()
        try:
            if op == "load" and len(parts) >= 2:
                self.rt.load_location(int(parts[1]))
                self.write_log(f"loaded location {parts[1]}")
            elif op == "move" and len(parts) >= 2:
                self.rt.move(parts[1][0])
                self.write_log(f"move {parts[1][0].upper()}")
            elif op == "step":
                self.rt.step()
            elif op == "trigger" and len(parts) >= 2:
                self.rt.trigger_event(int(parts[1]))
            elif op == "gold" and len(parts) >= 2:
                self.state.party_gold = int(parts[1])
                self.write_log(f"gold = {parts[1]}")
            elif op == "era" and len(parts) >= 2:
                self.state.gs.era_low = int(parts[1]) & 0xFF
                self.write_log(f"era_low = {parts[1]}")
            elif op == "pos" and len(parts) >= 3:
                self.state.set_party(int(parts[2]), int(parts[1]))
                self.write_log(f"pos ({parts[1]},{parts[2]})")
            else:
                self.write_log(f"unknown: {cmd}")
        except Exception as exc:
            self.write_log(f"error: {exc}")
        for line in self.state.log[-8:]:
            self.write_log(line)
        self.refresh_all()


def run_tui(location: int = 0, x: int = 8, y: int = 8, gold: int = 1000, era: int = 0) -> None:
    EventSimApp(location, x, y, gold, era).run()

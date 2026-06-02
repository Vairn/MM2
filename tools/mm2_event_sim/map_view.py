"""16x16 map grid renderer for simulator."""

from __future__ import annotations

GRID = 16
EVENT_FLAG = 0x80
PAGE = 256


def render_map(
    map_data: bytes,
    screen: int,
    party_x: int,
    party_y: int,
    triplets: list[tuple[int, int, int]] | None = None,
) -> str:
    lines: list[str] = []
    if screen >= 60 or len(map_data) < (screen + 1) * 512:
        lines.append("(no map.dat page — event-only record)")
        if triplets:
            for pos, evt, cond in triplets[:40]:
                y, x = (pos >> 4) & 0xF, pos & 0xF
                lines.append(f"  E{evt:02d} @ ({y},{x}) cond=0x{cond:02X}")
        return "\n".join(lines)

    base = screen * 512 + PAGE
    trip_cells = {}
    if triplets:
        for pos, evt, _cond in triplets:
            trip_cells[(pos & 0xF, (pos >> 4) & 0xF)] = evt

    lines.append("   " + "".join(f"{x:X}" for x in range(GRID)))
    for y in range(GRID - 1, -1, -1):
        row = f"{y:X}  "
        for x in range(GRID):
            cell = map_data[base + y * GRID + x]
            ch = "."
            if cell & 0x01:
                ch = "#"
            if cell & EVENT_FLAG:
                ch = "E"
            if (x, y) in trip_cells:
                ch = str(trip_cells[(x, y)] % 10) if trip_cells[(x, y)] < 10 else "+"
            if x == party_x and y == party_y:
                ch = "@"
            row += ch
        lines.append(row)
    lines.append("  . floor  # wall  E collision-event  @ party")
    return "\n".join(lines)

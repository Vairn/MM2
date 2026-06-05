"""map.dat page-1 collision helpers for party movement (tools + walkers)."""
from __future__ import annotations

MAP_GRID = 16

# Movement step deltas — facing 0=N, 1=E, 2=S, 3=W (party coords).
STEP_DX = (0, 1, 0, -1)
STEP_DY = (1, 0, -1, 0)


def collision_at(collision: bytes, x: int, y: int) -> int:
    if x < 0 or y < 0 or x >= MAP_GRID or y >= MAP_GRID:
        return 0xFF
    return collision[(y << 4) | x]


def collision_field_wall(cell: int, direction: int) -> bool:
    """True when page-1 `(dark<<1)|wall` has the wall bit set for direction 0=N..3=W."""
    d = direction & 3
    if d == 3:
        return (cell & 0x40) != 0
    return ((cell >> (d * 2)) & 1) != 0


def movement_to_collision_dir(facing: int) -> int:
    """Map movement facing to collision nibble index (page-1 N/E/S/W @ bits 0-1/2-3/4-5/6)."""
    return (3 - (facing & 3)) & 3


def movement_blocked_collision(
    visual: bytes,
    collision: bytes,
    x: int,
    y: int,
    facing: int,
    *,
    outdoor: bool = False,
) -> bool:
    """Block step using map.dat page 1; outdoor terrain uses page-0 high bits (no collision flag)."""
    d = facing & 3
    cd = movement_to_collision_dir(d)
    cell = collision_at(collision, x, y)
    if cell != 0xFF and collision_field_wall(cell, cd):
        return True

    nx = x + STEP_DX[d]
    ny = y + STEP_DY[d]
    if nx < 0 or ny < 0 or nx >= MAP_GRID or ny >= MAP_GRID:
        return False

    back = movement_to_collision_dir((d + 2) & 3)
    dest = collision_at(collision, nx, ny)
    if dest != 0xFF and collision_field_wall(dest, back):
        return True

    if outdoor:
        dest_v = visual[(ny << 4) | nx]
        if (dest_v & 0x60) == 0x60 or (dest_v & 0x80) != 0:
            return True
    return False

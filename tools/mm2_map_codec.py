#!/usr/bin/env python3
"""map.dat codec for Might & Magic II (mirror of EXTRACTED/decomp/mm2_map_codec.{h,c}).

map.dat is 60 map screens * 512 bytes = 30720 bytes, loaded flat at runtime
into the buffer pointed to by A4-$EEF4. Per screen (16x16 grid per page; disk
row 0 = SOUTH/bottom):

  +0x000  256 bytes  page 0 - VISUAL    (3D hood source, map_row_sampler @ 0x190C)
  +0x100  256 bytes  page 1 - COLLISION (movement / darkness / events)

Page 0 visual byte: four 2-bit wall fields per cell, NO event bit.
  bits 0-1 North | 2-3 East | 4-5 South | 6-7 West
  values: 0 open, 1 wall, 2 wall+torch, 3 door
(Outdoor surface screens reuse the low 5 bits as a terrain id for outb.32.)

Page 1 collision byte: per direction (dark<<1)|wall, except West's dark slot
is the tile EVENT flag:
  0x01 N wall  0x02 N dark
  0x04 E wall  0x08 E dark
  0x10 S wall  0x20 S dark
  0x40 W wall  0x80 EVENT flag (aligns with event.dat triplets)

See EXTRACTED/docs/21-map-dat-format.md.

Usage:
    python tools/mm2_map_codec.py verify map.dat
    python tools/mm2_map_codec.py dump map.dat 0          # screen summary
    python tools/mm2_map_codec.py grid map.dat 0          # visual wall grid
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from typing import List

SCREEN_COUNT = 60
GRID_DIM = 16
PAGE_SIZE = 256
SCREEN_SIZE = 512
FILE_SIZE = SCREEN_COUNT * SCREEN_SIZE

# Page-0 visual 2-bit field values.
WALL_OPEN = 0
WALL_SOLID = 1
WALL_TORCH = 2
WALL_DOOR = 3

# Page-1 collision bit masks.
COLL_N_WALL = 0x01
COLL_N_DARK = 0x02
COLL_E_WALL = 0x04
COLL_E_DARK = 0x08
COLL_S_WALL = 0x10
COLL_S_DARK = 0x20
COLL_W_WALL = 0x40
COLL_EVENT = 0x80   # event flag, NOT west-dark
COLL_PASS_WALL_MASK = 0x55  # passability @ 0x9424 AND #$55 (walls only)
COLL_WALL_MASK = 0x7F       # wall+dark with event stripped (display)


def visual_walls(cell: int):
    """Decode a page-0 byte -> (N, E, S, W) 2-bit field codes."""
    return cell & 3, (cell >> 2) & 3, (cell >> 4) & 3, (cell >> 6) & 3


def collision_has_event(cell: int) -> bool:
    return (cell & COLL_EVENT) != 0


@dataclass
class MapScreen:
    visual: bytearray     # page 0, 256 bytes
    collision: bytearray  # page 1, 256 bytes

    def visual_at(self, x: int, y: int) -> int:
        return self.visual[y * GRID_DIM + x]

    def collision_at(self, x: int, y: int) -> int:
        return self.collision[y * GRID_DIM + x]

    def has_event_at(self, x: int, y: int) -> bool:
        return collision_has_event(self.collision_at(x, y))


@dataclass
class MapFile:
    screens: List[MapScreen]

    @classmethod
    def decode(cls, data: bytes) -> "MapFile":
        if len(data) != FILE_SIZE:
            raise ValueError(f"map.dat must be {FILE_SIZE} bytes, got {len(data)}")
        screens = []
        for i in range(SCREEN_COUNT):
            rec = data[i * SCREEN_SIZE:(i + 1) * SCREEN_SIZE]
            screens.append(MapScreen(bytearray(rec[:PAGE_SIZE]), bytearray(rec[PAGE_SIZE:])))
        return cls(screens)

    def encode(self) -> bytes:
        out = bytearray()
        for s in self.screens:
            if len(s.visual) != PAGE_SIZE or len(s.collision) != PAGE_SIZE:
                raise ValueError("each page must be 256 bytes")
            out += s.visual
            out += s.collision
        return bytes(out)

    @classmethod
    def load(cls, path: str) -> "MapFile":
        with open(path, "rb") as fp:
            return cls.decode(fp.read())

    def save(self, path: str) -> None:
        with open(path, "wb") as fp:
            fp.write(self.encode())


def cmd_verify(path: str, _arg: str = "") -> int:
    with open(path, "rb") as fp:
        original = fp.read()
    mf = MapFile.decode(original)
    roundtrip = mf.encode()
    ok = roundtrip == original
    print(f"size={len(original)} screens={len(mf.screens)} round-trip={'OK' if ok else 'MISMATCH'}")
    return 0 if ok else 1


def cmd_dump(path: str, screen: str = "0") -> int:
    mf = MapFile.load(path)
    s = mf.screens[int(screen)]
    events = sum(1 for c in s.collision if collision_has_event(c))
    walls = sum(1 for c in s.visual if c)
    print(f"screen {int(screen)}: nonzero visual cells={walls} event tiles={events}")
    for y in range(GRID_DIM):
        if any(s.has_event_at(x, y) for x in range(GRID_DIM)):
            tiles = [f"({x},{y})" for x in range(GRID_DIM) if s.has_event_at(x, y)]
            print("  events @", " ".join(tiles))
    return 0


def cmd_grid(path: str, screen: str = "0") -> int:
    """Print the visual page as N/E/S/W field codes, north at the top."""
    mf = MapFile.load(path)
    s = mf.screens[int(screen)]
    print(f"screen {int(screen)} page 0 (visual), rows top=north (disk row 15):")
    for y in range(GRID_DIM - 1, -1, -1):
        cells = " ".join(f"{s.visual_at(x, y):02x}" for x in range(GRID_DIM))
        print(f"  y={y:2d}  {cells}")
    return 0


def main(argv: List[str]) -> int:
    if len(argv) < 3:
        print(__doc__)
        return 2
    cmd, path = argv[1], argv[2]
    arg = argv[3] if len(argv) > 3 else "0"
    dispatch = {
        "verify": cmd_verify,
        "dump": cmd_dump,
        "grid": cmd_grid,
    }
    if cmd not in dispatch:
        print(__doc__)
        return 2
    return dispatch[cmd](path, arg)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

#!/usr/bin/env python3
"""attrib.dat codec for Might & Magic II.

attrib.dat is the per-screen map attribute table: 60 screens * 64 bytes.
It parallels map.dat (tiles) and supplies environment, world-adjacency and
roof data. See EXTRACTED/docs/12-attrib-dat-format.md for the full field map.

Records are kept byte-exact (raw 64 bytes) so encode(decode(x)) == x even for
the still-unresolved fields. Confirmed fields are exposed as properties.

Usage:
    python tools/attrib_codec.py dump attrib.dat
    python tools/attrib_codec.py neighbors attrib.dat
    python tools/attrib_codec.py verify attrib.dat
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from typing import List

RECORD_SIZE = 0x40
RECORD_COUNT = 60
FILE_SIZE = RECORD_SIZE * RECORD_COUNT

ROOF_OFFSET = 0x20
ROOF_BYTES = 32
TILE_COUNT = 256

# Confirmed field offsets within a 64-byte record.
OFF_AREA_ID = 0x00
OFF_MAP_CATEGORY = 0x01
OFF_TILESET = 0x02
OFF_ENV_TYPE = 0x03
OFF_SURFACE_FLAG = 0x04
OFF_NEIGHBOR0 = 0x05  # opposite of NEIGHBOR2
OFF_NEIGHBOR1 = 0x06  # opposite of NEIGHBOR3
OFF_NEIGHBOR2 = 0x07
OFF_NEIGHBOR3 = 0x08
OFF_ENTRY_COORD = 0x0E  # packed (Y<<4)|X party entry pos (asm 0x123A)
OFF_ERA_GATE = 0x0F     # compared vs current era index (asm 0x172BC)
OFF_COMPLEX_ID = 0x15   # big-endian word (interior areas)
OFF_TRANSITION_COORD = 0x16  # packed (Y<<4)|X dest pos (asm 0xB2DC)
OFF_LEVEL = 0x17
OFF_LINK_AREA = 0x18    # transition destination screen (asm 0xB2F2)
OFF_FLAGS = 0x1A        # btst bitfield (bits 0,3,4,5,6)


def unpack_coord(packed: int):
    """Decode a packed (Y<<4)|X coordinate byte -> (x, y)."""
    return packed & 0x0F, (packed >> 4) & 0x0F

ENV_TOWN = 0x11
ENV_CAVERN = 0x12
ENV_CASTLE_A = 0x13
ENV_CASTLE_B = 0x14

ENV_NAMES = {
    ENV_TOWN: "town",
    ENV_CAVERN: "cavern",
    ENV_CASTLE_A: "castle",
    ENV_CASTLE_B: "castle",
}

# Screens 41..44: elemental planes — surface_flag==0 in attrib.dat but runtime
# uses the outdoor view (-$79E2) and outb.32 cartography (AreaNames.h, Cartography.h).
ELEMENTAL_PLANE_FIRST = 41
ELEMENTAL_PLANE_LAST = 44


def is_elemental_plane(area_id: int) -> bool:
    return ELEMENTAL_PLANE_FIRST <= area_id <= ELEMENTAL_PLANE_LAST


def is_outdoor_area(area_id: int, surface_flag: int) -> bool:
    """Outdoor first-person / overland view, including elemental planes."""
    return is_elemental_plane(area_id) or surface_flag != 0


@dataclass
class AttribRecord:
    raw: bytearray

    # --- confirmed accessors ---
    @property
    def area_id(self) -> int:
        return self.raw[OFF_AREA_ID]

    @property
    def map_category(self) -> int:
        return self.raw[OFF_MAP_CATEGORY]

    @property
    def tileset(self) -> int:
        return self.raw[OFF_TILESET]

    @property
    def env_type(self) -> int:
        return self.raw[OFF_ENV_TYPE]

    @property
    def env_name(self) -> str:
        if self.is_outdoor and not self.is_outside:
            return "outside"
        if self.is_outside:
            return "outside"
        return ENV_NAMES.get(self.env_type, "other")

    @property
    def surface_flag(self) -> int:
        return self.raw[OFF_SURFACE_FLAG]

    @property
    def is_outside(self) -> bool:
        """Raw attrib +0x04 (nonzero = overland sector)."""
        return self.surface_flag != 0

    @property
    def is_outdoor(self) -> bool:
        """Runtime outdoor view: surface sectors or elemental planes 41..44."""
        return is_outdoor_area(self.area_id, self.surface_flag)

    @property
    def neighbors(self) -> List[int]:
        """4 neighbour screen ids; slots (0,2) and (1,3) are opposite dirs."""
        return [self.raw[OFF_NEIGHBOR0 + i] for i in range(4)]

    @property
    def complex_id(self) -> int:
        """16-bit id (big-endian); shared by a town and its cavern (interior)."""
        return (self.raw[OFF_COMPLEX_ID] << 8) | self.raw[OFF_COMPLEX_ID + 1]

    @property
    def level(self) -> int:
        return self.raw[OFF_LEVEL]

    @property
    def era_gate(self) -> int:
        """Event records run when this == the current era index (asm 0x172BC)."""
        return self.raw[OFF_ERA_GATE]

    @property
    def entry_coord(self):
        """Party entry (x, y) on the 16x16 screen."""
        return unpack_coord(self.raw[OFF_ENTRY_COORD])

    @property
    def transition_coord(self):
        """Transition target (x, y), paired with transition_screen."""
        return unpack_coord(self.raw[OFF_TRANSITION_COORD])

    @property
    def transition_screen(self) -> int:
        return self.raw[OFF_LINK_AREA]

    @property
    def flags(self) -> int:
        return self.raw[OFF_FLAGS]

    def flag_bit(self, bit: int) -> int:
        return (self.raw[OFF_FLAGS] >> bit) & 1

    def roof_bit(self, tile: int) -> int:
        if not 0 <= tile < TILE_COUNT:
            raise IndexError(tile)
        byte = ROOF_OFFSET + (tile >> 3)
        return (self.raw[byte] >> (tile & 7)) & 1

    def set_roof_bit(self, tile: int, value: int) -> None:
        if not 0 <= tile < TILE_COUNT:
            raise IndexError(tile)
        byte = ROOF_OFFSET + (tile >> 3)
        mask = 1 << (tile & 7)
        if value:
            self.raw[byte] |= mask
        else:
            self.raw[byte] &= ~mask & 0xFF


@dataclass
class AttribFile:
    records: List[AttribRecord]

    @classmethod
    def decode(cls, data: bytes) -> "AttribFile":
        if len(data) != FILE_SIZE:
            raise ValueError(f"attrib.dat must be {FILE_SIZE} bytes, got {len(data)}")
        recs = [
            AttribRecord(bytearray(data[i * RECORD_SIZE:(i + 1) * RECORD_SIZE]))
            for i in range(RECORD_COUNT)
        ]
        return cls(recs)

    def encode(self) -> bytes:
        out = bytearray()
        for r in self.records:
            if len(r.raw) != RECORD_SIZE:
                raise ValueError("record must be 64 bytes")
            out += r.raw
        return bytes(out)

    @classmethod
    def load(cls, path: str) -> "AttribFile":
        with open(path, "rb") as fp:
            return cls.decode(fp.read())

    def save(self, path: str) -> None:
        with open(path, "wb") as fp:
            fp.write(self.encode())


def cmd_dump(path: str) -> int:
    af = AttribFile.load(path)
    print("area cat tile env     surf neighbors(0-3)   complex lvl")
    for r in af.records:
        nbr = " ".join(f"{n:02x}" for n in r.neighbors)
        print(
            f"{r.area_id:3d}  {r.map_category:02x}  {r.tileset:02x}  "
            f"{r.env_name:7s} {r.surface_flag:02x}   {nbr}   "
            f"{r.complex_id:04x}    {r.level:02x}"
        )
    return 0


def cmd_neighbors(path: str) -> int:
    af = AttribFile.load(path)
    for r in af.records:
        if r.is_outside:
            print(f"area {r.area_id:2d}: N/E/S/W -> {r.neighbors}")
    return 0


def cmd_eras(path: str) -> int:
    """Group screens by era_gate byte (0x0F) and show transition/flag data."""
    af = AttribFile.load(path)
    by_era = {}
    for r in af.records:
        by_era.setdefault(r.era_gate, []).append(r.area_id)
    print("era_gate (0x0F) -> screens that run their events in that era:")
    for era in sorted(by_era):
        ids = " ".join(str(a) for a in by_era[era])
        print(f"  era {era:#04x}: {ids}")
    print("\nscreens with transition data / non-zero flags:")
    for r in af.records:
        if r.flags or r.transition_coord != (0, 0):
            print(
                f"  area {r.area_id:2d}: flags={r.flags:08b} "
                f"trans->screen {r.transition_screen:2d} at {r.transition_coord} "
                f"entry={r.entry_coord}"
            )
    return 0


def cmd_verify(path: str) -> int:
    with open(path, "rb") as fp:
        original = fp.read()
    af = AttribFile.decode(original)
    roundtrip = af.encode()
    ok = roundtrip == original
    print(f"size={len(original)} round-trip={'OK' if ok else 'MISMATCH'}")

    # adjacency symmetry check (opposite pairs 0<->2, 1<->3)
    opp = {0: 2, 1: 3, 2: 0, 3: 1}
    asym = 0
    for r in af.records:
        if not r.is_outside:
            if set(r.neighbors) != {r.area_id}:
                print(f"  interior area {r.area_id} not self-referential: {r.neighbors}")
            continue
        for slot, back in opp.items():
            nbr = r.neighbors[slot]
            if nbr < RECORD_COUNT and af.records[nbr].neighbors[back] != r.area_id:
                asym += 1
    print(f"adjacency asymmetries={asym}")
    return 0 if (ok and asym == 0) else 1


def main(argv: List[str]) -> int:
    if len(argv) < 3:
        print(__doc__)
        return 2
    cmd, path = argv[1], argv[2]
    dispatch = {
        "dump": cmd_dump,
        "neighbors": cmd_neighbors,
        "eras": cmd_eras,
        "verify": cmd_verify,
    }
    if cmd not in dispatch:
        print(__doc__)
        return 2
    return dispatch[cmd](path)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

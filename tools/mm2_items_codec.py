#!/usr/bin/env python3
"""items.dat codec mirror — 256×20 bytes (doc 18)."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import List

ITEM_COUNT = 256
RECORD_SIZE = 0x14
NAME_SIZE = 12
FILE_SIZE = ITEM_COUNT * RECORD_SIZE


@dataclass
class ItemRecord:
    name: bytes
    separator: int
    forbidden_class_mask: int
    bonus_byte: int
    effect_byte: int
    damage: int
    pad: int
    gold: int

    def name_str(self) -> str:
        return self.name.rstrip(b" \x00").decode("ascii", errors="replace")


def decode(data: bytes) -> List[ItemRecord]:
    if len(data) < FILE_SIZE:
        raise ValueError(f"expected {FILE_SIZE} bytes, got {len(data)}")
    out: List[ItemRecord] = []
    for i in range(ITEM_COUNT):
        off = i * RECORD_SIZE
        chunk = data[off : off + RECORD_SIZE]
        gold = struct.unpack_from("<H", chunk, 0x12)[0]
        out.append(
            ItemRecord(
                name=chunk[0:NAME_SIZE],
                separator=chunk[0x0C],
                forbidden_class_mask=chunk[0x0D],
                bonus_byte=chunk[0x0E],
                effect_byte=chunk[0x0F],
                damage=chunk[0x10],
                pad=chunk[0x11],
                gold=gold,
            )
        )
    return out


def encode(records: List[ItemRecord]) -> bytes:
    if len(records) != ITEM_COUNT:
        raise ValueError(f"expected {ITEM_COUNT} records")
    buf = bytearray(FILE_SIZE)
    for i, r in enumerate(records):
        off = i * RECORD_SIZE
        buf[off : off + NAME_SIZE] = r.name.ljust(NAME_SIZE, b" ")[:NAME_SIZE]
        buf[off + 0x0C] = r.separator & 0xFF
        buf[off + 0x0D] = r.forbidden_class_mask & 0xFF
        buf[off + 0x0E] = r.bonus_byte & 0xFF
        buf[off + 0x0F] = r.effect_byte & 0xFF
        buf[off + 0x10] = r.damage & 0xFF
        buf[off + 0x11] = r.pad & 0xFF
        struct.pack_into("<H", buf, off + 0x12, r.gold & 0xFFFF)
    return bytes(buf)


def load(path: Path) -> List[ItemRecord]:
    return decode(path.read_bytes())


def save(path: Path, records: List[ItemRecord]) -> None:
    path.write_bytes(encode(records))


if __name__ == "__main__":
    import sys

    p = Path(sys.argv[1] if len(sys.argv) > 1 else "items.dat")
    recs = load(p)
    for i, r in enumerate(recs[:8]):
        print(f"{i:3d} {r.name_str()!r} mask=0x{r.forbidden_class_mask:02X} gold={r.gold}")

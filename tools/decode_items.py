#!/usr/bin/env python3
"""items.dat decoder/encoder for Might & Magic II (Amiga).

256 records * 20 bytes (0x14). Multibyte fields are little-endian on disk
(the 68000 port byte-swaps gold on load at asm 0x26030).

Per-record layout:
  0x00-0x0B  name (12 ASCII, space-padded)
  0x0C       separator/pad (0)
  0x0D       forbidden-class bitmask  (K P A C S R N B = 0x80..0x01); a SET bit
             means that class CANNOT use the item, so 0x00 = usable by everyone
  0x0E       bonus  : hi nibble = type (table below), lo nibble = amount
  0x0F       effect : hi nibble = type (table below), lo nibble = amount
  0x10       damage
  0x11       pad (0)
  0x12-0x13  gold (uint16 LE)

See EXTRACTED/docs/18-items-dat-format.md for how the type tables were decoded.
"""
from __future__ import annotations

import struct
import sys
from dataclasses import dataclass

try:
    from mm2_spells import decode_effect as _decode_effect
except ImportError:  # allow running from repo root
    import os
    sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
    from mm2_spells import decode_effect as _decode_effect

RECORD_SIZE = 0x14
ITEM_COUNT = 256
NAME_SIZE = 12

# Byte 0x0D bit -> class, MSB first. The byte is a *restriction* mask: a set bit
# means that class CANNOT use the item (see decode_record / Item.usable_classes).
CLASSES = ["Knight", "Paladin", "Archer", "Cleric",
           "Sorcerer", "Robber", "Ninja", "Barbarian"]  # 0x80 .. 0x01

# Bonus "special power" category (hi nibble of 0x0E), passive while equipped.
# Validated against RPGClassics/Fandom MM2 item tables. amount 0 == no bonus.
# Type 12: RPGClassics "PHP" (Max HP); Fandom "Poison" (source conflict).
BONUS_TYPE_NAMES = [
    "Might", "Intellect", "Personality", "Speed", "Accuracy", "Luck",
    "Magic resist", "Fire resist", "Elec resist", "Cold resist",
    "Energy resist", "Sleep resist", "Max HP", "Acid resist",
    "Thievery", "Armor Class",
]

# Effect "use power" is the whole byte 0x0F treated as a flat spell index (not
# type/amount nibbles): 0x80+ Sorcerer spell, 0xB0+ Cleric spell, <0x80 a stat
# boost. Decoded via mm2_spells.decode_effect (see that module for the encoding
# and the per-level spell-count table).


@dataclass
class Item:
    name: str = ""
    forbidden_mask: int = 0  # byte 0x0D: set bit = that class CANNOT use it
    bonus_type: int = 0
    bonus_amount: int = 0
    effect_type: int = 0
    effect_amount: int = 0
    damage: int = 0
    gold: int = 0

    @property
    def forbidden_classes(self) -> list[str]:
        return [c for i, c in enumerate(CLASSES)
                if self.forbidden_mask & (0x80 >> i)]

    @property
    def usable_classes(self) -> list[str]:
        # A class may use the item when its restriction bit is clear.
        return [c for i, c in enumerate(CLASSES)
                if not self.forbidden_mask & (0x80 >> i)]

    @property
    def bonus_type_name(self) -> str:
        return BONUS_TYPE_NAMES[self.bonus_type & 0xF]

    @property
    def effect_byte(self) -> int:
        return ((self.effect_type & 0xF) << 4) | (self.effect_amount & 0xF)

    @property
    def effect_text(self) -> str:
        return _decode_effect(self.effect_byte)['text']


def decode_record(rec: bytes) -> Item:
    if len(rec) != RECORD_SIZE:
        raise ValueError(f"record must be {RECORD_SIZE} bytes, got {len(rec)}")
    name = rec[:NAME_SIZE].decode("latin1").rstrip(" \x00")
    forbidden = rec[0x0D]
    bonus = rec[0x0E]
    effect = rec[0x0F]
    damage = rec[0x10]
    (gold,) = struct.unpack_from("<H", rec, 0x12)
    return Item(
        name=name,
        forbidden_mask=forbidden,
        bonus_type=bonus >> 4,
        bonus_amount=bonus & 0xF,
        effect_type=effect >> 4,
        effect_amount=effect & 0xF,
        damage=damage,
        gold=gold,
    )


def encode_record(it: Item) -> bytes:
    out = bytearray(RECORD_SIZE)
    name = it.name.encode("latin1")[:NAME_SIZE].ljust(NAME_SIZE, b" ")
    out[:NAME_SIZE] = name
    out[0x0C] = 0
    out[0x0D] = it.forbidden_mask & 0xFF
    out[0x0E] = ((it.bonus_type & 0xF) << 4) | (it.bonus_amount & 0xF)
    out[0x0F] = ((it.effect_type & 0xF) << 4) | (it.effect_amount & 0xF)
    out[0x10] = it.damage & 0xFF
    out[0x11] = 0
    struct.pack_into("<H", out, 0x12, it.gold & 0xFFFF)
    return bytes(out)


def load(path: str) -> list[Item]:
    data = open(path, "rb").read()
    return [decode_record(data[i * RECORD_SIZE:(i + 1) * RECORD_SIZE])
            for i in range(ITEM_COUNT)]


def save(path: str, items: list[Item]) -> None:
    with open(path, "wb") as f:
        for it in items:
            f.write(encode_record(it))


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(f"usage: {argv[0]} items.dat", file=sys.stderr)
        return 2
    items = load(argv[1])
    for i, it in enumerate(items):
        if not it.name:
            continue
        # amount 0 == no bonus (type 0 = Might / AC otherwise).
        b = f"{it.bonus_type_name}+{it.bonus_amount}" if it.bonus_amount else "-"
        e = it.effect_text if it.effect_byte else "-"
        usable = "all" if not it.forbidden_mask else ",".join(it.usable_classes) or "none"
        print(f"{i:3} {it.name:13} dmg={it.damage:3} gold={it.gold:5} "
              f"bonus=[{b}] effect=[{e}] use=[{usable}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

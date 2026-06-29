#!/usr/bin/env python3
"""MM2 "found item / treasure reward" buffer codec.

A 16-byte shared "pending loot" buffer in the A4 game-state block, filled by
two event opcodes and consumed by the Search payoff:

  * OP_2A "set treasure/reward block" @ 0x16D16
        reads u24 gold/exp, u16 gems, then 3x(id, charges, flags) from the
        script stream and writes the whole buffer; sets sentinel A4-$794C=0xFF.
  * OP_19 "give item to party"        @ 0x165D8 (overflow tail @ 0x166A0)
        when every party backpack is full, the item being given overflows into
        the first free buffer slot (scan id[0], id[1], else slot 2), writing
        id/charges/flags and setting A4-$794C=0xFF.

Consumption (the "you found..." distribution) is the Search handler @ 0x4800 ->
-$7D1C -> 0x1B19C (presentation/engine, not yet traced). Search treats the
buffer as non-empty when any id[0..2]!=0 OR gold!=0 OR gems!=0 (0x4832..0x4870).

Buffer memory layout (ascending A4 displacement), traced from the ASM stores in
EXTRACTED/mm2.capstone.annotated.asm:

    -$3F1C  byte[3]  id        ($C0E4)
    -$3F19  byte[3]  flags     ($C0E7)  (OP_19 attr3)
    -$3F16  byte[3]  charges   ($C0EA)  (OP_19 attr2)
    -$3F13  byte     reserved/pad
    -$3F12  u16      gems      ($C0EE)  big-endian in RAM (68k move.w)
    -$3F10  u32      gold_exp  ($C0F0)  big-endian, 24-bit value (move.l)

gold/gems are big-endian *in RAM* (Motorola CPU), NOT little-endian: this is a
runtime buffer, not a .dat file, so the CLAUDE.md disk-LE default does not apply.
The OP_2A *script stream* values, however, are little-endian (the byte/word/long
readers $155be/$155da/$155fc assemble low byte first).

See EXTRACTED/decomp/mm2_found_items.{h,c} for the matching C codec and
EXTRACTED/docs/07-event-script-opcodes.md for the opcode trace.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import List

A4_ANCHOR = 0x7FFE
ITEM_SLOTS = 3

# Signed A4 displacements.
OFF_ID = -0x3F1C
OFF_FLAGS = -0x3F19
OFF_CHARGES = -0x3F16
OFF_GEMS = -0x3F12
OFF_GOLD = -0x3F10
OFF_SENTINEL = -0x794C

# Image-relative offsets (image[0] == absolute address 0; A4 == image+A4_ANCHOR).
IMG_ID = A4_ANCHOR + OFF_ID
IMG_FLAGS = A4_ANCHOR + OFF_FLAGS
IMG_CHARGES = A4_ANCHOR + OFF_CHARGES
IMG_GEMS = A4_ANCHOR + OFF_GEMS
IMG_GOLD = A4_ANCHOR + OFF_GOLD
IMG_SENTINEL = A4_ANCHOR + OFF_SENTINEL

SENTINEL_EMPTY = 0x00
SENTINEL_PENDING = 0xFE  # combat-victory: auto-Search next loop (0x1276)
SENTINEL_FILLED = 0xFF   # set by OP_2A / OP_19 overflow

OP2A_BLOCK_LEN = 14


@dataclass
class FoundItems:
    """Host-order view of the found-item buffer."""

    id: List[int] = field(default_factory=lambda: [0, 0, 0])
    flags: List[int] = field(default_factory=lambda: [0, 0, 0])
    charges: List[int] = field(default_factory=lambda: [0, 0, 0])
    gems: int = 0
    gold_exp: int = 0
    sentinel: int = SENTINEL_EMPTY

    def count(self) -> int:
        """Occupied item slots (id != 0)."""
        return sum(1 for x in self.id if x != 0)

    def has_loot(self) -> bool:
        """Search "has loot" predicate (0x4832..0x4870)."""
        return any(x != 0 for x in self.id) or self.gold_exp != 0 or self.gems != 0


def decode_block(block: bytes) -> FoundItems:
    """Decode the 14-byte OP_2A inline script block (sets sentinel=0xFF).

    block layout (script-stream order, little-endian gold/gems):
        [0:3]  gold/exp u24 LE
        [3:5]  gems     u16 LE
        [5:14] 3x(id, charges, flags)
    """
    if len(block) < OP2A_BLOCK_LEN:
        raise ValueError(f"OP_2A block must be >= {OP2A_BLOCK_LEN} bytes")
    fi = FoundItems()
    fi.gold_exp = block[0] | (block[1] << 8) | (block[2] << 16)
    fi.gems = block[3] | (block[4] << 8)
    for i in range(ITEM_SLOTS):
        fi.id[i] = block[5 + i * 3]
        fi.charges[i] = block[6 + i * 3]
        fi.flags[i] = block[7 + i * 3]
    fi.sentinel = SENTINEL_FILLED
    return fi


def encode_block(fi: FoundItems) -> bytes:
    """Encode a FoundItems back into a 14-byte OP_2A inline block."""
    out = bytearray(OP2A_BLOCK_LEN)
    out[0] = fi.gold_exp & 0xFF
    out[1] = (fi.gold_exp >> 8) & 0xFF
    out[2] = (fi.gold_exp >> 16) & 0xFF
    out[3] = fi.gems & 0xFF
    out[4] = (fi.gems >> 8) & 0xFF
    for i in range(ITEM_SLOTS):
        out[5 + i * 3] = fi.id[i] & 0xFF
        out[6 + i * 3] = fi.charges[i] & 0xFF
        out[7 + i * 3] = fi.flags[i] & 0xFF
    return bytes(out)


def read_image(image: bytes) -> FoundItems:
    """Read the buffer out of a raw A4-anchored memory image (big-endian RAM)."""
    fi = FoundItems()
    for i in range(ITEM_SLOTS):
        fi.id[i] = image[IMG_ID + i]
        fi.flags[i] = image[IMG_FLAGS + i]
        fi.charges[i] = image[IMG_CHARGES + i]
    fi.gems = (image[IMG_GEMS] << 8) | image[IMG_GEMS + 1]
    fi.gold_exp = (
        (image[IMG_GOLD] << 24)
        | (image[IMG_GOLD + 1] << 16)
        | (image[IMG_GOLD + 2] << 8)
        | image[IMG_GOLD + 3]
    )
    fi.sentinel = image[IMG_SENTINEL]
    return fi


def write_image(image: bytearray, fi: FoundItems) -> None:
    """Write a FoundItems into a raw A4-anchored memory image (big-endian RAM)."""
    for i in range(ITEM_SLOTS):
        image[IMG_ID + i] = fi.id[i] & 0xFF
        image[IMG_FLAGS + i] = fi.flags[i] & 0xFF
        image[IMG_CHARGES + i] = fi.charges[i] & 0xFF
    image[IMG_GEMS] = (fi.gems >> 8) & 0xFF
    image[IMG_GEMS + 1] = fi.gems & 0xFF
    image[IMG_GOLD] = (fi.gold_exp >> 24) & 0xFF
    image[IMG_GOLD + 1] = (fi.gold_exp >> 16) & 0xFF
    image[IMG_GOLD + 2] = (fi.gold_exp >> 8) & 0xFF
    image[IMG_GOLD + 3] = fi.gold_exp & 0xFF
    image[IMG_SENTINEL] = fi.sentinel & 0xFF


def op2a_fill(image: bytearray, block: bytes) -> FoundItems:
    """Apply OP_2A to an image: decode block, write buffer + sentinel=0xFF."""
    fi = decode_block(block)
    write_image(image, fi)
    return fi


def overflow_append(image: bytearray, item_id: int, charges: int, flags: int) -> int:
    """OP_19 overflow tail @ 0x166A0: place one item into the first free slot
    (scan id[0], id[1]; if both occupied use slot 2). Sets sentinel=0xFF.
    Returns the slot index written (0..2)."""
    i = 0
    while i < 2 and image[IMG_ID + i] != 0:
        i += 1
    image[IMG_ID + i] = item_id & 0xFF
    image[IMG_FLAGS + i] = flags & 0xFF
    image[IMG_CHARGES + i] = charges & 0xFF
    image[IMG_SENTINEL] = SENTINEL_FILLED
    return i


def _new_image() -> bytearray:
    return bytearray(A4_ANCHOR + 0x8000)


def _selftest() -> None:
    # 1) Block round-trip.
    block = bytes([0x10, 0x27, 0x00,        # gold/exp = 0x002710 = 10000
                   0x64, 0x00,              # gems = 100
                   0x2A, 0x05, 0x80,        # item0: id=0x2A charges=5 flags=0x80
                   0x00, 0x00, 0x00,        # item1: empty
                   0xFF, 0x03, 0x01])       # item2: id=0xFF charges=3 flags=1
    fi = decode_block(block)
    assert fi.gold_exp == 10000, fi.gold_exp
    assert fi.gems == 100, fi.gems
    assert fi.id == [0x2A, 0x00, 0xFF], fi.id
    assert fi.charges == [5, 0, 3], fi.charges
    assert fi.flags == [0x80, 0, 1], fi.flags
    assert fi.sentinel == SENTINEL_FILLED
    assert encode_block(fi) == block, encode_block(fi).hex()

    # 2) Image fill + read-back round-trip (big-endian RAM).
    img = _new_image()
    op2a_fill(img, block)
    rd = read_image(img)
    assert rd == fi, (rd, fi)
    # Verify the exact RAM bytes the 68k stores (big-endian gold/gems).
    assert img[IMG_GOLD:IMG_GOLD + 4] == bytes([0x00, 0x00, 0x27, 0x10]), \
        img[IMG_GOLD:IMG_GOLD + 4].hex()
    assert img[IMG_GEMS:IMG_GEMS + 2] == bytes([0x00, 0x64]), \
        img[IMG_GEMS:IMG_GEMS + 2].hex()
    assert img[IMG_ID:IMG_ID + 3] == bytes([0x2A, 0x00, 0xFF])
    assert img[IMG_FLAGS:IMG_FLAGS + 3] == bytes([0x80, 0x00, 0x01])
    assert img[IMG_CHARGES:IMG_CHARGES + 3] == bytes([0x05, 0x00, 0x03])
    assert img[IMG_SENTINEL] == 0xFF
    assert read_image(img).has_loot()

    # 3) OP_19 overflow placement order (id[0], id[1], then slot 2).
    img = _new_image()
    assert overflow_append(img, 0x11, 1, 0) == 0
    assert overflow_append(img, 0x22, 2, 0) == 1
    assert overflow_append(img, 0x33, 3, 0) == 2  # both full -> slot 2
    assert overflow_append(img, 0x44, 4, 0) == 2  # still slot 2 (overwrite)
    rd = read_image(img)
    assert rd.id == [0x11, 0x22, 0x44], rd.id
    assert rd.charges == [1, 2, 4], rd.charges
    assert rd.sentinel == 0xFF

    # 4) Empty buffer is not "loot".
    assert not read_image(_new_image()).has_loot()

    print("mm2_found_items selftest OK")


if __name__ == "__main__":
    _selftest()

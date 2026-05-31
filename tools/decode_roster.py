#!/usr/bin/env python3
"""
Decode MM2 Amiga roster.dat — 64 character records, 130 bytes each.

Field map derived from disassembly of the Amiga executable (68000 code)
and cross-referenced with the PC/DOS community documentation.

Key ASM evidence:
  - Record size $82 (130): MULS #$0082,D0 appears ~40× in the binary.
  - LAB_47A6: canonical "get character pointer" (slot→A4-$D5C2 + slot*130).
  - Character sheet display routine at $3A2C–$3B10:
      TST.B  $C(A0)           → Sex (0=Male, else Female)
      MOVE.B $6A(A0) → tbl   → Alignment string
      MOVE.B $E(A0)  → tbl   → Race string
      MOVE.B $F(A0)  → tbl   → Class string
      MOVE.B $71(A0)          → Level (numeric)
      MOVE.W $5E(A0)          → HP max
      MOVE.W $74(A0)          → HP current
  - Party setup ($520E–$528C) copies $15→$70, $27→$73 (base stats echo).
  - Class branch at $7B36: $F==2(Archer) or $F==4(Sorcerer) → INT-based SP.
  - Byte $0B read via LEA A4-$D5CD (roster_base+$0B), masked with $7F.
    Bit 7 appears to be an "in-party" or "active" flag.

File byte order: LITTLE-ENDIAN for words/longs (matches DOS original;
the Amiga port byte-swaps on load to native big-endian).
"""

import struct
import sys
from pathlib import Path

RECORD_SIZE = 130
NUM_SLOTS = 64

CLASSES = [
    "Knight", "Paladin", "Archer", "Cleric",
    "Sorcerer", "Robber", "Ninja", "Barbarian"
]
RACES = ["Human", "Elf", "Dwarf", "Gnome", "Half-Orc"]
ALIGNMENTS = ["Good", "Neutral", "Evil"]
SEXES = {0: "Male", 1: "Female"}
TOWNS = {0: "(none)", 1: "Middlegate", 2: "Sandsobar",
          3: "Vulcania", 4: "Atlantium", 5: "Tundara"}


def decode_name(data, off):
    raw = data[off:off+11]
    end = raw.find(0)
    if end < 0:
        end = 11
    return raw[:end].decode("ascii", errors="replace").rstrip()


def u8(data, off):
    return data[off]


def u16le(data, off):
    return struct.unpack_from("<H", data, off)[0]


def u32le(data, off):
    return struct.unpack_from("<I", data, off)[0]


def decode_record(data, slot):
    off = slot * RECORD_SIZE
    rec = data[off:off+RECORD_SIZE]

    name = decode_name(data, off)
    if not name.strip() or all(b == 0 for b in rec[:10]):
        return None
    # Sanity: valid records have a town byte (1-5) and printable name
    town_raw = rec[0x0B]
    if (town_raw & 0x7F) == 0 and rec[0x0F] == 0 and rec[0x71] == 0:
        if not any(0x21 <= b <= 0x7E for b in rec[:10]):
            return None

    town_byte = u8(data, off + 0x0B)
    town_id = town_byte & 0x7F
    in_party = bool(town_byte & 0x80)

    sex_val   = u8(data, off + 0x0C)
    align_cur = u8(data, off + 0x0D)
    race_val  = u8(data, off + 0x0E)
    class_val = u8(data, off + 0x0F)

    # Current stats (can be buffed/debuffed)
    might_c = u8(data, off + 0x10)
    int_c   = u8(data, off + 0x11)
    per_c   = u8(data, off + 0x12)
    spd_c   = u8(data, off + 0x13)
    acc_c   = u8(data, off + 0x14)
    luck_c  = u8(data, off + 0x15)

    thievery = u8(data, off + 0x16)
    # Bytes 0x17-0x19: possibly sub-skills or resistances
    skill_a  = u8(data, off + 0x17)
    skill_b  = u8(data, off + 0x18)
    skill_c  = u8(data, off + 0x19)

    age       = u8(data, off + 0x21)
    ac        = u8(data, off + 0x24)
    food      = u8(data, off + 0x25)
    condition = u8(data, off + 0x26)
    end_c     = u8(data, off + 0x27)  # current Endurance

    # Equipment: 6 slots × 3 bytes (id, bonus, flags?)
    equip = []
    for i in range(6):
        base = off + 0x28 + i * 3
        equip.append((u8(data, base), u8(data, base+1), u8(data, base+2)))

    # Backpack: 6 slots × 3 bytes
    backpack = []
    for i in range(6):
        base = off + 0x3A + i * 3
        backpack.append((u8(data, base), u8(data, base+1), u8(data, base+2)))

    # Spell book area (offsets 0x4C–0x57, 12 bytes of bitmasks)
    spells = data[off + 0x4C:off + 0x58]

    # SP / Gems / HP / XP / Gold
    sp_max  = u16le(data, off + 0x58)
    sp_cur  = u16le(data, off + 0x5A)
    gems    = u16le(data, off + 0x5C)
    hp_max  = u16le(data, off + 0x5E)
    hp_unk  = u16le(data, off + 0x60)  # possibly temp HP or prior HP max
    xp      = u32le(data, off + 0x62)
    gold    = u32le(data, off + 0x66)

    # Tail / base-stat block
    align_base = u8(data, off + 0x6A)
    might_b    = u8(data, off + 0x6B)
    int_b      = u8(data, off + 0x6C)
    per_b      = u8(data, off + 0x6D)
    spd_b      = u8(data, off + 0x6E)
    acc_b      = u8(data, off + 0x6F)
    luck_b     = u8(data, off + 0x70)
    level      = u8(data, off + 0x71)
    spell_lvl  = u8(data, off + 0x72)  # secondary/spell level
    end_b      = u8(data, off + 0x73)
    hp_cur     = u16le(data, off + 0x74)

    return {
        "slot": slot,
        "name": name,
        "town_id": town_id,
        "town": TOWNS.get(town_id, f"?({town_id})"),
        "in_party": in_party,
        "sex": SEXES.get(sex_val, f"?({sex_val})"),
        "alignment": ALIGNMENTS[align_base] if align_base < 3 else f"?({align_base})",
        "race": RACES[race_val] if race_val < 5 else f"?({race_val})",
        "class": CLASSES[class_val] if class_val < 8 else f"?({class_val})",
        "level": level,
        "spell_lvl": spell_lvl,
        "age": age,
        "stats_current": {
            "Might": might_c, "Int": int_c, "Per": per_c,
            "Spd": spd_c, "Acc": acc_c, "Luck": luck_c, "End": end_c
        },
        "stats_base": {
            "Might": might_b, "Int": int_b, "Per": per_b,
            "Spd": spd_b, "Acc": acc_b, "Luck": luck_b, "End": end_b
        },
        "hp_max": hp_max,
        "hp_cur": hp_cur,
        "sp_max": sp_max,
        "sp_cur": sp_cur,
        "ac": ac,
        "food": food,
        "condition": condition,
        "thievery": thievery,
        "gems": gems,
        "xp": xp,
        "gold": gold,
        "equip": equip,
        "backpack": backpack,
        "spells_raw": spells.hex(),
    }


def format_items(items):
    parts = []
    for item_id, bonus, flags in items:
        if item_id == 0 and bonus == 0:
            continue
        s = f"#{item_id}"
        if bonus:
            s += f"+{bonus}"
        if flags:
            s += f"[{flags:02X}]"
        parts.append(s)
    return ", ".join(parts) if parts else "(empty)"


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "EXTRACTED" / "roster.dat"
    data = path.read_bytes()
    assert len(data) == RECORD_SIZE * NUM_SLOTS, f"Expected {RECORD_SIZE*NUM_SLOTS} bytes, got {len(data)}"

    print(f"{'='*72}")
    print(f"  MM2 Amiga roster.dat — {NUM_SLOTS} slots × {RECORD_SIZE} bytes = {len(data)} bytes")
    print(f"{'='*72}\n")

    for slot in range(NUM_SLOTS):
        r = decode_record(data, slot)
        if r is None:
            continue

        sc = r["stats_current"]
        sb = r["stats_base"]
        flag = " [IN PARTY]" if r["in_party"] else ""

        print(f"Slot {r['slot']:2d}: {r['name']:<12s}  "
              f"{r['sex']} {r['alignment']} {r['race']} {r['class']}{flag}")
        print(f"         Level {r['level']}  Age {r['age']}  "
              f"Town: {r['town']}")
        print(f"         HP {r['hp_cur']}/{r['hp_max']}  "
              f"SP {r['sp_cur']}/{r['sp_max']}  "
              f"AC {r['ac']}  Food {r['food']}  "
              f"Cond {r['condition']}  Thiev {r['thievery']}%")
        print(f"         XP {r['xp']:>10,}  Gold {r['gold']:>8,}  Gems {r['gems']}")

        stat_str = "  ".join(
            f"{k}:{sc[k]}" + (f"({sb[k]})" if sc[k] != sb[k] else "")
            for k in ["Might", "Int", "Per", "End", "Spd", "Acc", "Luck"]
        )
        print(f"         {stat_str}")
        print(f"         Equip: {format_items(r['equip'])}")
        print(f"         Pack:  {format_items(r['backpack'])}")
        if any(b != 0 for b in bytes.fromhex(r["spells_raw"])):
            print(f"         Spells: {r['spells_raw']}")
        print()


if __name__ == "__main__":
    main()

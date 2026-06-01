#!/usr/bin/env python3
"""MM2 monsters.dat decoder/encoder with ASM-confirmed ability decode.

Record geometry: 256 records x 26 bytes (0x1A). The per-record unpacker is at
asm 0x4C8E; the ability/verb tables were lifted from the data hunk
(mm2_data_00.bin) and verified (A4 = data_base + 0x7FFE).

Usage:
  python tools/decode_monsters.py decode <monsters.dat> <monsters.csv>
  python tools/decode_monsters.py encode <monsters.csv> <monsters.dat>
  python tools/decode_monsters.py show   <monsters.dat> [index]
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

RECORD_SIZE = 0x1A
RECORD_COUNT = 256
NAME_SIZE = 14
FILE_SIZE = RECORD_SIZE * RECORD_COUNT

OFF_HP, OFF_XP, OFF_TREASURE = 0x0E, 0x0F, 0x10
OFF_PABIL, OFF_SABIL, OFF_OABIL = 0x11, 0x12, 0x13
OFF_SPEED, OFF_PICTURE, OFF_AC = 0x14, 0x15, 0x16
OFF_DAMAGE, OFF_SPEED2, OFF_MRES = 0x17, 0x18, 0x19

HP_XP_MUL = (1, 10, 100, 1000)


def decode_hp(hp_code: int) -> int:
    """ASM 0x4C8E: ((code & 0x3F)+1) * mul[(code>>6)&3]."""
    return ((hp_code & 0x3F) + 1) * HP_XP_MUL[(hp_code >> 6) & 0x03]


def decode_xp(xp_code: int) -> int:
    """ASM 0x4C8E: ((code & 0x1F)+1) * mul[(code>>5)&3], x1000 if bit7."""
    xp = ((xp_code & 0x1F) + 1) * HP_XP_MUL[(xp_code >> 5) & 0x03]
    if xp_code & 0x80:
        xp *= 1000
    return xp


def decode_ac(ac: int) -> int:
    """Byte 0x16: low5+1, x10 if bit5."""
    val = (ac & 0x1F) + 1
    if ac & 0x20:
        val *= 10
    return val


def decode_damage(damage: int) -> int:
    """Byte 0x17: low5+1, x10 if bit5, capped 250."""
    val = (damage & 0x1F) + 1
    if damage & 0x20:
        val *= 10
    return min(val, 250)


def decode_speed2(speed2: int) -> int:
    """Byte 0x18 (combat initiative): low5+1, x10 if bit5, capped 250."""
    val = (speed2 & 0x1F) + 1
    if speed2 & 0x20:
        val *= 10
    return min(val, 250)


def decode_speed_nibbles(speed: int) -> tuple[int, int]:
    """Byte 0x14: low nibble+1, high nibble+1."""
    return (speed & 0x0F) + 1, ((speed >> 4) & 0x0F) + 1


def decode_treasure(treasure: int) -> dict[str, int | bool]:
    """Byte 0x10 reward pack (ASM 0x10B74): item level, gems flag, gold tier."""
    return {
        "item_drop_level": treasure & 0x03,
        "drops_gems": bool(treasure & 0x04),
        "gold_tier": (treasure >> 3) & 0x03,
    }


def enrich_decoded_stats(d: dict) -> dict:
    """Add ASM-decoded combat stats to a decode_record() dict."""
    spd_lo, spd_hi = decode_speed_nibbles(d["speed"])
    tres = decode_treasure(d["treasure"])
    ac_flags: list[str] = []
    if d["ac"] & 0x40:
        ac_flags.append("flag bit6")
    if d["ac"] & 0x80:
        ac_flags.append("flag bit7")
    return {
        **d,
        "hp": decode_hp(d["hp_code"]),
        "xp": decode_xp(d["xp_code"]),
        "ac_val": decode_ac(d["ac"]),
        "ac_flags": ac_flags,
        "damage_val": decode_damage(d["damage"]),
        "speed_lo": spd_lo,
        "speed_hi": spd_hi,
        "initiative": decode_speed2(d["speed2"]),
        **tres,
    }

# Sabil low-5 -> single-target status effect (victim-message table, asm ~0xFA1A).
SINGLE_EFFECT_NAMES = [
    "nothing", "loose gold", "loose gems", "Poison", "Disease", "Sleep",
    "Curse", "Silence", "Paralyse", "Collapse", "Kills", "Stones",
    "Eradicates", "Steals an Item", "Steals ALL Items", "Steals food",
    "Steals all Food", "loose all Gold", "Loose all Gems", "Loose all Valuables",
    "Ages (non perm)", "Ages (PERM)", "loose 1 level", "Halves all Stats",
    "gives you Personality", "loose experience levels", "halves experience level",
    "loose Experience", "Items Scrambled", "Spell points loose all",
    "Assassinates", "Random Effect",
]

# Pabil low-5 -> group attack verb (verb table at asm 0x10002 / 0xFB98+).
PARTY_VERB_NAMES = [
    "sprays poison", "sprays acid", "casts a curse", "breathes fire",
    "breathes lightning", "breathes cold", "breathes energy", "breathes gas",
    "breathes acid", "explodes", "gazes", "drains magic", "drains spell level",
    "vaporizes valuables", "juggles party", "energy blast", "sleep",
    "lightning bolts", "fireballs", "fingers of death", "disintegrate",
    "super shock", "dancing sword", "incinerate", "invokes power", "implosion",
    "inferno", "pain", "silence", "frenzies", "paralyze", "swarms",
]

CSV_HEADER = [
    "index", "name",
    "hp_code", "xp_code", "treasure",
    "party_verb", "party_chance",
    "single_effect", "single_misc", "archer", "undead",
    "add_friends_base", "add_friends_x10", "flee_tier", "multiplies",
    "speed", "picture", "ac", "damage", "speed2", "mres",
]


def name_to_str(rec: bytes) -> str:
    # Names are stored as char+128 per byte.
    s = "".join(chr((b - 128) & 0xFF) for b in rec[:NAME_SIZE])
    return s.rstrip(" \x00")


def name_to_bytes(name: str) -> bytes:
    out = bytearray()
    for i in range(NAME_SIZE):
        c = name[i] if i < len(name) else " "
        out.append((ord(c) + 128) & 0xFF)
    return bytes(out)


def decode_record(idx: int, rec: bytes) -> dict:
    pabil, sabil, oabil = rec[OFF_PABIL], rec[OFF_SABIL], rec[OFF_OABIL]
    return {
        "index": idx,
        "name": name_to_str(rec),
        "hp_code": rec[OFF_HP],
        "xp_code": rec[OFF_XP],
        "treasure": rec[OFF_TREASURE],
        "party_verb": pabil & 0x1F,
        "party_chance": (pabil >> 5) & 0x07,
        "single_effect": sabil & 0x1F,
        "single_misc": (sabil >> 5) & 0x01,
        "archer": (sabil >> 6) & 0x01,
        "undead": (sabil >> 7) & 0x01,
        "add_friends_base": oabil & 0x0F,
        "add_friends_x10": (oabil >> 4) & 0x01,
        "flee_tier": (oabil >> 5) & 0x03,
        "multiplies": (oabil >> 7) & 0x01,
        "speed": rec[OFF_SPEED],
        "picture": rec[OFF_PICTURE],
        "ac": rec[OFF_AC],
        "damage": rec[OFF_DAMAGE],
        "speed2": rec[OFF_SPEED2],
        "mres": rec[OFF_MRES],
    }


def encode_record(row: dict) -> bytes:
    def n(key: str) -> int:
        return int(row[key], 0) if isinstance(row[key], str) else int(row[key])

    rec = bytearray(name_to_bytes(row["name"]))
    while len(rec) < RECORD_SIZE:
        rec.append(0)
    rec[OFF_HP] = n("hp_code") & 0xFF
    rec[OFF_XP] = n("xp_code") & 0xFF
    rec[OFF_TREASURE] = n("treasure") & 0xFF
    rec[OFF_PABIL] = ((n("party_verb") & 0x1F) | ((n("party_chance") & 0x07) << 5)) & 0xFF
    rec[OFF_SABIL] = (
        (n("single_effect") & 0x1F)
        | ((n("single_misc") & 0x01) << 5)
        | ((n("archer") & 0x01) << 6)
        | ((n("undead") & 0x01) << 7)
    ) & 0xFF
    rec[OFF_OABIL] = (
        (n("add_friends_base") & 0x0F)
        | ((n("add_friends_x10") & 0x01) << 4)
        | ((n("flee_tier") & 0x03) << 5)
        | ((n("multiplies") & 0x01) << 7)
    ) & 0xFF
    rec[OFF_SPEED] = n("speed") & 0xFF
    rec[OFF_PICTURE] = n("picture") & 0xFF
    rec[OFF_AC] = n("ac") & 0xFF
    rec[OFF_DAMAGE] = n("damage") & 0xFF
    rec[OFF_SPEED2] = n("speed2") & 0xFF
    rec[OFF_MRES] = n("mres") & 0xFF
    return bytes(rec)


def cmd_decode(src: Path, dst: Path) -> None:
    data = src.read_bytes()
    if len(data) != FILE_SIZE:
        raise ValueError(f"monsters.dat size {len(data)} != {FILE_SIZE}")
    with dst.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=CSV_HEADER)
        w.writeheader()
        for idx in range(RECORD_COUNT):
            rec = data[idx * RECORD_SIZE : (idx + 1) * RECORD_SIZE]
            w.writerow(decode_record(idx, rec))


def cmd_encode(src: Path, dst: Path) -> None:
    out = bytearray()
    with src.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for expected, row in enumerate(reader):
            if int(row["index"], 0) != expected:
                raise ValueError(f"row index mismatch at {expected}")
            out.extend(encode_record(row))
    if len(out) != FILE_SIZE:
        raise ValueError(f"encoded size {len(out)} != {FILE_SIZE}")
    dst.write_bytes(out)


def cmd_show(src: Path, idx: int | None) -> None:
    data = src.read_bytes()
    indices = range(RECORD_COUNT) if idx is None else [idx]
    for i in indices:
        rec = data[i * RECORD_SIZE : (i + 1) * RECORD_SIZE]
        d = decode_record(i, rec)
        if not d["name"]:
            continue
        verb = PARTY_VERB_NAMES[d["party_verb"]]
        eff = SINGLE_EFFECT_NAMES[d["single_effect"]]
        friends = (d["add_friends_base"] + 1) * (10 if d["add_friends_x10"] else 1)
        flags = []
        if d["undead"]:
            flags.append("undead")
        if d["archer"]:
            flags.append("archer")
        if d["multiplies"]:
            flags.append("multiplies")
        print(
            f"#{i:3} {d['name']:<14} "
            f"group='{verb}'(ch{d['party_chance']}) "
            f"single='{eff}' "
            f"friends={friends} flee={d['flee_tier']} "
            f"{' '.join(flags)}"
        )


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print(__doc__.strip())
        return 1
    cmd = argv[1]
    if cmd == "decode" and len(argv) == 4:
        cmd_decode(Path(argv[2]), Path(argv[3]))
    elif cmd == "encode" and len(argv) == 4:
        cmd_encode(Path(argv[2]), Path(argv[3]))
    elif cmd == "show" and len(argv) in (3, 4):
        cmd_show(Path(argv[2]), int(argv[3], 0) if len(argv) == 4 else None)
    else:
        print(__doc__.strip())
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

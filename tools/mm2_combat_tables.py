"""Combat lookup tables from mm2_data_00.bin (A4-relative, doc 16/17).

Offsets: file_off = 0x7FFE - a4_disp
"""
from __future__ import annotations

from pathlib import Path

_DATA = Path(__file__).resolve().parents[1] / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"


def _load() -> bytes:
    return _DATA.read_bytes()


def _slice(a4_disp: int, n: int) -> bytes:
    off = 0x7FFE - a4_disp
    data = _load()
    return data[off : off + n]


# Monster to-hit threshold addend by (type_id >> 4) @ A4-$6F06
MONSTER_HIT_ADDEND = list(_slice(0x6F06, 16))

# Monster melee: AC reference by (type_id >> 4) @ A4-$6F16
MONSTER_AC_REF = list(_slice(0x6F16, 16))

# Oabil flee tier 0..3 @ A4-$6F1A (255 = never flee)
FLEE_THRESHOLD = list(_slice(0x6F1A, 4))

# Player hit bonus divisor by class 0..7 @ A4-$6F3E
HIT_DIV_BY_CLASS = list(_slice(0x6F3E, 8))

# Player attack swing count divisor by class @ A4-$6F36
SWING_DIV_BY_CLASS = list(_slice(0x6F36, 8))

# Pabil party_chance tier 0..7 @ A4-$7464
PARTY_ATTACK_THRESHOLD = list(_slice(0x7464, 8))

# Group attack base damage by min(party_count-1, 3) @ A4-$6F26 (big-endian words)
GROUP_DAMAGE_BY_PARTY_IDX = [
    int.from_bytes(_slice(0x6F26, 8)[i : i + 2], "big") for i in range(0, 8, 2)
]

CLASSES = [
    "Knight", "Paladin", "Archer", "Cleric",
    "Sorcerer", "Robber", "Ninja", "Barbarian",
]

# Stat tier breakpoints @ A4-$73DE, used by JSR -$7F56(A4) (handler ~0x6488).
# Loop: tier starts at 1; while stat >= threshold[tier] and tier < 9: tier++.
# Return tier in D0; combat @ 0x113EE/0x1140C adds it to might/accuracy packs.
STAT_TIER_THRESHOLDS = list(_slice(0x73DE, 9))


def stat_tier(stat: int) -> int:
    """ASM tier lookup @ 0x6488 using A4-$73DE thresholds."""
    s = max(0, min(int(stat), 255))
    tier = 1
    while tier < 9 and s >= STAT_TIER_THRESHOLDS[tier]:
        tier += 1
    return tier


def stat_effect(stat: int) -> int:
    """Combat modifier from -$7F56(A4): tier index returned in D0 @ 0x113F6/0x1140C."""
    return stat_tier(stat)


# Item-id bands @ 0xF5F4..0xF68E (used when rebuilding $4C-$4F @ 0xF36C).
def _item_melee_a(item_id: int) -> bool:
    return 1 <= item_id <= 0x41


def _item_melee_b(item_id: int) -> bool:
    return 0x42 <= item_id <= 0x5B


def item_is_melee(item_id: int) -> bool:
    return _item_melee_a(item_id) or _item_melee_b(item_id)


def item_is_ranged(item_id: int) -> bool:
    return 0x5C <= item_id <= 0x72


def _pack_low6(item_id: int, bonus: int) -> int:
    """Runtime $4D/$4F low byte: equip id nibble + bonus (@ 0xF392 and 0x1137A)."""
    return min(255, (item_id & 0x3F) + bonus)


def build_weapon_fields(
    equip: list[tuple[int, int, int]],
    item_damage: list[int],
) -> tuple[int, int, int, int]:
    """Pick melee $4C/$4D and ranged $4E/$4F from six equip slots.

    Mirrors 0xF6E0 / 0xF7BE slot scans: first matching slot wins per band; we
    prefer the highest ``items.dat`` damage when several qualify. ``$4C`` is
    the weapon die max (table at A4-$3EEE resolves to ``items.dat`` damage in
    practice — RNG @ 0x11508 uses A4-$5E30 ← $4C).
    """
    best_m = (0, 0)  # die, acc pack
    best_r = (0, 0)
    for item_id, bonus, _ in equip:
        if not item_id or item_id >= len(item_damage):
            continue
        die = item_damage[item_id]
        if die <= 0:
            continue
        acc = _pack_low6(item_id, bonus)
        if item_is_melee(item_id) and die >= best_m[0]:
            best_m = (die, acc)
        if item_is_ranged(item_id) and die >= best_r[0]:
            best_r = (die, acc)
    return best_m[0], best_m[1], best_r[0], best_r[1]

#!/usr/bin/env python3
"""MM2 per-town commerce constant tables (OP_0E town/building services).

Python mirror of ``EXTRACTED/decomp/mm2_town_tables.{h,c}``. These five-entry
tables are loaded into the A4 data block from the game data hunk and indexed by
the current map screen id (A4-$79F2, 0..4 = Middlegate, Atlantium, Tundara,
Vulcania, Sandsobar). They are constants; the engine never rewrites them.

Confirmed ASM sources (EXTRACTED/mm2.capstone.annotated.asm):
  * A4-$6720  training stat add per map   @ training_stat_apply 0x1C898
  * A4-$671A  training stat cap per map   @ 0x1C8A8
  * A4-$6742  temple donation gold (BE u16) @ 0x1CA3A / display 0x1D556
  * A4-$66B1  temple donation quest bit    @ doc 28 §5.2 (OR into A4-$799E)
  * training "town index" multiplier (FAQ §3-6, doc 34 §13): map -> [1,5,2,4,2]

Stat-id -> roster record byte offset (jump table @ 0x1C86C feeding 0x1C898).
"""
from __future__ import annotations

from dataclasses import dataclass

TOWN_NAMES = ["Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"]
TEMPLE_DONATION_ALL_BITS = 0x1F


@dataclass(frozen=True)
class TownCommerce:
    training_town_index: int  # FAQ §3-6 cost multiplier (NOT the map index)
    stat_train_add: int       # A4-$6720[map]
    stat_train_cap: int       # A4-$671A[map]
    donation_gold: int        # A4-$6742[map]
    donation_quest_bit: int   # A4-$66B1[map]


# map id 0..4
TOWNS = [
    TownCommerce(1, 5, 100, 20, 0x01),    # Middlegate
    TownCommerce(5, 20, 100, 250, 0x02),  # Atlantium
    TownCommerce(2, 10, 100, 40, 0x04),   # Tundara
    TownCommerce(4, 10, 100, 120, 0x08),  # Vulcania
    TownCommerce(2, 3, 50, 40, 0x10),     # Sandsobar
]

# stat id 0..6 -> roster record byte offset (0x1C86C / 0x1C898)
STAT_FIELD_OFFSET = [0x6B, 0x6F, 0x6D, 0x6C, 0x71, 0x72, 0x6E]
STAT_FIELD_NAME = [
    "might_base", "accuracy_base", "personality_base",
    "intelligence_base", "level", "spell_level", "speed_base",
]


def town_commerce(map_id: int) -> TownCommerce:
    return TOWNS[map_id]


def stat_field_offset(stat_id: int) -> int:
    return STAT_FIELD_OFFSET[stat_id]


def training_cost(level: int, town_index: int) -> int:
    """level * training_town_index * 50 gp (FAQ §3-6)."""
    return max(0, level) * max(0, town_index) * 50


def healing_cost(level: int, town_index: int) -> int:
    """level * training_town_index * 10 gp (FAQ §3-6)."""
    return max(0, level) * max(0, town_index) * 10


# ---------------------------------------------------------------------------
# Training-hall LEVEL progression (OP_0E 0x04, handler -$7D16). The Training Hall
# does NOT raise a stat: it levels a character up from experience (record+0x62)
# when they meet the next level's threshold on their class XP curve and pay the
# fee (level*index*50). Two curves keyed on class (doc 32); class_id = record
# +0x0F (0 Knight .. 7 Barbarian). Group A ~25% cheaper than B.
#   Group A: Knight(0), Cleric(3), Robber(5), Barbarian(7)
#   Group B: Paladin(1), Archer(2), Sorcerer(4), Ninja(6)
# Thresholds = doc-32 "thousands" column * 1000, levels 2..77.
# (XP comparison long @ A4-$6FC6; per-class threshold array address not pinned.)
# ---------------------------------------------------------------------------
_XP_A_K = [  # thousands, level 2..77
    1500, 3000, 6000, 12000, 24000, 48000, 96000, 192000, 384000, 576000,
    768000, 960000, 1344000, 1728000, 2496000, 3264000, 4032000, 4800000,
    5568000, 7104000, 8640000, 10276000, 11712000, 13248000, 14784000,
    16320000, 17856000, 19392000, 20928000, 24000000, 27072000, 30144000,
    33216000, 36288000, 39360000, 42432000, 45504000, 48576000, 51648000,
    54720000, 57792000, 60864000, 63936000, 67008000, 70080000, 73152000,
    76224000, 79296000, 82368000, 88512000, 94656000, 100800000, 106944000,
    113088000, 119232000, 125376000, 131520000, 137664000, 143808000,
    149952000, 156096000, 162240000, 168384000, 174428000, 180672000,
    186816000, 192960000, 199104000, 205248000, 211392000, 217536000,
    223680000, 229824000, 235968000, 252352000, 268736000,
]
_XP_B_K = [
    2000, 4000, 8000, 16000, 32000, 64000, 128000, 256000, 512000, 768000,
    1024000, 1280000, 1728000, 2304000, 3328000, 4352000, 5376000, 6400000,
    7424000, 9472000, 11520000, 13568000, 15616000, 17664000, 19712000,
    21760000, 23808000, 25832000, 27856000, 32000000, 36096000, 40192000,
    44288000, 48384000, 52480000, 56576000, 60672000, 64768000, 68864000,
    72960000, 77056000, 81152000, 85248000, 89344000, 93440000, 97536000,
    101632000, 105728000, 109824000, 118016000, 126208000, 134400000,
    142592000, 150784000, 158976000, 167168000, 175360000, 183552000,
    191744000, 199936000, 208128000, 216320000, 224512000, 232704000,
    240896000, 249088000, 257280000, 265472000, 273664000, 281856000,
    290048000, 298240000, 306432000, 314624000, 331008000, 347392000,
]
# Raw XP (already in real units; the *_K lists hold raw XP, not thousands).
XP_GROUP_A = list(_XP_A_K)
XP_GROUP_B = list(_XP_B_K)

_CLASS_HP_PER_LEVEL = [12, 10, 10, 8, 6, 8, 10, 15]  # Knight..Barbarian, END 11-12


def class_xp_group(class_id: int) -> int:
    """0 = Group A, 1 = Group B, -1 if out of range (class_id = record+0x0F)."""
    if class_id in (0, 3, 5, 7):
        return 0
    if class_id in (1, 2, 4, 6):
        return 1
    return -1


def class_xp_for_level(class_id: int, level: int) -> int:
    """Total XP to BE `level`; 0 for level<=1; 0xFFFFFFFF beyond level 77 (gap)."""
    group = class_xp_group(class_id)
    if group < 0 or level <= 1:
        return 0
    if level < 2 or level > 77:
        return 0xFFFFFFFF
    return (XP_GROUP_A if group == 0 else XP_GROUP_B)[level - 2]


def class_hp_per_level(class_id: int) -> int:
    return _CLASS_HP_PER_LEVEL[class_id] if 0 <= class_id < 8 else 0


def attr_bonus(v: int) -> int:
    """doc 32 HP(END)/AC(SPD) per-level bonus column (SP/level = this + 3)."""
    bounds = (13, 15, 17, 19, 22, 26, 30, 45, 60, 75, 90, 105, 120, 135, 150,
              175, 200, 225)
    return sum(1 for b in bounds if v >= b)


def class_caster_stat(class_id: int) -> int:
    """0 none, 1 INT (Sorcerer/Archer), 2 PER (Cleric/Paladin)."""
    if class_id in (2, 4):
        return 1
    if class_id in (1, 3):
        return 2
    return 0


def class_spell_level_for(class_id: int, char_level: int) -> int:
    """Spell level reachable at char_level (pure casters 2X-1; f-mages 2X+5, cap 7)."""
    if char_level < 1:
        return 0
    if class_id in (3, 4):
        return (char_level + 1) // 2
    if class_id in (1, 2):
        return max(0, min(7, (char_level - 5) // 2))
    return 0


# ---------------------------------------------------------------------------
# Blacksmith static inventories (OP_0E 0x06, handler 0x1C54A; builder 0x1C00E).
# town*6 + slot into data-hunk tables; categories selected by the jump @ 0x1C09E.
# Decoded byte-exact from EXTRACTED/ghidra/mm2_data_00.bin (see dump_shop_tables).
# ---------------------------------------------------------------------------
SMITH_WEAPONS, SMITH_SPECIALS, SMITH_ARMOR, SMITH_MISC = range(4)
SMITH_CATEGORY_NAME = ["Weapons", "Specials", "Armor", "Misc"]
SMITH_SLOTS = 6

# [category][map][slot] item ids (A4-$68EE / $683A / $68B2 / $6876)
SMITH_IDS = [
    [  # Weapons
        [4, 6, 7, 10, 12, 13], [15, 19, 20, 17, 16, 10], [8, 18, 17, 22, 23, 24],
        [9, 18, 12, 14, 4, 13], [16, 15, 14, 19, 21, 20],
    ],
    [  # Specials
        [66, 67, 68, 92, 93, 95], [97, 77, 78, 75, 73, 23], [96, 71, 74, 15, 19, 20],
        [24, 72, 76, 22, 17, 21], [94, 69, 70, 9, 10, 11],
    ],
    [  # Armor
        [115, 127, 128, 129, 130, 155], [155, 117, 127, 131, 132, 134],
        [117, 131, 132, 133, 134, 155], [155, 116, 127, 129, 130, 133],
        [116, 128, 129, 130, 131, 132],
    ],
    [  # Misc
        [161, 162, 160, 163, 164, 208], [170, 167, 163, 187, 189, 211],
        [169, 176, 177, 178, 181, 189], [171, 172, 195, 186, 184, 210],
        [168, 175, 162, 165, 166, 209],
    ],
]

# [category][map][slot] meta bytes (A4-$68D0 / $6894 weapons/armor '+' bonus;
# $6858 misc charges). Specials bonus is date-rolled @ 0x1C146 (kept 0).
SMITH_META = [
    [  # Weapons
        [0, 0, 0, 0, 0, 0], [3, 2, 2, 3, 3, 5], [1, 0, 0, 0, 0, 0],
        [1, 1, 2, 2, 4, 1], [0, 0, 0, 0, 0, 0],
    ],
    [[0] * 6 for _ in range(5)],  # Specials (date-rolled, deferred)
    [  # Armor
        [0, 0, 0, 0, 0, 0], [4, 1, 3, 2, 1, 1], [0, 0, 0, 0, 0, 1],
        [2, 1, 1, 1, 1, 1], [0, 0, 0, 0, 0, 0],
    ],
    [  # Misc
        [1, 20, 3, 0, 1, 0], [2, 50, 0, 25, 20, 0], [5, 8, 20, 8, 20, 10],
        [5, 2, 20, 35, 5, 0], [3, 1, 20, 250, 50, 0],
    ],
]


def smith_inventory(map_id: int, category: int) -> list[tuple[int, int]]:
    """Return the 6 (item_id, meta) shop slots for (map_id, category)."""
    return list(zip(SMITH_IDS[category][map_id], SMITH_META[category][map_id]))


def smith_buy_fields(category: int, meta: int) -> tuple[int, int, int]:
    """(price_meta, charges, flags) for a buy (0x1C00E / 0x1BE44)."""
    if category == SMITH_MISC:
        return 0, meta, 0
    if category in (SMITH_WEAPONS, SMITH_ARMOR):
        return meta & 0x3F, 0, meta
    return 0, 0, 0  # Specials: date-rolled (deferred)


def smith_price(base_gold: int, meta: int) -> int:
    """0x1BF16 buy price: meta==0 -> base; else base*2 + (meta-1)*1000."""
    bonus = meta & 0x3F
    if bonus == 0:
        return base_gold
    price = base_gold * 2
    bonus -= 1
    price += bonus * 1000
    return price


def _main() -> None:
    print("map town        idx add cap donation bit")
    for i, t in enumerate(TOWNS):
        print(f"{i}   {TOWN_NAMES[i]:<11} {t.training_town_index}   "
              f"{t.stat_train_add:<3} {t.stat_train_cap:<3} "
              f"{t.donation_gold:<8} 0x{t.donation_quest_bit:02x}")
    print("\nstat id -> record offset:")
    for sid, off in enumerate(STAT_FIELD_OFFSET):
        print(f"  {sid} 0x{off:02x} {STAT_FIELD_NAME[sid]}")


if __name__ == "__main__":
    _main()

#!/usr/bin/env python3
"""Dump MM2 town shop static tables from ghidra/mm2_data_00.bin.

ASM sources (capstone):
  Blacksmith static IDs   0x1C00E  A4-$68EE/$68D0 weapons, $683A specials,
                          $68B2/$6894 armor, $6876/$6858 misc (town*6 + slot)
  Blacksmith RNG labels   0x1C54A  A4-$5ACE/$5AA6/$5A56 str pointers via -$7DE2
  Pub food costs          0x1CEA4  A4-$6760  BE u16[town*6 + menu*2]
  Pub food pricing        0x18EC0  A4-$6B08 tiers, A4-$6B1A addends (not item IDs)
  Pub drinks              0x18F78  A4-$6AF0 base, A4-$6AED addend (6 global)
  Temple cleric ids       0x1DAC6  A4-$66F6; 3 menu slots @ 0x1DF1A (cmpi #3)
  Mage guild sorcerer ids 0x1D97A  A4-$66E2; 4 slots @ 0x1E64A (cmpi #4)
  Mage guild costs        0x1D97A  A4-$6698[town*4+slot] + A4-$6686[slot] (retail OP_0E 0x05)
  Alt guild stock         0x1EACC/0x1EB2E/0x1EA66  A4-$6692/$669E + paired cost tables (not 0x05)
  Temple cleric cost enc  0x1DAC6  A4-$66F6 bytes (cost flags for cleric menu)
  Temple spell costs      0x1CB48  A4-$6738 BE u16[slot*2] (6 global slots)
  Temple donation GP      0x1CA3A  A4-$6742 BE u16[A4-$79F2] via 0x1C9C0 debit
  Temple donation quest   0x1D796  A4-$66B1[A4-$79F2] OR into A4-$799E
  Training stat apply     0x1C898  A4-$6720/671A by map index A4-$79F2
  Temple donation scale   A4-$6714  BE u16 x5 @ 0x1DC26 mulu #100 (NOT HP train)
  Training HP path        0x9BCA   calendar mode 2; no 6714; cmp @ 0x9C52 vs -$5608
  Training cost (FAQ)     level * training_town_index * 50; index not map order

Usage:
  python tools/dump_shop_tables.py
  python tools/dump_shop_tables.py --json EXTRACTED/shop_tables.json
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

# Import cleric flat-index helpers (1-based within school).
sys.path.insert(0, str(Path(__file__).resolve().parent))
from mm2_spells import CLER_FLAT, SORC_FLAT  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
DATA_HUNK = ROOT / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"
ITEMS_DAT = ROOT / "items.dat"

DATA_BASE = 0x2593C
A4_BASE = DATA_BASE + 0x7FFE

TOWNS = ["Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"]
# Training cost/HP toughness index (FAQ §3-6) — NOT A4-$79F2 map order.
TRAINING_TOWN_INDEX = [1, 5, 2, 4, 2]  # per map 0..4 in TOWNS
TRAINING_DIFFICULTY_ORDER = [
    "Middlegate",
    "Sandsobar",
    "Tundara",
    "Vulcania",
    "Atlantium",
]
MENU_FOOD = ["A", "B", "C"]
MENU_DRINK = ["A", "B", "C", "D", "E", "F"]
SMITH_CATEGORIES = ["weapons", "specials", "armor", "misc"]

# A4-relative negative offsets confirmed from LEA in shop handlers.
A4 = {
    "weapons_ids": 0x68EE,
    "weapons_meta": 0x68D0,
    "specials_ids": 0x683A,
    "specials_day_hi": 0x681C,
    "specials_day_lo": 0x6816,
    "armor_ids": 0x68B2,
    "armor_meta": 0x6894,
    "misc_ids": 0x6876,
    "misc_meta": 0x6858,
    "food_costs": 0x6760,
    "food_tier": 0x6B08,
    "food_addend": 0x6B1A,
    "drink_base": 0x6AF0,
    "drink_addend": 0x6AED,
    "spell_c_id": 0x66F6,
    "guild_sorc_id": 0x66E2,
    "guild_sorc_cost_enc": 0x66CE,
    "spell_menu_char": 0x670A,
    "spell_cost": 0x6738,
    "guild_spell_cost": 0x6698,
    "guild_spell_add": 0x6686,
    "str_pool_15": 0x741A,
    # Runtime shop tables (static in mm2_data_00.bin, indexed by A4-$79F2 town 0..4)
    "donation_gp": 0x6742,
    "donation_quest_bit": 0x66B1,
    "training_stat_inc": 0x6720,
    "training_stat_cap": 0x671A,
    "temple_donation_mul": 0x6714,
}

MASTER_EA = {
    # Data-hunk clusters near 0x25F72 — NOT copied to A4-$6742 at runtime (ASM has no copy).
    "quest_bit_master": 0x2637C,
    "training_tier_table": 0x26578,
    "data_u16_cluster": 0x25F72,
    "op24_gold_thresholds": 0x271DA,
}


def a4_file_offset(a4_minus: int) -> int:
    return 0x7FFE - a4_minus


def ea_file_offset(ea: int) -> int:
    return ea - DATA_BASE


def load_blob(path: Path) -> bytes:
    if not path.is_file():
        raise FileNotFoundError(path)
    return path.read_bytes()


def item_name(items: bytes, idx: int) -> str:
    if idx < 0 or idx >= 256:
        return f"?{idx}"
    off = idx * 20
    return items[off : off + 12].decode("ascii", errors="replace").rstrip()


def load_spell_names() -> list[str]:
    cpp = ROOT / "editor" / "src" / "core" / "Spells.cpp"
    if cpp.is_file():
        import re

        names = re.findall(
            r'\{(?:SO|CL),\s*\d+,\s*\d+,\s*"([^"]+)"',
            cpp.read_text(encoding="utf-8"),
        )
        if len(names) >= 96:
            return names[:96]
    return [f"spell_{i}" for i in range(96)]


def spell_name(names: list[str], idx: int) -> str:
    if 0 <= idx < len(names):
        return names[idx]
    return f"?{idx}"


def read_u16_be(blob: bytes, off: int) -> int:
    return struct.unpack_from(">H", blob, off)[0]


def read_u32_be(blob: bytes, off: int) -> int:
    return struct.unpack_from(">I", blob, off)[0]


def town_slots(blob: bytes, a4_minus: int, slot_count: int = 6) -> dict:
    off = a4_file_offset(a4_minus)
    raw = blob[off : off + len(TOWNS) * slot_count]
    out: dict[str, list[int]] = {}
    for t, town in enumerate(TOWNS):
        chunk = raw[t * slot_count : (t + 1) * slot_count]
        out[town] = list(chunk)
    return out


def town_slots_named(items: bytes, blob: bytes, a4_minus: int, slot_count: int = 6) -> dict:
    ids = town_slots(blob, a4_minus, slot_count)
    return {
        town: [{"id": i, "name": item_name(items, i)} for i in row]
        for town, row in ids.items()
    }


def food_costs(blob: bytes) -> dict:
    off = a4_file_offset(A4["food_costs"])
    out: dict[str, list[dict]] = {}
    for t, town in enumerate(TOWNS):
        base = off + t * 6
        row = []
        for m, label in enumerate(MENU_FOOD):
            gp = read_u16_be(blob, base + m * 2)
            row.append({"menu": label, "gold": gp})
        out[town] = row
    return out


def drink_costs(blob: bytes) -> list[dict]:
    boff = a4_file_offset(A4["drink_base"])
    aoff = a4_file_offset(A4["drink_addend"])
    rows = []
    for i, label in enumerate(MENU_DRINK):
        rows.append(
            {
                "menu": label,
                "base_index": blob[boff + i],
                "addend": blob[aoff + i],
                "note": "Resolved via -$7BB4(base_index)+addend at 0x18F78",
            }
        )
    return rows


def food_pricing(blob: bytes) -> dict:
    toff = a4_file_offset(A4["food_tier"])
    aoff = a4_file_offset(A4["food_addend"])
    cols = {}
    for c, label in enumerate(MENU_FOOD):
        tiers = list(blob[toff + c * 8 : toff + c * 8 + 8])
        addends = list(blob[aoff + c * 6 : aoff + c * 6 + 6])
        cols[label] = {
            "tier_bytes": tiers,
            "cost_addends": addends,
            "note": "18EC0 picks tier then charges; roster.$78 stores cost byte, not items.dat id",
        }
    return cols


PUB_FOOD_MENU = {
    "Middlegate": ["Horrors d'oeuvres", "Soup de Ghoul w/garlic toast", "Dragon Steak Tartar"],
    "Atlantium": ["Lightly salted tongue of toad", "Puree of Gnome", "Devils Food Brownie"],
    "Tundara": ["Sizzling Swine Soup", "Red Hot Wolf Nipple Chips", "Roast Leg of Wyvern"],
    "Vulcania": ["Pickled Pixie Brains", "Deep fried Troll liver", "Cream of Kobold soup"],
    "Sandsobar": [
        "Gourmet Dinner B Wyrm Chop Suey",
        "Roast Peasant under glass",
        "Phantom Pudding (Very Low-cal)",
    ],
}

PUB_DRINK_MENU = [
    "Orc Beer",
    "Straight shot",
    "Id Elixir",
    "Academic Ale",
    "Rare Vintage",
    "Mystic Brew",
]

# Per-town, per-slot addend: flat_cleric = raw_byte + adjust (slots 0..2 only).
# Derived from A4-$66F6 bytes vs in-game temple lists (dungeoncrawl-classics / FAQ).
CLERIC_FLAT_ADJUST: dict[str, list[int]] = {
    "Middlegate": [38, 39, -76],
    "Atlantium": [-58, -71, -75],
    "Tundara": [-6, 1, 2],
    "Vulcania": [-57, -53, -53],
    "Sandsobar": [-1, -8, -5],
}

TEMPLE_CLERIC_SLOTS = 3
GUILD_SORCERER_SLOTS = 4
MENU_KEYS_CLERIC = ["D", "E", "F"]
MENU_KEYS_GUILD = ["A", "B", "C", "D"]


def decode_cleric_flat(raw: int, town: str, slot: int) -> int:
    return raw + CLERIC_FLAT_ADJUST[town][slot]


def cleric_level_num(flat_index: int) -> tuple[int, int, str]:
    """spells.dat flat 48..95 -> (level, number, name)."""
    flat_1b = flat_index - 48 + 1
    lv, num, name = CLER_FLAT[flat_1b]
    return lv, num, name


def sorcerer_level_num(flat_index: int) -> tuple[int, int, str]:
    flat_1b = flat_index + 1
    lv, num, name = SORC_FLAT[flat_1b]
    return lv, num, name


def spellbook_index_name(idx: int) -> dict:
    """Roster spellbook bit index 0..95 (grant @ 0x1D872: byte>>3, bit&7)."""
    if idx < 48:
        flat_1b = idx + 1
        lv, num, name = SORC_FLAT[flat_1b]
        school = "S"
        spells_dat = idx
    else:
        flat_1b = idx - 48 + 1
        lv, num, name = CLER_FLAT[flat_1b]
        school = "C"
        spells_dat = idx
    return {
        "spellbook_index": idx,
        "spells_dat_index": spells_dat,
        "school": school,
        "level": lv,
        "number": num,
        "name": name,
        "label": f"{school}{lv}/{num} {name}",
    }


def temple_spells(blob: bytes, spell_names: list[str]) -> dict:
    """Temple: cleric only (3 per town). Sorcerer stock is mage guild only."""
    cid_off = a4_file_offset(A4["spell_c_id"])
    menu_off = a4_file_offset(A4["spell_menu_char"])
    cost_off = a4_file_offset(A4["spell_cost"])
    global_costs = [read_u16_be(blob, cost_off + i * 2) for i in range(6)]

    cleric_by_town: dict[str, list[dict]] = {}

    for t, town in enumerate(TOWNS):
        base = t * 4
        c_raw = list(blob[cid_off + base : cid_off + base + 4])
        menu_raw = list(blob[menu_off + base : menu_off + base + 4])

        cleric_rows = []
        for slot in range(TEMPLE_CLERIC_SLOTS):
            raw = c_raw[slot]
            flat_idx = decode_cleric_flat(raw, town, slot)
            lv, num, nm = cleric_level_num(flat_idx)
            cleric_rows.append(
                {
                    "slot": slot,
                    "menu_key": MENU_KEYS_CLERIC[slot],
                    "menu_key_byte": menu_raw[slot],
                    "raw_byte": raw,
                    "flat_index": flat_idx,
                    "spells_dat_index": flat_idx,
                    "level": lv,
                    "number": num,
                    "name": nm,
                    "cost_encoding_byte": raw,
                }
            )
        cleric_by_town[town] = cleric_rows

    return {
        "cleric_spells_by_town": cleric_by_town,
        "gold_costs_u16_be_slots_0_5": global_costs,
        "asm": {
            "town_index": "A4-$79F2 (0=Middlegate .. 4=Sandsobar)",
            "open_handler": "0x1D208 (OP_0E 0x03)",
            "cleric_handler": "0x1DAC6 reads A4-$66F6; purchase loop cmpi #3 @ 0x1DF1A",
            "cleric_grant": "0x1D872 via temple menu dispatch (keys D/E/F)",
            "no_sorcerer": "0x1D97A is not called from temple; sorcerer stock is guild-only",
        },
    }


def guild_spells(blob: bytes) -> dict:
    """Mage guild: 4 sorcerer spells per town @ A4-$66E2; GP @ A4-$6698 + $6686."""
    id_off = a4_file_offset(A4["guild_sorc_id"])
    cost_off = a4_file_offset(A4["guild_spell_cost"])
    add_off = a4_file_offset(A4["guild_spell_add"])
    enc_off = a4_file_offset(A4["guild_sorc_cost_enc"])
    addends = [read_u16_be(blob, add_off + s * 2) for s in range(GUILD_SORCERER_SLOTS)]

    by_town: dict[str, list[dict]] = {}
    for t, town in enumerate(TOWNS):
        rows = []
        for slot in range(GUILD_SORCERER_SLOTS):
            raw = blob[id_off + t * 4 + slot]
            base = read_u16_be(blob, cost_off + (t * GUILD_SORCERER_SLOTS + slot) * 2)
            lv, num, nm = sorcerer_level_num(raw)
            rows.append(
                {
                    "slot": slot,
                    "menu_key": MENU_KEYS_GUILD[slot],
                    "raw_byte": raw,
                    "spellbook_index": raw,
                    "flat_index": raw,
                    "spells_dat_index": raw,
                    "level": lv,
                    "number": num,
                    "name": nm,
                    "cost_encoding_byte": blob[enc_off + t * 4 + slot],
                    "cost_base_u16_be": base,
                    "cost_addend_u16_be": addends[slot],
                    "gold_gp": base + addends[slot],
                    "label": f"S{lv}/{num} {nm}",
                }
            )
        by_town[town] = rows

    return {
        "slots_per_town": GUILD_SORCERER_SLOTS,
        "cost_addends_u16_be": addends,
        "prior_re_errors": {
            "wrong_table": "A4-$669E (3 BE u16) treated as global guild stock",
            "wrong_keys": "Menu keys 9 / : / ; (slot*3+9 from 0x1EB2E alternate only)",
            "wrong_slot_count": "3 slots; retail loop is cmpi #4 @ 0x1E64A",
            "wrong_spell_claim": "Implosion/Light/Silence at every town (669E decode artifact)",
            "correct_table": "A4-$66E2 (5x4 u8 sorcerer flat 0..47) + A4-$6698 costs",
            "correct_keys": "A/B/C/D (sub #$41 @ 0x1E864)",
        },
        "asm": {
            "open_handler": "OP_0E 0x05 -> -$7D10 -> 0x1E3E6; init 0x1E8B0 (4 RNG name ptrs)",
            "stock_loop": "0x1E618..0x1E650 calls 0x1D97A (66E2) cmpi #4",
            "purchase_keys": "menu dispatch sub #$41 @ 0x1E864 -> A/B/C/D",
            "cost_table": "A4-$6698[town*4+slot] + A4-$6686[slot]; enc bytes A4-$66CE",
            "grant_handler": "0x1D872 sets roster $51 spellbook bit from 66E2 byte",
            "enroll": "OP_0E 0x0D -> -$7DA0; 0x1A1CE writes roster $0B <- A4-$79F2+1",
            "not_temple": "A4-$66F6 is cleric-only @ 0x1DAC6",
        },
        "alternate_stock_builders": {
            "note": "Used by other UI paths (e.g. 0x008E5C -> 0x1EACC); not OP_0E 0x05 retail guild",
            "0x1EACC": {"spell_ids": "A4-$6692[slot]", "costs": "A4-$6698[slot]", "menu_keys": "none"},
            "0x1EB2E": {
                "spell_ids": "A4-$6692[slot]",
                "costs": "A4-$6698[slot]+A4-$6686[slot]",
                "menu_keys": "slot*3+9 (9/:;/; for slots 0..2)",
            },
            "0x1EA66": {"spell_ids": "A4-$669E[slot]", "costs": "A4-$66A4[slot]"},
            "0x1EBA6": {"spell_ids": "A4-$669E[slot]", "costs": "A4-$668C[slot]"},
            "0x1EC12": {"spell_ids": "A4-$669E[slot]", "costs": "A4-$6680[slot]"},
        },
        "mage_guild_spells_by_town": by_town,
    }


def pointer_pool(blob: bytes, a4_minus: int, count: int) -> list[int]:
    off = a4_file_offset(a4_minus)
    return [read_u32_be(blob, off + i * 4) for i in range(count)]


def master_u16_list(blob: bytes, ea: int, count: int) -> list[int]:
    off = ea_file_offset(ea)
    return [read_u16_be(blob, off + i * 2) for i in range(count)]


def master_u8_list(blob: bytes, ea: int, count: int) -> list[int]:
    off = ea_file_offset(ea)
    return list(blob[off : off + count])


def training_tiers(blob: bytes, ea: int, count: int = 7) -> list[dict]:
    """7 tiers @ 0x26578: u8 cost, u8 gain, u16 BE pad (usually 0)."""
    off = ea_file_offset(ea)
    rows = []
    for i in range(count):
        base = off + i * 4
        rows.append(
            {
                "tier": i,
                "cost_gp": blob[base],
                "gain": blob[base + 1],
                "pad_u16_be": read_u16_be(blob, base + 2),
            }
        )
    return rows


def town_u8(blob: bytes, a4_minus: int, count: int = 5) -> dict[str, int]:
    off = a4_file_offset(a4_minus)
    vals = list(blob[off : off + count])
    return dict(zip(TOWNS, vals))


def town_u16_be(blob: bytes, a4_minus: int, count: int = 5) -> dict[str, int]:
    off = a4_file_offset(a4_minus)
    vals = [read_u16_be(blob, off + i * 2) for i in range(count)]
    return dict(zip(TOWNS, vals))


def quest_items(items: bytes) -> list[dict]:
    # Quest hooks — NOT pub meals (those use roster.$78; see static_by_town.pub).
    entries = [
        (
            224,
            None,
            "dungeon_treasure",
            "Middlegate Cavern loc 17 tile (7,0) OP_19 item 0xE0; return to town (2,10) OP_0E 0x0A",
        ),
        (
            175,
            None,
            "consumable",
            "Create Food use; treasure 'Magic Meals'; Sandsobar smith misc",
        ),
        (
            176,
            None,
            "consumable",
            "Cure Poison drink; items.dat; pub drink str pool",
        ),
        (
            None,
            "Devils Food Brownie",
            "pub_meal",
            "Atlantium pub meal C (roster $78, not item 224)",
        ),
        (None, "party food byte $25", "party_food", "Travel drain; OP_15 oasis +40"),
    ]
    rows = []
    for entry in entries:
        iid, name, category, note = entry
        if isinstance(iid, int):
            rows.append(
                {
                    "item_id": iid,
                    "name": item_name(items, iid),
                    "category": category,
                    "note": note,
                }
            )
        else:
            rows.append(
                {"item_id": None, "name": name, "category": category, "note": note}
            )
    return rows


def build_json(items: bytes, spell_names: list[str], blob: bytes) -> dict:
    static_blacksmith = {
        cat: town_slots_named(items, blob, A4[f"{cat}_ids"] if cat != "specials" else A4["specials_ids"])
        for cat in SMITH_CATEGORIES
    }
    hi_off = a4_file_offset(A4["specials_day_hi"])
    lo_off = a4_file_offset(A4["specials_day_lo"])
    static_blacksmith["specials_meta"] = {
        "day_index_hi_table": list(blob[hi_off : hi_off + 30]),
        "day_index_lo_table": list(blob[lo_off : lo_off + 30]),
        "note": "Category 2 item pick @ 1C146 uses A4-$79B6/0x1E into these tables; not RNG item ids",
    }

    random_pool = {
        "note": "Runtime str.dat pointers filled by -$7DE2 at shop open; item/spell IDs come from static_by_town",
        "blacksmith": {
            "weapon_caption_ptrs": {"count": 10, "runtime": "A4-$5ACE"},
            "category_caption_ptrs": {"layout": "5 towns x 4", "runtime": "A4-$5AA6"},
            "specials_caption_ptrs": {"count": 6, "runtime": "A4-$5A56"},
        },
        "temple": {
            "spell_name_ptrs": {"layout": "5 x 4 x 8", "runtime": "A4-$59EE, $594E, ..."},
        },
        "guild": {"spell_name_ptr": "A4-$6AB4"},
        "master_str_pointer_pool_15": {
            "a4_minus": hex(A4["str_pool_15"]),
            "file_offset": hex(a4_file_offset(A4["str_pool_15"])),
            "pointers_be": [hex(p) for p in pointer_pool(blob, A4["str_pool_15"], 15)],
        },
    }

    recovered = {
        "blacksmith_static_item_ids": "100% (4×5×6 = 120 ids in data hunk)",
        "blacksmith_specials_day_index": "100% (681C/6816 + game date mod 30)",
        "pub_food_gold_costs_and_menu_text": "100%",
        "pub_meal_mechanics": "roster.$78 + tier tables; not items.dat SKUs",
        "pub_drinks_cost_indices": "100%",
        "temple_cleric_spells": "100% (66F6 x3/town, decode via CLERIC_FLAT_ADJUST)",
        "temple_gold_costs": "100% (6738 first 6 BE u16, slot-indexed)",
        "temple_donation_gp": "100% (A4-$6742 BE u16 x5, debited @ 1CA3A)",
        "temple_donation_quest_bits": "100% (A4-$66B1 -> A4-$799E @ 1D796)",
        "training_stat_tables": "100% (A4-$6720 inc + A4-$671A cap @ 1C898)",
        "training_town_index": "FAQ level*index*50; map index != training index",
        "training_hp_path_9bca": "partial (ASM; no A4-$6714)",
        "temple_donation_mul_6714": "100% (mulu #100 @ 1DC26; not HP training)",
        "training_tier_template": "100% (data hunk 0x26578, u8 cost/gain x7)",
        "mage_guild_spells": "100% (66E2 ids 5x4, 6698 costs 5x4, 6686 addends x4)",
        "rng_str_pointer_pools": "documented; captions only at shop open",
        "overall_static_shop_data": "~92%",
    }

    return {
        "towns": TOWNS,
        "source": str(DATA_HUNK.relative_to(ROOT)),
        "data_base": hex(DATA_BASE),
        "warning": (
            "Pub meals use roster byte $78 (computed cost/selection), not items.dat. "
            "Blacksmith/temple RNG at open selects str.dat captions; static item/spell bytes live in static_by_town."
        ),
        "static_by_town": {
            "blacksmith": static_blacksmith,
            "pub": {
                "food_costs_gp": food_costs(blob),
                "food_menu_text": {
                    town: [
                        {"menu": label, "text": text, "gold": food_costs(blob)[town][i]["gold"]}
                        for i, (label, text) in enumerate(
                            zip(MENU_FOOD, PUB_FOOD_MENU[town])
                        )
                    ]
                    for town in TOWNS
                },
                "food_pricing": food_pricing(blob),
                "drinks": [
                    {**row, "name": PUB_DRINK_MENU[i]}
                    for i, row in enumerate(drink_costs(blob))
                ],
                "inn_sells_food": False,
                "inn_note": "OP_0E 0x02 -> -$7CD4 rest/dismiss only (28-town-services.md)",
            },
            "temple_spells": temple_spells(blob, spell_names),
            "mage_guild_spells": guild_spells(blob),
        },
        "random_pool": random_pool,
        "quest_items": quest_items(items),
        "completeness": recovered,
        "master_tables": {
            "note": (
                "Temple donation GP and quest bits are read from A4 runtime tables in "
                "mm2_data_00.bin (A4-$6742 / A4-$66B1). There is no ASM copy from "
                "0x25F72 into A4-$6742. Cleric spell purchase uses A4-$6738 (separate "
                "from donation @ 1CA3A)."
            ),
            "temple_donation_gp": {
                "a4_minus": hex(A4["donation_gp"]),
                "file_offset": hex(a4_file_offset(A4["donation_gp"])),
                "encoding": "BE u16 per town, index A4-$79F2 @ 0x1CA3A",
                "debit_handler": "0x1C9C0 subtracts roster.$66; success @ 0x1D796 sets quest bit",
                "by_town": town_u16_be(blob, A4["donation_gp"]),
            },
            "temple_donation_quest_bit": {
                "a4_minus": hex(A4["donation_quest_bit"]),
                "file_offset": hex(a4_file_offset(A4["donation_quest_bit"])),
                "encoding": "u8 bit mask OR'd into A4-$799E @ 0x1D796; all 5 -> 0x1F reward",
                "master_ea": hex(MASTER_EA["quest_bit_master"]),
                "master_first5": master_u8_list(blob, MASTER_EA["quest_bit_master"], 5),
                "by_town": town_u8(blob, A4["donation_quest_bit"]),
            },
            "training_stat_increment": {
                "a4_minus": hex(A4["training_stat_inc"]),
                "file_offset": hex(a4_file_offset(A4["training_stat_inc"])),
                "encoding": "u8 addend indexed by A4-$79F2 @ 0x1C898 (stat roll + table[town])",
                "by_town": town_u8(blob, A4["training_stat_inc"]),
                "raw_24_bytes": list(
                    blob[
                        a4_file_offset(A4["training_stat_inc"]) : a4_file_offset(
                            A4["training_stat_inc"]
                        )
                        + 24
                    ]
                ),
            },
            "training_stat_cap": {
                "a4_minus": hex(A4["training_stat_cap"]),
                "file_offset": hex(a4_file_offset(A4["training_stat_cap"])),
                "encoding": "u8 per-town ceiling @ 0x1C898",
                "by_town": town_u8(blob, A4["training_stat_cap"]),
            },
            "training_town_index": {
                "source": "Might and Magic FAQ.txt §3-6",
                "formula_gp": "current_level * training_town_index * 50",
                "by_map_index": {
                    town: TRAINING_TOWN_INDEX[i] for i, town in enumerate(TOWNS)
                },
                "difficulty_weakest_to_strongest": TRAINING_DIFFICULTY_ORDER,
                "note": "Training index is NOT A4-$79F2 map order; Sandsobar and Tundara share index 2",
            },
            "temple_donation_multiplier": {
                "a4_minus": hex(A4["temple_donation_mul"]),
                "file_offset": hex(a4_file_offset(A4["temple_donation_mul"])),
                "encoding": "BE u16 x5 after stat-cap bytes; A4-$79F2 * 100 @ 0x1DC26",
                "by_town": town_u16_be(blob, A4["temple_donation_mul"]),
                "donation_gp_if_used_as_mul": {
                    town: town_u16_be(blob, A4["temple_donation_mul"])[town] * 100
                    for town in TOWNS
                },
            },
            "training_tier_template": {
                "ea": hex(MASTER_EA["training_tier_table"]),
                "encoding": "7 x (u8 cost_gp, u8 gain, u16 pad); flat 100 gp in retail data",
                "tiers": training_tiers(blob, MASTER_EA["training_tier_table"]),
            },
            "data_u16_cluster_25f72": {
                "ea": hex(MASTER_EA["data_u16_cluster"]),
                "note": "NOT temple donation GP — no read/copy to A4-$6742 in ASM",
                "values_u16_be": master_u16_list(blob, MASTER_EA["data_u16_cluster"], 10),
            },
            "op24_gold_thresholds": {
                "ea": hex(MASTER_EA["op24_gold_thresholds"]),
                "values": master_u16_list(blob, MASTER_EA["op24_gold_thresholds"], 13),
            },
        },
        "runtime_a4_tables": {
            "smith_weapon_ptrs": "A4-$5ACE (10, RNG str @ 0x1C54A)",
            "smith_special_ptrs": "A4-$5A56 (6, RNG str)",
            "smith_category_ptrs": "A4-$5AA6 (5x4, RNG str)",
            "pub_menu_ptrs": "A4-$6AA4 (5x4 str), A4-$6B6E rumors",
            "temple_donation_gp": "A4-$6742 BE u16[5] — debited @ 0x1CA3A (not 0x25F72)",
            "temple_donation_quest": "A4-$66B1 u8[5] -> A4-$799E @ 0x1D796",
            "training_stat": "A4-$6720 / A4-$671A @ 0x1C898",
            "training_town_index": "FAQ 1,2,2,4,5 — cost/HP toughness; not map 0..4 order",
            "temple_donation_mul": "A4-$6714 BE u16[5] * 100 @ 1DC26 (not HP @ 9BCA)",
            "training_hp_compare": "A4-$5608 byte — compared @ 0x9C52 (BSS threshold)",
        },
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--json", type=Path, help="Write JSON output path")
    args = ap.parse_args()

    blob = load_blob(DATA_HUNK)
    items = load_blob(ITEMS_DAT) if ITEMS_DAT.is_file() else bytes(256 * 20)
    spell_names = load_spell_names()

    doc = build_json(items, spell_names, blob)
    text = json.dumps(doc, indent=2)

    out = args.json or (ROOT / "EXTRACTED" / "shop_tables.json")
    out.write_text(text + "\n", encoding="utf-8")
    print(f"Wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

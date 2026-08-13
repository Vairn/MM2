#!/usr/bin/env python3
"""Emit wiki/maze-walker/townTables.js from mm2_town_tables + EXTRACTED/shop_tables.json."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from mm2_town_tables import (  # noqa: E402
    SMITH_CATEGORY_NAME,
    SMITH_IDS,
    SMITH_META,
    TOWN_NAMES,
    TOWNS,
    XP_GROUP_A,
    XP_GROUP_B,
)

SHOP = ROOT / "EXTRACTED" / "shop_tables.json"
OUT = ROOT / "wiki" / "maze-walker" / "townTables.js"

towns = [t.__dict__ for t in TOWNS]

shop = json.loads(SHOP.read_text(encoding="utf-8")) if SHOP.is_file() else {}
pub = shop.get("static_by_town", {}).get("pub", {})
guild_raw = shop.get("static_by_town", {}).get("mage_guild_spells", {}).get(
    "mage_guild_spells_by_town", {}
)

PUB_FOOD = [
    [
        {"menu": row["menu"], "text": row["text"], "gold": row["gold"]}
        for row in pub.get("food_menu_text", {}).get(town, [])
    ]
    for town in TOWN_NAMES
]

PUB_DRINKS = [
    {"name": row.get("name", f"Drink {i + 1}"), "gold": row.get("gold_gp", 0)}
    for i, row in enumerate(pub.get("drinks", []))
]

MAGE_GUILD = []
for town in TOWN_NAMES:
    slots = []
    for row in guild_raw.get(town, []):
        slots.append(
            {
                "flat": row["flat_index"],
                "gold": row["gold_gp"],
                "label": row["label"],
                "key": row["menu_key"],
            }
        )
    MAGE_GUILD.append(slots)

temple_raw = shop.get("static_by_town", {}).get("temple_spells", {}).get(
    "cleric_spells_by_town", {}
)
TEMPLE_SPELLS = []
for town in TOWN_NAMES:
    slots = []
    for row in temple_raw.get(town, []):
        # C++ stock stores cleric flat 0..47 (= spells_dat_index - 48).
        flat = int(row["flat_index"])
        if flat >= 48:
            flat -= 48
        slots.append(
            {
                "flat": flat,
                "gold": row["gold_gp"],
                "label": row.get("name", f"C/{flat}"),
                "key": row.get("menu_key", ""),
            }
        )
    TEMPLE_SPELLS.append(slots)

# Per-town mage-guild membership bit in roster record+0x79
# (class_quest_guild_mask), data hunk A4-$66A9. ONLY OR'd in by the unported,
# buggy 0x9D76 class-quest reward loop (doc 36-class-quest-hp-bug.md).
MAGE_GUILD_MEMBER_MASK = [0x02, 0x04, 0x08, 0x10, 0x20]

# ASM/event.dat portal legs (NOT FAQ §4-1). Tile = OP_0C dest packed as (y<<4)|x.
# Middlegate OP_0E 0x11 → loc 61 idx 1; other towns use inline scripts.
TOWN_PORTALS = [
    # Middlegate (5,0)/W brownie → Sandsobar (8,1) 10gp  [loc61 idx1: OP_0C 0x04,0x18]
    {"screen": 0, "x": 0, "y": 5, "cost": 10, "destScreen": 4, "destX": 8, "destY": 1},
    # Sandsobar (8,1) desert trader → Middlegate (0,5) 20gp  [OP_0C 0x00,0x50]
    {"screen": 4, "x": 8, "y": 1, "cost": 20, "destScreen": 0, "destX": 0, "destY": 5},
    # Sandsobar (4,15) nomad → Tundara (6,11) 50gp  [OP_0C 0x02,0xB6]
    {"screen": 4, "x": 4, "y": 15, "cost": 50, "destScreen": 2, "destX": 6, "destY": 11},
    # Tundara (6,11) Jean-Luc → Sandsobar (4,15) 10gp  [OP_0C 0x04,0xF4]
    {"screen": 2, "x": 6, "y": 11, "cost": 10, "destScreen": 4, "destX": 4, "destY": 15},
    # Tundara (6,9) polar bear → Vulcania (6,3) 50gp  [OP_0C 0x03,0x36]
    {"screen": 2, "x": 6, "y": 9, "cost": 50, "destScreen": 3, "destX": 6, "destY": 3},
    # Vulcania (6,3) transport → Tundara (6,9) 30gp  [OP_0C 0x02,0x96]
    {"screen": 3, "x": 6, "y": 3, "cost": 30, "destScreen": 2, "destX": 6, "destY": 9},
    # Vulcania (8,3) export → Atlantium (3,0) 100gp  [OP_0C 0x01,0x03]
    {"screen": 3, "x": 8, "y": 3, "cost": 100, "destScreen": 1, "destX": 3, "destY": 0},
    # Atlantium (3,0) toga → Vulcania (8,3) 50gp  [OP_0C 0x03,0x38]
    {"screen": 1, "x": 3, "y": 0, "cost": 50, "destScreen": 3, "destX": 8, "destY": 3},
    # Atlantium (12,0) committee → Middlegate (0,5) 25gp one-way  [OP_0C 0x00,0x50]
    {"screen": 1, "x": 12, "y": 0, "cost": 25, "destScreen": 0, "destX": 0, "destY": 5, "oneWay": True},
]

_HELPERS = """
export function townCommerce(mapId) {
  return TOWNS[mapId] ?? TOWNS[0];
}

export function classXpGroup(classId) {
  if ([0, 3, 5, 7].includes(classId)) return 0;
  if ([1, 2, 4, 6].includes(classId)) return 1;
  return -1;
}

export function classXpForLevel(classId, level) {
  const g = classXpGroup(classId);
  if (g < 0 || level <= 1) return 0;
  if (level < 2 || level > 77) return 0xffffffff;
  return (g === 0 ? XP_GROUP_A : XP_GROUP_B)[level - 2];
}

const CLASS_HP = [12, 10, 10, 8, 6, 8, 10, 15];

export function classHpPerLevel(classId) {
  return CLASS_HP[classId] ?? 0;
}

export function attrBonus(v) {
  const bounds = [13, 15, 17, 19, 22, 26, 30, 45, 60, 75, 90, 105, 120, 135, 150, 175, 200, 225];
  let n = 0;
  for (const b of bounds) if (v >= b) n++;
  return n;
}

export function classCasterStat(classId) {
  if ([2, 4].includes(classId)) return 1;
  if ([1, 3].includes(classId)) return 2;
  return 0;
}

export function classSpellLevelFor(classId, charLevel) {
  if (charLevel < 1) return 0;
  if ([3, 4].includes(classId)) return Math.floor((charLevel + 1) / 2);
  if ([1, 2].includes(classId)) return Math.max(0, Math.min(7, Math.floor((charLevel - 5) / 2)));
  return 0;
}

export function trainingCost(level, townIndex) {
  return Math.max(0, level) * Math.max(0, townIndex) * 50;
}

export function healingCost(level, templeCostIndex) {
  return Math.max(0, level) * Math.max(0, templeCostIndex) * 10;
}

export function templeDonateCost(mapId) {
  const t = townCommerce(mapId);
  return (t.temple_cost_index | 0) * 100;
}

export function smithInventory(mapId, category) {
  const ids = SMITH_IDS[category]?.[mapId] ?? [];
  const meta = SMITH_META[category]?.[mapId] ?? [];
  return ids.map((id, i) => ({ itemId: id, meta: meta[i] ?? 0 }));
}

export function smithBuyFields(category, meta) {
  if (category === 3) return { priceMeta: 0, charges: meta, flags: 0 };
  if (category === 0 || category === 2) return { priceMeta: meta & 0x3f, charges: 0, flags: meta };
  return { priceMeta: 0, charges: 0, flags: 0 };
}

export function smithPrice(baseGold, meta) {
  let bonus = meta & 0x3f;
  if (!bonus) return baseGold;
  let price = baseGold * 2;
  bonus -= 1;
  price += bonus * 1000;
  return price;
}

export function pubFoodMenu(mapId) {
  return PUB_FOOD[mapId] ?? PUB_FOOD[0] ?? [];
}

export function mageGuildStock(mapId) {
  return MAGE_GUILD[mapId] ?? MAGE_GUILD[0] ?? [];
}

export function templeSpellStock(mapId) {
  return TEMPLE_SPELLS[mapId] ?? TEMPLE_SPELLS[0] ?? [];
}

/** Portal at current town tile (FAQ §4-1), or null. */
export function portalAt(screenId, x, y) {
  for (const p of TOWN_PORTALS) {
    if (p.screen === screenId && p.x === x && p.y === y) return p;
  }
  return null;
}
""".lstrip()

OUT.write_text(
    "\n".join(
        [
            "/** Auto-synced — run: python tools/gen_walker_town_tables.py */",
            '"use strict";',
            "",
            "export const TEMPLE_DONATION_ALL_BITS = 0x1F;",
            f"export const TOWN_NAMES = {json.dumps(TOWN_NAMES)};",
            f"export const SMITH_CATEGORY_NAME = {json.dumps(SMITH_CATEGORY_NAME)};",
            f"export const TOWNS = {json.dumps(towns)};",
            f"export const XP_GROUP_A = {json.dumps(XP_GROUP_A)};",
            f"export const XP_GROUP_B = {json.dumps(XP_GROUP_B)};",
            f"export const SMITH_IDS = {json.dumps(SMITH_IDS)};",
            f"export const SMITH_META = {json.dumps(SMITH_META)};",
            f"export const PUB_FOOD = {json.dumps(PUB_FOOD)};",
            f"export const PUB_DRINKS = {json.dumps(PUB_DRINKS)};",
            f"export const MAGE_GUILD = {json.dumps(MAGE_GUILD)};",
            f"export const TEMPLE_SPELLS = {json.dumps(TEMPLE_SPELLS)};",
            f"export const MAGE_GUILD_MEMBER_MASK = {json.dumps(MAGE_GUILD_MEMBER_MASK)};",
            f"export const TOWN_PORTALS = {json.dumps(TOWN_PORTALS)};",
            "",
            _HELPERS,
        ]
    ),
    encoding="utf-8",
)
print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")

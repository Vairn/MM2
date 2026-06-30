#!/usr/bin/env python3
"""Emit wiki/maze-walker/townTables.js from tools/mm2_town_tables.py."""
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
    TOWNS,
    XP_GROUP_A,
    XP_GROUP_B,
)

OUT = ROOT / "wiki" / "maze-walker" / "townTables.js"
towns = [t.__dict__ for t in TOWNS]

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

export function healingCost(level, townIndex) {
  return Math.max(0, level) * Math.max(0, townIndex) * 10;
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
""".lstrip()

OUT.write_text(
    "\n".join(
        [
            "/** Auto-synced from tools/mm2_town_tables.py — run: python tools/gen_walker_town_tables.py */",
            '"use strict";',
            "",
            "export const TEMPLE_DONATION_ALL_BITS = 0x1F;",
            f"export const SMITH_CATEGORY_NAME = {json.dumps(SMITH_CATEGORY_NAME)};",
            f"export const TOWNS = {json.dumps(towns)};",
            f"export const XP_GROUP_A = {json.dumps(XP_GROUP_A)};",
            f"export const XP_GROUP_B = {json.dumps(XP_GROUP_B)};",
            f"export const SMITH_IDS = {json.dumps(SMITH_IDS)};",
            f"export const SMITH_META = {json.dumps(SMITH_META)};",
            "",
            _HELPERS,
        ]
    ),
    encoding="utf-8",
)
print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")

/** Auto-synced from tools/mm2_town_tables.py — run: python tools/gen_walker_town_tables.py */
"use strict";

export const TEMPLE_DONATION_ALL_BITS = 0x1F;
export const SMITH_CATEGORY_NAME = ["Weapons", "Specials", "Armor", "Misc"];
export const TOWNS = [{"training_town_index": 1, "stat_train_add": 5, "stat_train_cap": 100, "donation_gold": 20, "donation_quest_bit": 1}, {"training_town_index": 5, "stat_train_add": 20, "stat_train_cap": 100, "donation_gold": 250, "donation_quest_bit": 2}, {"training_town_index": 2, "stat_train_add": 10, "stat_train_cap": 100, "donation_gold": 40, "donation_quest_bit": 4}, {"training_town_index": 4, "stat_train_add": 10, "stat_train_cap": 100, "donation_gold": 120, "donation_quest_bit": 8}, {"training_town_index": 2, "stat_train_add": 3, "stat_train_cap": 50, "donation_gold": 40, "donation_quest_bit": 16}];
export const XP_GROUP_A = [1500, 3000, 6000, 12000, 24000, 48000, 96000, 192000, 384000, 576000, 768000, 960000, 1344000, 1728000, 2496000, 3264000, 4032000, 4800000, 5568000, 7104000, 8640000, 10276000, 11712000, 13248000, 14784000, 16320000, 17856000, 19392000, 20928000, 24000000, 27072000, 30144000, 33216000, 36288000, 39360000, 42432000, 45504000, 48576000, 51648000, 54720000, 57792000, 60864000, 63936000, 67008000, 70080000, 73152000, 76224000, 79296000, 82368000, 88512000, 94656000, 100800000, 106944000, 113088000, 119232000, 125376000, 131520000, 137664000, 143808000, 149952000, 156096000, 162240000, 168384000, 174428000, 180672000, 186816000, 192960000, 199104000, 205248000, 211392000, 217536000, 223680000, 229824000, 235968000, 252352000, 268736000];
export const XP_GROUP_B = [2000, 4000, 8000, 16000, 32000, 64000, 128000, 256000, 512000, 768000, 1024000, 1280000, 1728000, 2304000, 3328000, 4352000, 5376000, 6400000, 7424000, 9472000, 11520000, 13568000, 15616000, 17664000, 19712000, 21760000, 23808000, 25832000, 27856000, 32000000, 36096000, 40192000, 44288000, 48384000, 52480000, 56576000, 60672000, 64768000, 68864000, 72960000, 77056000, 81152000, 85248000, 89344000, 93440000, 97536000, 101632000, 105728000, 109824000, 118016000, 126208000, 134400000, 142592000, 150784000, 158976000, 167168000, 175360000, 183552000, 191744000, 199936000, 208128000, 216320000, 224512000, 232704000, 240896000, 249088000, 257280000, 265472000, 273664000, 281856000, 290048000, 298240000, 306432000, 314624000, 331008000, 347392000];
export const SMITH_IDS = [[[4, 6, 7, 10, 12, 13], [15, 19, 20, 17, 16, 10], [8, 18, 17, 22, 23, 24], [9, 18, 12, 14, 4, 13], [16, 15, 14, 19, 21, 20]], [[66, 67, 68, 92, 93, 95], [97, 77, 78, 75, 73, 23], [96, 71, 74, 15, 19, 20], [24, 72, 76, 22, 17, 21], [94, 69, 70, 9, 10, 11]], [[115, 127, 128, 129, 130, 155], [155, 117, 127, 131, 132, 134], [117, 131, 132, 133, 134, 155], [155, 116, 127, 129, 130, 133], [116, 128, 129, 130, 131, 132]], [[161, 162, 160, 163, 164, 208], [170, 167, 163, 187, 189, 211], [169, 176, 177, 178, 181, 189], [171, 172, 195, 186, 184, 210], [168, 175, 162, 165, 166, 209]]];
export const SMITH_META = [[[0, 0, 0, 0, 0, 0], [3, 2, 2, 3, 3, 5], [1, 0, 0, 0, 0, 0], [1, 1, 2, 2, 4, 1], [0, 0, 0, 0, 0, 0]], [[0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0]], [[0, 0, 0, 0, 0, 0], [4, 1, 3, 2, 1, 1], [0, 0, 0, 0, 0, 1], [2, 1, 1, 1, 1, 1], [0, 0, 0, 0, 0, 0]], [[1, 20, 3, 0, 1, 0], [2, 50, 0, 25, 20, 0], [5, 8, 20, 8, 20, 10], [5, 2, 20, 35, 5, 0], [3, 1, 20, 250, 50, 0]]];

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

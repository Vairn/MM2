/** Auto-synced — run: python tools/gen_walker_town_tables.py */
"use strict";

export const TEMPLE_DONATION_ALL_BITS = 0x1F;
export const TOWN_NAMES = ["Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"];
export const SMITH_CATEGORY_NAME = ["Weapons", "Specials", "Armor", "Misc"];
export const TOWNS = [{"training_town_index": 1, "stat_train_add": 5, "stat_train_cap": 100, "temple_cost_index": 1, "feeding_frenzy_gold": 20, "donation_quest_bit": 1}, {"training_town_index": 5, "stat_train_add": 20, "stat_train_cap": 100, "temple_cost_index": 5, "feeding_frenzy_gold": 250, "donation_quest_bit": 2}, {"training_town_index": 2, "stat_train_add": 10, "stat_train_cap": 100, "temple_cost_index": 2, "feeding_frenzy_gold": 40, "donation_quest_bit": 4}, {"training_town_index": 4, "stat_train_add": 10, "stat_train_cap": 100, "temple_cost_index": 3, "feeding_frenzy_gold": 120, "donation_quest_bit": 8}, {"training_town_index": 2, "stat_train_add": 3, "stat_train_cap": 50, "temple_cost_index": 2, "feeding_frenzy_gold": 40, "donation_quest_bit": 16}];
export const XP_GROUP_A = [1500, 3000, 6000, 12000, 24000, 48000, 96000, 192000, 384000, 576000, 768000, 960000, 1344000, 1728000, 2496000, 3264000, 4032000, 4800000, 5568000, 7104000, 8640000, 10276000, 11712000, 13248000, 14784000, 16320000, 17856000, 19392000, 20928000, 24000000, 27072000, 30144000, 33216000, 36288000, 39360000, 42432000, 45504000, 48576000, 51648000, 54720000, 57792000, 60864000, 63936000, 67008000, 70080000, 73152000, 76224000, 79296000, 82368000, 88512000, 94656000, 100800000, 106944000, 113088000, 119232000, 125376000, 131520000, 137664000, 143808000, 149952000, 156096000, 162240000, 168384000, 174428000, 180672000, 186816000, 192960000, 199104000, 205248000, 211392000, 217536000, 223680000, 229824000, 235968000, 252352000, 268736000];
export const XP_GROUP_B = [2000, 4000, 8000, 16000, 32000, 64000, 128000, 256000, 512000, 768000, 1024000, 1280000, 1728000, 2304000, 3328000, 4352000, 5376000, 6400000, 7424000, 9472000, 11520000, 13568000, 15616000, 17664000, 19712000, 21760000, 23808000, 25832000, 27856000, 32000000, 36096000, 40192000, 44288000, 48384000, 52480000, 56576000, 60672000, 64768000, 68864000, 72960000, 77056000, 81152000, 85248000, 89344000, 93440000, 97536000, 101632000, 105728000, 109824000, 118016000, 126208000, 134400000, 142592000, 150784000, 158976000, 167168000, 175360000, 183552000, 191744000, 199936000, 208128000, 216320000, 224512000, 232704000, 240896000, 249088000, 257280000, 265472000, 273664000, 281856000, 290048000, 298240000, 306432000, 314624000, 331008000, 347392000];
export const SMITH_IDS = [[[4, 6, 7, 10, 12, 13], [15, 19, 20, 17, 16, 10], [8, 18, 17, 22, 23, 24], [9, 18, 12, 14, 4, 13], [16, 15, 14, 19, 21, 20]], [[66, 67, 68, 92, 93, 95], [97, 77, 78, 75, 73, 23], [96, 71, 74, 15, 19, 20], [24, 72, 76, 22, 17, 21], [94, 69, 70, 9, 10, 11]], [[115, 127, 128, 129, 130, 155], [155, 117, 127, 131, 132, 134], [117, 131, 132, 133, 134, 155], [155, 116, 127, 129, 130, 133], [116, 128, 129, 130, 131, 132]], [[161, 162, 160, 163, 164, 208], [170, 167, 163, 187, 189, 211], [169, 176, 177, 178, 181, 189], [171, 172, 195, 186, 184, 210], [168, 175, 162, 165, 166, 209]]];
export const SMITH_META = [[[0, 0, 0, 0, 0, 0], [3, 2, 2, 3, 3, 5], [1, 0, 0, 0, 0, 0], [1, 1, 2, 2, 4, 1], [0, 0, 0, 0, 0, 0]], [[0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0]], [[0, 0, 0, 0, 0, 0], [4, 1, 3, 2, 1, 1], [0, 0, 0, 0, 0, 1], [2, 1, 1, 1, 1, 1], [0, 0, 0, 0, 0, 0]], [[1, 20, 3, 0, 1, 0], [2, 50, 0, 25, 20, 0], [5, 8, 20, 8, 20, 10], [5, 2, 20, 35, 5, 0], [3, 1, 20, 250, 50, 0]]];
export const PUB_FOOD = [[{"menu": "A", "text": "Horrors d'oeuvres", "gold": 10}, {"menu": "B", "text": "Soup de Ghoul w/garlic toast", "gold": 50}, {"menu": "C", "text": "Dragon Steak Tartar", "gold": 100}], [{"menu": "A", "text": "Lightly salted tongue of toad", "gold": 1000}, {"menu": "B", "text": "Puree of Gnome", "gold": 2000}, {"menu": "C", "text": "Devils Food Brownie", "gold": 3000}], [{"menu": "A", "text": "Sizzling Swine Soup", "gold": 200}, {"menu": "B", "text": "Red Hot Wolf Nipple Chips", "gold": 100}, {"menu": "C", "text": "Roast Leg of Wyvern", "gold": 1000}], [{"menu": "A", "text": "Pickled Pixie Brains", "gold": 5000}, {"menu": "B", "text": "Deep fried Troll liver", "gold": 500}, {"menu": "C", "text": "Cream of Kobold soup", "gold": 1000}], [{"menu": "A", "text": "Gourmet Dinner B Wyrm Chop Suey", "gold": 20}, {"menu": "B", "text": "Roast Peasant under glass", "gold": 50}, {"menu": "C", "text": "Phantom Pudding (Very Low-cal)", "gold": 250}]];
export const PUB_DRINKS = [{"name": "Orc Beer", "gold": 0}, {"name": "Straight shot", "gold": 0}, {"name": "Id Elixir", "gold": 0}, {"name": "Academic Ale", "gold": 0}, {"name": "Rare Vintage", "gold": 0}, {"name": "Mystic Brew", "gold": 0}];
export const MAGE_GUILD = [[{"flat": 0, "gold": 10, "label": "S1/1 Awaken", "key": "A"}, {"flat": 2, "gold": 1000, "label": "S1/3 Energy Blast", "key": "B"}, {"flat": 6, "gold": 50, "label": "S1/7 Sleep", "key": "C"}, {"flat": 9, "gold": 100, "label": "S2/3 Identify Monster", "key": "D"}], [{"flat": 41, "gold": 50000, "label": "S8/2 Mega Volts", "key": "A"}, {"flat": 42, "gold": 50000, "label": "S8/3 Meteor Shower", "key": "B"}, {"flat": 44, "gold": 100000, "label": "S9/1 Implosion", "key": "C"}, {"flat": 45, "gold": 100000, "label": "S9/2 Inferno", "key": "D"}], [{"flat": 21, "gold": 600, "label": "S4/2 Feeble Mind", "key": "A"}, {"flat": 22, "gold": 2000, "label": "S4/3 Fire Ball", "key": "B"}, {"flat": 26, "gold": 3000, "label": "S5/1 Disrupt", "key": "C"}, {"flat": 28, "gold": 3000, "label": "S5/3 Sand Storm", "key": "D"}], [{"flat": 31, "gold": 5000, "label": "S6/1 Disintegration", "key": "A"}, {"flat": 33, "gold": 5000, "label": "S6/3 Fantastic Freeze", "key": "B"}, {"flat": 35, "gold": 5000, "label": "S6/5 Super Shock", "key": "C"}, {"flat": 37, "gold": 25000, "label": "S7/2 Duplication", "key": "D"}], [{"flat": 13, "gold": 400, "label": "S2/7 Protection from Magic", "key": "A"}, {"flat": 14, "gold": 200, "label": "S3/1 Acid Stream", "key": "B"}, {"flat": 17, "gold": 1000, "label": "S3/4 Lightning Bolt", "key": "C"}, {"flat": 20, "gold": 500, "label": "S4/1 Cold Beam", "key": "D"}]];
export const TEMPLE_SPELLS = [[{"flat": 0, "gold": 10, "label": "Apparition", "key": "D"}, {"flat": 1, "gold": 10, "label": "Awaken", "key": "E"}, {"flat": 5, "gold": 1000, "label": "Power Cure", "key": "F"}], [{"flat": 42, "gold": 20000, "label": "Mass Distortion", "key": "D"}, {"flat": 46, "gold": 50000, "label": "Resurrection", "key": "E"}, {"flat": 47, "gold": 100000, "label": "Uncurse Item", "key": "F"}], [{"flat": 14, "gold": 400, "label": "Cold Ray", "key": "D"}, {"flat": 18, "gold": 100, "label": "Lasting Light", "key": "E"}, {"flat": 23, "gold": 500, "label": "Restore Alignment", "key": "F"}], [{"flat": 25, "gold": 2000, "label": "Holy Bonus", "key": "D"}, {"flat": 30, "gold": 3000, "label": "Remove Condition", "key": "E"}, {"flat": 37, "gold": 10000, "label": "Fiery Flail", "key": "F"}], [{"flat": 8, "gold": 250, "label": "Heroism", "key": "D"}, {"flat": 11, "gold": 300, "label": "Protection From Elements", "key": "E"}, {"flat": 13, "gold": 200, "label": "Weaken", "key": "F"}]];
export const MAGE_GUILD_MEMBER_MASK = [2, 4, 8, 16, 32];
export const TOWN_PORTALS = [{"screen": 0, "x": 0, "y": 5, "cost": 10, "destScreen": 4, "destX": 6, "destY": 1, "facing": 2}, {"screen": 4, "x": 4, "y": 14, "cost": 50, "destScreen": 2, "destX": 6, "destY": 11, "facing": 1}, {"screen": 2, "x": 6, "y": 9, "cost": 50, "destScreen": 3, "destX": 6, "destY": 3, "facing": 3}, {"screen": 3, "x": 8, "y": 3, "cost": 100, "destScreen": 1, "destX": 3, "destY": 0, "facing": 0}, {"screen": 1, "x": 12, "y": 0, "cost": 25, "destScreen": 0, "destX": 0, "destY": 5, "facing": 0, "oneWay": true}];

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

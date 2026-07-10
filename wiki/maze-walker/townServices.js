/** OP_0E town-service menus + transactions — mirrors TownServiceMenu/Transactions.cpp */
"use strict";

import { arenaGoldReward, arenaMonsterType } from "./selectorBin.js";

import {
  townCommerce,
  trainingCost,
  classXpForLevel,
  classHpPerLevel,
  attrBonus,
  classCasterStat,
  classSpellLevelFor,
  smithInventory,
  smithBuyFields,
  smithPrice,
  SMITH_CATEGORY_NAME,
  TEMPLE_DONATION_ALL_BITS,
  pubFoodMenu,
  mageGuildStock,
  templeSpellStock,
  portalAt,
  TOWN_NAMES,
  PUB_DRINKS,
  templeDonateCost,
} from "./townTables.js";

import {
  syncSessionGoldFromParty,
  getPartyMember,
  ensureParty,
  memberBackpackFreeSlot,
  memberGiveItem,
  itemNameFromManifest,
  itemGoldFromManifest,
  memberSpellKnown,
  memberGrantSpell,
  memberGuildMember,
  applyInnRegistry,
  findArenaTicket,
  consumeArenaTicket,
  ARENA_TICKET_COLOR_NAMES,
} from "./sessionState.js";

/** @param {object} member @param {number} amount */
function charGoldDeduct(member, amount) {
  const need = amount | 0;
  if (member.gold < need) return false;
  member.gold -= need;
  return true;
}

/** @param {object} member */
function restoreHp(member) {
  if (member.hpMax < member.hpAux) {
    member.hpMax = member.hpAux;
    member.hpCurrent = member.hpAux;
    return true;
  }
  return false;
}

/** @param {object} member */
export function svcRestoreCondition(member) {
  member.condition = 0;
}

/** Temple heal cost 0x1DCA2: base by condition, ×level ×A4-$6714. */
export function templeHealCost(member, mapId) {
  const town = townCommerce(mapId);
  const scale = town.temple_cost_index | 0;
  const cond = member.condition | 0;
  let base = 0;
  if (cond === 0xff) base = 1000;
  else if (cond >= 0x80) base = 100;
  else if (cond !== 0 || (member.hpMax | 0) < (member.hpAux | member.hpMax | 0)) base = 10;
  else return 0;
  let cost = base;
  const level = member.level | 0;
  if (level) cost *= level;
  return cost * scale;
}

/** Temple align cost 0x1DC3A: 0 if matched; else 100×level×$6714. */
export function templeAlignCost(member, mapId) {
  const cur = member.alignment ?? member.alignmentCurrent ?? 0;
  const baseAlign = member.alignmentBase ?? cur;
  if (cur === baseAlign) return 0;
  const town = townCommerce(mapId);
  let cost = 100;
  const level = member.level | 0;
  if (level) cost *= level;
  return cost * (town.temple_cost_index | 0);
}

export function svcRestoreAlignment(member, cost) {
  const r = { paid: false, cost: cost | 0, restored: false };
  if ((cost | 0) > 0 && !charGoldDeduct(member, cost)) {
    r.cost = 0;
    return r;
  }
  const cur = member.alignment ?? member.alignmentCurrent ?? 0;
  member.alignmentBase = cur;
  member.alignment = cur;
  r.paid = true;
  r.restored = true;
  return r;
}

/** @param {object} member @param {number} cost */
export function svcHeal(member, cost) {
  const r = { paid: false, cost: cost | 0, hpRestored: false };
  if ((cost | 0) > 0 && !charGoldDeduct(member, cost)) {
    r.cost = 0;
    return r;
  }
  r.paid = true;
  if ((cost | 0) === 0) return r;
  r.hpRestored = restoreHp(member);
  svcRestoreCondition(member);
  return r;
}

/** @param {object} session @param {object} member @param {number} mapId */
export function svcTempleDonate(session, member, mapId) {
  const town = townCommerce(mapId);
  const cost = templeDonateCost(mapId);
  const r = { paid: false, cost, allTemples: false };
  if (!charGoldDeduct(member, cost)) {
    r.cost = 0;
    return r;
  }
  r.paid = true;
  session.questDonationBits = (session.questDonationBits | town.donation_quest_bit) & 0xff;
  r.allTemples = (session.questDonationBits & TEMPLE_DONATION_ALL_BITS) === TEMPLE_DONATION_ALL_BITS;
  /* 0x1D7E8: sentinel $FE + item 0xD4 in found buffer; clr donation bits. */
  if (r.allTemples) {
    session.foundItemSentinel = 0xfe;
    session.foundItemIds = session.foundItemIds || [0, 0, 0];
    session.foundItemIds[0] = 0xd4;
    session.questDonationBits = 0;
    r.rewardQueued = true;
  }
  return r;
}

/** @param {object} member @param {number} mapId */
export function svcTrainLevelUp(member, mapId) {
  const town = townCommerce(mapId);
  const r = {
    eligible: false,
    paid: false,
    leveled: false,
    cost: 0,
    requiredXp: 0,
    oldLevel: member.level,
    newLevel: member.level,
    hpGain: 0,
    spGain: 0,
    spellLevel: member.spellLevel,
  };
  const next = member.level + 1;
  const threshold = classXpForLevel(member.classId, next);
  r.requiredXp = threshold;
  r.newLevel = next;
  if (!threshold || threshold === 0xffffffff || member.experience < threshold) {
    r.newLevel = member.level;
    return r;
  }
  r.eligible = true;
  const cost = trainingCost(member.level, town.training_town_index);
  r.cost = cost;
  if (!charGoldDeduct(member, cost)) {
    r.cost = 0;
    return r;
  }
  r.paid = true;
  member.level = next;
  /* doc-32 average + confirmed 0x9BCA RNG addend (calendar mode / HP write deferred). */
  let hpGain = classHpPerLevel(member.classId) + attrBonus(member.endurance);
  const roll = Math.floor(Math.random() * 0x6d) + 1;
  const addend = Math.floor(roll / 10) & 0xff;
  if (addend !== 5) hpGain += addend;
  if (hpGain < 1) hpGain = 1;
  member.hpAux += hpGain;
  member.hpMax = member.hpAux;
  member.hpCurrent = member.hpAux;
  r.hpGain = hpGain;
  const caster = classCasterStat(member.classId);
  if (caster) {
    const statVal = caster === 1 ? member.intelligence : member.personality;
    const spGain = attrBonus(statVal) + 3;
    member.spMax += spGain;
    member.spCurrent = member.spMax;
    r.spGain = spGain;
  }
  const newSl = classSpellLevelFor(member.classId, next);
  if (newSl > member.spellLevel) member.spellLevel = newSl;
  r.spellLevel = member.spellLevel;
  r.leveled = true;
  return r;
}

/** @param {object} member @param {number} itemId @param {number} charges @param {number} flags @param {number} price */
export function svcSmithBuy(member, itemId, charges, flags, price) {
  const r = { bought: false, reject: null, price: price | 0, slot: -1 };
  if (member.condition !== 0) {
    r.reject = "condition";
    r.price = 0;
    return r;
  }
  const slot = memberBackpackFreeSlot(member);
  if (slot < 0) {
    r.reject = "full";
    r.price = 0;
    return r;
  }
  if (!charGoldDeduct(member, price)) {
    r.reject = "gold";
    r.price = 0;
    return r;
  }
  memberGiveItem(member, itemId, charges, flags, slot);
  r.bought = true;
  r.slot = slot;
  return r;
}

function memberSummary(member) {
  return `${member.name} L${member.level}  ${member.hpCurrent}/${member.hpMax} HP  ${member.gold} gp`;
}

function partyRosterLines(session) {
  return ensureParty(session).map((m, i) => `${i + 1}) ${memberSummary(m)}`).join("\n");
}

/** @param {object} ctx @returns {Promise<number|null>} party slot 0..5 */
export async function promptSelectMember(ctx) {
  const { session, promptMenuKey, sprite } = ctx;
  const party = ensureParty(session);
  const keys = party.map((_, i) => String(i + 1)).join("") + "0";
  const text = ["Select character:", "", partyRosterLines(session), "", "1–6 — 0/Esc cancel"].join("\n");
  const key = await promptMenuKey(text, keys, sprite, 0x0e);
  if (!key || key === "0") return null;
  return parseInt(key, 10) - 1;
}

function afterTxn(ctx) {
  syncSessionGoldFromParty(ctx.session);
  ctx.onSessionChange?.(ctx.session);
}

/** @param {object} ctx */
export async function runTempleService(ctx) {
  const { screenId, session, waitForSpace, promptMenuKey, note, sprite, title } = ctx;
  const town = townCommerce(screenId);
  const spells = templeSpellStock(screenId);

  while (true) {
    const donateGp = templeDonateCost(screenId);
    const donated = (session.questDonationBits & town.donation_quest_bit) !== 0;
    const spellLines = spells.map(
      (s, i) => `${String.fromCharCode(68 + i)}) ${s.label} — ${s.gold} gp`
    );
    const text = [
      title,
      partyRosterLines(session),
      "",
      "A) Restore Cond (heal HP + clear condition)",
      "B) Restore Algn (current → base)",
      "C) Donations",
      `   ${donateGp} gp${donated ? " (already donated here)" : ""}`,
      ...spellLines,
      "",
      "A–F — 0 or Esc to leave",
    ].join("\n");

    const key = await promptMenuKey(text, "abcdef0", sprite, 0x0e);
    if (!key || key === "0") break;

    if (key >= "d" && key <= "f") {
      const spellSlot = key.charCodeAt(0) - 100;
      const slot = await promptSelectMember(ctx);
      if (slot == null) continue;
      const member = getPartyMember(session, slot);
      const sp = spells[spellSlot];
      if (!sp?.gold) {
        await waitForSpace("That spell is not for sale.", sprite, 0x0e);
        continue;
      }
      const r = svcGuildBuySpell(member, sp.flat, sp.gold);
      if (r.reject === "gold") {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
      } else if (r.bought) {
        await waitForSpace(`${member.name} learned ${sp.label}. −${r.price} gp`, sprite, 0x0e);
        note(`Temple spell ${sp.label} −${r.price} gp`);
      } else {
        await waitForSpace("Could not learn that spell.", sprite, 0x0e);
      }
      afterTxn(ctx);
      continue;
    }

    const slot = await promptSelectMember(ctx);
    if (slot == null) continue;
    const member = getPartyMember(session, slot);

    if (key === "a") {
      const healGp = templeHealCost(member, screenId);
      const r = svcHeal(member, healGp);
      if (healGp > 0 && !r.paid) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
        note(`Temple: ${member.name} heal failed (gold)`);
      } else if (healGp === 0) {
        await waitForSpace(`${member.name} needs no healing.`, sprite, 0x0e);
      } else {
        await waitForSpace(
          `${member.name} healed for ${r.cost} gp.\n${memberSummary(member)}`,
          sprite,
          0x0e
        );
        note(`Temple heal ${member.name} −${r.cost} gp`);
      }
    } else if (key === "b") {
      const alignGp = templeAlignCost(member, screenId);
      const r = svcRestoreAlignment(member, alignGp);
      if (alignGp > 0 && !r.paid) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
      } else if (!r.restored && alignGp === 0) {
        await waitForSpace(`${member.name}'s alignment is already true.`, sprite, 0x0e);
      } else {
        await waitForSpace(
          `${member.name}: alignment restored (−${r.cost} gp).\n${memberSummary(member)}`,
          sprite,
          0x0e
        );
        note(`Temple align ${member.name} −${r.cost} gp`);
      }
    } else if (key === "c") {
      if (donated) {
        await waitForSpace("You have already donated here.", sprite, 0x0e);
        continue;
      }
      const r = svcTempleDonate(session, member, screenId);
      if (!r.paid) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
        note(`Temple: ${member.name} donation failed (gold)`);
      } else {
        let msg = `${member.name} donated ${r.cost} gp.\n${memberSummary(member)}`;
        if (r.allTemples) msg += "\n(All five temples donated — quest bit 0x1F)";
        await waitForSpace(msg, sprite, 0x0e);
        note(`Temple donation ${member.name} −${r.cost} gp`);
      }
    }
    afterTxn(ctx);
  }
}

/** @param {object} ctx */
export async function runTrainingService(ctx) {
  const { screenId, session, waitForSpace, promptMenuKey, note, sprite, title } = ctx;
  const town = townCommerce(screenId);

  while (true) {
    const text = [
      title,
      partyRosterLines(session),
      "",
      "A) Level up (XP threshold + level×town×50 gp)",
      "",
      "A — 0 or Esc to leave",
    ].join("\n");

    const key = await promptMenuKey(text, "a0", sprite, 0x0e);
    if (!key || key === "0") break;

    const slot = await promptSelectMember(ctx);
    if (slot == null) continue;
    const member = getPartyMember(session, slot);
    const fee = trainingCost(member.level, town.training_town_index);
    const needXp = classXpForLevel(member.classId, member.level + 1);

    const r = svcTrainLevelUp(member, screenId);
    if (!r.eligible) {
      await waitForSpace(
        `${member.name}: not enough experience.\nNeed ${needXp} XP (have ${member.experience}).\nFee would be ${fee} gp.`,
        sprite,
        0x0e
      );
      note(`Training: ${member.name} insufficient XP`);
    } else if (!r.paid) {
      await waitForSpace(`${member.name}: not enough gold! (${fee} gp)`, sprite, 0x0e);
      note(`Training: ${member.name} insufficient gold`);
    } else {
      await waitForSpace(
        `${member.name}: L${r.oldLevel} → L${r.newLevel}!  −${r.cost} gp\n+${r.hpGain} HP` +
          (r.spGain ? `  +${r.spGain} SP` : "") +
          `\n${memberSummary(member)}`,
        sprite,
        0x0e
      );
      note(`Training ${member.name} level-up −${r.cost} gp`);
    }
    afterTxn(ctx);
  }
}

/** @param {object} ctx */
export async function runSmithService(ctx) {
  const { screenId, session, manifest, waitForSpace, promptMenuKey, note, sprite, title } = ctx;
  if (!manifest?.itemsGold?.length) {
    await waitForSpace(
      `${title}\n\n(itemsGold missing — re-run export_map_walker.py)`,
      sprite,
      0x0e
    );
    note("Smith: itemsGold not in manifest");
    return;
  }

  while (true) {
    const catText = [
      title,
      partyRosterLines(session),
      "",
      "A) Weapons  B) Today's Specials",
      "C) Armor     D) Misc",
      "(Sell / Identify deferred)",
      "",
      "A–D — 0 or Esc to leave smith",
    ].join("\n");
    const catKey = await promptMenuKey(catText, "abcd0", sprite, 0x0e);
    if (!catKey || catKey === "0") break;
    const category = { a: 0, b: 1, c: 2, d: 3 }[catKey];
    if (category == null) continue;

    const slots = smithInventory(screenId, category);
    const lines = [`${SMITH_CATEGORY_NAME[category]}`, partyRosterLines(session), ""];
    const priced = [];
    for (let i = 0; i < slots.length; i++) {
      const { itemId, meta } = slots[i];
      if (!itemId) {
        lines.push(`${i + 1}) —`);
        priced.push(null);
        continue;
      }
      const fields = smithBuyFields(category, meta);
      const base = itemGoldFromManifest(manifest, itemId);
      const price = smithPrice(base, fields.priceMeta);
      const name = itemNameFromManifest(manifest, itemId);
      const bonus =
        fields.flags && category !== 3 ? ` +${fields.flags & 0x3f}` : "";
      const chg = fields.charges ? ` (${fields.charges} ch)` : "";
      lines.push(`${i + 1}) ${name}${bonus}${chg} — ${price} gp`);
      priced.push({ itemId, price, charges: fields.charges, flags: fields.flags });
    }
    lines.push("", "1–6 buy slot — 0 back");

    const pick = await promptMenuKey(lines.join("\n"), "1234560", sprite, 0x0e);
    if (!pick || pick === "0") continue;
    const idx = parseInt(pick, 10) - 1;
    const offer = priced[idx];
    if (!offer) continue;

    const memberSlot = await promptSelectMember(ctx);
    if (memberSlot == null) continue;
    const member = getPartyMember(session, memberSlot);

    const r = svcSmithBuy(member, offer.itemId, offer.charges, offer.flags, offer.price);
    if (r.bought) {
      const name = itemNameFromManifest(manifest, offer.itemId);
      await waitForSpace(
        `${member.name} bought ${name} for ${r.price} gp.\n${memberSummary(member)}`,
        sprite,
        0x0e
      );
      note(`Smith ${member.name} buy ${name} −${r.price} gp`);
    } else if (r.reject === "gold") {
      await waitForSpace("Not enough gold!", sprite, 0x0e);
    } else if (r.reject === "full") {
      await waitForSpace("Backpack full!", sprite, 0x0e);
    } else if (r.reject === "condition") {
      await waitForSpace("Cannot buy while afflicted.", sprite, 0x0e);
    }
    afterTxn(ctx);
  }
}

/** Full HP/SP restore (inn rest leaf — no condition clear). */
export function svcInnRest(member) {
  member.hpCurrent = member.hpMax;
  if (member.spMax > 0) member.spCurrent = member.spMax;
}

/** @param {object} member @param {number} mapId @param {number} menuIdx 0..2 */
export function svcPubBuyFood(member, mapId, menuIdx) {
  const menu = pubFoodMenu(mapId);
  const meal = menu[menuIdx];
  if (!meal) return { paid: false, cost: 0 };
  const cost = meal.gold | 0;
  if (!charGoldDeduct(member, cost)) return { paid: false, cost: 0 };
  /* 0x1C8D4: OR A4-$786C mask into +$76 (temp_score_word). */
  const masks = [
    [1, 2, 4],
    [4096, 8192, 16384],
    [64, 128, 256],
    [512, 1024, 2048],
    [8, 16, 32],
  ];
  const town = mapId >= 0 && mapId < 5 ? mapId : 0;
  member.tempScoreWord = ((member.tempScoreWord | 0) | (masks[town][menuIdx] | 0)) & 0xffff;
  /* 0x18EC0 encode → +$78; +$7C bit0 clear (food). No invented meal→tile coords. */
  const tiers = [
    [56, 24, 13, 6, 3, 8, 2, 1],
    [66, 29, 6, 7, 7, 15, 2, 1],
    [37, 12, 7, 10, 2, 5, 1, 1],
  ];
  const addends = [
    [1, 66, 92, 115, 127, 155],
    [25, 79, 98, 118, 135, 157],
    [54, 85, 105, 125, 150, 159],
  ];
  const t = tiers[menuIdx];
  let enc = (Math.floor(Math.random() * t[0]) + 1) - 1;
  if (enc < 0) enc = 0;
  let i = 1;
  for (; i < 7; i++) {
    if (enc < t[i]) break;
    enc = (enc - t[i + 1]) & 0xff;
  }
  i -= 1;
  if (i < 0) i = 0;
  if (i > 5) i = 5;
  enc = (enc + addends[menuIdx][i]) & 0xff;
  member.scriptWorkFlag = enc;
  member.pubMeal = enc;
  if (member.rawBytes) {
    member.rawBytes[0x78] = enc;
    member.rawBytes[0x7c] = (member.rawBytes[0x7c] & 0xfe);
  }
  return { paid: true, cost, meal, encoding: enc };
}

/** Tavern B @ 0x1CAC4 — A–F costs A4-$6738; apply base-stat bump (0x1C7EC family). */
const STAT_BOOST = [
  { key: "might", cost: 5, label: "Might" },
  { key: "accuracy", cost: 5, label: "Accuracy" },
  { key: "personality", cost: 20, label: "Personality" },
  { key: "intelligence", cost: 20, label: "Intelligence" },
  { key: "level", cost: 50, label: "Level" },
  { key: "spellLevel", cost: 100, label: "Spell Level" },
];

export function svcTavernStatBoost(member, slot, mapId) {
  const row = STAT_BOOST[slot];
  if (!row) return { paid: false, cost: 0 };
  if ((member.condition | 0) !== 0) return { paid: false, cost: row.cost, ok: false };
  if (!charGoldDeduct(member, row.cost)) return { paid: false, cost: 0 };
  const town = townCommerce(mapId);
  const add = town.stat_train_add | 0;
  const cap = town.stat_train_cap | 0;
  if (row.key === "level" || row.key === "spellLevel") {
    member[row.key] = Math.min(255, (member[row.key] | 0) + add);
  } else {
    const baseKey = row.key;
    let v = (member[baseKey] | 0) + add;
    if (v < cap && v >= add) member[baseKey] = v;
  }
  if ((member.speed | 0) >= 2) member.speed = (member.speed | 0) - 2;
  const endHi = (member.endurance | 0) + 10;
  const sick = Math.floor(Math.random() * Math.max(1, endHi)) + 1 === 2;
  if (sick) member.condition = (member.condition | 0) | 0x08;
  return { paid: true, cost: row.cost, sick, label: row.label, ok: true };
}

/** @param {object} member @param {number} amount */
function satSubByte(member, key, amount) {
  const n = amount | 0;
  let v = member[key] | 0;
  if (v < n) v = 0;
  else v -= n;
  member[key] = v;
}

/** General store 0xA3AE jump table — saturate-subtract per skill nibble (ASM). */
function generalStoreApplySkillNibble(member, skillId) {
  switch (skillId & 0x0f) {
    case 1:
      satSubByte(member, "accuracy", 5);
      break;
    case 2:
      satSubByte(member, "speed", 5);
      break;
    case 5:
      satSubByte(member, "personality", 5);
      break;
    case 6:
      satSubByte(member, "luck", 5);
      break;
    case 7:
      satSubByte(member, "might", 5);
      break;
    case 8:
      for (const k of ["might", "intelligence", "personality", "speed", "accuracy", "luck", "endurance"]) {
        satSubByte(member, k, 1);
      }
      break;
    case 9:
      satSubByte(member, "intelligence", 5);
      break;
    case 14:
      satSubByte(member, "age", 15);
      break;
    case 15:
      satSubByte(member, "endurance", 5);
      break;
    default:
      break;
  }
}

/**
 * General store convert leaf 0xA62C: 100gp + apply both +$50 nibbles via 0xA3AE, clear pack.
 * @param {object} member
 */
export function svcGeneralStoreConvert(member) {
  const pack = (member.skillPack ?? 0) & 0xff;
  if (pack === 0) {
    return { converted: false, paid: false, message: "The secondary skills are gone." };
  }
  if (!charGoldDeduct(member, 100)) {
    return { converted: false, paid: false, message: "Sorry, you must have 100 gold." };
  }
  generalStoreApplySkillNibble(member, pack & 0x0f);
  generalStoreApplySkillNibble(member, (pack >> 4) & 0x0f);
  member.skillPack = 0;
  return { converted: true, paid: true, message: "The secondary skills are gone." };
}

/** @param {object} member @param {number} drinkIdx */
export function svcPubBuyDrink(member, drinkIdx, drinks) {
  const row = drinks[drinkIdx];
  if (!row) return { paid: false, cost: 0 };
  const cost = row.gold | 0;
  if (cost > 0 && !charGoldDeduct(member, cost)) return { paid: false, cost: 0 };
  /* ASM-family RNG(1,50)==2 → sick; FAQ bonuses when not sick (VM handlers untraced). */
  const sick = Math.floor(Math.random() * 50) + 1 === 2;
  if (sick) {
    return { paid: true, cost, sick: true, name: row.name };
  }
  const add = (k, n) => {
    member[k] = Math.min(255, ((member[k] | 0) + n) | 0);
  };
  switch (drinkIdx) {
    case 0: add("might", 5); break;
    case 1: add("accuracy", 20); break;
    case 2: add("personality", 10); break;
    case 3: add("intelligence", 10); break;
    case 4: add("level", 3); break;
    case 5: add("spellLevel", 1); break;
    default: break;
  }
  return { paid: true, cost, sick: false, name: row.name };
}

/**
 * Mage guild / temple spell-purchase leaf (0x1D872). ASM gate order: (1) slot
 * cost != 0 (0x1D882 — an unpopulated slot, not an "already known" check),
 * (2) gold check+deduct (0x1D90C), (3) grant: raw record+0x51+(n>>3) bit OR
 * keyed only on the flat index (0x1D8D4/0x1D8FA). No class-id check was found
 * in the traced grant function, so none is enforced here (mirrors
 * game/src/events/TownServiceTransactions.cpp townSvcBuySpell).
 */
export function svcGuildBuySpell(member, flatIndex, price) {
  const r = { bought: false, reject: null, price: price | 0 };
  if (!price) {
    r.reject = "notForSale";
    return r;
  }
  if (!charGoldDeduct(member, price)) {
    r.reject = "gold";
    r.price = 0;
    return r;
  }
  memberGrantSpell(member, flatIndex);
  r.bought = true;
  return r;
}

/** @param {object} ctx */
export async function runInnService(ctx) {
  /* ASM: inn OP_0E 0x01 is registry only (0x1A1B2); rest is world R @ 0x19E20. */
  const { waitForSpace, note, sprite, title } = ctx;
  await waitForSpace(
    `${title}\nSigned the registry.\n(Rest with R in exploration — 0x19E20)`,
    sprite,
    0x0e
  );
  note("Inn: registry complete (rest is world Rest key)");
}

/** @param {object} ctx */
export async function runTavernService(ctx) {
  /* OP_0E 0x03 → 0x1D208 / 0x1D650: A frenzy, B stat-boost, C specialties, D tip, E rumors.
   * Drinks are selector 0xCA (0x18F78), not key B. */
  const { screenId, session, waitForSpace, promptMenuKey, note, sprite, title } = ctx;
  const food = pubFoodMenu(screenId);

  while (true) {
    const text = [
      title,
      partyRosterLines(session),
      "",
      "A) Feeding frenzy (all you can carry)",
      "B) Buy (stat boost A–F)",
      "C) Specialties (food A–C)",
      "D) Tip the bartender",
      "E) Listen for rumors",
      "",
      "A–E — 0 or Esc to leave",
    ].join("\n");
    const key = await promptMenuKey(text, "abcde0", sprite, 0x0e);
    if (!key || key === "0") break;

    if (key === "e") {
      await waitForSpace(
        "You listen… rumors of temples, quests, and distant lands.\n(Event bytecode A4-$119A — display only in walker)",
        sprite,
        0x0e
      );
      continue;
    }

    if (key === "d") {
      await waitForSpace("Thank you -\nPlease come again\n(Tip pool A4-$58AE)", sprite, 0x0e);
      continue;
    }

    if (key === "a") {
      const slot = await promptSelectMember(ctx);
      if (slot == null) continue;
      const member = getPartyMember(session, slot);
      const town = townCommerce(screenId);
      const cost = town.feeding_frenzy_gold | 0;
      if (!charGoldDeduct(member, cost)) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
      } else {
        for (const m of ensureParty(session)) {
          if ((m.food | 0) < 0x28) m.food = 0x28;
        }
        await waitForSpace(`${member.name}: party fed to 40 (−${cost} gp)`, sprite, 0x0e);
        note(`Tavern feeding frenzy −${cost} gp`);
      }
      afterTxn(ctx);
      continue;
    }

    if (key === "b") {
      const lines = ["Buy (A4-$6738):", partyRosterLines(session), ""];
      STAT_BOOST.forEach((d, i) => lines.push(`${i + 1}) ${d.label} — ${d.cost} gp`));
      lines.push("", "1–6 pick — 0 back");
      const pick = await promptMenuKey(lines.join("\n"), "1234560", sprite, 0x0e);
      if (!pick || pick === "0") continue;
      const slot = await promptSelectMember(ctx);
      if (slot == null) continue;
      const member = getPartyMember(session, slot);
      const r = svcTavernStatBoost(member, parseInt(pick, 10) - 1, screenId);
      if (!r.paid) {
        await waitForSpace("Not enough gold / afflicted!", sprite, 0x0e);
      } else if (r.sick) {
        await waitForSpace(`${member.name}: ${r.label} — you feel sick!`, sprite, 0x0e);
        note(`Tavern boost ${r.label}: sick`);
      } else {
        await waitForSpace(`${member.name} bought ${r.label} (−${r.cost} gp).`, sprite, 0x0e);
        note(`Tavern boost ${r.label} −${r.cost} gp`);
      }
      afterTxn(ctx);
      continue;
    }

    if (key === "c") {
      const lines = ["Specialties:", partyRosterLines(session), ""];
      food.forEach((m, i) => lines.push(`${String.fromCharCode(65 + i)}) ${m.text} — ${m.gold} gp`));
      lines.push("", "A–C — 0 back");
      const pick = await promptMenuKey(lines.join("\n"), "abc0", sprite, 0x0e);
      if (!pick || pick === "0") continue;
      const menuIdx = { a: 0, b: 1, c: 2 }[pick];
      if (menuIdx == null || !food[menuIdx]) continue;
      const meal = food[menuIdx];
      const slot = await promptSelectMember(ctx);
      if (slot == null) continue;
      const member = getPartyMember(session, slot);
      const r = svcPubBuyFood(member, screenId, menuIdx);
      if (!r.paid) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
      } else {
        await waitForSpace(
          `${member.name} ordered ${meal.text}.\n−${r.cost} gp  (+$78=0x${(r.encoding & 0xff).toString(16)})`,
          sprite,
          0x0e
        );
        note(`Tavern meal ${meal.text} −${r.cost} gp enc=${r.encoding}`);
      }
      afterTxn(ctx);
      continue;
    }
  }
}

/**
 * OP_0E 0x05 mage guild. ASM-confirmed shop-open gate (0x1E410): the whole
 * shop only opens when >=1 party member already has the record+0x79 town bit
 * (memberGuildMember). That bit is earned via OP_0E 0x0D guild enroll, event-
 * script OP_15/18 selector 0x74, or the class-quest reward loop at 0x9D76.
 * @param {object} ctx
 */
export async function runGuildService(ctx) {
  const { screenId, session, waitForSpace, promptMenuKey, note, sprite, title } = ctx;
  const stock = mageGuildStock(screenId);
  const townName = TOWN_NAMES[screenId] ?? "Town";
  const party = ensureParty(session);

  if (!party.some((m) => memberGuildMember(m, screenId))) {
    await waitForSpace(
      `${title}\n\nNo one in your party belongs to the ${townName} mage guild.\n` +
        "(Enroll at the guild hall — OP_0E 0x0D — or earn membership via quest)",
      sprite,
      0x0e
    );
    note(`Guild ${townName}: no member in party (0x1E410 gate)`);
    return;
  }

  while (true) {
    const spellLines = stock.map((s, i) => `${s.key}) ${s.label} — ${s.gold} gp`);
    const text = [
      title,
      `${townName} Mage Guild — sorcerer spells (A–D)`,
      partyRosterLines(session),
      "",
      ...spellLines,
      "",
      "A–D buy spell — 0 or Esc to leave",
    ].join("\n");

    const key = await promptMenuKey(text, "abcd0", sprite, 0x0e);
    if (!key || key === "0") break;
    const idx = { a: 0, b: 1, c: 2, d: 3 }[key];
    const offer = stock[idx];
    if (!offer) continue;

    const slot = await promptSelectMember(ctx);
    if (slot == null) continue;
    const member = getPartyMember(session, slot);

    const r = svcGuildBuySpell(member, offer.flat, offer.gold);
    if (r.bought) {
      await waitForSpace(
        `${member.name} learned ${offer.label}!\n−${r.price} gp\n${memberSummary(member)}`,
        sprite,
        0x0e
      );
      note(`Guild spell ${offer.label} −${r.price} gp`);
    } else if (r.reject === "gold") {
      await waitForSpace("Not enough gold!", sprite, 0x0e);
    } else if (r.reject === "notForSale") {
      await waitForSpace("Not for sale.", sprite, 0x0e);
    }
    afterTxn(ctx);
  }
}

/** Inn registry write after exe innkeeper greet y/n (eventInnApplyRegistry). */
export async function runInnRegistry(ctx) {
  const { screenId, session, waitForSpace, note, sprite } = ctx;
  const townName = TOWN_NAMES[screenId] ?? "Town";
  applyInnRegistry(session, screenId);
  await waitForSpace(`Signed the registry.\nHome town set to ${townName}.`, sprite, 0x0e);
  note(`Inn registry signed @ ${townName}`);
  ctx.onSessionChange?.(session);
}

/** OP_0E 0x64 — inter-town portal when standing on portal tile. */
export async function runPortalTravel(ctx) {
  const { screenId, tileX, tileY, session, waitForSpace, promptYesNo, note, sprite, onTeleport } = ctx;
  const leg = portalAt(screenId, tileX, tileY);
  if (!leg) {
    await waitForSpace("No portal here.", sprite, 0x0e);
    note("Portal 0x64: not on portal tile");
    return false;
  }
  const destName = TOWN_NAMES[leg.destScreen] ?? `screen ${leg.destScreen}`;
  const yes = await promptYesNo(
    `Travel to ${destName} for ${leg.cost} gold (y/n)?`,
    sprite,
    0x0e
  );
  if (!yes) {
    note("Portal travel declined");
    return false;
  }
  const payer = getPartyMember(session, 0);
  if ((payer.gold | 0) < leg.cost) {
    await waitForSpace("Not enough gold!", sprite, 0x0e);
    return false;
  }
  payer.gold -= leg.cost;
  syncSessionGoldFromParty(session);
  onTeleport?.(leg.destScreen, leg.destX, leg.destY, leg.facing ?? 0);
  ctx.teleported = true;
  note(`Portal → ${destName} (−${leg.cost} gp)`);
  return true;
}

/** Display-only stub for engine-deferred services (general store 0x07 / special
 *  shop 0x08 — distinct fixed handlers -$7DB8/-$7DBE, NOT the arena engine). */
export async function runDeferredServiceMenu(ctx, sel, title, sprite, lines) {
  const { waitForSpace } = ctx;
  const body = [title, "", ...lines, "", "(Engine menu — not fully simulated)"].join("\n");
  await waitForSpace(body, sprite, 0x0e);
}

/**
 * OP_0E 0x08 — Arena Games ticket engine (thunk -$7DBE -> 0x9D76).
 * Explicit selector 0x08 ONLY; default-range dispatch uses event_dat_loader
 * (thunk -$7DFA -> 0x92F2), not this engine — see selectorBin.js.
 */
export async function runArenaTicketSelector(ctx, sel) {
  const { session, waitForSpace, promptYesNo, promptCombatResult, note, sprite, screenId, onSessionChange } =
    ctx;
  const s = sel & 0xff;
  const party = ensureParty(session);
  const ticket = findArenaTicket(party);

  if (!ticket.found) {
    /* asm 0x9DF6-0x9E2E, str @ code 0xA082/0xA0A7 (byte-exact). */
    await waitForSpace("Sorry, but you must have a ticket to\ncompete in these games.", sprite, 0x0e);
    note(`exec_selector(0x${s.toString(16)}): no arena ticket — denied`);
    return;
  }

  /* asm 0x9E32-0x9E84: consume ticket, accept text, then arm combat. */
  consumeArenaTicket(party, ticket);
  onSessionChange?.(session);
  await waitForSpace("The games master accepts your ticket.\n Let the battle begin!", sprite, 0x0e);

  const roll = 1 + Math.floor(Math.random() * 16); /* asm rng(1,16) @ thunk -$7BB4 */
  const monsterType = arenaMonsterType(ticket.color, screenId, roll);
  const colorName = ARENA_TICKET_COLOR_NAMES[ticket.color] ?? `#${ticket.color}`;
  const enc = {
    text: `Arena Games — ${colorName} ticket\nmonster type 0x${monsterType.toString(16)} (asm 0x9D76 → same combat thunk as OP_12)`,
    heading: "Arena Games",
    names: ["Opponents"],
  };
  const won = promptCombatResult
    ? await promptCombatResult(enc)
    : await promptYesNo("Enter combat?", sprite, 0x0e);

  if (won) {
    session.combatVictory = true;
    /* asm 0x9F1E-0xA048: gold from the 4x3 table (data 0xE7A) to the first
     * eligible member; "Winner, you receive N gold" (0xA0FC/0xA111). The
     * documented record+0x79 ROM bug (doc 36) is NOT replicated. */
    const gold = arenaGoldReward(ticket.color, screenId);
    const payee = party[0];
    if (payee) {
      payee.gold = (payee.gold | 0) + gold;
    }
    syncSessionGoldFromParty(session);
    await waitForSpace(`Winner, you receive ${gold} gold`, sprite, 0x0e);
    note(`exec_selector(0x${s.toString(16)}): arena victory (${colorName} ticket), +${gold} gp`);
    onSessionChange?.(session);
  } else {
    note(`exec_selector(0x${s.toString(16)}): arena fight declined/lost (${colorName} ticket)`);
  }
}

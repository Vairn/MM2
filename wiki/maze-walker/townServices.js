/** OP_0E town-service menus + transactions — mirrors TownServiceMenu/Transactions.cpp */
"use strict";

import { arenaGoldReward, arenaMonsterType } from "./selectorBin.js";

import {
  townCommerce,
  trainingCost,
  classXpForLevel,
  attrBonus,
  trainHpGain,
  tipDayPairBase,
  classCasterStat,
  classSpellLevelFor,
  smithInventory,
  smithBuyFields,
  smithPrice,
  smithSellPrice,
  smithIdentifyCost,
  smithSpecialsDateBonus,
  SMITH_CATEGORY_NAME,
  TEMPLE_DONATION_ALL_BITS,
  pubFoodMenu,
  mageGuildStock,
  templeSpellStock,
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
  memberHasSkillId,
  ARENA_TICKET_COLOR_NAMES,
} from "./sessionState.js";

const SKILL_MERCHANT = 10;
const STAT_BOOST_LIMITS = [2, 3, 3, 3, 3, 5];

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
  /* 0x1D7C4: RNG(1,100)>0x5A → blessed buff writes (session mirror of A4-$79AB..). */
  const blessRoll = Math.floor(Math.random() * 100) + 1;
  if (blessRoll > 0x5a) {
    r.blessed = true;
    session.lightFactor = 0xc8;
    session.magicProtect = 0x3c;
    session.forcesProtect = 0x3c;
    session.levitate = 1;
    session.walkWater = 1;
    session.guardDog = 1;
    session.templeBlessCtr = ((session.templeBlessCtr | 0) + 1) & 0xffff;
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
  /* HP @ 0x20390: ($64DA[class]*$64EE[map])/$64E4[map] + -$7F56(+$27). */
  const endCur = member.enduranceCurrent ?? member.endurance ?? 0;
  let hpGain = trainHpGain(member.classId, mapId, endCur);
  if (hpGain < 0) hpGain = 0;
  member.hpAux = (member.hpAux | 0) + hpGain;
  member.hpCurrent = (member.hpCurrent | 0) + hpGain;
  member.hpMax = (member.hpMax | 0) + hpGain;
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
  let pay = price | 0;
  if (memberHasSkillId(member, SKILL_MERCHANT)) pay = (pay / 2) | 0;
  r.price = pay;
  if (!charGoldDeduct(member, pay)) {
    r.reject = "gold";
    r.price = 0;
    return r;
  }
  memberGiveItem(member, itemId, charges, flags, slot);
  r.bought = true;
  r.slot = slot;
  return r;
}

/** Sell leaf 0x1BC26 — `halfPrice` is buy-style/2; no Merchant → /2 again. */
export function svcSmithSell(member, backpackSlot, halfPrice) {
  const r = { sold: false, reject: null, price: halfPrice | 0, slot: backpackSlot };
  if (member.condition !== 0) {
    r.reject = "condition";
    r.price = 0;
    return r;
  }
  const bp = member.backpack?.[backpackSlot];
  if (!bp || !bp.id) {
    r.reject = "empty";
    r.price = 0;
    return r;
  }
  let credit = halfPrice | 0;
  if (!memberHasSkillId(member, SKILL_MERCHANT)) credit = (credit / 2) | 0;
  r.price = credit;
  member.gold = (member.gold | 0) + credit;
  bp.id = 0;
  bp.charges = 0;
  bp.flags = 0;
  r.sold = true;
  return r;
}

/** Identify leaf 0x1B6E0 — deduct cost; summary is presentation. */
export function svcSmithIdentify(member, backpackSlot, cost) {
  const r = { identified: false, reject: null, cost: cost | 0, summary: "" };
  if (member.condition !== 0) {
    r.reject = "condition";
    r.cost = 0;
    return r;
  }
  const bp = member.backpack?.[backpackSlot];
  if (!bp || !bp.id) {
    r.reject = "empty";
    r.cost = 0;
    return r;
  }
  if (!charGoldDeduct(member, cost)) {
    r.reject = "gold";
    r.cost = 0;
    return r;
  }
  r.identified = true;
  r.summary = `Item #${bp.id} flags=${bp.flags & 0x3f} chg=${bp.charges}`;
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
      "E) Sell      F) Identify",
      "",
      "A–F — 0 or Esc to leave smith",
    ].join("\n");
    const catKey = await promptMenuKey(catText, "abcdef0", sprite, 0x0e);
    if (!catKey || catKey === "0") break;

    if (catKey === "e" || catKey === "f") {
      const sellMode = catKey === "e";
      const memberSlot = await promptSelectMember(ctx);
      if (memberSlot == null) continue;
      const member = getPartyMember(session, memberSlot);
      const lines = [
        sellMode ? "Sell item:" : "Identify item:",
        partyRosterLines(session),
        "",
      ];
      const priced = [];
      for (let i = 0; i < 6; i++) {
        const bp = member.backpack?.[i];
        if (!bp?.id) {
          lines.push(`${i + 1}) —`);
          priced.push(null);
          continue;
        }
        const name = itemNameFromManifest(manifest, bp.id);
        const base = itemGoldFromManifest(manifest, bp.id);
        const buy = smithPrice(base, bp.flags & 0x3f);
        const half = smithSellPrice(buy);
        const price = sellMode
          ? memberHasSkillId(member, SKILL_MERCHANT)
            ? half
            : (half / 2) | 0
          : smithIdentifyCost(bp.flags);
        lines.push(`${i + 1}) ${name} — ${price} gp`);
        priced.push({ slot: i, half, idCost: smithIdentifyCost(bp.flags) });
      }
      lines.push("", "1–6 — 0 back");
      const pick = await promptMenuKey(lines.join("\n"), "1234560", sprite, 0x0e);
      if (!pick || pick === "0") continue;
      const offer = priced[parseInt(pick, 10) - 1];
      if (!offer) continue;
      if (sellMode) {
        const r = svcSmithSell(member, offer.slot, offer.half);
        if (r.sold) {
          await waitForSpace(`${member.name} sold item for ${r.price} gp.`, sprite, 0x0e);
          note(`Smith sell +${r.price} gp`);
        } else {
          await waitForSpace(`${member.name}: cannot sell.`, sprite, 0x0e);
        }
      } else {
        const r = svcSmithIdentify(member, offer.slot, offer.idCost);
        if (r.identified) {
          await waitForSpace(`${member.name}: ${r.summary}\n−${r.cost} gp`, sprite, 0x0e);
          note(`Smith identify −${r.cost} gp`);
        } else if (r.reject === "gold") {
          await waitForSpace(`${member.name}: not enough gold!`, sprite, 0x0e);
        } else {
          await waitForSpace(`${member.name}: cannot identify.`, sprite, 0x0e);
        }
      }
      afterTxn(ctx);
      continue;
    }

    const category = { a: 0, b: 1, c: 2, d: 3 }[catKey];
    if (category == null) continue;

    const slots = smithInventory(screenId, category);
    const day = session.day | 0;
    const specialsBonus = category === 1 ? smithSpecialsDateBonus(day) : 0;
    const lines = [`${SMITH_CATEGORY_NAME[category]}`, partyRosterLines(session), ""];
    const priced = [];
    for (let i = 0; i < slots.length; i++) {
      const { itemId, meta } = slots[i];
      if (!itemId) {
        lines.push(`${i + 1}) —`);
        priced.push(null);
        continue;
      }
      let fields = smithBuyFields(category, meta);
      if (category === 1) {
        fields = { priceMeta: specialsBonus, charges: 0, flags: specialsBonus };
      }
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
      await waitForSpace(
        `${member.name} bought ${itemNameFromManifest(manifest, offer.itemId)} for ${r.price} gp.`,
        sprite,
        0x0e
      );
      note(`Smith buy −${r.price} gp`);
    } else if (r.reject === "gold") {
      await waitForSpace(`${member.name}: not enough gold!`, sprite, 0x0e);
    } else if (r.reject === "full") {
      await waitForSpace(`${member.name}'s pack is full.`, sprite, 0x0e);
    } else {
      await waitForSpace(`${member.name}: cannot buy.`, sprite, 0x0e);
    }
    afterTxn(ctx);
  }
}

/** Full HP/SP restore (inn rest leaf — no condition clear). */
export function svcInnRest(member) {
  member.hpCurrent = member.hpMax;
  if (member.spMax > 0) member.spCurrent = member.spMax;
}

/**
 * Tavern C specialties @ 0x1CD2E / 0x1CEA4: pay A4-$6760, then sick OR mask.
 * Sick RNG(1,end+5)==1 → bset #2,$26 and SKIP 0x1C8D4. Else OR A4-$786C → +$76.
 * No 0x18EC0 encode (selector 0xC9). No FAQ meal→tile coords.
 */
export function svcPubBuyFood(member, mapId, menuIdx) {
  const menu = pubFoodMenu(mapId);
  const meal = menu[menuIdx];
  if (!meal) return { paid: false, cost: 0, sick: false };
  if ((member.condition | 0) !== 0) return { paid: false, cost: meal.gold | 0, sick: false };
  const cost = meal.gold | 0;
  if (!charGoldDeduct(member, cost)) return { paid: false, cost: 0, sick: false };
  const masks = [
    [1, 2, 4],
    [4096, 8192, 16384],
    [64, 128, 256],
    [512, 1024, 2048],
    [8, 16, 32],
  ];
  const town = mapId >= 0 && mapId < 5 ? mapId : 0;
  const endHi = (member.endurance | 0) + 5;
  const sick = Math.floor(Math.random() * Math.max(1, endHi)) + 1 === 1;
  if (sick) {
    member.condition = (member.condition | 0) | 0x04;
    return { paid: true, cost, meal, sick: true };
  }
  member.tempScoreWord = ((member.tempScoreWord | 0) | (masks[town][menuIdx] | 0)) & 0xffff;
  return { paid: true, cost, meal, sick: false };
}

/** Selector 0xC9 food encode 0x18EC0 → party +$78 (no gold). Not tavern C. */
export function svcFoodEncodePurchase(party, menuIdx) {
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
  const t = tiers[menuIdx] || tiers[0];
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
  enc = (enc + addends[menuIdx | 0][i]) & 0xff;
  for (const m of party || []) {
    m.scriptWorkFlag = enc;
    if (m.rawBytes) {
      m.rawBytes[0x78] = enc;
      m.rawBytes[0x7c] = m.rawBytes[0x7c] & 0xfe;
    }
  }
  return { ok: true, encoding: enc };
}

/** Selector 0xCA drink encode 0x18F78 → party +$78, +$7C bit0. Menu A–C = drink 0..2. */
export function svcDrinkEncodePurchase(party, drinkIdx) {
  const base = [48, 64, 48, 31, 79, 143];
  const add = [31, 79, 143, 48, 64, 80];
  const d = drinkIdx | 0;
  const hi = base[d] || base[0];
  const enc = ((Math.floor(Math.random() * hi) + 1) + (add[d] || add[0])) & 0xff;
  for (const m of party || []) {
    m.scriptWorkFlag = enc;
    if (m.rawBytes) {
      m.rawBytes[0x78] = enc;
      m.rawBytes[0x7c] = (m.rawBytes[0x7c] & 0xfe) | 0x01;
    }
  }
  return { ok: true, encoding: enc };
}

/** 0x191A6 Lord's Quest arm: set +$7C bit2 + mode; food mask $08 / drink $10. */
export function svcQuestLordArm(party, drink) {
  const mask = drink ? 0x10 : 0x08;
  const modeBit = drink ? 0x01 : 0x00;
  let armed = 0;
  for (const m of party || []) {
    if (!m.rawBytes) continue;
    let flags = m.rawBytes[0x7c] | 0;
    if (flags & mask) continue;
    armed += 1;
    flags = (flags & 0xfe) | 0x04 | modeBit;
    m.rawBytes[0x7c] = flags;
  }
  return armed > 0 ? armed : -1;
}

/** 0x18FBA: Valor/Honor/Nobility (0xE2..0xE4) present → consume all. */
export function svcQuestHoardallItemsReady(party) {
  for (let id = 0xe2; id <= 0xe4; id++) {
    if (!svcPartyFindBackpackItem(party, id)) return false;
  }
  for (let id = 0xe2; id <= 0xe4; id++) {
    svcPartyConsumeBackpackItem(party, id);
  }
  return true;
}

/** 0x193AC + 0x19516: bit2 reward then encoding apply. */
export function svcQuestCompleteReward(party, drink, manifest) {
  let itemsGate = -1;
  let activity = 0;
  let membersRewarded = 0;
  let encodingsApplied = 0;
  let xpEach = 0;
  for (const m of party || []) {
    if (!m.rawBytes) continue;
    const flags = m.rawBytes[0x7c] | 0;
    if ((flags & 0x04) === 0) continue;
    if (((flags & 0x01) !== 0) !== !!drink) continue;
    let eligible = 0;
    if (drink) {
      if ((flags & 0xe0) === 0xe0) {
        m.rawBytes[0x7c] = flags & 0x1f;
        if ((flags & 0x10) === 0) eligible = 1;
      }
    } else if ((flags & 0x08) === 0) {
      if (itemsGate < 0) itemsGate = svcQuestHoardallItemsReady(party) ? 1 : 0;
      if (itemsGate > 0) eligible = 1;
    }
    if (!eligible) continue;
    activity += 1;
    m.rawBytes[0x7c] = (m.rawBytes[0x7c] & 0xfb) | (drink ? 0x10 : 0x08);
    xpEach = drink ? 1000000 : 100000;
    m.experience = ((m.experience | 0) + xpEach) >>> 0;
    membersRewarded += 1;
  }
  for (const m of party || []) {
    const hit = drink
      ? svcApplyDrinkEncoding(m).applied
      : svcApplyFoodEncoding(m, manifest).applied;
    if (hit) {
      encodingsApplied += 1;
      activity += 1;
    }
  }
  return { activity, membersRewarded, encodingsApplied, xpEach };
}

/** 0x1961E busy gate: bit2 or +$78 with matching mode. */
export function svcQuestBusy(party, drink) {
  for (const m of party || []) {
    if (!m.rawBytes) continue;
    const flags = m.rawBytes[0x7c] | 0;
    if (((flags & 0x01) !== 0) !== !!drink) continue;
    if (flags & 0x04) return true;
    if (m.rawBytes[0x78]) return true;
  }
  return false;
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

export function svcTavernStatBoost(member, slot, mapId, session) {
  const row = STAT_BOOST[slot];
  if (!row) return { paid: false, cost: 0, applied: false };
  if ((member.condition | 0) !== 0) return { paid: false, cost: row.cost, ok: false, applied: false };
  if (!charGoldDeduct(member, row.cost)) return { paid: false, cost: 0, applied: false };
  if (!session.tavernStatBuyCount) session.tavernStatBuyCount = [0, 0, 0, 0, 0, 0];
  const count = session.tavernStatBuyCount[slot] | 0;
  const limit = STAT_BOOST_LIMITS[slot] | 0;
  let applied = false;
  if (count < limit) {
    session.tavernStatBuyCount[slot] = count + 1;
  } else {
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
    applied = true;
  }
  const endHi = (member.endurance | 0) + 10;
  const sick = Math.floor(Math.random() * Math.max(1, endHi)) + 1 === 2;
  if (sick) member.condition = (member.condition | 0) | 0x08;
  return { paid: true, cost: row.cost, sick, label: row.label, ok: true, applied };
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

/** FAQ drink caption stub — encode 0xCA / apply 0x18D3A (XP from A4-$6AE0). */
export function svcPubBuyDrink(member, drinkIdx, drinks) {
  const row = drinks[drinkIdx];
  if (!row) return { paid: false, cost: 0 };
  void member;
  return { paid: true, cost: row.gold | 0, sick: false, name: row.name, encodeOnly: true };
}

/** A4-$6AEA / -$6AE0 — drink apply @ 0x18D3A (XP → +$62). */
const DRINK_GOLD_THRESH = [0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0];
const DRINK_GOLD_AWARD = [2000, 4000, 5000, 7000, 10000, 15000, 25000, 50000, 100000, 250000];

export function svcApplyDrinkEncoding(member) {
  const raw = member.rawBytes;
  if (!raw || ((raw[0x7c] | 0) & 0x02) === 0) return { applied: false, gold: 0 };
  const enc = raw[0x78] | 0;
  if (!enc || ((member.condition | 0) & 0xff) >= 0x80) return { applied: false, gold: 0 };
  let tier = 0;
  while (tier < 10 && enc >= DRINK_GOLD_THRESH[tier]) tier++;
  if (tier >= 10) tier = 9;
  const gold = DRINK_GOLD_AWARD[tier];
  member.experience = ((member.experience | 0) + gold) >>> 0;
  raw[0x78] = 0;
  raw[0x7c] = (raw[0x7c] | 0) & 0xfd;
  return { applied: true, gold };
}

/** Food EXP apply @ 0x18DE0: +$78 as items.dat id; XP = gold×8 (A4-$3EEC). */
export function svcApplyFoodEncoding(member, manifest) {
  const raw = member.rawBytes;
  if (!raw) return { applied: false, xp: 0 };
  const enc = raw[0x78] | 0;
  if (!enc || ((member.condition | 0) & 0xff) >= 0x80) return { applied: false, xp: 0 };
  const itemGold = itemGoldFromManifest(manifest, enc) | 0;
  const xp = (itemGold << 3) >>> 0;
  member.experience = ((member.experience | 0) + xp) >>> 0;
  raw[0x78] = 0;
  return { applied: true, xp };
}

/** 0x18CE6: first party backpack slot with id (1-based), or 0. */
export function svcPartyFindBackpackItem(party, itemId) {
  const id = itemId & 0xff;
  if (!id) return 0;
  for (const m of party || []) {
    const pack = m.backpack || m.rawBytes;
    if (!pack) continue;
    for (let s = 0; s < 6; s++) {
      const slotId = m.backpackId?.[s] ?? pack[0x3a + s] ?? 0;
      if ((slotId | 0) === id) return s + 1;
    }
  }
  return 0;
}

/** 0x18D06: clear first matching backpack id. */
export function svcPartyConsumeBackpackItem(party, itemId) {
  const id = itemId & 0xff;
  if (!id) return false;
  for (const m of party || []) {
    if (m.backpackId) {
      for (let s = 0; s < 6; s++) {
        if ((m.backpackId[s] | 0) === id) {
          m.backpackId[s] = 0;
          if (m.backpackCharges) m.backpackCharges[s] = 0;
          if (m.backpackFlags) m.backpackFlags[s] = 0;
          return true;
        }
      }
    }
    const raw = m.rawBytes;
    if (!raw) continue;
    for (let s = 0; s < 6; s++) {
      if ((raw[0x3a + s] | 0) === id) {
        raw[0x3a + s] = 0;
        raw[0x40 + s] = 0;
        raw[0x46 + s] = 0;
        return true;
      }
    }
  }
  return false;
}

/** Combat 0x10C66: arm +$7C bit1 when +$78 matches killed monster type. */
export function svcArmDrinkMatchOnKill(party, monsterType) {
  const t = monsterType & 0xff;
  for (const m of party || []) {
    const raw = m.rawBytes;
    if (!raw || (raw[0x78] | 0) !== t) continue;
    if (((raw[0x7c] | 0) & 0x01) === 0) continue;
    raw[0x7c] = (raw[0x7c] | 0) | 0x02;
  }
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
      /* E @ 0x1D0B4: day-pair rumors (A4-$594E). */
      const slot = await promptSelectMember(ctx);
      if (slot == null) continue;
      const member = getPartyMember(session, slot);
      if ((member.condition | 0) !== 0) {
        await waitForSpace("Cannot listen while afflicted.", sprite, 0x0e);
        continue;
      }
      const day = session.day | 0;
      const base = tipDayPairBase(day);
      await waitForSpace(
        `Rumors (day pair ${base}/${base + 1}):\n(A4-$594E — pool strings)`,
        sprite,
        0x0e
      );
      continue;
    }

    if (key === "d") {
      /* D @ 0x1CFCA: 1gp + RNG + day-pair tips. */
      const slot = await promptSelectMember(ctx);
      if (slot == null) continue;
      const member = getPartyMember(session, slot);
      if ((member.condition | 0) !== 0) {
        await waitForSpace("Cannot tip while afflicted.", sprite, 0x0e);
        continue;
      }
      if (!charGoldDeduct(member, 1)) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
        continue;
      }
      const day = session.day | 0;
      const base = tipDayPairBase(day);
      await waitForSpace(
        `Tip (day pair ${base}/${base + 1}):\n(A4-$58AE — pool strings)`,
        sprite,
        0x0e
      );
      note(`Tavern tip −1 gp day-pair ${base}`);
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
      const r = svcTavernStatBoost(member, parseInt(pick, 10) - 1, screenId, session);
      if (!r.paid) {
        await waitForSpace("Not enough gold / afflicted!", sprite, 0x0e);
      } else if (r.sick) {
        await waitForSpace(`${member.name}: ${r.label} — you feel sick!`, sprite, 0x0e);
        note(`Tavern boost ${r.label}: sick`);
      } else if (!r.applied) {
        await waitForSpace(`${member.name} paid for ${r.label} (building up).`, sprite, 0x0e);
        note(`Tavern boost ${r.label} count-only`);
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
        await waitForSpace("Not enough gold / afflicted!", sprite, 0x0e);
      } else if (r.sick) {
        await waitForSpace(`${member.name}: ${meal.text} — you feel sick!`, sprite, 0x0e);
        note(`Tavern specialty ${meal.text}: sick (no +$76 mask)`);
      } else {
        await waitForSpace(
          `${member.name} ordered ${meal.text}.\n−${r.cost} gp  (+$76 mask)`,
          sprite,
          0x0e
        );
        note(`Tavern specialty ${meal.text} −${r.cost} gp`);
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

/** OP_0E 0x64 is Circus (thunk -$7D9A → 0xDF04), not inter-town portals.
 *  Town portal tiles use other selectors / OP_0C — do not call this for 0x64. */
export async function runPortalTravel(ctx) {
  const { waitForSpace, note, sprite } = ctx;
  await waitForSpace("No portal handler (0x64 is Circus).", sprite, 0x0e);
  note("Portal stub: 0x64 is Circus @ 0xDF04");
  return false;
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

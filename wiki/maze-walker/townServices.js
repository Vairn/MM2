/** OP_0E town-service menus + transactions — mirrors TownServiceMenu/Transactions.cpp */
"use strict";

import {
  townCommerce,
  healingCost,
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
} from "./townTables.js";

import {
  syncSessionGoldFromParty,
  getPartyMember,
  ensureParty,
  memberBackpackFreeSlot,
  memberGiveItem,
  itemNameFromManifest,
  itemGoldFromManifest,
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

/** @param {object} member @param {number} cost */
export function svcHeal(member, cost) {
  const r = { paid: false, cost: cost | 0, hpRestored: false };
  if (!charGoldDeduct(member, cost)) {
    r.cost = 0;
    return r;
  }
  r.paid = true;
  r.hpRestored = restoreHp(member);
  svcRestoreCondition(member);
  return r;
}

/** @param {object} session @param {object} member @param {number} mapId */
export function svcTempleDonate(session, member, mapId) {
  const town = townCommerce(mapId);
  const r = { paid: false, cost: town.donation_gold, allTemples: false };
  if (!charGoldDeduct(member, town.donation_gold)) {
    r.cost = 0;
    return r;
  }
  r.paid = true;
  session.questDonationBits = (session.questDonationBits | town.donation_quest_bit) & 0xff;
  r.allTemples = session.questDonationBits === TEMPLE_DONATION_ALL_BITS;
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
  const hpGain = classHpPerLevel(member.classId) + attrBonus(member.endurance);
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
async function promptSelectMember(ctx) {
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

  while (true) {
    const donateGp = town.donation_gold;
    const donated = (session.questDonationBits & town.donation_quest_bit) !== 0;
    const text = [
      title,
      partyRosterLines(session),
      "",
      "A) Heal (HP + condition) — level×town×10 gp",
      "B) Restore Condition (free)",
      "C) Donations",
      `   ${donateGp} gp${donated ? " (already donated here)" : ""}`,
      "",
      "A/B/C — 0 or Esc to leave",
    ].join("\n");

    const key = await promptMenuKey(text, "abc0", sprite, 0x0e);
    if (!key || key === "0") break;

    const slot = await promptSelectMember(ctx);
    if (slot == null) continue;
    const member = getPartyMember(session, slot);

    if (key === "a") {
      const healGp = healingCost(member.level, town.training_town_index);
      const r = svcHeal(member, healGp);
      if (!r.paid) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
        note(`Temple: ${member.name} heal failed (gold)`);
      } else {
        await waitForSpace(
          `${member.name} healed for ${r.cost} gp.\n${memberSummary(member)}`,
          sprite,
          0x0e
        );
        note(`Temple heal ${member.name} −${r.cost} gp`);
      }
    } else if (key === "b") {
      svcRestoreCondition(member);
      await waitForSpace(`${member.name}: condition cleared.\n${memberSummary(member)}`, sprite, 0x0e);
      note(`Temple: restore condition ${member.name}`);
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

/** Display-only stub for engine-deferred services (tavern RNG, guild spells, inn rest). */
export async function runDeferredServiceMenu(ctx, sel, title, sprite, lines) {
  const { waitForSpace } = ctx;
  const body = [title, "", ...lines, "", "(Engine menu — not fully simulated)"].join("\n");
  await waitForSpace(body, sprite, 0x0e);
}

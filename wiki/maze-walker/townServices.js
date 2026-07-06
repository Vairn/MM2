/** OP_0E town-service menus + transactions — mirrors TownServiceMenu/Transactions.cpp */
"use strict";

import { arenaGoldReward, arenaMonsterType } from "./selectorBin.js";

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
  pubFoodMenu,
  mageGuildStock,
  portalAt,
  TOWN_NAMES,
  PUB_DRINKS,
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
  applyGuildEnroll,
  applyBrainDetox,
  applyInnRegistry,
  findArenaTicket,
  consumeArenaTicket,
  ARENA_TICKET_COLOR_NAMES,
} from "./sessionState.js";

import { applySkillBuy } from "./skillBuy.js";

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
  member.pubMeal = ((mapId & 0x0f) << 4) | ((menuIdx + 1) & 0x0f);
  member.food = Math.min(0x28, (member.food | 0) + 10);
  return { paid: true, cost, meal };
}

/** @param {object} member @param {number} drinkIdx */
export function svcPubBuyDrink(member, drinkIdx, drinks) {
  const row = drinks[drinkIdx];
  if (!row) return { paid: false, cost: 0 };
  const cost = row.gold | 0;
  if (!charGoldDeduct(member, cost)) return { paid: false, cost: 0 };
  const sick = Math.random() < 0.04;
  return { paid: true, cost, sick, name: row.name };
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
  const { screenId, session, waitForSpace, promptMenuKey, note, sprite, title } = ctx;
  const town = townCommerce(screenId);

  while (true) {
    const text = [
      title,
      partyRosterLines(session),
      "",
      "A) Rest whole party (healing cost × each member)",
      "B) Rest one member",
      "(HP/SP to max; day +1; condition unchanged)",
      "",
      "A/B — 0 or Esc to leave",
    ].join("\n");

    const key = await promptMenuKey(text, "ab0", sprite, 0x0e);
    if (!key || key === "0") break;

    if (key === "a") {
      let total = 0;
      for (const m of ensureParty(session)) {
        total += healingCost(m.level, town.training_town_index);
      }
      const payer = getPartyMember(session, 0);
      if ((payer.gold | 0) < total) {
        await waitForSpace(`Not enough gold! Need ${total} gp.`, sprite, 0x0e);
        continue;
      }
      payer.gold -= total;
      for (const m of ensureParty(session)) svcInnRest(m);
      session.day = ((session.day | 0) + 1) % 180 || 1;
      await waitForSpace(
        `The party rests. −${total} gp from ${payer.name}.\nDay is now ${session.day}.`,
        sprite,
        0x0e
      );
      note(`Inn party rest −${total} gp, day=${session.day}`);
    } else {
      const slot = await promptSelectMember(ctx);
      if (slot == null) continue;
      const member = getPartyMember(session, slot);
      const cost = healingCost(member.level, town.training_town_index);
      if (!charGoldDeduct(member, cost)) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
        continue;
      }
      svcInnRest(member);
      session.day = ((session.day | 0) + 1) % 180 || 1;
      await waitForSpace(
        `${member.name} rests for ${cost} gp.\nDay is now ${session.day}.\n${memberSummary(member)}`,
        sprite,
        0x0e
      );
      note(`Inn rest ${member.name} −${cost} gp`);
    }
    afterTxn(ctx);
  }
}

/** @param {object} ctx */
export async function runTavernService(ctx) {
  const { screenId, session, waitForSpace, promptMenuKey, note, sprite, title } = ctx;
  const food = pubFoodMenu(screenId);

  while (true) {
    const text = [
      title,
      partyRosterLines(session),
      "",
      "A/B/C) Order entree (fixed gp from A4-$6760 table)",
      "D) Drinks menu",
      "E) Listen for rumors (display only)",
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
      const lines = ["Drinks:", partyRosterLines(session), ""];
      PUB_DRINKS.forEach((d, i) => lines.push(`${i + 1}) ${d.name} — ${d.gold} gp`));
      lines.push("", "1–6 pick drink — 0 back");
      const pick = await promptMenuKey(lines.join("\n"), "1234560", sprite, 0x0e);
      if (!pick || pick === "0") continue;
      const slot = await promptSelectMember(ctx);
      if (slot == null) continue;
      const member = getPartyMember(session, slot);
      const r = svcPubBuyDrink(member, parseInt(pick, 10) - 1, PUB_DRINKS);
      if (!r.paid) {
        await waitForSpace("Not enough gold!", sprite, 0x0e);
      } else if (r.sick) {
        await waitForSpace(`${member.name} got sick!\n(ASM 0x19B28 stat reset — not simulated)`, sprite, 0x0e);
        note(`Tavern drink ${r.name}: sick roll`);
      } else {
        await waitForSpace(`${member.name} enjoyed ${r.name}. −${r.cost} gp`, sprite, 0x0e);
        note(`Tavern drink ${r.name} −${r.cost} gp`);
      }
      afterTxn(ctx);
      continue;
    }

    const menuIdx = { a: 0, b: 1, c: 2 }[key];
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
        `${member.name} ordered ${meal.text}.\n−${r.cost} gp  (pubMeal=0x${(member.pubMeal & 0xff).toString(16)})`,
        sprite,
        0x0e
      );
      note(`Tavern meal ${meal.text} −${r.cost} gp`);
    }
    afterTxn(ctx);
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

/**
 * OP_0E 0x0D guild enroll transaction (eventApplyGuildEnroll) — called after
 * the shared-string-bank intro y/n in eventVm.js.
 * @param {object} ctx
 */
export async function runGuildEnrollTransaction(ctx) {
  const { screenId, session, waitForSpace, note, sprite, title } = ctx;
  const townName = TOWN_NAMES[screenId] ?? "Town";
  const r = applyGuildEnroll(session, screenId);
  if (!r.ok) {
    await waitForSpace("Not enough gold!", sprite, 0x0e);
    note("Guild enroll failed (insufficient gold)");
    return;
  }
  await waitForSpace(
    `Enrolled in the ${townName} mage guild!\n−20 gp  Home town set.\n(record+0x79 membership bit set)`,
    sprite,
    0x0e
  );
  note(`Guild enroll @ ${townName} (−20 gp, +0x79 mask, registry)`);
  ctx.onSessionChange?.(session);
}

/**
 * OP_0E 0x10 brain detox @ Middlegate — member select after intro y/n.
 * @param {object} ctx
 */
export async function runBrainDetoxService(ctx) {
  const { session, waitForSpace, note, sprite, title } = ctx;
  const slot = await promptSelectMember(ctx);
  if (slot == null) {
    note("Brain detox cancelled");
    return;
  }
  const member = getPartyMember(session, slot);
  const r = applyBrainDetox(session, slot);
  if (!r.ok) {
    await waitForSpace("Not enough gold!", sprite, 0x0e);
    note(`Brain detox: ${member.name} insufficient gold`);
    return;
  }
  await waitForSpace(
    `${member.name} cleansed of all secondary skills.\n−100 gp\n${memberSummary(member)}`,
    sprite,
    0x0e
  );
  note(`Brain detox ${member.name} −100 gp, skill pack cleared`);
  ctx.onSessionChange?.(session);
}

/** OP_0E default-range skill vendor transaction (eventApplySkillBuy). */
export async function runSkillBuyTransaction(ctx, offer) {
  const { session, waitForSpace, note, sprite, title } = ctx;
  let slot = session.selectedMember ?? 0;
  if (!offer.memberAlreadySelected) {
    const picked = await promptSelectMember(ctx);
    if (picked == null) {
      note(`Skill buy 0x${offer.execSelector.toString(16)} cancelled`);
      return;
    }
    slot = picked;
  }
  const r = applySkillBuy(session, slot, offer.skillId, offer.goldCost);
  if (!r.ok) {
    if (r.reason === "full") {
      await waitForSpace("You already have two skills!", sprite, 0x0e);
    } else {
      await waitForSpace("Not enough gold!", sprite, 0x0e);
    }
    note(`Skill buy failed (${r.reason})`);
    return;
  }
  await waitForSpace(
    `${title}\n\nSkill #${offer.skillId} granted to party member ${slot + 1}.\n(${offer.goldCost} gp checked — bytecode does not deduct)`,
    sprite,
    0x0e
  );
  note(`Skill buy 0x${offer.execSelector.toString(16)} skill=${offer.skillId}`);
  ctx.onSessionChange?.(session);
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

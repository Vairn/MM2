/**
 * Walker CombatSession public contract — mirrors game/src/combat/CombatSession.cpp
 * enter / finishVictory / finishLeave (+ EncounterPicker.cpp for mode < $80).
 * Per-turn fight: combatRuntime.js (tick / party options / AI) hosted by runCombatContract.
 */
"use strict";

import { arenaGoldReward } from "./selectorBin.js";
import { ensureParty, syncSessionGoldFromParty } from "./sessionState.js";
import {
  combatStartFight,
  combatTick,
  combatAwaitingInput,
  combatPromptText,
} from "./combatRuntime.js";

const DISPOSITION_MOD = [0, 1, 2, 5];
const FOUND_ITEM_SLOTS = 3;

const LOOT_THRESH = [25, 40, 50, 55, 70, 75, 100];
const LOOT_ROWS = [
  [0, 24, 53, 65],
  [65, 13, 19, 26],
  [91, 6, 13, 19],
  [114, 3, 10, 12],
  [126, 8, 23, 28],
  [154, 2, 4, 5],
  [159, 22, 34, 48],
  [0, 0, 6, 97],
];
const LOOT_CHARGES = [5, 60, 200, 0];

function rngRange(rng, lo, hi) {
  if (typeof rng === "function") return rng(lo, hi);
  if (lo >= hi) return lo;
  return lo + Math.floor(Math.random() * (hi - lo + 1));
}

function monsterDef(manifest, id) {
  const list = manifest?.monsters;
  if (!Array.isArray(list) || id < 0 || id >= list.length) return null;
  return list[id] ?? null;
}

function friendCount(manifest, type) {
  const m = monsterDef(manifest, type);
  const n = m?.add_friends | 0;
  return n > 0 ? Math.min(255, n) : 1;
}

/** mm2_encounter_live_count @ 0x12CE0. */
export function encounterRecomputeLiveCount(session) {
  const slots = session.monsterSlots ?? [];
  let j = 0;
  for (let i = 0; i < 10; i++) {
    if ((slots[i] ?? 0) !== 0) j++;
  }
  if ((session.encounterOverflowType | 0) !== 0) {
    j += session.monsterCount | 0;
  }
  if (j > 255) j = 255;
  session.monsterCount = j & 0xff;
  return session.monsterCount;
}

/** encounterInitXpBudget @ EncounterPicker.cpp. */
export function encounterInitXpBudget(session) {
  const party = ensureParty(session);
  let totalHp = 0;
  let maxHalf = 0;
  for (const m of party) {
    totalHp += m.hpCurrent | 0;
    const half = (m.level | 0) >> 1;
    if (half > maxHalf) maxHalf = half;
  }
  let budget = Math.floor(totalHp / 8);
  const disp = (session.disposition ?? 2) & 3;
  if (disp === 0) budget = Math.floor(budget / 4);
  else if (disp === 1) budget = Math.floor(budget / 2);
  else if (disp === 3) budget *= 2;
  session.partyXpBudget = budget >>> 0;
  session.pickerTierMod = maxHalf & 0xff;
}

function encounterPickerBudgetCheck(session, screen) {
  session.pickerDone = 0;
  const live = session.monsterCount | 0;
  const scan = live > 10 ? 10 : live;
  let total = 0;
  const slots = session.monsterSlots ?? [];
  for (let i = 0; i < scan; i++) {
    total += ((slots[i] ?? 0) >> 4) + 1;
  }
  if (live > 10) {
    const overflow = session.encounterOverflowType | 0;
    total += ((overflow >> 4) + 1) * (live - 10);
  }
  if (total >= (session.partyXpBudget >>> 0)) {
    session.pickerDone = 1;
    return;
  }
  const gate = screen?.encounter?.groupGate ?? 8;
  if (live >= gate) session.pickerDone = 1;
}

function encounterAddsFriends(session, screen, manifest, rng) {
  const disp = (session.disposition ?? 2) & 3;
  const tierMod = session.pickerTierMod | 0;
  let ceiling = DISPOSITION_MOD[disp] + tierMod + 1;
  const roll100 = rngRange(rng, 1, 100);
  let bonus = 0;
  if (roll100 >= 0x3d && roll100 < 0x51) bonus = 1;
  else if (roll100 >= 0x51 && roll100 < 0x60) bonus = 2;
  else if (roll100 >= 0x60) bonus = 3;
  ceiling += bonus;
  if (ceiling > 13) ceiling = 14;

  const minM = screen?.encounter?.minMonsters ?? 1;
  const maxM = screen?.encounter?.maxMonsters ?? 3;
  let tierPick = rngRange(rng, 1, Math.max(1, ceiling));
  if (tierPick < minM) tierPick = minM;
  if (tierPick > maxM) tierPick = maxM;
  const tierIndex = tierPick - 1;

  const roll16 = rngRange(rng, 1, 16);
  const typeId = ((tierIndex * 16 + roll16 - 1) & 0xff) >>> 0;
  const friendField = friendCount(manifest, typeId);
  let remaining = rngRange(rng, 1, friendField > 0 ? friendField : 1);

  let live = session.monsterCount | 0;
  const slots = session.monsterSlots ?? new Array(10).fill(0);
  while (live < 11 && remaining > 0) {
    if (live < 10) slots[live] = typeId;
    live++;
    remaining--;
  }
  if (remaining > 0) {
    if (remaining > 0xf0) remaining = 0xf0;
    let total = live + remaining;
    if (total > 0xfa) total = 0xfa;
    live = total;
  }
  session.monsterSlots = slots;
  session.monsterCount = live & 0xff;
}

/** encounterRunRandomPicker — mode < $80 only. */
export function encounterRunRandomPicker(session, screen, manifest, rng) {
  encounterPickerBudgetCheck(session, screen);
  let guard = 0;
  while (!session.pickerDone && guard++ < 64) {
    encounterAddsFriends(session, screen, manifest, rng);
    encounterPickerBudgetCheck(session, screen);
  }
}

function partyEligibleForRewards(m) {
  return (m?.condition ?? 0) < 0x80;
}

function applyTreasureForType(session, monType, manifest, rng) {
  const m = monsterDef(manifest, monType);
  if (!m) return;
  const treasure = m.treasure | 0;
  if (treasure & 0x04) {
    session.foundGems = ((session.foundGems | 0) + rngRange(rng, 1, 0x0a)) & 0xffff;
  }
  const goldTier = (treasure >> 3) & 0x03;
  if (goldTier !== 0) {
    let t = monType & 0xff;
    if (goldTier === 2) t = (t / 0x10) | 0;
    else if (goldTier >= 3) t = (t / 2) | 0;
    let typeRoll = 0;
    if (t > 0) typeRoll = rngRange(rng, 1, t);
    let typeContrib = (t + typeRoll) & 0xff;
    let base = rngRange(rng, 1, 0x32) + 6;
    if (goldTier === 1) typeContrib = 0;
    session.foundGoldExp = ((session.foundGoldExp | 0) + base + (typeContrib << 8)) >>> 0;
  }
  const itemTier = treasure & 0x03;
  if (itemTier !== 0) {
    const best = session.lootItemTier | 0;
    if (itemTier >= best) {
      session.lootItemTypeHi = (monType >> 4) & 0xff;
      session.lootItemTier = itemTier;
    }
  }
}

function fillFoundItemSlot(session, slotIndex, manifest, rng) {
  let tier = session.lootItemTier | 0;
  if (tier > 2) {
    tier = 2;
    session.lootItemTier = 2;
  }
  const roll100 = rngRange(rng, 1, 0x64);
  let bucket = 0;
  for (; bucket < 7; bucket++) {
    if (roll100 <= LOOT_THRESH[bucket]) break;
  }
  const base = LOOT_ROWS[bucket][0];
  const span = LOOT_ROWS[bucket][tier + 1];
  let id = base;
  if (span > 0) id = base + rngRange(rng, 1, span);
  let charges = 0;
  let flags = 0;
  const gold = manifest?.itemsGold;
  /* effect/bonus fidelity needs items.dat; charge when record exists in gold table. */
  if (Array.isArray(gold) && id >= 0 && id < gold.length) {
    charges = LOOT_CHARGES[tier] ?? 0;
  }
  const typeHi = session.lootItemTypeHi | 0;
  if (typeHi >= 2) {
    let fr = rngRange(rng, 1, 7);
    if (typeHi >= 2 && typeHi <= 0x0c) fr = rngRange(rng, 1, typeHi);
    if (typeHi === 0x0d) fr = rngRange(rng, 1, 0x15) + 0x0b;
    flags = fr & 0xff;
    if (flags >= 5) {
      const r2 = rngRange(rng, 1, 0x64);
      if (r2 < 0x29) flags |= 0x80;
      else if (r2 < 0x47) flags |= 0x40;
      else flags |= 0xc0;
    }
  }
  if (!session.foundItemSlots) session.foundItemSlots = [];
  session.foundItemSlots[slotIndex] = { id: id & 0xff, flags: flags & 0xff, charges: charges & 0xff };
}

function generateVictoryItems(session, manifest, rng) {
  if ((session.lootItemTier | 0) === 0 && (session.lootItemTypeHi | 0) === 0) return;
  const roll = rngRange(rng, 1, 0x64);
  let count = 0;
  if (roll < 0x0b) count = 3;
  else if (roll < 0x2e) count = 2;
  else if (roll < 0x5b) count = 1;
  for (let i = 0; i < count && i < FOUND_ITEM_SLOTS; i++) {
    if ((session.lootItemTier | 0) !== 0) {
      session.lootItemTier = ((session.lootItemTier | 0) - 1) & 0xff;
    }
    fillFoundItemSlot(session, i, manifest, rng);
  }
  /* Merge into found-item buffer (Search can collect). ASM finishVictory clears
   * -$794C and does not re-arm $FE — leave sentinel alone here. */
  const items = (session.foundItemSlots ?? []).filter((it) => it && (it.id | 0) !== 0);
  if (items.length || (session.foundGoldExp | 0) || (session.foundGems | 0)) {
    session.foundItemBuffer = {
      gold: session.foundGoldExp | 0,
      gems: session.foundGems | 0,
      items,
    };
  }
}

/**
 * CombatSession::enter — GS seed + picker; returns false when fight refused (live==0).
 * @param {object} session
 * @param {{ screen?: object, manifest?: object, rng?: Function }} opts
 */
export function combatEnter(session, opts = {}) {
  const { screen = null, manifest = null, rng = null } = opts;
  session.combatVictory = false;
  session.combatXpPool = 0;
  session.lootItemTier = 0;
  session.lootItemTypeHi = 0;
  session.foundGems = 0;
  session.foundGoldExp = 0;
  session.foundItemSlots = [];
  session.combatActive = false;
  session.combatOutcome = "none";

  encounterRecomputeLiveCount(session);
  let mode = session.encounterMode | 0;
  if (mode < 0x80) {
    encounterInitXpBudget(session);
    encounterRunRandomPicker(session, screen, manifest, rng);
  } else {
    session.encounterMode = (mode - 0x80) & 0xff;
  }

  const live = session.monsterCount | 0;
  if (live === 0) {
    session.combatActive = false;
    session.combatOutcome = "none";
    return false;
  }

  /* Bribe gates from treasure bits 5/6/7 — enter unpack @ 0x4C8E. */
  let bribeFood = 0;
  let bribeGold = 0;
  let bribeGems = 0;
  const slotCount = live < 11 ? live : 11;
  const overflow = session.encounterOverflowType | 0;
  for (let i = 0; i < slotCount; i++) {
    const type =
      i < 10 ? (session.monsterSlots?.[i] ?? 0) & 0xff : overflow & 0xff;
    const m = monsterDef(manifest, type);
    const treasure = m?.treasure | 0;
    if (treasure & 0x20) bribeFood++;
    if (treasure & 0x40) bribeGold++;
    if (treasure & 0x80) bribeGems++;
  }
  session.bribeFood = bribeFood & 0xff;
  session.bribeGold = bribeGold & 0xff;
  session.bribeGems = bribeGems & 0xff;
  session.retreatDiff = screen?.encounter?.retreat ?? 0;
  session.combatActive = true;
  session.combatOutcome = "none";
  return true;
}

/**
 * Simulate wipe of all live monsters (V = victory) then CombatSession::finishVictory.
 * Accrues XP/treasure as onMonsterKilled would for each slot.
 */
export function combatResolveVictoryKills(session, manifest, rng) {
  const live = session.monsterCount | 0;
  const overflow = session.encounterOverflowType | 0;
  const n = live < 11 ? live : 11;
  let xp = 0;
  for (let i = 0; i < n; i++) {
    const type =
      i < 10 ? (session.monsterSlots?.[i] ?? 0) & 0xff : overflow & 0xff;
    const m = monsterDef(manifest, type);
    const killXp = (m?.xp | 0) >>> 0;
    xp += killXp;
    applyTreasureForType(session, type, manifest, rng);
  }
  if (live > 10) {
    /* Overflow excess XP copies @ onMonsterKilled when slot >= 0x0A. */
    const m = monsterDef(manifest, overflow);
    const killXp = (m?.xp | 0) >>> 0;
    xp += killXp * (live - 10);
  }
  session.combatXpPool = xp >>> 0;
  session.monsterCount = 0;
}

/** CombatSession::finishVictory @ 0x12430. */
export function combatFinishVictory(session, opts = {}) {
  const { manifest = null, rng = null } = opts;
  session.combatVictory = true;
  session.foundItemSentinel = 0; /* 0x1243e clr.b -$794C */
  generateVictoryItems(session, manifest, rng);

  const party = ensureParty(session);
  let rewardCount = 0;
  for (const m of party) {
    if (partyEligibleForRewards(m)) rewardCount++;
  }
  const pool = session.combatXpPool | 0;
  if (rewardCount > 0 && pool > 0) {
    const share = Math.floor(pool / rewardCount) >>> 0;
    for (const m of party) {
      if (!partyEligibleForRewards(m)) continue;
      m.experience = ((m.experience | 0) + share) >>> 0;
    }
  }

  let gold = 0;
  const arena = session.arenaReward;
  if (arena?.active) {
    gold = arenaGoldReward(arena.color, arena.screen) >>> 0;
    for (const m of party) {
      if (!partyEligibleForRewards(m)) continue;
      m.gold = ((m.gold | 0) + gold) >>> 0;
      break;
    }
  }
  session.arenaReward = null;
  syncSessionGoldFromParty(session);
  session.combatActive = false;
  session.combatOutcome = "victory";
  session.scriptAbort = 0;
  return { xp: pool, gold };
}

/**
 * CombatSession::finishLeave — fled=true leaves coords/conds; defeat wipes.
 * Coord restore is walker-side (restoreEntryCoord) before calling with fled=false.
 */
export function combatFinishLeave(session, fled) {
  session.combatVictory = false;
  session.arenaReward = null;
  session.combatActive = false;
  session.combatOutcome = fled ? "fled" : "defeated";
  session.scriptAbort = 0;
  if (!fled) {
    session.encounterRedraw = 1;
    for (const m of ensureParty(session)) {
      if ((m.condition ?? 0) >= 0x10) m.condition = 0x81;
    }
  }
}

/**
 * Shared fight host used by OP_12/13, tile ambient, arena, 0x7F, FD.
 * Seeds already written; runs enter → CombatSession::tick loop → finish*.
 * @returns {Promise<'victory'|'fled'|'defeated'|'refused'>}
 */
export async function runCombatContract(ctx, enc, op) {
  const { session, manifest, maps, screenId, rng, note, onSessionChange } = ctx;
  const screen = maps?.screens?.[screenId] ?? null;
  const armed = combatEnter(session, { screen, manifest, rng });
  if (!armed) {
    session.scriptAbort = 0;
    note?.(`${enc?.heading ?? "Encounter"}: enter refused (live=0)`);
    onSessionChange?.(session);
    return "refused";
  }
  session.scriptAbort = 1;

  const fight = combatStartFight(session, { screen, manifest, rng });
  if (!fight) {
    session.scriptAbort = 0;
    onSessionChange?.(session);
    return "refused";
  }

  const { waitForCombatKey, waitForSpace, promptCombatResult } = ctx;
  const sprite = ctx.sprite ?? enc?.sprite ?? null;

  /* Prefer real tick loop; fall back to legacy V/N/D only if no combat key host. */
  if (typeof waitForCombatKey === "function") {
    let guard = 0;
    while (fight.state !== "Inactive" && guard++ < 10000) {
      if (combatAwaitingInput(fight)) {
        const text = combatPromptText(fight, enc);
        const keys = await waitForCombatKey(text, sprite, op);
        const ended = combatTick(fight, keys || {});
        onSessionChange?.(session);
        if (ended) break;
      } else {
        /* Should not idle without input; force ack advance. */
        combatTick(fight, { space: true });
      }
    }
    const result = fight.outcome || session.combatOutcome || "fled";
    if (result === "defeated") {
      ctx.onCombatDefeat?.();
    }
    note?.(
      `${enc?.heading ?? "Encounter"}: ${result}` +
        (result === "victory" ? ` XP=${session.combatXpPool | 0}` : "")
    );
    onSessionChange?.(session);
    return result;
  }

  /* Legacy V/N/D host (tests / callers without waitForCombatKey). */
  const modalText = `${enc?.text ?? "Encounter!"}\n\nV = victory\nN = flee\nD = defeat wipe`;
  if (waitForSpace) await waitForSpace(modalText, sprite, op);
  let result = "fled";
  if (promptCombatResult) {
    const won = await promptCombatResult(enc);
    if (won) result = "victory";
    else if (session.combatOutcome === "defeated") result = "defeated";
    else result = "fled";
  }
  if (result === "victory") {
    combatResolveVictoryKills(session, manifest, rng);
    const r = combatFinishVictory(session, { manifest, rng });
    note?.(`${enc?.heading ?? "Encounter"}: victory latch XP=${r.xp}`);
  } else if (result === "defeated") {
    note?.(`${enc?.heading ?? "Encounter"}: defeated`);
  } else {
    combatFinishLeave(session, true);
    note?.(`${enc?.heading ?? "Encounter"}: fled — script ends`);
  }
  onSessionChange?.(session);
  return result;
}

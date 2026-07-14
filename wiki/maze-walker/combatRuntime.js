/**
 * Walker CombatSession::tick port — mirrors game/src/combat/CombatSession.cpp
 * state machine + party options + round loop + attack/monster AI GS mutations.
 * Spell dispatch via combatSpells.js (CombatSession.cpp:1155–3677).
 */
"use strict";

import { ensureParty, memberSpellKnown, sessionTryPayFood, sessionTryPayGold, sessionTryPayGems } from "./sessionState.js";
import { combatFinishVictory, combatFinishLeave } from "./combatSession.js";
import {
  resolvePlayerCast as spellResolvePlayerCast,
  tickCastTarget,
  tickPartyPick,
  tickItemPick,
  applyEncaseDot,
  appendSingleEffectFeea,
  seedCombatGs,
  combatActionAckFrames,
  exportPabilTables,
  castSpellFromSheet,
  tickSheetCastAux,
  applyItemUse,
} from "./combatSpells.js";

export { castSpellFromSheet, tickSheetCastAux, applyItemUse };

export function combatSpellApi(fight) {
  return spellApi(fight);
}

/**
 * Inactive CombatSession stand-in for exploration sheet Cast/Use
 * (GameSession CharacterSheet → castSpellFromSheet / applyItemUse).
 */
export function ensureExplorationSheetFight(session, opts = {}) {
  const active = session._combatFight;
  if (active && active.state && active.state !== STATE.Inactive) {
    return active;
  }
  const party = ensureParty(session);
  const fight = {
    state: STATE.Inactive,
    session,
    manifest: opts.manifest ?? session.manifest ?? null,
    rng: opts.rng ?? null,
    party,
    partyCount: party.length,
    activePartySlot: -1,
    monsterSlots: [],
    statusLine: "",
    outcome: "none",
    explorationCast: false,
    pendingCastFlat: -1,
    pendingCastSchool: -1,
    castAux: -1,
    forceCastSchool: -1,
    skipCastCost: false,
    castLevel: 0,
    castTargetSlot: -1,
    spellActed: 0,
    msgQueue: [],
    msgIdx: 0,
    ackFrames: 0,
  };
  seedCombatGs(fight);
  session._sheetFight = fight;
  return fight;
}

/** GameSession.cpp:1859 — sheet Cast → CombatSession::castSpellFromSheet. */
export function dispatchSheetCast(session, partySlot, flat0, opts = {}) {
  const fight = ensureExplorationSheetFight(session, opts);
  castSpellFromSheet(fight, spellApi(fight), partySlot, flat0);
  return fight.statusLine || "";
}

/** GameSession.cpp:1867 — sheet Use → CombatSession::applyItemUse. */
export function dispatchSheetUse(session, partySlot, useSlot, opts = {}) {
  const fight = ensureExplorationSheetFight(session, opts);
  const backpack = useSlot >= 6;
  const slot = backpack ? useSlot - 6 : useSlot;
  applyItemUse(fight, spellApi(fight), partySlot, backpack, slot);
  return fight.statusLine || "";
}

const STATE = {
  Inactive: "Inactive",
  AwaitingSurpriseDismiss: "AwaitingSurpriseDismiss",
  AwaitingPartyOptions: "AwaitingPartyOptions",
  AwaitingBribeKind: "AwaitingBribeKind",
  AwaitingBribeAmount: "AwaitingBribeAmount",
  AwaitingCommand: "AwaitingCommand",
  AwaitingCastLevel: "AwaitingCastLevel",
  AwaitingCastNumber: "AwaitingCastNumber",
  AwaitingCastTarget: "AwaitingCastTarget",
  AwaitingPartyPick: "AwaitingPartyPick",
  AwaitingItemPick: "AwaitingItemPick",
  AwaitingAttackTarget: "AwaitingAttackTarget",
  AwaitingExchangeWith: "AwaitingExchangeWith",
  AwaitingActionAck: "AwaitingActionAck",
};

const HIT_DIV = [1, 1, 1, 3, 4, 2, 2, 1];
const SWING_DIV = [4, 4, 5, 7, 10, 5, 5, 4];
const FLEE_CHANCE = [3, 9, 24, 255];
const MON_HIT = [40, 45, 50, 55, 60, 65, 70, 75, 80, 90, 100, 120, 150, 250, 100, 200];
const PABIL_CHANCE = [0, 10, 20, 35, 50, 75, 90, 100, 1, 1, 1, 1, 0, 1, 1, 15];
const LUCK_THRESH = [4, 6, 9, 13, 15, 17, 19, 22, 26, 30, 45, 60, 75, 90, 105, 120, 135, 150, 175, 200, 225, 250, 255];

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

function restSpellBonusFactor(attr) {
  /* Movement.cpp:264 — 0x4442 thresholds. */
  let bonus = 0xfd;
  for (let i = 0; i < LUCK_THRESH.length; i++) {
    if ((attr & 0xff) <= LUCK_THRESH[i]) break;
    bonus = (bonus + 1) & 0xff;
  }
  return bonus;
}

function itemIsMelee(id) {
  return (id >= 1 && id <= 0x41) || (id >= 0x42 && id <= 0x5b);
}
function itemIsRanged(id) {
  return id >= 0x5c && id <= 0x72;
}

/** 0xF6E0 rebuild of roster +$4C..+$4F from equip (mm2_combat_tables.build_weapon_fields). */
function buildWeaponFields(member, manifest) {
  const gold = manifest?.itemsDamage ?? [];
  let bestM = [0, 0];
  let bestR = [0, 0];
  const eq = member?.equipped ?? [];
  for (const slot of eq) {
    const id = slot?.id | 0;
    if (!id) continue;
    const die = (Array.isArray(gold) ? gold[id] : 0) | 0;
    if (die <= 0) continue;
    const bonus = (slot.flags | 0) & 0x0f;
    const acc = Math.min(255, (id & 0x3f) + bonus);
    if (itemIsMelee(id) && die >= bestM[0]) bestM = [die, acc];
    if (itemIsRanged(id) && die >= bestR[0]) bestR = [die, acc];
  }
  return { w4c: bestM[0], w4d: bestM[1], w4e: bestR[0], w4f: bestR[1] };
}

function decodeMonsterSpeed(m) {
  return ((m?.speed | 0) & 0x0f) + 1;
}

function decodeMonsterSides(m) {
  const b = m?.damage | 0;
  let sides = (b & 0x1f) + 1;
  if (b & 0x20) sides = sides > 0x19 ? 0xfa : sides * 10;
  return sides > 250 ? 250 : sides;
}

function decodeMonsterAc(m) {
  const ac = m?.ac | 0;
  let v = (ac & 0x1f) + 1;
  if (ac & 0x20) v *= 10;
  return v & 0xff;
}

function decodeMonsterSwings(m) {
  return (((m?.speed2 | 0) & 0x0f) + 1) & 0xff;
}

function decodeChargeInit(m) {
  return ((((m?.speed2 | 0) >> 4) & 0x0f) + 1) & 0xff;
}

function partyMember(fight, slot) {
  const party = ensureParty(fight.session);
  if (slot < 0 || slot >= fight.partyCount || slot >= party.length) return null;
  return party[slot];
}

function partyAlive(fight, slot) {
  const m = partyMember(fight, slot);
  return m != null && (m.hpMax | 0) > 0;
}

function partyInFight(fight, slot) {
  const m = partyMember(fight, slot);
  return m != null && (m.hpMax | 0) > 0 && (m.condition | 0) <= 0x10;
}

function partyEligible(fight, slot) {
  const m = partyMember(fight, slot);
  return m != null && (m.condition | 0) < 0x80;
}

function firstAliveMonster(fight) {
  for (let i = 0; i < 11; i++) if (fight.slots[i]?.alive) return i;
  return -1;
}

function countAliveMonsters(fight) {
  let n = 0;
  for (let i = 0; i < 11; i++) if (fight.slots[i]?.alive) n++;
  return n;
}

function nthAliveMonster(fight, n) {
  let seen = 0;
  for (let i = 0; i < 11; i++) {
    if (!fight.slots[i]?.alive) continue;
    if (seen === n) return i;
    seen++;
  }
  return -1;
}

function setStatus(fight, msg) {
  fight.statusLine = msg ?? "";
}

function syncMonsterSlots(fight) {
  const session = fight.session;
  const slots = session.monsterSlots ?? new Array(10).fill(0);
  let alive = 0;
  for (let i = 0; i < 11; i++) {
    const s = fight.slots[i];
    if (i < 10) slots[i] = s?.alive ? s.type & 0xff : 0;
    if (s?.alive) alive++;
  }
  session.monsterSlots = slots;
  session.monsterCount = alive & 0xff;
  session.combatXpPool = fight.xpPool >>> 0;
}

const MRES_CHANCE = [0, 10, 20, 35, 50, 75, 90, 100];

function loadMonGlobals(fight, slot) {
  const s = fight.slots[slot];
  const g = {
    ac: 0,
    acBit6: 0,
    swings: 0,
    sides: 0,
    pabil: 0,
    pabilChance: 0,
    archer: 0,
    multiplies: 0,
    friendCount: 1,
    fleeTier: 0,
    chargeInit: 0,
    sabil: 0,
    mres: 0,
    undead: 0,
  };
  if (!s?.alive) {
    fight.monG = g;
    return g;
  }
  const m = monsterDef(fight.manifest, s.type);
  if (!m) {
    fight.monG = g;
    return g;
  }
  const pabil = m.party_verb | 0;
  g.pabil = pabil & 0x1f;
  /* Reconstruct Pabil byte: low5 = verb, bits5-7 = chance tier (decode_monsters).
   * C++ 0x4E0A: index = (pabil & $E0)>>4 into -$7464. */
  const rawPabil = (((m.party_chance | 0) & 7) << 5) | (pabil & 0x1f);
  const pchIdx = (rawPabil & 0xe0) >> 4;
  g.pabilChance = PABIL_CHANCE[pchIdx] ?? 0;
  g.sabil = m.single_effect | 0;
  g.archer = m.archer ? 1 : 0;
  g.multiplies = m.multiplies ? 1 : 0;
  g.friendCount = Math.min(255, m.add_friends | 0) || 1;
  g.fleeTier = (m.flee_tier | 0) & 3;
  g.ac = decodeMonsterAc(m);
  g.acBit6 = (m.ac | 0) & 0x40 ? 1 : 0;
  g.swings = decodeMonsterSwings(m);
  g.sides = decodeMonsterSides(m);
  g.chargeInit = decodeChargeInit(m);
  /* 0x4F7x: mres chance = -$6F2x[(mres>>5)&7] — not raw byte. */
  g.mres = MRES_CHANCE[(m.mres >> 5) & 7] ?? 0;
  g.undead = m.undead ? 1 : 0;
  fight.monG = g;
  return g;
}

function enqueueMsg(fight, msg) {
  if (!msg || fight.msgQueue.length >= 12) return;
  fight.msgQueue.push(String(msg));
}

function advanceMsgQueue(fight) {
  if (!fight.msgQueue.length) return false;
  fight.msgIdx++;
  if (fight.msgIdx >= fight.msgQueue.length) {
    fight.msgQueue = [];
    fight.msgIdx = 0;
    return false;
  }
  setStatus(fight, fight.msgQueue[fight.msgIdx]);
  return true;
}

function beginActionAck(fight) {
  fight.ackFrames = combatActionAckFrames(fight);
  fight.state = STATE.AwaitingActionAck;
}

function spellApi(fight) {
  return {
    applyHp10ED4,
    onMonsterKilled,
    loadMonGlobals,
    countAlive: countAliveMonsters,
    beginActionAck,
  };
}

function applyTreasure(fight, monType) {
  const session = fight.session;
  const m = monsterDef(fight.manifest, monType);
  if (!m) return;
  const treasure = m.treasure | 0;
  const rng = fight.rng;
  if (treasure & 0x04) {
    session.foundGems = ((session.foundGems | 0) + rngRange(rng, 1, 0x0a)) & 0xffff;
  }
  const goldTier = (treasure >> 3) & 0x03;
  if (goldTier !== 0) {
    let t = monType & 0xff;
    if (goldTier === 2) t = (t / 0x10) | 0;
    else if (goldTier >= 3) t = (t / 2) | 0;
    let typeRoll = t > 0 ? rngRange(rng, 1, t) : 0;
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

function compactMonsterSlot(fight, slot) {
  let live = fight.session.monsterCount | 0;
  if (live > 0) live--;
  fight.session.monsterCount = live & 0xff;
  if (fight.encounterLiveTotal > 0) fight.encounterLiveTotal--;
  if (fight.meleeRangeCount > live) fight.meleeRangeCount = live;
  if (slot >= 0x0a) {
    fight.slots[slot] = { type: 0, status: 0, hp: 0, maxHp: 0, speed: 0, alive: false };
    fight.monsterActed[slot] = false;
    syncMonsterSlots(fight);
    return;
  }
  const last = live < 0x0a ? live : 0x0a;
  for (let i = slot; i < last; i++) {
    fight.slots[i] = fight.slots[i + 1];
    fight.monsterActed[i] = fight.monsterActed[i + 1];
    fight.specialCharges[i] = fight.specialCharges[i + 1];
  }
  if (last < 11) {
    fight.slots[last] = { type: 0, status: 0, hp: 0, maxHp: 0, speed: 0, alive: false };
    fight.monsterActed[last] = false;
    fight.specialCharges[last] = 0;
  }
  syncMonsterSlots(fight);
}

function onMonsterKilled(fight, slot, killXp) {
  const type = fight.slots[slot]?.type | 0;
  const name = monsterDef(fight.manifest, type)?.name?.trim() || "Monster";
  applyTreasure(fight, type);
  fight.xpPool = (fight.xpPool + (killXp >>> 0)) >>> 0;
  fight.session.combatXpPool = fight.xpPool;
  setStatus(fight, `${name} goes down!`);
  enqueueMsg(fight, fight.statusLine);
  fight.slots[slot].hp = 0;
  fight.slots[slot].alive = false;
  fight.slots[slot].status = 0;
  compactMonsterSlot(fight, slot);
  if (slot >= 0x0a) {
    let live = fight.session.monsterCount | 0;
    if (live > 9) {
      const n = live - 9;
      for (let i = 0; i < n; i++) fight.xpPool = (fight.xpPool + killXp) >>> 0;
      fight.session.monsterCount = 0x0a;
      fight.encounterLiveTotal = 0x0a;
      fight.session.combatXpPool = fight.xpPool;
    }
  }
}

function applyHp10ED4(fight, slot, dmg) {
  const m = fight.slots[slot];
  if (!m?.alive) return false;
  let st = m.status;
  if (st <= 0x80) st = 0;
  st = (st & 0xef) | 0x01;
  m.status = st;
  const hp = m.hp > 0 ? m.hp : 0;
  if (hp <= dmg) {
    const def = monsterDef(fight.manifest, m.type);
    onMonsterKilled(fight, slot, (def?.xp | 0) >>> 0);
    return true;
  }
  m.hp = hp - dmg;
  return false;
}

function applyPartyDamage(fight, partySlot, dmg, monName) {
  const victim = partyMember(fight, partySlot);
  if (!victim) return;
  const vname = victim.name || "Hero";
  if (!dmg) {
    setStatus(fight, `${monName} attacks ${vname} and misses.`);
    return;
  }
  victim.condition = (victim.condition | 0) & 0xef;
  if ((victim.condition | 0) & 0x40) {
    victim.condition = 0x81;
    victim.hpMax = 0;
    if (victim.hpCurrent != null) victim.hpCurrent = 0;
    setStatus(fight, `${monName} attacks ${vname} - ${vname} falls!`);
  } else if ((victim.hpMax | 0) > dmg) {
    victim.hpMax = (victim.hpMax | 0) - dmg;
    if (victim.hpCurrent != null) victim.hpCurrent = Math.min(victim.hpCurrent | 0, victim.hpMax);
    setStatus(fight, `${monName} attacks ${vname} for ${dmg} damage.`);
  } else {
    victim.condition = (victim.condition | 0) | 0x40;
    victim.hpMax = 0;
    if (victim.hpCurrent != null) victim.hpCurrent = 0;
    setStatus(fight, `${monName} attacks ${vname} - ${vname} falls!`);
  }
}

function monsterMeleeDamage(fight, monSlot, partySlot) {
  const g = loadMonGlobals(fight, monSlot);
  const monType = fight.slots[monSlot]?.type | 0;
  let hitChance = MON_HIT[(monType >> 4) & 0x0f] ?? 40;
  const victim = partyMember(fight, partySlot);
  if (!victim) return 0;
  const ac = victim.armorClass | 0;
  let ceiling = 5;
  if (ac <= hitChance) ceiling = (hitChance - ac) & 0xff;
  if ((fight.slots[monSlot].status & 0x08) !== 0) ceiling = (ceiling / 2) | 0;
  let total = 0;
  const sides = g.sides > 0 ? g.sides : 1;
  const swings = g.swings | 0;
  for (let s = 0; s < swings; s++) {
    const raw = rngRange(fight.rng, 0x0a, 0x3f1);
    const roll = (raw / 10) | 0;
    if (ceiling > (roll & 0xff)) {
      total += rngRange(fight.rng, 1, sides);
    }
  }
  if ((fight.slots[monSlot].status & 0x04) !== 0) total = (total / 2) | 0;
  return total & 0xffff;
}

function deliverMonsterHit(fight, monSlot, targetSlot, monName) {
  let dmg = monsterMeleeDamage(fight, monSlot, targetSlot);
  if (fight.partyBlocking[targetSlot]) dmg = (dmg / 2) | 0;
  if (fight.powerShieldCtr) dmg = (dmg / 2) | 0;
  if (!fight.rangedLatch && fight.shieldCtr) dmg = (dmg / 2) | 0;
  applyPartyDamage(fight, targetSlot, dmg, monName);
  if (dmg > 0) {
    const victim = partyMember(fight, targetSlot);
    if (victim) appendSingleEffectFeea(fight, victim.name || "Hero");
  }
}

function pickMeleeTarget(fight) {
  /* 0x103BA: round-robin living front-rank. */
  const front = Math.max(1, fight.frontRankCount | 0);
  let cur = fight.combatTargetSlot | 0;
  for (let tries = 0; tries < front + 2; tries++) {
    if (cur < 0 || cur >= fight.partyCount) cur = 0;
    if (cur < front && partyAlive(fight, cur) && partyEligible(fight, cur)) {
      fight.combatTargetSlot = cur;
      return cur;
    }
    cur++;
    if (cur >= fight.partyCount) cur = 0;
  }
  return -1;
}

function pickRangedTarget(fight) {
  const pc = fight.partyCount;
  if (pc <= 0) return -1;
  let cur = rngRange(fight.rng, 1, pc) - 1;
  for (let tries = 0; tries < pc + 2; tries++) {
    if (cur < 0 || cur >= pc) cur = 0;
    const m = partyMember(fight, cur);
    if (m && (m.condition | 0) < 0x80) {
      fight.combatTargetSlot = cur;
      return cur;
    }
    cur++;
    if (cur >= pc) cur = 0;
  }
  return -1;
}

function monsterFlee(fight, slot) {
  const type = fight.slots[slot]?.type | 0;
  const name = monsterDef(fight.manifest, type)?.name?.trim() || "Monster";
  setStatus(fight, `${name} runs away!`);
  enqueueMsg(fight, fight.statusLine);
  fight.slots[slot].alive = false;
  fight.slots[slot].hp = 0;
  compactMonsterSlot(fight, slot);
}

function monsterSpecialGate(fight, slot) {
  const roll = rngRange(fight.rng, 1, 0x64);
  if ((fight.slots[slot].status & 0x40) !== 0) return false;
  const charges = fight.specialCharges[slot] | 0;
  if (!charges) return false;
  const g = loadMonGlobals(fight, slot);
  if ((g.pabilChance | 0) < roll) return false;
  fight.specialCharges[slot] = (charges - 1) & 0xff;
  return true;
}

function luckBonus4442(fight, luck) {
  let result = 0xfd;
  const tbl = fight.luckThreshTbl ?? exportPabilTables().luckThresh;
  for (let i = 0; i < tbl.length; i++) {
    if ((luck & 0xff) <= tbl[i]) break;
    result++;
  }
  return result;
}

function partyResistCheck7F0E(fight, partySlot) {
  const rec = partyMember(fight, partySlot);
  if (!rec) return 0;
  const roll = rngRange(fight.rng, 1, 0x64);
  if (roll <= 5) return 0;
  if (roll >= 0x5f) return 1;
  const level = (rec.level | 0) > 0 ? rec.level | 0 : 1;
  let thr = luckBonus4442(fight, rec.luck | 0) + level;
  if (thr < level) thr = 2;
  return thr >= roll ? 1 : 0;
}

function partyResistFlags7D94(fight, partySlot) {
  const rec = partyMember(fight, partySlot);
  if (!rec || (rec.condition | 0) >= 0x80) return;
  if (fight.resistFlagB) {
    const roll = rngRange(fight.rng, 1, 0x64);
    const thr = (rec.thievery | 0) + (fight.resistBuffA | 0);
    if (thr >= roll) return;
    fight.resistFlagB = 0;
  }
  if (fight.resistFlagA) {
    if (partyResistCheck7F0E(fight, partySlot)) {
      fight.hpApply = ((fight.hpApply | 0) / 2) | 0;
    } else fight.resistFlagA = 0;
  }
  if (fight.resistFlagC === 3 || fight.resistFlagC === 0xff) fight.resistFlagC = 0;
  else if (fight.resistFlagC) {
    let thr = fight.resistBuffC | 0;
    if (fight.resistFlagC === 1) thr += rec.might | 0;
    else if (fight.resistFlagC === 2) thr += rec.unlockSkill | 0;
    else if (fight.resistFlagC === 0x24) thr += rec.armorClass | 0;
    else thr += fight.resistFlagC | 0;
    const roll = rngRange(fight.rng, 1, 0x64);
    if (thr >= roll) fight.hpApply = ((fight.hpApply | 0) / 4) | 0;
    else fight.resistFlagC = 0;
  }
}

function shieldHalvePartyDmg(fight, dmg) {
  if (fight.powerShieldCtr) dmg = (dmg / 2) | 0;
  if (!fight.rangedLatch && fight.shieldCtr) dmg = (dmg / 2) | 0;
  return dmg;
}

function applyPabilDamage1F55E(fight, monName) {
  const target = fight.combatTargetSlot | 0;
  const saved = fight.hpApply | 0;
  partyResistFlags7D94(fight, target);
  if (fight.resistFlagB) {
    fight.hpApply = saved;
    return;
  }
  let dmg = shieldHalvePartyDmg(fight, fight.hpApply | 0);
  applyPartyDamage(fight, target, dmg, monName);
  fight.hpApply = saved;
}

function applyPabilDamageSingle(fight, monName) {
  const saved = fight.combatTargetSlot | 0;
  const tgt = pickRangedTarget(fight);
  if (tgt < 0) {
    fight.combatTargetSlot = saved;
    return;
  }
  applyPabilDamage1F55E(fight, monName);
  fight.combatTargetSlot = saved;
}

function applyPabilDamageMulti(fight, monName) {
  const pc = fight.partyCount | 0;
  if (pc <= 0) return;
  let dead = 0;
  for (let i = 0; i < pc; i++) {
    const m = partyMember(fight, i);
    if (m && (m.condition | 0) >= 0x80) dead++;
  }
  let hits = rngRange(fight.rng, 1, 4) + 3 - dead;
  if (hits > pc) hits = pc;
  if (hits < 1) hits = 1;
  const roll = rngRange(fight.rng, 1, 0x64);
  let base = 0;
  if (roll >= 0x28) base++;
  if (roll >= 0x3c) base++;
  if (roll >= 0x50) base++;
  base *= 8;
  if (base > 0x18) base = 0;
  const ord = fight.pabilTargetOrd ?? exportPabilTables().ord;
  const savedRa = fight.resistFlagA | 0;
  const savedRb = fight.resistFlagB | 0;
  const savedRc = fight.resistFlagC | 0;
  for (let n = 0; n < hits; n++) {
    const t = ord[base + n] | 0;
    if (t >= pc) continue;
    const m = partyMember(fight, t);
    if (!m || (m.condition | 0) >= 0x80) continue;
    fight.combatTargetSlot = t;
    fight.resistFlagA = savedRa;
    fight.resistFlagB = savedRb;
    fight.resistFlagC = savedRc;
    applyPabilDamage1F55E(fight, monName);
  }
}

function partyConditionResistPass1F64E(fight, partySlot) {
  const rec = partyMember(fight, partySlot);
  if (!rec) return false;
  const roll = rngRange(fight.rng, 1, 0x64);
  if (fight.resistFlagB) {
    const thr = (rec.thievery | 0) + (fight.resistBuffA | 0);
    if (thr >= roll) return false;
    fight.resistFlagB = 0;
  }
  if (fight.resistFlagA) {
    fight.resistFlagA = partyResistCheck7F0E(fight, partySlot) ? 1 : 0;
    if (fight.resistFlagA) return false;
  }
  const rc = fight.resistFlagC | 0;
  if (rc) {
    let thr = fight.resistBuffC | 0;
    if (rc === 0x10) thr += rec.might | 0;
    else if (rc === 0x15) thr += rec.luck | 0;
    else if (rc === 0x16) thr += rec.thievery | 0;
    else if (rc === 0x24) thr += rec.armorClass | 0;
    else thr += rc;
    if (thr >= roll) return false;
    fight.resistFlagC = 0;
  }
  return true;
}

function applyPabilCondition(fight, mask) {
  const pc = fight.partyCount | 0;
  const savedRa = fight.resistFlagA | 0;
  const savedRb = fight.resistFlagB | 0;
  const savedRc = fight.resistFlagC | 0;
  for (let i = 0; i < pc; i++) {
    const m = partyMember(fight, i);
    if (!m || (m.condition | 0) >= 0x80) continue;
    fight.combatTargetSlot = i;
    fight.resistFlagA = savedRa;
    fight.resistFlagB = savedRb;
    fight.resistFlagC = savedRc;
    if (partyConditionResistPass1F64E(fight, i)) m.condition = (m.condition | mask) & 0xff;
  }
}

function applyPabilConditionRandom(fight, mask) {
  const target = pickRangedTarget(fight);
  if (target < 0) return;
  const m = partyMember(fight, target);
  if (!m) return;
  if (partyConditionResistPass1F64E(fight, target)) m.condition = (m.condition | mask) & 0xff;
}

function rollPabilDamage(fight, monSlot, sides) {
  const monType = fight.slots[monSlot]?.type | 0;
  const rolls = (monType >> 4) | 0;
  const hi = sides > 0 ? sides : 1;
  let sum = 0;
  for (let i = 0; i < rolls; i++) sum += rngRange(fight.rng, 1, hi);
  fight.hpApply = (sum * 2 + 1) & 0xffff;
}

function applyPabilEffect(fight, slot, pabil) {
  const type = fight.slots[slot]?.type | 0;
  const name = monsterDef(fight.manifest, type)?.name?.trim() || "Monster";
  const idx = pabil & 0x1f;
  const tbl = exportPabilTables();
  fight.resistFlagA = (fight.pabilResA ?? tbl.resA)[idx] ?? 0;
  fight.resistFlagB = (fight.pabilResB ?? tbl.resB)[idx] ?? 0;
  fight.resistFlagC = (((fight.pabilResC ?? tbl.resC)[idx] ?? 0) - 0x11) & 0xff;
  let dmgFlag = pabil < 2 || (pabil >= 3 && pabil <= 8) || pabil === 0x18 || pabil === 0x1f;
  if (dmgFlag) {
    let dmg = fight.slots[slot].hp > 0 ? fight.slots[slot].hp : 1;
    if (type < 0xb3) dmg = Math.max(1, (dmg / 2) | 0);
    fight.hpApply = dmg;
    applyPabilDamageMulti(fight, name);
    return;
  }
  switch (pabil) {
    case 2:
      fight.buff79a5 = Math.min(0xff, (fight.buff79a5 | 0) + 1);
      break;
    case 9: {
      fight.hpApply = ((fight.slots[slot].hp / 2) | 0) + 1;
      applyPabilDamageMulti(fight, name);
      fight.eradicateSkipPrint = 1;
      onMonsterKilled(fight, slot, (monsterDef(fight.manifest, type)?.xp | 0) >>> 0);
      fight.eradicateSkipPrint = 0;
      break;
    }
    case 10:
      applyPabilCondition(fight, 0x82);
      break;
    case 11:
      for (let i = 0; i < fight.partyCount; i++) {
        const m = partyMember(fight, i);
        if (m) m.spMax = 0;
      }
      break;
    case 12:
      for (let i = 0; i < fight.partyCount; i++) {
        const m = partyMember(fight, i);
        if (m && (m.spellLevel | 0) > 0) m.spellLevel = ((m.spellLevel | 0) - 1) & 0xff;
      }
      break;
    case 13: {
      const roll = rngRange(fight.rng, 1, 0x64);
      for (let i = 0; i < fight.partyCount; i++) {
        const m = partyMember(fight, i);
        if (!m || roll <= 0x41) continue;
        const ridx = fight.session.rosterSlots?.[i] ?? i;
        if (ridx < 0x18) m.gold = 0;
        else m.gems = 0;
      }
      break;
    }
    case 14:
      rollPabilDamage(fight, slot, 3);
      applyPabilDamageMulti(fight, name);
      if (fight.partyCount >= 3) {
        const swaps = rngRange(fight.rng, 1, 8);
        const party = ensureParty(fight.session);
        const slots = fight.session.rosterSlots ?? [];
        for (let s = 0; s < swaps; s++) {
          const a = rngRange(fight.rng, 1, fight.partyCount) - 1;
          const b = rngRange(fight.rng, 1, fight.partyCount) - 1;
          if (a === b || a < 0 || b < 0) continue;
          [party[a], party[b]] = [party[b], party[a]];
          [slots[a], slots[b]] = [slots[b], slots[a]];
        }
        fight.session.rosterSlots = slots;
      }
      break;
    case 15:
      rollPabilDamage(fight, slot, 6);
      applyPabilDamageSingle(fight, name);
      break;
    case 16:
      applyPabilCondition(fight, 0x10);
      break;
    case 17:
    case 18:
      rollPabilDamage(fight, slot, 6);
      applyPabilDamageMulti(fight, name);
      break;
    case 19:
      applyPabilConditionRandom(fight, 0x81);
      break;
    case 20:
      applyPabilConditionRandom(fight, 0xff);
      break;
    case 21:
      rollPabilDamage(fight, slot, 0x28);
      applyPabilDamageSingle(fight, name);
      break;
    case 22:
      rollPabilDamage(fight, slot, 0x0c);
      applyPabilDamageMulti(fight, name);
      break;
    case 23:
      rollPabilDamage(fight, slot, 0x32);
      applyPabilDamageSingle(fight, name);
      break;
    case 24:
      break;
    case 25:
      fight.hpApply = 0x3e8;
      applyPabilDamageSingle(fight, name);
      break;
    case 26:
      rollPabilDamage(fight, slot, 0x14);
      applyPabilDamageMulti(fight, name);
      break;
    case 27: {
      const r = rngRange(fight.rng, 1, 0x0f);
      fight.hpApply = (r + 1) & 0xffff;
      applyPabilDamageSingle(fight, name);
      break;
    }
    case 28:
      applyPabilCondition(fight, 0x02);
      break;
    case 29: {
      const g = loadMonGlobals(fight, slot);
      const hi = g.sides > 0 ? g.sides : 1;
      let sum = 0;
      for (let s = 0; s < (g.swings | 0); s++) sum += rngRange(fight.rng, 1, hi);
      fight.hpApply = sum & 0xffff;
      applyPabilDamageMulti(fight, name);
      fight.eradicateSkipPrint = 1;
      onMonsterKilled(fight, slot, (monsterDef(fight.manifest, type)?.xp | 0) >>> 0);
      fight.eradicateSkipPrint = 0;
      break;
    }
    case 30:
      applyPabilCondition(fight, 0x20);
      break;
    default:
      break;
  }
}

function monsterGroupAttack(fight, slot) {
  const type = fight.slots[slot]?.type | 0;
  const def = monsterDef(fight.manifest, type);
  const name = def?.name?.trim() || "Monster";
  const g = loadMonGlobals(fight, slot);
  const verb = def?.party_verb_name || "attacks";
  setStatus(fight, `${name} ${verb}!`);
  fight.hpApply = 0;
  applyPabilEffect(fight, slot, g.pabil & 0x1f);
}

function monsterAdvance(fight, slot) {
  const g = loadMonGlobals(fight, slot);
  if (!g.acBit6) return;
  const meleeN = fight.meleeRangeCount | 0;
  let swapped = false;
  for (let i = 0; i < meleeN && i < 11; i++) {
    if (i === slot || !fight.slots[i]?.alive) continue;
    const gi = loadMonGlobals(fight, i);
    if (gi.acBit6) continue;
    const tmp = fight.slots[slot];
    fight.slots[slot] = fight.slots[i];
    fight.slots[i] = tmp;
    const acted = fight.monsterActed[slot];
    fight.monsterActed[slot] = fight.monsterActed[i];
    fight.monsterActed[i] = acted;
    const ch = fight.specialCharges[slot];
    fight.specialCharges[slot] = fight.specialCharges[i];
    fight.specialCharges[i] = ch;
    fight.activeMonsterSlot = i;
    const name = monsterDef(fight.manifest, fight.slots[i].type)?.name?.trim() || "Monster";
    setStatus(fight, `${name} advances!`);
    enqueueMsg(fight, fight.statusLine);
    beginActionAck(fight);
    swapped = true;
    break;
  }
  if (!swapped) {
    const name = monsterDef(fight.manifest, fight.slots[slot].type)?.name?.trim() || "Monster";
    if (g.multiplies && (fight.session.monsterCount | 0) < 11) {
      /* adds friends latch path (simplified) */
      setStatus(fight, `${name} adds friends!`);
    } else {
      setStatus(fight, `${name} waits for opening!`);
    }
    enqueueMsg(fight, fight.statusLine);
    beginActionAck(fight);
  }
  syncMonsterSlots(fight);
}

function resolveMonsterTurn(fight, slot) {
  fight.activeMonsterSlot = slot;
  fight.activePartySlot = -1;
  if (slot < 0 || !fight.slots[slot]?.alive) return;
  const m = fight.slots[slot];
  const name = monsterDef(fight.manifest, m.type)?.name?.trim() || "Monster";
  fight.monsterActed[slot] = true;
  loadMonGlobals(fight, slot);

  if ((m.status & 0x30) !== 0) return;
  if ((m.status | 0) >= 0x80) {
    applyEncaseDot(fight, spellApi(fight), slot);
    beginActionAck(fight);
    return;
  }
  if ((m.status & 0x40) !== 0) {
    setStatus(fight, `${name} wanders mindlessly!`);
    enqueueMsg(fight, fight.statusLine);
    beginActionAck(fight);
    return;
  }

  if (!fight.entrapment) {
    const tier = (fight.monG.fleeTier | 0) & 3;
    const chance = FLEE_CHANCE[tier] ?? 255;
    const partyMod = fight.pickerTierMod | 0;
    if (chance < partyMod) {
      const roll = rngRange(fight.rng, 1, 0x64);
      if (roll <= 0x32) {
        monsterFlee(fight, slot);
        return;
      }
    }
  }

  if (monsterSpecialGate(fight, slot)) {
    const pabil = fight.monG.pabil & 0x1f;
    if (pabil < 0x0f || pabil === 0x1d || pabil >= 0x1f) {
      monsterGroupAttack(fight, slot);
    } else {
      monsterGroupAttack(fight, slot);
    }
  } else {
    const meleeN = fight.meleeRangeCount | 0;
    if (slot < meleeN) {
      fight.rangedLatch = 0;
      const tgt = pickMeleeTarget(fight);
      if (tgt >= 0) deliverMonsterHit(fight, slot, tgt, name);
    } else if (fight.monG.archer) {
      const roll = rngRange(fight.rng, 1, 0x64);
      if (roll <= 0x50) {
        fight.rangedLatch = 1;
        const tgt = pickRangedTarget(fight);
        if (tgt >= 0) deliverMonsterHit(fight, slot, tgt, name);
      } else {
        monsterAdvance(fight, slot);
        return;
      }
    } else {
      monsterAdvance(fight, slot);
      return;
    }
  }
  fight.rangedLatch = 0;
}

function commandFlags(fight) {
  const slot = fight.activePartySlot;
  const rec = partyMember(fight, slot);
  const out = { melee: false, shoot: false, cast: false };
  if (!rec) return out;
  out.melee = slot < fight.frontRankCount;
  const w = buildWeaponFields(rec, fight.manifest);
  const bow = w.w4e !== 0;
  out.shoot = ((rec.classId | 0) === 2 || slot >= fight.frontRankCount) && bow;
  out.cast =
    ((rec.condition | 0) & 0x02) === 0 && (rec.spellLevel | 0) !== 0 && (rec.spCurrent | 0) > 0;
  return out;
}

function recomputeRangeCounts(fight) {
  const viewMode = fight.outdoor ? 1 : 0;
  const handicap = fight.encounterMode | 0;
  const partyCount = fight.partyCount;
  let melee;
  if (viewMode === 0) melee = ((rngRange(fight.rng, 10, 39) / 10) | 0) + 3;
  else melee = ((partyCount / 2) | 0) + ((rngRange(fight.rng, 10, 69) / 10) | 0);
  if (handicap === 2) melee = (melee / 2) | 0;
  else if (handicap === 3) melee *= 2;
  let alive = 0;
  for (let i = 0; i < 11; i++) if (fight.slots[i]?.alive) alive++;
  const live = fight.encounterLiveTotal > alive ? fight.encounterLiveTotal : alive;
  if (melee > live) melee = live;
  if (melee > 10) melee = 10;
  fight.meleeRangeCount = melee;

  let front;
  if (viewMode === 0) front = ((((rngRange(fight.rng, 10, 79) / 10) | 0) + 3) >> 1);
  else {
    front = partyCount;
    if (front >= 6) front = ((((rngRange(fight.rng, 10, 39) / 10) | 0) / 2) | 0) + partyCount - 2;
  }
  if (handicap === 3) front *= 2;
  if (handicap === 2) {
    if (partyCount < 2) front = 2;
    front -= 1;
  }
  if (front > partyCount) front = partyCount;
  fight.frontRankCount = front;
}

function beginRound(fight) {
  fight.activeMonsterSlot = -1;
  for (let i = 0; i < 11; i++) fight.monsterActed[i] = false;
  for (let i = 0; i < 8; i++) {
    fight.partyActed[i] = false;
    fight.partyBlocking[i] = false;
  }
  recomputeRangeCounts(fight);
  const pc = Math.max(1, fight.partyCount);
  fight.combatTargetSlot = rngRange(fight.rng, 1, pc) - 1;
  syncMonsterSlots(fight);
}

function endFightVictory(fight) {
  const result = combatFinishVictory(fight.session, { manifest: fight.manifest, rng: fight.rng });
  const xp = (result?.xp ?? fight.session.combatXpPool ?? 0) >>> 0;
  const gold = (result?.gold ?? 0) >>> 0;
  fight.state = STATE.Inactive;
  fight.outcome = "victory";
  /* CombatSession::finishVictory status_line_ */
  if (gold > 0) {
    setStatus(fight, `You are victorious! ${xp} XP earned. Winner, you receive ${gold} gold.`);
  } else {
    setStatus(fight, `You are victorious! ${xp} experience points earned.`);
  }
}

function endFightLeave(fight, fled) {
  combatFinishLeave(fight.session, !!fled);
  fight.state = STATE.Inactive;
  fight.outcome = fled ? "fled" : "defeated";
  /* CombatSession::finishLeave — overwrites any prior Success! line. */
  setStatus(fight, fled ? "The party flees!" : "The party has been defeated...");
}

function checkOutcome(fight) {
  if (firstAliveMonster(fight) < 0) {
    endFightVictory(fight);
    return true;
  }
  if (fight.partyCount <= 0) {
    const fled = fight.partyRanLatch || fight.timeDistort;
    endFightLeave(fight, !!fled);
    return true;
  }
  for (let i = 0; i < fight.partyCount; i++) {
    if (partyInFight(fight, i)) return false;
  }
  endFightLeave(fight, false);
  return true;
}

function runUntilDecisionOrEnd(fight) {
  for (;;) {
    if (checkOutcome(fight)) return;
    let bestMon = -1;
    let bestMonSp = -1;
    for (let i = 0; i < 11; i++) {
      if (fight.slots[i]?.alive && !fight.monsterActed[i] && fight.slots[i].speed > bestMonSp) {
        bestMon = i;
        bestMonSp = fight.slots[i].speed;
      }
    }
    let bestParty = -1;
    let bestPartySp = -1;
    for (let i = 0; i < fight.partyCount; i++) {
      if (!partyAlive(fight, i) || fight.partyActed[i]) continue;
      const sp = partyMember(fight, i)?.speed | 0;
      if (sp > bestPartySp) {
        bestParty = i;
        bestPartySp = sp;
      }
    }
    if (bestMon < 0 && bestParty < 0) {
      if (fight.partyRanLatch || fight.timeDistort) {
        endFightLeave(fight, true);
        return;
      }
      beginRound(fight);
      continue;
    }
    if (bestParty >= 0 && (bestMon < 0 || bestPartySp > bestMonSp)) {
      fight.activeMonsterSlot = -1;
      fight.activePartySlot = bestParty;
      fight.partyActed[bestParty] = true;
      const ri = partyMember(fight, bestParty);
      if (ri && (ri.condition | 0) >= 0x10) {
        fight.activePartySlot = -1;
        continue;
      }
      fight.state = STATE.AwaitingCommand;
      setStatus(fight, "");
      return;
    }
    resolveMonsterTurn(fight, bestMon);
    fight.monsterActed[bestMon] = true;
    if (checkOutcome(fight)) return;
    if (fight.state !== STATE.AwaitingActionAck) beginActionAck(fight);
    return;
  }
}

function startRoundLoop(fight) {
  recomputeRangeCounts(fight);
  runUntilDecisionOrEnd(fight);
}

function resolvePlayerAttack(fight, shooting, targetSlot) {
  const target = targetSlot >= 0 ? targetSlot : firstAliveMonster(fight);
  if (target < 0 || !fight.slots[target]?.alive) return;
  const attacker = partyMember(fight, fight.activePartySlot);
  if (!attacker) return;
  const name = attacker.name || "Hero";
  const g = loadMonGlobals(fight, target);
  const monName = monsterDef(fight.manifest, fight.slots[target].type)?.name?.trim() || "Monster";
  const w = buildWeaponFields(attacker, fight.manifest);
  const cls = attacker.classId | 0;
  const level = (attacker.level | 0) > 0 ? attacker.level | 0 : 1;
  const hd = HIT_DIV[cls < 8 ? cls : 0] || 1;
  const sd = SWING_DIV[cls < 8 ? cls : 0] || 1;
  const hitBonus = (level / hd) | 0;
  let swings = ((level / sd) | 0) + 1;
  let sides = shooting ? w.w4e || w.w4c || 1 : w.w4c || 1;
  let hitAcc = shooting ? w.w4f || w.w4d : w.w4d;
  let dmgAdd = hitAcc;
  if (shooting) {
    if (cls === 2) {
      const lv = Math.min(0x64, level);
      dmgAdd = rngRange(fight.rng, 1, lv > 0 ? lv : 1);
    } else {
      sides = w.w4e || 1;
    }
  }
  dmgAdd = (dmgAdd + restSpellBonusFactor(attacker.might | 0)) & 0xff;
  hitAcc = (hitAcc + restSpellBonusFactor(attacker.accuracy | 0) + (fight.blessCtr | 0)) & 0xff;

  const verb = shooting ? "shoots" : "attacks";
  let hits = 0;
  let dmg = 0;
  for (let s = 0; s < swings; s++) {
    let hit = false;
    const r100 = rngRange(fight.rng, 1, 0x64);
    if (r100 < 6) hit = true;
    else if (r100 < 9) hit = false;
    else {
      let base = (attacker.condition | 0) & 0x01 ? 3 : 0x19;
      let ceiling = base + hitBonus;
      if (ceiling > 0xfa) ceiling = 0xfa;
      let roll = rngRange(fight.rng, 1, ceiling > 0 ? ceiling : 1) + hitAcc;
      if (roll > 0xff) hit = true;
      else if (roll <= 0x0a) hit = false;
      else {
        let lo = roll & 0xff;
        lo -= fight.buff79a5 | 0;
        if (lo >= 0x80) hit = false;
        else hit = lo >= (g.ac | 0);
      }
    }
    if (!hit) continue;
    hits++;
    let piece = rngRange(fight.rng, 1, sides > 0 ? sides : 1) + dmgAdd;
    if (piece > 0xfa) piece = 1;
    dmg += piece;
  }
  if (hits > 0 && fight.holyBonusCtr) dmg += fight.holyBonusCtr | 0;
  if (!shooting) {
    let luck = Math.min(0x64, attacker.spellLevel | 0);
    const lr = rngRange(fight.rng, 1, luck + 0x64);
    if (cls === 5 && (lr > 0x5a || lr < 5)) dmg *= 2;
    else if (cls === 6 && (lr > 0x5e || lr < 5)) dmg *= 4;
  }
  if (hits === 0 || dmg === 0) {
    setStatus(fight, `${name} ${verb} ${monName} - ${monName} is not affected!`);
    syncMonsterSlots(fight);
    return;
  }
  const killed = applyHp10ED4(fight, target, dmg & 0xffff);
  if (killed) setStatus(fight, `${name} ${verb} ${monName} for ${dmg}.`);
  else {
    setStatus(fight, `${name} ${verb} ${monName} for ${dmg} damage.`);
    syncMonsterSlots(fight);
  }
}

function beginPlayerStrike(fight, shooting, pickTarget) {
  const alive = countAliveMonsters(fight);
  if (alive <= 0) return;
  if (!pickTarget || alive <= 1) {
    resolvePlayerAttack(fight, shooting, nthAliveMonster(fight, 0));
    return;
  }
  fight.attackPickShooting = shooting;
  fight.state = STATE.AwaitingAttackTarget;
  setStatus(fight, `${shooting ? "Shoot" : "Fight"} which (A-${String.fromCharCode(0x41 + alive - 1)})?`);
}

function exchangePartySlots(fight, a, b) {
  const party = ensureParty(fight.session);
  if (a < 0 || b < 0 || a >= fight.partyCount || b >= fight.partyCount) return;
  const tmp = party[a];
  party[a] = party[b];
  party[b] = tmp;
  const acted = fight.partyActed[a];
  fight.partyActed[a] = fight.partyActed[b];
  fight.partyActed[b] = acted;
  const blk = fight.partyBlocking[a];
  fight.partyBlocking[a] = fight.partyBlocking[b];
  fight.partyBlocking[b] = blk;
}

function shrinkPartyAfterCharRun(fight, runner) {
  if (fight.partyCount <= 0 || runner < 0 || runner >= fight.partyCount) return;
  const last = fight.partyCount - 1;
  if (runner < last) exchangePartySlots(fight, runner, last);
  fight.partyActed[last] = false;
  fight.partyBlocking[last] = false;
  fight.partyCount--;
  if (fight.frontRankCount > fight.partyCount) fight.frontRankCount = fight.partyCount;
  fight.combatTargetSlot = 0;
}

function resolvePlayerRun(fight) {
  const rec = partyMember(fight, fight.activePartySlot);
  const name = rec?.name || "Hero";
  const thresh = fight.retreatDiff | 0;
  const roll = rngRange(fight.rng, 1, 0x64);
  if (roll < thresh) {
    fight.partyRanLatch = 1;
    shrinkPartyAfterCharRun(fight, fight.activePartySlot);
    setStatus(fight, `${name} flees!`);
    return;
  }
  setStatus(fight, `${name} fails to flee!`);
}

function resolveBribeTry(fight) {
  const amountLo = fight.bribeAmount & 0xff;
  const mon0 = fight.slots[0]?.type | 0;
  if (fight.bribeKind !== 2 && amountLo < mon0) {
    setStatus(fight, "Your bribe is refused!");
    fight.state = STATE.AwaitingPartyOptions;
    return;
  }
  let paid = false;
  const session = fight.session;
  if (fight.bribeKind === 1) {
    paid =
      (session.bribeFood | 0) !== 0 &&
      fight.bribeRoll >= fight.bribeDemand &&
      sessionTryPayFood(session, amountLo);
  } else if (fight.bribeKind === 2) {
    paid =
      (session.bribeGold | 0) !== 0 &&
      fight.bribeRoll >= fight.bribeDemand &&
      sessionTryPayGold(session, fight.bribeAmount);
  } else if (fight.bribeKind === 3) {
    paid =
      (session.bribeGems | 0) !== 0 &&
      fight.bribeRoll >= fight.bribeDemand &&
      sessionTryPayGems(session, fight.bribeAmount);
  }
  if (paid) {
    endFightLeave(fight, true);
    return;
  }
  setStatus(fight, "Your bribe is refused!");
  fight.state = STATE.AwaitingPartyOptions;
}

function tickBribe(fight, key, escape) {
  if (fight.state === STATE.AwaitingBribeKind) {
    if (escape || key === "\u001b") {
      fight.state = STATE.AwaitingPartyOptions;
      setStatus(fight, "");
      return true;
    }
    if (key < "1" || key > "3") return false;
    fight.bribeKind = key.charCodeAt(0) - 0x30;
    fight.bribeDigits = 0;
    fight.bribeAmount = 0;
    fight.state = STATE.AwaitingBribeAmount;
    setStatus(fight, "");
    return true;
  }
  if (fight.state !== STATE.AwaitingBribeAmount) return false;
  if (escape || key === "\u001b") {
    fight.state = STATE.AwaitingPartyOptions;
    setStatus(fight, "");
    return true;
  }
  if (key >= "0" && key <= "9" && fight.bribeDigits < 4) {
    fight.bribeAmount = fight.bribeAmount * 10 + (key.charCodeAt(0) - 0x30);
    fight.bribeDigits++;
    return false;
  }
  if (key !== "\r" && key !== "\n" && key !== " ") return false;
  if (fight.bribeDigits === 0 || fight.bribeAmount === 0) {
    fight.state = STATE.AwaitingPartyOptions;
    setStatus(fight, "");
    return true;
  }
  resolveBribeTry(fight);
  return true;
}

function resolvePartyHide(fight) {
  const roll = rngRange(fight.rng, 1, 100);
  let sum = 0;
  let n = 0;
  for (let i = 0; i < fight.partyCount; i++) {
    if (!partyAlive(fight, i)) continue;
    sum += partyMember(fight, i)?.thievery | 0;
    n++;
  }
  let thresh = n > 0 ? (sum / n) | 0 : 0;
  if (thresh > 0xff) thresh = 0xff;
  if (roll < thresh) {
    endFightLeave(fight, true);
    return;
  }
  setStatus(fight, "You failed to hide!");
  fight.state = STATE.AwaitingPartyOptions;
}

function resolvePartyRun(fight) {
  const mode = fight.encounterMode | 0;
  let ok = mode === 2;
  if (!ok) {
    const roll = rngRange(fight.rng, 1, 0x64);
    ok = roll < (fight.retreatDiff | 0);
  }
  if (ok) {
    endFightLeave(fight, true);
    return;
  }
  setStatus(fight, "The party fails to run!");
  startRoundLoop(fight);
}

/** Cast dispatch — CombatSession.cpp:1155+ via combatSpells.js */
function resolvePlayerCast(fight, flat0) {
  spellResolvePlayerCast(fight, spellApi(fight), flat0);
}

function tickCastPicker(fight, key) {
  if (key < "1" || key > "9") return false;
  const digit = key.charCodeAt(0) - 0x30;
  const rec = partyMember(fight, fight.activePartySlot);
  if (!rec) {
    fight.state = STATE.AwaitingCommand;
    return true;
  }
  if (fight.state === STATE.AwaitingCastLevel) {
    const maxSl = rec.spellLevel | 0;
    if (digit < 1 || digit > maxSl || digit > 9) return false;
    fight.castLevel = digit;
    fight.state = STATE.AwaitingCastNumber;
    return true;
  }
  const flat = (fight.castLevel - 1) * 7 + (digit - 1);
  if (flat < 0 || flat > 47 || !memberSpellKnown(rec, flat)) return false;
  resolvePlayerCast(fight, flat);
  fight.castLevel = 0;
  if (
    fight.state !== STATE.AwaitingCastTarget &&
    fight.state !== STATE.AwaitingPartyPick &&
    fight.state !== STATE.AwaitingItemPick
  ) {
    beginActionAck(fight);
  }
  return true;
}

/**
 * After combatEnter succeeds — unpack slots and enter surprise / party-options UI.
 * @returns {object|null} fight handle
 */
export function combatStartFight(session, opts = {}) {
  const { screen = null, manifest = null, rng = null } = opts;
  if (!session.combatActive) return null;
  const party = ensureParty(session);
  const live = session.monsterCount | 0;
  const slotCount = live < 11 ? live : 11;
  const overflow = session.encounterOverflowType | 0;
  const slots = [];
  const specialCharges = [];
  for (let i = 0; i < 11; i++) {
    if (i >= slotCount) {
      slots.push({ type: 0, status: 0, hp: 0, maxHp: 0, speed: 0, alive: false });
      specialCharges.push(0);
      continue;
    }
    const type = i < 10 ? (session.monsterSlots?.[i] ?? 0) & 0xff : overflow & 0xff;
    const def = monsterDef(manifest, type);
    const hp = def?.hp | 0;
    slots.push({
      type,
      status: hp > 0 ? 1 : 0,
      hp,
      maxHp: hp,
      speed: decodeMonsterSpeed(def),
      alive: hp > 0,
    });
    specialCharges.push(decodeChargeInit(def));
  }

  const fight = {
    session,
    manifest,
    rng,
    screen,
    state: STATE.AwaitingPartyOptions,
    outcome: "none",
    statusLine: "",
    slots,
    specialCharges,
    monsterActed: new Array(11).fill(false),
    partyActed: new Array(8).fill(false),
    partyBlocking: new Array(8).fill(false),
    partyCount: Math.min(8, party.length),
    activePartySlot: -1,
    activeMonsterSlot: -1,
    meleeRangeCount: 0,
    frontRankCount: 0,
    encounterLiveTotal: live,
    encounterMode: session.encounterMode | 0,
    surpriseMode: 0,
    xpPool: 0,
    retreatDiff: screen?.encounter?.retreat ?? session.retreatDiff ?? 0,
    bribeDemand: screen?.encounter?.bribeDemand ?? 100,
    bribeKind: 0,
    bribeDigits: 0,
    bribeAmount: 0,
    bribeRoll: 0,
    outdoor: !!screen?.outdoor,
    pickerTierMod: session.pickerTierMod | 0,
    combatTargetSlot: 0,
    attackPickShooting: false,
    castLevel: 0,
    pendingCastFlat: -1,
    pendingCastSchool: -1,
    castAux: -1,
    forceCastSchool: -1,
    skipCastCost: false,
    explorationCast: false,
    castTargetSlot: -1,
    spellActed: 0,
    msgQueue: [],
    msgIdx: 0,
    ackFrames: 0,
    partyRanLatch: 0,
    timeDistort: 0,
    entrapment: 0,
    blessCtr: 0,
    shieldCtr: 0,
    powerShieldCtr: 0,
    holyBonusCtr: 0,
    buff79a5: 0,
    rangedLatch: 0,
    hpApply: 0,
    monG: null,
  };

  seedCombatGs(fight);

  /* Surprise @ 0x12EE2 / beginEncounterUi */
  let mode = fight.encounterMode | 0;
  if (mode === 3) {
    fight.surpriseMode = 3;
    setStatus(fight, "The monsters are surprised!");
    fight.state = STATE.AwaitingSurpriseDismiss;
  } else if (mode === 0) {
    const roll = rngRange(rng, 1, 100);
    let frontSpeed = 10;
    for (let i = 0; i < fight.partyCount; i++) {
      if (!partyAlive(fight, i)) continue;
      frontSpeed = partyMember(fight, i)?.speed | 0;
      break;
    }
    if (roll <= 40 && roll <= frontSpeed) {
      fight.surpriseMode = 2;
      fight.encounterMode = 2;
      session.encounterMode = 2;
      setStatus(fight, "The monsters surprised you!");
      fight.state = STATE.AwaitingSurpriseDismiss;
    } else if ((session.levitateFlag | 0) === 0 && roll >= 90) {
      fight.surpriseMode = 3;
      fight.encounterMode = 3;
      session.encounterMode = 3;
      setStatus(fight, "The monsters are surprised!");
      fight.state = STATE.AwaitingSurpriseDismiss;
    }
  } else if (fight.surpriseMode === 2) {
    setStatus(fight, "The monsters surprised you!");
    fight.state = STATE.AwaitingSurpriseDismiss;
  }

  session._combatFight = fight;
  syncMonsterSlots(fight);
  return fight;
}

export function combatAwaitingInput(fight) {
  if (!fight || fight.state === STATE.Inactive) return false;
  return (
    fight.state === STATE.AwaitingSurpriseDismiss ||
    fight.state === STATE.AwaitingPartyOptions ||
    fight.state === STATE.AwaitingBribeKind ||
    fight.state === STATE.AwaitingBribeAmount ||
    fight.state === STATE.AwaitingCommand ||
    fight.state === STATE.AwaitingCastLevel ||
    fight.state === STATE.AwaitingCastNumber ||
    fight.state === STATE.AwaitingCastTarget ||
    fight.state === STATE.AwaitingPartyPick ||
    fight.state === STATE.AwaitingItemPick ||
    fight.state === STATE.AwaitingAttackTarget ||
    fight.state === STATE.AwaitingExchangeWith ||
    fight.state === STATE.AwaitingActionAck
  );
}

export function combatPromptText(fight, enc) {
  if (!fight) return "";
  /* Mirror gfx::drawCombatOptionsBar + drawCombatRightColumn text — no walker chrome. */
  void enc;
  const lines = [];
  const labeled =
    fight.state !== STATE.Inactive &&
    fight.state !== STATE.AwaitingPartyOptions &&
    fight.state !== STATE.AwaitingBribeKind &&
    fight.state !== STATE.AwaitingBribeAmount &&
    fight.state !== STATE.AwaitingSurpriseDismiss &&
    fight.state !== STATE.AwaitingPartyPick &&
    fight.state !== STATE.AwaitingItemPick;

  /* Right-column monster list (CombatPanel.cpp). Pre-round: names only. Round: " A) Name". */
  const monLines = [];
  if (fight.state !== STATE.Inactive) {
    let listed = 0;
    for (let i = 0; i < 11 && listed < 10; i++) {
      if (!fight.slots[i]?.alive) continue;
      const n =
        monsterDef(fight.manifest, fight.slots[i].type)?.name?.trim() || `Mon#${fight.slots[i].type}`;
      if (labeled) {
        const letter = String.fromCharCode(0x41 + listed);
        const check = listed < (fight.meleeRangeCount | 0) ? "\u0017" : " ";
        const hurt =
          fight.slots[i].hp > 0 && fight.slots[i].hp < fight.slots[i].maxHp ? "/Hurt" : "";
        monLines.push(`${check}${letter}) ${n}${hurt}`);
      } else {
        monLines.push(n);
      }
      listed++;
    }
  }
  if (monLines.length) lines.push(monLines.join("\n"));

  if (fight.state === STATE.Inactive) {
    if (fight.statusLine) lines.push(fight.statusLine);
    return lines.filter((s) => s != null && s !== "").join("\n");
  }

  switch (fight.state) {
    case STATE.AwaitingSurpriseDismiss:
      if (fight.statusLine) lines.push(fight.statusLine);
      break;
    case STATE.AwaitingPartyOptions:
      /* string @ 0x13222 */
      lines.push("Options: A-Attack B-Bribe H-Hide R-Run");
      break;
    case STATE.AwaitingBribeKind:
      /* string @ 0x13249 */
      lines.push("Bribe with:  1-Food  2-Gold  3-Gems");
      break;
    case STATE.AwaitingBribeAmount:
      /* string @ 0x1326D — digits accumulate off-band in C++ */
      lines.push(`How much?${fight.bribeAmount ? ` ${fight.bribeAmount}` : ""}`);
      break;
    case STATE.AwaitingCommand: {
      const rec = partyMember(fight, fight.activePartySlot);
      const flags = commandFlags(fight);
      /* combat_command_bar_build @ 0x11866 */
      lines.push(" Options for:");
      lines.push(` ${rec?.name || "Hero"}`);
      const cmds = [];
      if (flags.melee) {
        cmds.push("A-Attack", "F-Fight");
      }
      if (flags.shoot) cmds.push("S-Shoot");
      if (flags.cast) cmds.push("C-Cast");
      cmds.push("U-Use", "B-Block", "R-Run", "E-Exch", "V-View");
      lines.push(cmds.join("  "));
      break;
    }
    case STATE.AwaitingCastLevel:
      lines.push(" Spell Level: ");
      break;
    case STATE.AwaitingCastNumber:
      lines.push(` Spell Level: ${fight.castLevel}`);
      lines.push("Number: ");
      break;
    case STATE.AwaitingCastTarget:
    case STATE.AwaitingAttackTarget:
      lines.push(fight.statusLine || "Which monster?");
      break;
    case STATE.AwaitingPartyPick:
      if (fight.statusLine) lines.push(fight.statusLine);
      break;
    case STATE.AwaitingItemPick:
      lines.push(fight.statusLine || "Use which (1-6/A-F)?");
      break;
    case STATE.AwaitingExchangeWith:
      lines.push(fight.statusLine || `Exchange with (1-${fight.partyCount})?`);
      break;
    case STATE.AwaitingActionAck:
      if (fight.statusLine) lines.push(fight.statusLine);
      break;
    default:
      if (fight.statusLine) lines.push(fight.statusLine);
      break;
  }
  return lines.filter((s) => s != null && s !== "").join("\n");
}

/**
 * Advance one input tick. keys: { ascii, escape, space, enter, ctrl }
 * @returns {boolean} true if combat ended
 */
export function combatTick(fight, keys = {}) {
  if (!fight || fight.state === STATE.Inactive) return true;
  const ascii = keys.ascii != null ? keys.ascii : 0;
  const ch = ascii ? String.fromCharCode(ascii) : "";
  const upper = ch.toUpperCase();
  const escape = !!keys.escape;

  if (fight.state === STATE.AwaitingActionAck) {
    const keyed = ascii !== 0 || keys.space || keys.enter;
    if (!keyed && fight.ackFrames > 0) {
      fight.ackFrames--;
      return false;
    }
    fight.ackFrames = 0;
    if (advanceMsgQueue(fight)) {
      fight.ackFrames = combatActionAckFrames(fight);
      return false;
    }
    runUntilDecisionOrEnd(fight);
    return fight.state === STATE.Inactive;
  }

  if (fight.state === STATE.AwaitingSurpriseDismiss) {
    if (!ascii && !keys.space && !keys.enter) return false;
    if (fight.surpriseMode === 3) startRoundLoop(fight);
    else {
      fight.state = STATE.AwaitingPartyOptions;
      setStatus(fight, "");
    }
    return fight.state === STATE.Inactive;
  }

  if (fight.state === STATE.AwaitingPartyOptions) {
    if (upper !== "A" && upper !== "B" && upper !== "H" && upper !== "R") return false;
    if (upper === "A") startRoundLoop(fight);
    else if (upper === "B") {
      fight.bribeKind = 0;
      fight.bribeDigits = 0;
      fight.bribeAmount = 0;
      fight.bribeRoll = rngRange(fight.rng, 1, 100) & 0xff;
      fight.state = STATE.AwaitingBribeKind;
      setStatus(fight, "");
    } else if (upper === "H") resolvePartyHide(fight);
    else if (upper === "R") resolvePartyRun(fight);
    return fight.state === STATE.Inactive;
  }

  if (fight.state === STATE.AwaitingBribeKind || fight.state === STATE.AwaitingBribeAmount) {
    if (!ascii && !escape) return false;
    if (!tickBribe(fight, ch, escape)) return false;
    return fight.state === STATE.Inactive;
  }

  if (fight.state === STATE.AwaitingCastLevel || fight.state === STATE.AwaitingCastNumber) {
    if (!ascii && !escape) return false;
    if (escape || ascii === 0x1b) {
      fight.castLevel = 0;
      fight.pendingCastFlat = -1;
      fight.state = STATE.AwaitingCommand;
      setStatus(fight, "");
      return false;
    }
    if (!tickCastPicker(fight, upper || ch)) return false;
    if (
      fight.state === STATE.AwaitingCastLevel ||
      fight.state === STATE.AwaitingCastNumber ||
      fight.state === STATE.AwaitingCastTarget ||
      fight.state === STATE.AwaitingPartyPick ||
      fight.state === STATE.AwaitingItemPick
    ) {
      return false;
    }
    if (fight.state === STATE.AwaitingActionAck) {
      fight.partyActed[fight.activePartySlot] = true;
      fight.activePartySlot = -1;
    }
    return false;
  }

  if (fight.state === STATE.AwaitingCastTarget) {
    if (!ascii && !escape) return false;
    if (escape || ascii === 0x1b) {
      fight.pendingCastFlat = -1;
      fight.castTargetSlot = -1;
      fight.spellActed = 0;
      fight.state = STATE.AwaitingCommand;
      setStatus(fight, "");
      return false;
    }
    if (!tickCastTarget(fight, spellApi(fight), upper || ch)) return false;
    if (fight.state === STATE.AwaitingActionAck) {
      fight.partyActed[fight.activePartySlot] = true;
      fight.activePartySlot = -1;
    }
    return false;
  }

  if (fight.state === STATE.AwaitingPartyPick) {
    if (!ascii && !escape) return false;
    if (escape || ascii === 0x1b) {
      fight.pendingCastFlat = -1;
      fight.pendingCastSchool = -1;
      fight.castAux = -1;
      fight.spellActed = 0;
      fight.state = STATE.AwaitingCommand;
      setStatus(fight, "");
      return false;
    }
    if (!tickPartyPick(fight, spellApi(fight), ch)) return false;
    if (fight.state === STATE.AwaitingActionAck) {
      fight.partyActed[fight.activePartySlot] = true;
      fight.activePartySlot = -1;
    }
    return false;
  }

  if (fight.state === STATE.AwaitingItemPick) {
    if (!ascii && !escape) return false;
    if (escape || ascii === 0x1b) {
      fight.pendingCastFlat = -1;
      fight.pendingCastSchool = -1;
      fight.spellActed = 0;
      fight.state = STATE.AwaitingCommand;
      setStatus(fight, "");
      return false;
    }
    if (!tickItemPick(fight, spellApi(fight), upper || ch)) return false;
    if (fight.state === STATE.AwaitingActionAck) {
      fight.partyActed[fight.activePartySlot] = true;
      fight.activePartySlot = -1;
    }
    return false;
  }

  if (fight.state === STATE.AwaitingAttackTarget) {
    if (!ascii && !escape) return false;
    if (escape || ascii === 0x1b) {
      fight.attackPickShooting = false;
      fight.state = STATE.AwaitingCommand;
      setStatus(fight, "");
      return false;
    }
    if (upper < "A" || upper > "J") return false;
    const letter = upper.charCodeAt(0) - 0x41;
    const alive = countAliveMonsters(fight);
    if (letter < 0 || letter >= alive) return false;
    const slot = nthAliveMonster(fight, letter);
    const shooting = fight.attackPickShooting;
    fight.attackPickShooting = false;
    resolvePlayerAttack(fight, shooting, slot);
    beginActionAck(fight);
    fight.activePartySlot = -1;
    return false;
  }

  if (fight.state === STATE.AwaitingExchangeWith) {
    if (!ascii && !escape) return false;
    if (escape || ascii === 0x1b) {
      fight.state = STATE.AwaitingCommand;
      setStatus(fight, "");
      return true;
    }
    if (ch < "1" || ch > "9") return false;
    const withSlot = ch.charCodeAt(0) - 0x31;
    if (withSlot < 0 || withSlot >= fight.partyCount) return false;
    const cur = fight.activePartySlot;
    if (withSlot !== cur) exchangePartySlots(fight, cur, withSlot);
    fight.activePartySlot = -1;
    setStatus(fight, "Exchanged.");
    beginActionAck(fight);
    return false;
  }

  if (fight.state !== STATE.AwaitingCommand) return false;

  const flags = commandFlags(fight);
  if ((keys.ctrl && upper === "A") || ascii === 1) {
    if (flags.shoot) beginPlayerStrike(fight, true, false);
    else if (flags.melee) beginPlayerStrike(fight, false, false);
    if (fight.state === STATE.AwaitingAttackTarget) return false;
    if (fight.state === STATE.Inactive) return true;
    fight.activePartySlot = -1;
    beginActionAck(fight);
    return false;
  }

  switch (upper) {
    case "A":
      if (!flags.melee) return false;
      beginPlayerStrike(fight, false, false);
      break;
    case "F":
      if (!flags.melee) return false;
      beginPlayerStrike(fight, false, true);
      break;
    case "S":
      if (!flags.shoot) return false;
      beginPlayerStrike(fight, true, true);
      break;
    case "C":
      if (!flags.cast) return false;
      fight.castLevel = 0;
      fight.state = STATE.AwaitingCastLevel;
      setStatus(fight, "");
      return false;
    case "B":
      break;
    case "D": {
      fight.partyBlocking[fight.activePartySlot] = true;
      const n = partyMember(fight, fight.activePartySlot)?.name || "Hero";
      setStatus(fight, `${n} blocks.`);
      break;
    }
    case "R":
      resolvePlayerRun(fight);
      break;
    case "E":
      fight.state = STATE.AwaitingExchangeWith;
      setStatus(fight, `Exchange with (1-${fight.partyCount})?`);
      return false;
    case "U":
    case "P":
      fight.pendingCastFlat = -2;
      fight.state = STATE.AwaitingItemPick;
      setStatus(fight, "Use which (1-6/A-F)?");
      return false;
    case "V":
    case "Q":
      return false;
    default:
      return false;
  }

  if (fight.state === STATE.AwaitingAttackTarget) return false;
  if (fight.state === STATE.Inactive) return true;
  fight.activePartySlot = -1;
  beginActionAck(fight);
  return false;
}

export { STATE as CombatState };

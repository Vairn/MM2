/**
 * CombatSession spell jump table + cast aux + item-use + applySpell108BC.
 * Port of game/src/combat/CombatSession.cpp:1155–2418, 3214–3677.
 * GS-clear / combat-fx leaves match C++; unresolved flats use the same
 * "(stub)." banner as CombatSession.cpp:1799 / 3599.
 */
"use strict";

import { ensureParty } from "./sessionState.js";

export const SPELLS_PER_SCHOOL = 48;
export const SCHOOL_SORC = 0;
export const SCHOOL_CLER = 1;
export const SCHOOL_NONE = -1;

export const kSorcererSpells = [
  { name: "Awaken", level: 1, number: 1, sp: 1, perLevel: false, gems: 0 },
  { name: "Detect Magic", level: 1, number: 2, sp: 1, perLevel: false, gems: 0 },
  { name: "Energy Blast", level: 1, number: 3, sp: 1, perLevel: true, gems: 1 },
  { name: "Flame Arrow", level: 1, number: 4, sp: 1, perLevel: false, gems: 0 },
  { name: "Light", level: 1, number: 5, sp: 1, perLevel: false, gems: 0 },
  { name: "Location", level: 1, number: 6, sp: 1, perLevel: false, gems: 0 },
  { name: "Sleep", level: 1, number: 7, sp: 1, perLevel: false, gems: 0 },
  { name: "Eagle Eye", level: 2, number: 1, sp: 2, perLevel: true, gems: 0 },
  { name: "Electric Arrow", level: 2, number: 2, sp: 2, perLevel: false, gems: 0 },
  { name: "Identify Monster", level: 2, number: 3, sp: 2, perLevel: false, gems: 1 },
  { name: "Jump", level: 2, number: 4, sp: 2, perLevel: false, gems: 0 },
  { name: "Levitate", level: 2, number: 5, sp: 2, perLevel: false, gems: 0 },
  { name: "Lloyd's Beacon", level: 2, number: 6, sp: 2, perLevel: false, gems: 1 },
  { name: "Protection from Magic", level: 2, number: 7, sp: 1, perLevel: true, gems: 1 },
  { name: "Acid Stream", level: 3, number: 1, sp: 1, perLevel: true, gems: 2 },
  { name: "Fly", level: 3, number: 2, sp: 3, perLevel: false, gems: 0 },
  { name: "Invisibility", level: 3, number: 3, sp: 3, perLevel: false, gems: 0 },
  { name: "Lightning Bolt", level: 3, number: 4, sp: 1, perLevel: true, gems: 2 },
  { name: "Web", level: 3, number: 5, sp: 3, perLevel: false, gems: 2 },
  { name: "Wizard Eye", level: 3, number: 6, sp: 3, perLevel: true, gems: 2 },
  { name: "Cold Beam", level: 4, number: 1, sp: 1, perLevel: true, gems: 3 },
  { name: "Feeble Mind", level: 4, number: 2, sp: 4, perLevel: false, gems: 3 },
  { name: "Fire Ball", level: 4, number: 3, sp: 1, perLevel: true, gems: 3 },
  { name: "Guard Dog", level: 4, number: 4, sp: 4, perLevel: false, gems: 0 },
  { name: "Shield", level: 4, number: 5, sp: 4, perLevel: false, gems: 0 },
  { name: "Time Distortion", level: 4, number: 6, sp: 4, perLevel: false, gems: 3 },
  { name: "Disrupt", level: 5, number: 1, sp: 5, perLevel: false, gems: 5 },
  { name: "Fingers of Death", level: 5, number: 2, sp: 5, perLevel: false, gems: 5 },
  { name: "Sand Storm", level: 5, number: 3, sp: 2, perLevel: true, gems: 5 },
  { name: "Shelter", level: 5, number: 4, sp: 5, perLevel: false, gems: 0 },
  { name: "Teleport", level: 5, number: 5, sp: 5, perLevel: false, gems: 0 },
  { name: "Disintegration", level: 6, number: 1, sp: 6, perLevel: false, gems: 6 },
  { name: "Entrapment", level: 6, number: 2, sp: 6, perLevel: false, gems: 6 },
  { name: "Fantastic Freeze", level: 6, number: 3, sp: 2, perLevel: true, gems: 6 },
  { name: "Recharge Item", level: 6, number: 4, sp: 6, perLevel: false, gems: 6 },
  { name: "Super Shock", level: 6, number: 5, sp: 2, perLevel: true, gems: 6 },
  { name: "Dancing Sword", level: 7, number: 1, sp: 3, perLevel: true, gems: 7 },
  { name: "Duplication", level: 7, number: 2, sp: 7, perLevel: false, gems: 100 },
  { name: "Etherealize", level: 7, number: 3, sp: 7, perLevel: false, gems: 7 },
  { name: "Prismatic Light", level: 7, number: 4, sp: 7, perLevel: false, gems: 7 },
  { name: "Incinerate", level: 8, number: 1, sp: 3, perLevel: true, gems: 8 },
  { name: "Mega Volts", level: 8, number: 2, sp: 3, perLevel: true, gems: 8 },
  { name: "Meteor Shower", level: 8, number: 3, sp: 8, perLevel: false, gems: 8 },
  { name: "Power Shield", level: 8, number: 4, sp: 8, perLevel: false, gems: 8 },
  { name: "Implosion", level: 9, number: 1, sp: 10, perLevel: false, gems: 10 },
  { name: "Inferno", level: 9, number: 2, sp: 3, perLevel: true, gems: 10 },
  { name: "Star Burst", level: 9, number: 3, sp: 10, perLevel: false, gems: 20 },
  { name: "Enchant Item", level: 9, number: 4, sp: 50, perLevel: false, gems: 50 },
];

export const kClericSpells = [
  { name: "Apparition", level: 1, number: 1, sp: 1, perLevel: false, gems: 0 },
  { name: "Awaken", level: 1, number: 2, sp: 1, perLevel: false, gems: 0 },
  { name: "Bless", level: 1, number: 3, sp: 1, perLevel: false, gems: 0 },
  { name: "First Aid", level: 1, number: 4, sp: 1, perLevel: false, gems: 0 },
  { name: "Light", level: 1, number: 5, sp: 1, perLevel: false, gems: 0 },
  { name: "Power Cure", level: 1, number: 6, sp: 1, perLevel: true, gems: 1 },
  { name: "Turn Undead", level: 1, number: 7, sp: 1, perLevel: false, gems: 0 },
  { name: "Cure Wounds", level: 2, number: 1, sp: 2, perLevel: false, gems: 0 },
  { name: "Heroism", level: 2, number: 2, sp: 2, perLevel: false, gems: 1 },
  { name: "Nature's Gate", level: 2, number: 3, sp: 2, perLevel: false, gems: 0 },
  { name: "Pain", level: 2, number: 4, sp: 2, perLevel: false, gems: 0 },
  { name: "Protection From Elements", level: 2, number: 5, sp: 2, perLevel: false, gems: 1 },
  { name: "Silence", level: 2, number: 6, sp: 2, perLevel: false, gems: 0 },
  { name: "Weaken", level: 2, number: 7, sp: 2, perLevel: false, gems: 1 },
  { name: "Cold Ray", level: 3, number: 1, sp: 3, perLevel: false, gems: 2 },
  { name: "Create Food", level: 3, number: 2, sp: 3, perLevel: false, gems: 2 },
  { name: "Cure Poison", level: 3, number: 3, sp: 3, perLevel: false, gems: 0 },
  { name: "Immobilize", level: 3, number: 4, sp: 3, perLevel: false, gems: 0 },
  { name: "Lasting Light", level: 3, number: 5, sp: 3, perLevel: false, gems: 0 },
  { name: "Walk on Water", level: 3, number: 6, sp: 3, perLevel: false, gems: 2 },
  { name: "Acid Spray", level: 4, number: 1, sp: 4, perLevel: false, gems: 3 },
  { name: "Air Transmutation", level: 4, number: 2, sp: 4, perLevel: false, gems: 3 },
  { name: "Cure Disease", level: 4, number: 3, sp: 4, perLevel: false, gems: 0 },
  { name: "Restore Alignment", level: 4, number: 4, sp: 4, perLevel: false, gems: 3 },
  { name: "Surface", level: 4, number: 5, sp: 4, perLevel: false, gems: 0 },
  { name: "Holy Bonus", level: 4, number: 6, sp: 4, perLevel: false, gems: 3 },
  { name: "Air Encasement", level: 5, number: 1, sp: 5, perLevel: false, gems: 5 },
  { name: "Deadly Swarm", level: 5, number: 2, sp: 5, perLevel: false, gems: 5 },
  { name: "Frenzy", level: 5, number: 3, sp: 5, perLevel: false, gems: 5 },
  { name: "Paralyze", level: 5, number: 4, sp: 5, perLevel: false, gems: 5 },
  { name: "Remove Condition", level: 5, number: 5, sp: 5, perLevel: false, gems: 5 },
  { name: "Earth Transmutation", level: 6, number: 1, sp: 6, perLevel: false, gems: 6 },
  { name: "Rejuvenate", level: 6, number: 2, sp: 6, perLevel: false, gems: 6 },
  { name: "Stone to Flesh", level: 6, number: 3, sp: 6, perLevel: false, gems: 6 },
  { name: "Water Encasement", level: 6, number: 4, sp: 6, perLevel: false, gems: 6 },
  { name: "Water Transmutation", level: 6, number: 5, sp: 6, perLevel: false, gems: 6 },
  { name: "Earth Encasement", level: 7, number: 1, sp: 7, perLevel: false, gems: 7 },
  { name: "Fiery Flail", level: 7, number: 2, sp: 7, perLevel: false, gems: 7 },
  { name: "Moon Ray", level: 7, number: 3, sp: 7, perLevel: false, gems: 7 },
  { name: "Raise Dead", level: 7, number: 4, sp: 7, perLevel: false, gems: 7 },
  { name: "Fire Encasement", level: 8, number: 1, sp: 8, perLevel: false, gems: 8 },
  { name: "Fire Transmutation", level: 8, number: 2, sp: 8, perLevel: false, gems: 8 },
  { name: "Mass Distortion", level: 8, number: 3, sp: 8, perLevel: false, gems: 8 },
  { name: "Town Portal", level: 8, number: 4, sp: 8, perLevel: false, gems: 8 },
  { name: "Divine Intervention", level: 9, number: 1, sp: 10, perLevel: false, gems: 20 },
  { name: "Holy Word", level: 9, number: 2, sp: 10, perLevel: false, gems: 10 },
  { name: "Resurrection", level: 9, number: 3, sp: 10, perLevel: false, gems: 10 },
  { name: "Uncurse Item", level: 9, number: 4, sp: 10, perLevel: false, gems: 50 },
];

/* Continuation of combatSpells.js — appended via tools concat. */

const PICK = {
  CastTarget: "AwaitingCastTarget",
  PartyPick: "AwaitingPartyPick",
  ItemPick: "AwaitingItemPick",
  Command: "AwaitingCommand",
  ActionAck: "AwaitingActionAck",
};

const K_STATUS_NAMES = ["silenced", "weakened", "frightened", "slept", "held", "mindless", "encased", "killed"];

const K_SORC_AUTO_AOE = [36, 39, 41, 42, 45, 46];
const K_CLER_AUTO_AOE = [0, 13, 27, 29, 38];
const K_SORC_COMBAT = [2, 3, 6, 8, 9, 14, 17, 18, 20, 21, 22, 26, 27, 28, 31, 33, 35, 40, 44];
const K_CLER_COMBAT = [10, 12, 14, 17, 20, 26, 34, 36, 37, 40, 42];

const K_SINGLE_EFFECT_NAMES = [
  "nothing", "loose gold", "loose gems", "Poison", "Disease", "Sleep",
  "Curse", "Silence", "Paralyse", "Collapse", "Kills", "Stones",
  "Eradicates", "Steals an Item", "Steals ALL Items", "Steals food",
  "Steals all Food", "loose all Gold", "Loose all Gems", "Loose all Valuables",
  "Ages (non perm)", "Ages (PERM)", "loose 1 level", "Halves all Stats",
  "gives you Personality", "loose experience levels", "halves experience level",
  "loose Experience", "Items Scrambled", "Spell points loose all",
  "Assassinates", "Random Effect",
];

function rngRange(fight, lo, hi) {
  const rng = fight.rng;
  if (typeof rng === "function") return rng(lo, hi);
  if (lo >= hi) return lo;
  return lo + Math.floor(Math.random() * (hi - lo + 1));
}

function partyMember(fight, slot) {
  const party = ensureParty(fight.session);
  if (slot < 0 || slot >= fight.partyCount || slot >= party.length) return null;
  return party[slot];
}

function monsterDef(manifest, id) {
  const list = manifest?.monsters;
  if (!Array.isArray(list) || id < 0 || id >= list.length) return null;
  return list[id] ?? null;
}

function monName(fight, slot) {
  const s = fight.slots[slot];
  return monsterDef(fight.manifest, s?.type)?.name?.trim() || "Monster";
}

function setStatus(fight, msg) {
  fight.statusLine = msg ?? "";
}

function enqueueMsg(fight, msg) {
  if (!msg || (fight.msgQueue?.length ?? 0) >= 12) return;
  if (!fight.msgQueue) fight.msgQueue = [];
  fight.msgQueue.push(String(msg));
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

function spellSchoolForClass(classId) {
  switch (classId | 0) {
    case 2:
    case 4:
      return SCHOOL_SORC;
    case 1:
    case 3:
      return SCHOOL_CLER;
    default:
      return SCHOOL_NONE;
  }
}

function schoolSpellTable(school) {
  if (school === SCHOOL_SORC) return kSorcererSpells;
  if (school === SCHOOL_CLER) return kClericSpells;
  return null;
}

function inList(tab, flat) {
  return tab.includes(flat);
}

function tileRtFlags(session) {
  return session.tileRtFlags | 0;
}

/** Prefer itemsEffect[] (export); fall back to items[id].effect_byte objects. */
function itemEffectByte(manifest, id) {
  if (id < 0) return 0;
  const effArr = manifest?.itemsEffect;
  if (Array.isArray(effArr) && id < effArr.length) return effArr[id] | 0;
  const list = manifest?.items;
  if (Array.isArray(list) && id < list.length) {
    const rec = list[id];
    if (rec && typeof rec === "object") return rec.effect_byte | 0;
  }
  return 0;
}

export function exportPabilTables() {
  return {
    pabilChance: [0, 10, 20, 35, 50, 75, 90, 100, 1, 1, 1, 1, 0, 1, 1, 15],
    resA: [1, 1, 99, 1, 1, 1, 1, 1, 1, 1, 1, 99, 99, 99, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0],
    resB: [0, 0, 99, 0, 0, 0, 0, 0, 0, 0, 1, 99, 99, 99, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0],
    resC: [28, 29, 99, 23, 24, 25, 26, 28, 29, 26, 21, 99, 99, 99, 26, 26, 21, 24, 23, 0, 26, 24, 0, 23, 26, 0, 23, 0, 0, 31, 28, 0],
    ord: [0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 0, 0, 2, 4, 6, 1, 3, 5, 7, 1, 3, 5, 7, 0, 2, 4, 6],
    luckThresh: [4, 6, 9, 13, 15, 17, 19, 22, 26, 30, 45, 60, 75, 90, 105, 120, 135, 150, 175, 200, 225, 250, 255],
  };
}

export function seedCombatGs(fight) {
  const t = exportPabilTables();
  fight.statusBitTbl = [0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00];
  fight.encaseDmgTbl = [10, 20, 40, 80];
  fight.encaseModeTbl = [6, 5, 4, 5];
  fight.flyScreens = [5, 9, 12, 15, 6, 10, 13, 16, 7, 11, 14, 38, 8, 34, 36, 39, 33, 35, 37, 40];
  fight.pabilResA = t.resA.slice();
  fight.pabilResB = t.resB.slice();
  fight.pabilResC = t.resC.slice();
  fight.pabilTargetOrd = t.ord.slice();
  fight.luckThreshTbl = t.luckThresh.slice();
  fight.spellDamage = 0;
  fight.spellStatusMode = 0;
  fight.spellStatusCode = 0;
  fight.spellSkipResist = 0;
  fight.encaseTier = 0;
  fight.holyWordGate = 0;
  fight.eradicateSkipPrint = 0;
  fight.resistFlagA = 0;
  fight.resistFlagB = 0;
  fight.resistFlagC = 0;
  fight.resistBuffA = 0;
  fight.resistBuffC = 0;
}

export function combatActionAckFrames(fight) {
  const arg = ((fight.session?.delay | 0) * 0x19 + 1) | 0;
  let half = (arg / 2) | 0;
  if (half < 1) half = 1;
  return half + 1;
}

function spellTargetPower13362(fight, level) {
  let result = 0x0a;
  let live = fight.session.monsterCount | 0;
  if (live > 0x0a) live = 0x0a;
  const lv = level > 0 ? level : 1;
  if (lv < 7) result = (lv + 4) & 0xff;
  else if (lv >= live) result = 0x0b;
  return result;
}

/** 0x1338E / CombatSession.cpp:2691 — per caster level: piece=rng(1,sides)|0 then +=piece+addend. */
function rollSpellDamage133B6(fight, level, sides, addend) {
  let total = 0;
  const n = level > 0 ? level : 1;
  for (let i = 0; i < n; i++) {
    let piece = 0;
    if (sides) piece = rngRange(fight, 1, sides);
    total = (total + piece + (addend | 0)) & 0xffff;
  }
  fight.spellDamage = total;
  return total;
}

const MRES_CHANCE = [0, 10, 20, 35, 50, 75, 90, 100];

function loadMonsterCombatGlobals(fight, slot, api) {
  api.loadMonGlobals(fight, slot);
  const type = fight.slots[slot]?.type | 0;
  const m = monsterDef(fight.manifest, type);
  if (!m) {
    fight.monUndead = 0;
    fight.monMres = 0;
    fight.monType = 0;
    fight.monFlagBase = new Array(7).fill(0);
    return;
  }
  fight.monUndead = m.undead ? 1 : 0;
  fight.monType = type & 0xff;
  const mres = m.mres | 0;
  const tier = (mres >> 5) & 7;
  fight.monMres = MRES_CHANCE[tier] ?? 0;
  const dmg = m.damage | 0;
  const spd2 = m.speed2 | 0;
  const flags = new Array(7).fill(0);
  if (dmg & 0x40) flags[0] = 1;
  if (dmg & 0x80) flags[1] = 1;
  if (spd2 & 0x40) flags[2] = 1;
  if (spd2 & 0x80) flags[3] = 1;
  if (mres & 0x02) flags[4] = 1;
  if (mres & 0x01) flags[5] = 1;
  if (mres & 0x04) flags[6] = 1;
  fight.monFlagBase = flags;
  /* Keep monG.mres as chance for other callers. */
  if (fight.monG) fight.monG.mres = fight.monMres;
}

function facingDelta(session) {
  let dx = 0;
  let dy = 0;
  switch (session.facing ?? "N") {
    case "E":
      dx = 1;
      break;
    case "N":
      dy = 1;
      break;
    case "S":
      dy = -1;
      break;
    case "W":
      dx = -1;
      break;
    default:
      break;
  }
  return { dx, dy };
}

function beginPartyPickCast(fight, flat0, prompt) {
  fight.pendingCastFlat = flat0;
  fight.pendingCastSchool = SCHOOL_SORC;
  if (fight.forceCastSchool === SCHOOL_CLER) fight.pendingCastSchool = SCHOOL_CLER;
  else if (fight.forceCastSchool === SCHOOL_SORC) fight.pendingCastSchool = SCHOOL_SORC;
  else {
    const rec = partyMember(fight, fight.activePartySlot);
    if (rec && spellSchoolForClass(rec.classId) === SCHOOL_CLER) fight.pendingCastSchool = SCHOOL_CLER;
  }
  fight.state = PICK.PartyPick;
  fight.spellActed = 0;
  const pc = fight.partyCount > 0 ? fight.partyCount : 1;
  if (prompt === "On whom") setStatus(fight, `On whom (1-${pc})?`);
  else if (prompt) setStatus(fight, prompt);
}

function beginItemPickCast(fight, flat0, prompt) {
  fight.pendingCastFlat = flat0;
  fight.pendingCastSchool = SCHOOL_SORC;
  if (fight.forceCastSchool === SCHOOL_CLER) fight.pendingCastSchool = SCHOOL_CLER;
  else if (fight.forceCastSchool !== SCHOOL_SORC) {
    const rec = partyMember(fight, fight.activePartySlot);
    if (rec && spellSchoolForClass(rec.classId) === SCHOOL_CLER) fight.pendingCastSchool = SCHOOL_CLER;
  }
  fight.state = PICK.ItemPick;
  fight.spellActed = 0;
  if (prompt) setStatus(fight, prompt);
}

function applyHealCa58(fight, partySlot, heal, spellName) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt || (tgt.condition | 0) >= 0x80) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  tgt.condition = (tgt.condition | 0) & 0x2f;
  const sum = (tgt.hpMax | 0) + (heal | 0);
  tgt.hpMax = sum > 0xffff ? 0xffff : sum;
  const ceil = tgt.hpCurrent != null ? tgt.hpCurrent | 0 : tgt.hpAux | 0;
  if ((tgt.hpMax | 0) > ceil) tgt.hpMax = ceil;
  if (partySlot === 0) fight.session.posChangedLatch = 1;
  const tname = tgt.name || "Hero";
  setStatus(fight, `${spellName}: ${tname} +${heal} HP.`);
}

function applyCureMaskToPartySlot(fight, partySlot, andMask, clearAll, spellName) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  if ((tgt.condition | 0) >= 0x80 && !clearAll) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  if (clearAll) {
    if ((tgt.condition | 0) >= 0x80) {
      setStatus(fight, "* Spell Failed *");
      return;
    }
    tgt.condition = 0;
  } else tgt.condition = (tgt.condition | 0) & andMask;
  if (partySlot === 0) fight.session.posChangedLatch = 1;
  setStatus(fight, `${spellName}: ${tgt.name || "Hero"}.`);
}

function applyRestoreAlignmentToPartySlot(fight, partySlot) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  tgt.alignmentBase = tgt.alignmentCurrent ?? tgt.alignmentBase ?? 0;
  if (partySlot === fight.activePartySlot) fight.session.posChangedLatch = 1;
  setStatus(fight, `Restore Alignment: ${tgt.name || "Hero"}.`);
}

function applyRejuvenateToPartySlot(fight, partySlot) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  const delta = rngRange(fight, 1, 10);
  const roll = rngRange(fight, 1, 100);
  if (roll < 50 && (tgt.age | 0) >= 0x12) {
    tgt.age = (tgt.age | 0) > delta ? (tgt.age | 0) - delta : 0;
    setStatus(fight, `Rejuvenate: ${tgt.name || "Hero"} (-${delta} yr).`);
  } else {
    let age = (tgt.age | 0) + delta;
    if (age > 0xc8) age = 0xc8;
    tgt.age = age;
    setStatus(fight, "* Spell Failed *");
  }
}

function applyRaiseDeadToPartySlot(fight, partySlot) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt || (tgt.condition | 0) < 0x80) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  const caster = partyMember(fight, fight.activePartySlot);
  if (caster) {
    let ca = (caster.age | 0) + 1;
    if (ca > 0xc8) ca = 0xc8;
    caster.age = ca;
  }
  let ta = (tgt.age | 0) + 5;
  if (ta > 0xc8) ta = 0xc8;
  tgt.age = ta;
  const end = tgt.endurance ?? tgt.enduranceBase ?? 14;
  if (!end) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  tgt.endurance = end - 1;
  tgt.enduranceBase = tgt.endurance;
  tgt.condition = 0;
  if (partySlot === fight.activePartySlot) fight.session.posChangedLatch = 1;
  setStatus(fight, `Raise Dead: ${tgt.name || "Hero"}.`);
}

function applyStoneToFleshToPartySlot(fight, partySlot) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt || (tgt.condition | 0) !== 0x82) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  tgt.condition = 0;
  if (partySlot === 0) fight.session.posChangedLatch = 1;
  setStatus(fight, `Stone to Flesh: ${tgt.name || "Hero"}.`);
}

function applyResurrectionToPartySlot(fight, partySlot) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt || (tgt.condition | 0) !== 0x81) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  const caster = partyMember(fight, fight.activePartySlot);
  if (caster) {
    let ca = (caster.age | 0) + 1;
    if (ca > 0xc8) ca = 0xc8;
    caster.age = ca;
  }
  let ta = (tgt.age | 0) + 1;
  if (ta > 0xc8) ta = 0xc8;
  tgt.age = ta;
  const roll = rngRange(fight, 1, 100);
  if (roll >= 11) {
    tgt.condition = 0;
    tgt.hpMax = 1;
    if (partySlot === 0) fight.session.posChangedLatch = 1;
    setStatus(fight, `Resurrection: ${tgt.name || "Hero"}.`);
    return;
  }
  if (roll === 10) tgt.condition = 0xff;
  setStatus(fight, "* Spell Failed *");
}

function applyHeroismToPartySlot(fight, partySlot) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  if ((tgt.level | 0) < 0xf9) tgt.level = ((tgt.level | 0) + 6) & 0xff;
  else tgt.level = 0xff;
  setStatus(fight, `${tgt.name || "Hero"} gains Heroism.`);
}

function applyFrenzyToPartySlot(fight, api, partySlot) {
  const tgt = partyMember(fight, partySlot);
  if (!tgt || (tgt.condition | 0) !== 0 || !(tgt.endurance ?? tgt.enduranceBase)) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  fight.session.frenzyLatch = 1;
  tgt.condition = 0x40;
  const end = tgt.endurance ?? tgt.enduranceBase ?? 14;
  tgt.endurance = end - 1;
  tgt.enduranceBase = tgt.endurance;
  tgt.hpMax = 0;
  const book = tgt.spellBook ?? [];
  const dmg = (((book[0] ?? 0) + (book[1] ?? 0) + 10) * 2) & 0xffff;
  fight.spellDamage = dmg;
  fight.spellStatusMode = 0;
  fight.spellSkipResist = 1;
  applySpell108BC(fight, api, 0, 10, 0);
  fight.spellSkipResist = 0;
}

function applyTurnUndeadC050(fight, api, arg) {
  const session = fight.session;
  if (session.turnUndeadUsed) {
    setStatus(fight, "* Spell Failed *");
    return 0;
  }
  session.turnUndeadUsed = 1;
  let remain = session.monsterCount | 0;
  if (arg && remain > 0x0a) remain = 0x0a;
  let killed = 0;
  let slot = 0;
  fight.msgQueue = [];
  fight.msgIdx = 0;
  while (remain > 0 && slot < 11) {
    if (!fight.slots[slot]?.alive) {
      slot++;
      remain--;
      continue;
    }
    api.loadMonGlobals(fight, slot);
    if (!fight.monG?.undead) {
      slot++;
      remain--;
      continue;
    }
    let roll = 0xff;
    if (arg) roll = rngRange(fight, 1, arg) & 0xff;
    const monType = fight.slots[slot].type & 0xff;
    if (roll < monType) {
      slot++;
      remain--;
      continue;
    }
    const name = monName(fight, slot);
    const def = monsterDef(fight.manifest, fight.slots[slot].type);
    enqueueMsg(fight, `${name} is eradicated!`);
    fight.eradicateSkipPrint = 1;
    api.onMonsterKilled(fight, slot, (def?.xp | 0) >>> 0);
    fight.eradicateSkipPrint = 0;
    killed++;
    remain--;
  }
  fight.holyWordGate = 0;
  if (!killed) setStatus(fight, "* Spell Failed *");
  else if (fight.msgQueue.length) setStatus(fight, fight.msgQueue[0]);
  return killed;
}

export function appendSingleEffectFeea(fight, victimName) {
  const sabil = fight.monG?.sabil | 0;
  if (!sabil || sabil > K_SINGLE_EFFECT_NAMES.length) return;
  const eff = K_SINGLE_EFFECT_NAMES[sabil];
  if (!eff || eff === "nothing") return;
  enqueueMsg(fight, `${victimName} ${eff}!`);
}

export function applySpell108BC(fight, api, startSlot, hitCount, modeD) {
  let live = countAliveMonsters(fight);
  if (hitCount > live) hitCount = live;
  if (hitCount < 1) hitCount = 1;
  let maxSlot = 0x0a;
  if (fight.holyWordGate) maxSlot = 0x0b;
  if (fight.spellStatusMode === 2) fight.spellStatusMode = 0;
  const statusMode = fight.spellStatusMode | 0;
  let statusCode = fight.spellStatusCode | 0;
  const savedDmg = fight.spellDamage | 0;
  const caster = partyMember(fight, fight.activePartySlot);
  const casterLevel = (caster?.level | 0) > 0 ? caster.level | 0 : 1;
  let lastMsg = "";
  fight.msgQueue = [];
  fight.msgIdx = 0;
  let applied = 0;
  for (let s = startSlot; s < 11 && applied < hitCount; ) {
    if (s >= maxSlot) break;
    if (!fight.slots[s]?.alive) {
      s++;
      continue;
    }
    loadMonsterCombatGlobals(fight, s, api);
    fight.spellDamage = savedDmg;
    fight.spellStatusMode = statusMode;
    fight.spellStatusCode = statusCode;
    const name = monName(fight, s);
    let miss = false;
    if (!fight.spellSkipResist) {
      const mres = fight.monMres | 0;
      if (mres) {
        const roll = rngRange(fight, casterLevel, 0x5a);
        if (roll < mres) miss = true;
      }
      if (!miss && modeD) {
        const idx = (modeD - 1) | 0;
        if (fight.monFlagBase?.[idx]) miss = true;
      }
      if (!miss) {
        const tr = rngRange(fight, 1, 0xbf);
        const monType = fight.monType | 0;
        if (tr <= monType) {
          if (statusMode === 0) fight.spellDamage = ((fight.spellDamage | 0) / 2) | 0;
          else if (statusMode === 1) {
            if (statusCode < 9) miss = true;
            else {
              fight.spellDamage = 0x32;
              fight.spellStatusMode = 0;
            }
          }
        }
      }
    }
    if (miss && statusMode === 1 && statusCode === 8 && fight.monUndead) miss = false;
    if (!miss && fight.spellStatusMode === 0 && !(fight.spellDamage | 0)) miss = true;
    if (miss) {
      lastMsg = `${name} is not affected!`;
      enqueueMsg(fight, lastMsg);
      applied++;
      s++;
      continue;
    }
    let didKill = false;
    if (fight.spellStatusMode !== 0) {
      let code = fight.spellStatusCode | 0;
      if (code > 0) code--;
      if (code < 7) {
        const bit = fight.statusBitTbl[code] ?? 0;
        fight.slots[s].status = (fight.slots[s].status | bit) & 0xff;
        lastMsg = `${name} is ${K_STATUS_NAMES[code] ?? "affected"}!`;
        enqueueMsg(fight, lastMsg);
      } else {
        lastMsg = `${name} is eradicated!`;
        enqueueMsg(fight, lastMsg);
        fight.eradicateSkipPrint = 1;
        const def = monsterDef(fight.manifest, fight.slots[s].type);
        api.onMonsterKilled(fight, s, (def?.xp | 0) >>> 0);
        fight.eradicateSkipPrint = 0;
        didKill = true;
      }
    } else {
      const dmg = fight.spellDamage | 0;
      lastMsg =
        fight.slots[s].hp <= dmg
          ? `${name} takes ${dmg} Pts`
          : `${name} takes ${dmg} ${dmg === 1 ? "Pt" : "Pts"}`;
      enqueueMsg(fight, lastMsg);
      didKill = api.applyHp10ED4(fight, s, dmg);
    }
    applied++;
    if (!didKill) s++;
  }
  fight.spellDamage = savedDmg;
  fight.spellStatusMode = statusMode;
  fight.spellStatusCode = statusCode;
  fight.holyWordGate = 0;
  if (fight.msgQueue.length) setStatus(fight, fight.msgQueue[0]);
  else if (lastMsg) setStatus(fight, lastMsg);
}

export function applyCastToMonsterTarget(fight, api, slot, flat0) {
  const caster = partyMember(fight, fight.activePartySlot);
  if (!caster) return;
  const name = caster.name || "Hero";
  const school = spellSchoolForClass(caster.classId);
  const table = schoolSpellTable(school);
  const meta = table && flat0 >= 0 && flat0 < SPELLS_PER_SCHOOL ? table[flat0] : null;
  const spellName = meta?.name ?? "?";
  const level = (caster.level | 0) > 0 ? caster.level | 0 : 1;

  if (school === SCHOOL_SORC && flat0 === 9) {
    if (slot < 0 || slot >= 11 || !fight.slots[slot]?.alive) {
      setStatus(fight, `${name} casts ${spellName}.`);
      return;
    }
    loadMonsterCombatGlobals(fight, slot, api);
    const mn = monName(fight, slot);
    fight.msgQueue = [];
    enqueueMsg(fight, `${String.fromCharCode(0x41 + slot)} ${mn}:`);
    enqueueMsg(fight, `HP = ${fight.slots[slot].hp | 0}`);
    enqueueMsg(fight, `AC = ${fight.monG?.ac ?? 0}`);
    enqueueMsg(fight, `Undead (${fight.monUndead ? "Y" : "N"})`);
    enqueueMsg(fight, `Special Power (${fight.monG?.chargeInit ? "Y" : "N"})`);
    enqueueMsg(fight, `Bonus on Touch (${fight.monG?.sabil ? "Y" : "N"})`);
    enqueueMsg(fight, `Magic Resistance (${fight.monMres ? "Y" : "N"})`);
    setStatus(fight, fight.msgQueue[0]);
    fight.msgIdx = 0;
    return;
  }

  if (school === SCHOOL_CLER && flat0 === 42) {
    if (slot < 0 || !fight.slots[slot]?.alive) {
      setStatus(fight, `${name} casts ${spellName}.`);
      return;
    }
    const dmg = (fight.slots[slot].hp >> 1) & 0xffff;
    fight.spellDamage = dmg;
    fight.spellStatusMode = 0;
    applySpell108BC(fight, api, slot, 2, 0);
    return;
  }

  if (school === SCHOOL_SORC && flat0 === 45) {
    rollSpellDamage133B6(fight, level, 16, 4);
    fight.spellStatusMode = 0;
    fight.spellStatusCode = 0;
    applySpell108BC(fight, api, 0, 10, 0);
    if (!fight.statusLine) setStatus(fight, `${name} casts ${spellName}.`);
    return;
  }

  if (school === SCHOOL_CLER && flat0 === 38) {
    const moonRoll = () => rngRange(fight, 1, 0x5b) + 9;
    fight.spellDamage = moonRoll();
    fight.spellStatusMode = 0;
    fight.spellStatusCode = 0;
    applySpell108BC(fight, api, 0, 10, 0);
    for (let p = 0; p < fight.partyCount; p++) {
      const rec = partyMember(fight, p);
      if (!rec || (rec.condition | 0) >= 0x80) continue;
      const heal = moonRoll();
      rec.condition = (rec.condition | 0) & 0x2f;
      const sum = (rec.hpMax | 0) + heal;
      rec.hpMax = sum > 0xffff ? 0xffff : sum;
      const ceil = rec.hpCurrent != null ? rec.hpCurrent | 0 : rec.hpAux | 0;
      if ((rec.hpMax | 0) > ceil) rec.hpMax = ceil;
    }
    if (!fight.statusLine) setStatus(fight, `${name} casts ${spellName}.`);
    return;
  }

  if (school === SCHOOL_SORC && flat0 === 39) {
    const st = rngRange(fight, 1, 9);
    if (st === 7) fight.encaseTier = rngRange(fight, 1, 4) & 0xff;
    fight.spellStatusCode = st;
    fight.spellStatusMode = 1;
    applySpell108BC(fight, api, 0, 0x0a, 0);
    if (fight.msgQueue.length) setStatus(fight, fight.msgQueue[0]);
    else if (!fight.statusLine) setStatus(fight, `${name} casts ${spellName}.`);
    return;
  }

  const kSorcFx = [
    { flat: 2, kind: 0, a: 5, b: 1, c: 0, hits: 1, sm: 0, md: 0 },
    { flat: 3, kind: 1, a: 5, b: 1, c: 3, hits: 1, sm: 0, md: 1 },
    { flat: 6, kind: 2, a: 0, b: 0, c: 4, hits: 0xff, sm: 1, md: 5 },
    { flat: 8, kind: 1, a: 9, b: 1, c: 7, hits: 1, sm: 0, md: 2 },
    { flat: 14, kind: 0, a: 5, b: 3, c: 0, hits: 1, sm: 0, md: 4 },
    { flat: 17, kind: 0, a: 5, b: 1, c: 0, hits: 4, sm: 0, md: 2 },
    { flat: 18, kind: 2, a: 0, b: 0, c: 5, hits: 0xff, sm: 1, md: 6 },
    { flat: 20, kind: 0, a: 0, b: 6, c: 0, hits: 1, sm: 0, md: 3 },
    { flat: 21, kind: 2, a: 0, b: 0, c: 6, hits: 5, sm: 1, md: 0 },
    { flat: 22, kind: 0, a: 5, b: 1, c: 0, hits: 6, sm: 0, md: 1 },
    { flat: 26, kind: 3, a: 0x64, b: 0, c: 0, hits: 1, sm: 0, md: 0 },
    { flat: 27, kind: 2, a: 0, b: 0, c: 8, hits: 3, sm: 1, md: 6 },
    { flat: 28, kind: 0, a: 7, b: 1, c: 0, hits: 10, sm: 0, md: 0 },
    { flat: 31, kind: 2, a: 0, b: 0, c: 9, hits: 3, sm: 1, md: 0 },
    { flat: 33, kind: 0, a: 0, b: 10, c: 0, hits: 3, sm: 0, md: 3 },
    { flat: 35, kind: 0, a: 0, b: 20, c: 0, hits: 1, sm: 0, md: 2 },
    { flat: 36, kind: 0, a: 11, b: 1, c: 0, hits: 10, sm: 0, md: 0 },
    { flat: 40, kind: 0, a: 21, b: 19, c: 0, hits: 1, sm: 0, md: 1 },
    { flat: 41, kind: 0, a: 9, b: 7, c: 0, hits: 10, sm: 0, md: 0 },
    { flat: 42, kind: 1, a: 0x15, b: 1, c: 0x18, hits: 0xfe, sm: 0, md: 0 },
    { flat: 44, kind: 3, a: 0x3e8, b: 0, c: 0, hits: 1, sm: 0, md: 0 },
    { flat: 46, kind: 1, a: 0xa1, b: 1, c: 0x27, hits: 0xfe, sm: 0, md: 0 },
  ];
  const kClerFx = [
    { flat: 0, kind: 2, a: 0, b: 0, c: 3, hits: 6, sm: 1, md: 0 },
    { flat: 10, kind: 1, a: 12, b: 1, c: 3, hits: 1, sm: 0, md: 0 },
    { flat: 12, kind: 2, a: 0, b: 0, c: 1, hits: 0xff, sm: 1, md: 6 },
    { flat: 13, kind: 2, a: 0, b: 0, c: 2, hits: 10, sm: 1, md: 0 },
    { flat: 14, kind: 3, a: 0x19, b: 0, c: 0, hits: 5, sm: 0, md: 3 },
    { flat: 17, kind: 2, a: 0, b: 0, c: 5, hits: 5, sm: 1, md: 6 },
    { flat: 20, kind: 1, a: 49, b: 1, c: 11, hits: 3, sm: 0, md: 4 },
    { flat: 26, kind: 2, a: 0, b: 0, c: 7, hits: 1, sm: 1, md: 0 },
    { flat: 27, kind: 1, a: 33, b: 1, c: 7, hits: 10, sm: 0, md: 0 },
    { flat: 29, kind: 2, a: 0, b: 0, c: 5, hits: 6, sm: 1, md: 0 },
    { flat: 34, kind: 2, a: 0, b: 0, c: 7, hits: 1, sm: 1, md: 3 },
    { flat: 36, kind: 2, a: 0, b: 0, c: 7, hits: 1, sm: 1, md: 4 },
    { flat: 37, kind: 1, a: 400, b: 255, c: 0, hits: 1, sm: 0, md: 0 },
    { flat: 40, kind: 2, a: 0, b: 0, c: 7, hits: 1, sm: 1, md: 1 },
  ];
  const fxTable = school === SCHOOL_SORC ? kSorcFx : kClerFx;
  const fx = fxTable.find((row) => row.flat === flat0) ?? null;
  fight.spellStatusMode = 0;
  fight.spellStatusCode = 0;
  fight.spellDamage = 0;
  if (!fx) {
    setStatus(fight, `${name} casts ${spellName} (stub).`);
    return;
  }
  let gated = false;
  if (school === SCHOOL_SORC) gated = [18, 22, 26, 33].includes(flat0);
  else if (school === SCHOOL_CLER) gated = [14, 20].includes(flat0);
  if (gated && slot >= 0 && slot < (fight.meleeRangeCount | 0)) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  let hits = fx.hits;
  if (hits === 0xff) hits = spellTargetPower13362(fight, level);
  else if (hits === 0xfe) {
    hits = countAliveMonsters(fight) || 1;
    if ((fight.holyWordGate | 0) < 0xff) fight.holyWordGate = (fight.holyWordGate | 0) + 1;
  }
  if (fx.kind === 0) rollSpellDamage133B6(fight, level, fx.a & 0xff, fx.b & 0xff);
  else if (fx.kind === 1) {
    const hi = fx.a > 0 ? fx.a : 1;
    const lo = fx.b > 0 ? fx.b : 1;
    fight.spellDamage = (rngRange(fight, lo, hi) + fx.c) & 0xffff;
  } else if (fx.kind === 3) fight.spellDamage = fx.a & 0xffff;
  else {
    fight.spellStatusMode = fx.sm;
    fight.spellStatusCode = fx.c;
    if (school === SCHOOL_CLER && fx.c === 7) {
      const tierMap = { 26: 1, 34: 2, 36: 3, 40: 4 };
      if (tierMap[flat0]) fight.encaseTier = tierMap[flat0];
    }
  }
  applySpell108BC(fight, api, slot, hits > 0 ? hits : 1, fx.md);
  if (fx.kind === 2 && !fight.statusLine) setStatus(fight, `${name} casts ${spellName}.`);
}

export function applyEncaseDot(fight, api, slot) {
  let idx = fight.encaseTier | 0;
  if (idx !== 0) idx--;
  if (idx > 3) idx = 3;
  fight.spellStatusMode = 2;
  fight.spellDamage = fight.encaseDmgTbl[idx] ?? 10;
  const modeD = fight.encaseModeTbl[idx] ?? 6;
  applySpell108BC(fight, api, slot, 1, modeD);
  if (fight.msgQueue?.length) setStatus(fight, fight.msgQueue[0]);
}

function applyRechargeToBackpackSlot(fight, packSlot) {
  const rec = partyMember(fight, fight.activePartySlot);
  if (!rec?.backpack?.[packSlot]?.id) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  const bp = rec.backpack[packSlot];
  if (!(bp.charges | 0)) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  const add = rngRange(fight, 1, 6);
  bp.charges = Math.min(255, (bp.charges | 0) + add);
  setStatus(fight, `Recharged +${add}.`);
}

function applyDuplicationFromBackpackSlot(fight, packSlot) {
  const rec = partyMember(fight, fight.activePartySlot);
  const bp = rec?.backpack?.[packSlot];
  if (!bp?.id || (bp.id | 0) >= 0xd0) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  let empty = -1;
  for (let i = 0; i < 6; i++) if (!rec.backpack[i]?.id) {
    empty = i;
    break;
  }
  if (empty < 0) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  rec.backpack[empty] = { id: bp.id, charges: bp.charges, flags: bp.flags };
  fight.session.posChangedLatch = 1;
  setStatus(fight, "Duplicated.");
}

function applyEnchantToBackpackSlot(fight, packSlot) {
  const rec = partyMember(fight, fight.activePartySlot);
  const bp = rec?.backpack?.[packSlot];
  if (!bp?.id) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  const flags = bp.flags | 0;
  const hi = flags & 0xc0;
  let plus = flags & 0x3f;
  const need = (plus * 0x32) & 0xffff;
  if ((rec.spMax | 0) < need) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  plus = (plus + 1) & 0xff;
  bp.flags = hi | plus;
  setStatus(fight, "Enchanted.");
}

function applyUncurseToBackpackSlot(fight, packSlot) {
  const rec = partyMember(fight, fight.activePartySlot);
  const bp = rec?.backpack?.[packSlot];
  if (!bp?.id || (bp.charges | 0) === 0xff) {
    setStatus(fight, "* Spell Failed *");
    return;
  }
  bp.charges = 1;
  setStatus(fight, "Uncursed.");
}

export function resolvePlayerCast(fight, api, flat0) {
  const caster = partyMember(fight, fight.activePartySlot);
  if (!caster) {
    fight.state = PICK.Command;
    return;
  }
  const name = caster.name || "Hero";
  let school = spellSchoolForClass(caster.classId);
  if (fight.forceCastSchool === SCHOOL_SORC) school = SCHOOL_SORC;
  else if (fight.forceCastSchool === SCHOOL_CLER) school = SCHOOL_CLER;
  const table = schoolSpellTable(school);
  const meta = table && flat0 >= 0 && flat0 < SPELLS_PER_SCHOOL ? table[flat0] : null;
  const spellName = meta?.name ?? "?";
  if (meta && !fight.skipCastCost) {
    let spCost = meta.sp | 0;
    if (meta.perLevel) spCost *= (caster.level | 0) > 0 ? caster.level | 0 : 1;
    const gemCost = meta.gems | 0;
    if ((caster.gems | 0) >= gemCost) caster.gems = ((caster.gems | 0) - gemCost) & 0xffff;
    else caster.gems = 0;
    if ((caster.spCurrent | 0) >= spCost) caster.spCurrent = ((caster.spCurrent | 0) - spCost) & 0xffff;
    if (!fight.session.scriptAbort) fight.session.posChangedLatch = 1;
  }
  let effect = false;
  const session = fight.session;
  const rt = tileRtFlags(session);
  const addEye = (field) => {
    const lv = (caster.level | 0) > 0 ? caster.level | 0 : 1;
    let t = session[field] | 0;
    if (t > 0xfa) session[field] = 0xff;
    for (let i = 0; i < lv; i++) {
      t = session[field] | 0;
      if (t < 0xfa) session[field] = (t + 5) & 0xff;
    }
    session.scriptAbort = 1;
    session.posChangedLatch = 0;
    effect = true;
  };
  const addqFlag = (field) => {
    let v = session[field] | 0;
    if (v < 0xff) session[field] = (v + 1) & 0xff;
    session.scriptAbort = 1;
    session.posChangedLatch = 0;
    effect = true;
  };

  if (meta && school === SCHOOL_SORC) {
    if (flat0 === 0) {
      for (let p = 0; p < fight.partyCount; p++) {
        const m = partyMember(fight, p);
        if (m && (m.condition | 0) < 0x80) m.condition = (m.condition | 0) & 0x6f;
      }
      session.posChangedLatch = 1;
      effect = true;
    } else if (flat0 === 1) {
      let line = "Charges:";
      for (let i = 0; i < 6; i++) line += ` ${String.fromCharCode(0x41 + i)}-${caster.backpack?.[i]?.charges ?? 0}`;
      setStatus(fight, line);
      fight.spellActed = 1;
      return;
    } else if (flat0 === 5) {
      setStatus(fight, `Location: map ${session.screen ?? 0} at (${session.x ?? 0},${session.y ?? 0}).`);
      fight.spellActed = 1;
      return;
    } else if (flat0 === 4) {
      session.lightFactor = Math.min(0xfe, (session.lightFactor | 0) + 1);
      session.posChangedLatch = 1;
      session.scriptAbort = 1;
      effect = true;
    } else if (flat0 === 7) {
      addEye("eagleEyeTimer");
      effect = true;
    } else if (flat0 === 11) {
      addqFlag("levitateFlag");
      effect = true;
    } else if (flat0 === 10) {
      if (rt & 0x20) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      const { dx, dy } = facingDelta(session);
      session.x = ((session.x | 0) + dx) & 0x0f;
      session.y = ((session.y | 0) + dy) & 0x0f;
      session.scriptAbort = 1;
      session.posChangedLatch = 0;
      effect = true;
    } else if (flat0 === 12) {
      if (rt & 0x40) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "1-Set 2-Recall?");
      return;
    } else if (flat0 === 13) {
      const lv = (caster.level | 0) > 0 ? caster.level | 0 : 1;
      session.magicProtect = (lv + 0x0a) & 0xff;
      session.scriptAbort = 1;
      session.posChangedLatch = 0;
      effect = true;
    } else if (flat0 === 15) {
      if (rt & 0x40) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      fight.castAux = -1;
      beginPartyPickCast(fight, flat0, "Fly to (A-E)?");
      return;
    } else if (flat0 === 16) {
      session.invisCounter = Math.min(0xff, (session.invisCounter | 0) + 1);
      effect = true;
    } else if (flat0 === 30) {
      if (rt & 0x10) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "Teleport (1-9)?");
      return;
    } else if (flat0 === 19) {
      if (fight.explorationCast) {
        addEye("wizardEyeTimer");
        effect = true;
      } else {
        const st = rngRange(fight, 1, 9);
        if (st === 7) fight.encaseTier = rngRange(fight, 1, 4) & 0xff;
        fight.spellStatusCode = st;
        fight.spellStatusMode = 1;
        applySpell108BC(fight, api, 0, 0x0a, 0);
        if (fight.msgQueue?.length) setStatus(fight, fight.msgQueue[0]);
        else setStatus(fight, `${name} casts ${spellName}.`);
        fight.spellActed = 1;
        return;
      }
    } else if (flat0 === 23) {
      addqFlag("guardDogFlag");
      effect = true;
    } else if (flat0 === 24) {
      fight.shieldCtr = Math.min(0xff, (fight.shieldCtr | 0) + 1);
      effect = true;
    } else if (flat0 === 25) {
      if (fight.explorationCast || rt & 0x08) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      fight.timeDistort = ((fight.timeDistort | 0) + 1) & 0xff;
      effect = true;
    } else if (flat0 === 29) {
      addqFlag("shelterFlag");
      effect = true;
    } else if (flat0 === 32) {
      if (fight.explorationCast || rt & 0x01) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      fight.entrapment = ((fight.entrapment | 0) + 1) & 0xff;
      effect = true;
    } else if (flat0 === 43) {
      fight.powerShieldCtr = Math.min(0xff, (fight.powerShieldCtr | 0) + 1);
      effect = true;
    } else if (flat0 === 34) {
      beginItemPickCast(fight, flat0, "Recharge which (A-F)?");
      return;
    } else if (flat0 === 37) {
      beginItemPickCast(fight, flat0, "Duplicate which (A-F)?");
      return;
    } else if (flat0 === 38) {
      if (rt & 0x20) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      const { dx, dy } = facingDelta(session);
      session.x = ((session.x | 0) + dx) & 0x0f;
      session.y = ((session.y | 0) + dy) & 0x0f;
      session.scriptAbort = 1;
      session.posChangedLatch = 0;
      effect = true;
    } else if (flat0 === 47) {
      beginItemPickCast(fight, flat0, "Enchant which (A-F)?");
      return;
    }
  } else if (meta && school === SCHOOL_CLER) {
    if (flat0 === 1) {
      for (let p = 0; p < fight.partyCount; p++) {
        const m = partyMember(fight, p);
        if (m && (m.condition | 0) < 0x80) m.condition = (m.condition | 0) & 0x6f;
      }
      session.posChangedLatch = 1;
      effect = true;
    } else if (flat0 === 2) {
      fight.blessCtr = Math.min(0xff, (fight.blessCtr | 0) + 1);
      effect = true;
    } else if (flat0 === 3) {
      if (fight.partyCount <= 1) {
        applyHealCa58(fight, 0, 8, spellName);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 5) {
      if (fight.partyCount <= 1) {
        const lv = (caster.level | 0) > 0 ? caster.level | 0 : 1;
        let heal = 0;
        for (let i = 0; i < lv; i++) heal += rngRange(fight, 1, 10);
        applyHealCa58(fight, 0, heal, spellName);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 7) {
      if (fight.partyCount <= 1) {
        applyHealCa58(fight, 0, 15, spellName);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 16) {
      if (fight.partyCount <= 1) {
        applyCureMaskToPartySlot(fight, 0, 0x77, false, "Cure Poison");
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 22) {
      if (fight.partyCount <= 1) {
        applyCureMaskToPartySlot(fight, 0, 0x7b, false, "Cure Disease");
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 11) {
      const lv = (caster.level | 0) > 0 ? caster.level | 0 : 1;
      session.forcesProtect = (lv + 0x14) & 0xff;
      session.scriptAbort = 1;
      session.posChangedLatch = 0;
      effect = true;
    } else if (flat0 === 30) {
      if (fight.partyCount <= 1) {
        applyCureMaskToPartySlot(fight, 0, 0, true, "Remove Condition");
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 4) {
      session.lightFactor = Math.min(0xfe, (session.lightFactor | 0) + 1);
      session.posChangedLatch = 1;
      session.scriptAbort = 1;
      effect = true;
    } else if (flat0 === 15) {
      if ((caster.food | 0) < 0x28) {
        caster.food = ((caster.food | 0) + 8) & 0xff;
        session.posChangedLatch = 1;
        effect = true;
      }
    } else if (flat0 === 18) {
      const light = session.lightFactor | 0;
      session.lightFactor = light > 0xeb ? 0xff : (light + 0x14) & 0xff;
      session.posChangedLatch = 1;
      session.scriptAbort = 1;
      effect = true;
    } else if (flat0 === 19) {
      session.walkWaterFlag = 1;
      session.scriptAbort = 1;
      session.posChangedLatch = 0;
      effect = true;
    } else if (flat0 === 21) {
      session.buff79a3 = 1;
      effect = true;
    } else if (flat0 === 31) {
      session.buff79a1 = 1;
      effect = true;
    } else if (flat0 === 35) {
      session.talismanBase = 1;
      effect = true;
    } else if (flat0 === 41) {
      session.buff79a2 = 1;
      effect = true;
    } else if (flat0 === 6) {
      let arg = caster.level | 0;
      if (arg < 0x10) arg = (arg * 0x10) & 0xff;
      const killed = applyTurnUndeadC050(fight, api, arg);
      if (!fight.msgQueue?.length) setStatus(fight, `${name} casts ${spellName} (${killed}).`);
      fight.spellActed = 1;
      return;
    } else if (flat0 === 8) {
      if (fight.partyCount <= 1) {
        applyHeroismToPartySlot(fight, 0);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 9) {
      /* Nature's Gate @ 0xB0EA — CombatSession.cpp:1561–1600. */
      let fail = 0;
      if ((session.attribFlags | 0) & 0x40) fail++;
      if ((session.era | 0) !== 9) fail++;
      if (!fail) {
        const dayTbl = session.eraDay ?? session.days;
        const day =
          Array.isArray(dayTbl) && dayTbl.length > 9
            ? dayTbl[9] | 0
            : session.day | 0;
        const kMonth = [20, 40, 60, 80, 93, 94, 100, 120, 130, 140, 150, 181, 255];
        const kSeasonA = [11, 14, 33, 9, 37, 8, 37, 6, 14, 39, 8, 11, 11];
        const kSeasonB = [
          0xb5, 0x61, 0x74, 0x2c, 0x55, 0x65, 0x55, 0xd4, 0x0f, 0x73, 0xec, 0x37, 0xb8,
        ];
        let tier = 12;
        if (day & 1) {
          tier = 0;
          while (tier < 13 && day >= kMonth[tier]) tier++;
        }
        if (day >= 0x96) {
          session.era = 8;
          tier = 11;
        }
        if (tier > 12) tier = 12;
        session.screenId = kSeasonA[tier];
        session.screen = kSeasonA[tier];
        session.mapId = kSeasonA[tier];
        const packed = kSeasonB[tier];
        session.x = packed & 0x0f;
        session.y = (packed >> 4) & 0x0f;
        session.exitFlags = 1;
        session.scriptAbort = 1;
        effect = true;
      } else {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
    } else if (flat0 === 23) {
      if (fight.partyCount <= 1) {
        applyRestoreAlignmentToPartySlot(fight, 0);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 24) {
      let fail = 0;
      if ((session.attribFlags | 0) & 0x40) fail++;
      if (!(session.attribRecallCoord | 0)) fail++;
      if (!fail) {
        const packed = session.attribRecallCoord | 0;
        session.x = packed & 0x0f;
        session.y = (packed >> 4) & 0x0f;
        session.screen = session.attribRecallScreen | 0;
        session.posChangedLatch = 1;
        session.scriptAbort = 1;
        effect = true;
      } else {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
    } else if (flat0 === 25) {
      const lv = (caster.level | 0) > 0 ? caster.level | 0 : 1;
      const add = (lv >> 1) & 0xff;
      fight.holyBonusCtr = Math.min(0xff, (fight.holyBonusCtr | 0) + add);
      effect = true;
    } else if (flat0 === 28) {
      if (session.frenzyLatch) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      if (fight.partyCount <= 1) {
        applyFrenzyToPartySlot(fight, api, 0);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 32) {
      if (fight.partyCount <= 1) {
        applyRejuvenateToPartySlot(fight, 0);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 39) {
      if (fight.partyCount <= 1) {
        applyRaiseDeadToPartySlot(fight, 0);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 33) {
      if (fight.partyCount <= 1) {
        applyStoneToFleshToPartySlot(fight, 0);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 43) {
      if ((session.attribFlags | 0) & 0x40) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "Town (1-5)?");
      return;
    } else if (flat0 === 46) {
      if (fight.partyCount <= 1) {
        applyResurrectionToPartySlot(fight, 0);
        fight.spellActed = 1;
        return;
      }
      beginPartyPickCast(fight, flat0, "On whom");
      return;
    } else if (flat0 === 47) {
      beginItemPickCast(fight, flat0, "Uncurse which (A-F)?");
      return;
    } else if (flat0 === 44) {
      if (fight.explorationCast) {
        setStatus(fight, "* Spell Failed *");
        fight.spellActed = 1;
        return;
      }
      if (session.divineInterventionUsed) {
        setStatus(fight, "Divine Intervention already used.");
        fight.spellActed = 1;
        return;
      }
      session.divineInterventionUsed = 1;
      for (let p = 0; p < fight.partyCount; p++) {
        const m = partyMember(fight, p);
        if (m && (m.condition | 0) !== 0xff) {
          m.condition = 0;
          m.hpMax = m.hpCurrent != null ? m.hpCurrent | 0 : m.hpAux | 0;
        }
      }
      effect = true;
    } else if (flat0 === 45) {
      session.holyWordGate = 1;
      fight.holyWordGate = 1;
      const killed = applyTurnUndeadC050(fight, api, 0);
      if (!fight.msgQueue?.length) setStatus(fight, `${name} casts ${spellName} (${killed}).`);
      fight.spellActed = 1;
      return;
    }
  }
  fight.spellActed = 1;
  if (effect) {
    setStatus(fight, `${name} casts ${spellName}.`);
    return;
  }
  if (
    (school === SCHOOL_SORC && inList(K_SORC_AUTO_AOE, flat0)) ||
    (school === SCHOOL_CLER && inList(K_CLER_AUTO_AOE, flat0))
  ) {
    if (fight.explorationCast) {
      setStatus(fight, "* Spell Failed *");
      return;
    }
    applyCastToMonsterTarget(fight, api, 0, flat0);
    return;
  }
  let combatTarget = false;
  if (school === SCHOOL_SORC) combatTarget = inList(K_SORC_COMBAT, flat0);
  else if (school === SCHOOL_CLER) combatTarget = inList(K_CLER_COMBAT, flat0);
  if (combatTarget) {
    if (fight.explorationCast) {
      setStatus(fight, "* Spell Failed *");
      return;
    }
    fight.pendingCastFlat = flat0;
    fight.castTargetSlot = -1;
    fight.state = PICK.CastTarget;
    fight.spellActed = 0;
    fight.spellDamage = 0;
    setStatus(fight, "Which monster?");
    return;
  }
  setStatus(fight, `${name} casts ${spellName} (stub).`);
}

export function tickCastTarget(fight, api, key) {
  const uk = key.toUpperCase();
  if (uk < "A" || uk > "J") return false;
  const letter = uk.charCodeAt(0) - 0x41;
  const alive = countAliveMonsters(fight);
  if (letter >= alive) return false;
  fight.castTargetSlot = letter;
  fight.spellActed = 1;
  const flat = fight.pendingCastFlat | 0;
  const slot = nthAliveMonster(fight, letter);
  applyCastToMonsterTarget(fight, api, slot, flat);
  fight.pendingCastFlat = -1;
  api.beginActionAck(fight);
  return true;
}

export function tickPartyPick(fight, api, key) {
  const flat = fight.pendingCastFlat | 0;
  const pc = fight.partyCount | 0;
  const uk = key.toUpperCase();
  if (fight.pendingCastSchool === SCHOOL_SORC && flat === 12) {
    if (key === "1") {
      sessionSetLloyd(fight);
      setStatus(fight, "Beacon set.");
    } else if (key === "2") {
      sessionRecallLloyd(fight);
      setStatus(fight, "Beacon recalled.");
    } else return false;
    fight.pendingCastFlat = -1;
    fight.pendingCastSchool = -1;
    fight.spellActed = 1;
    api.beginActionAck(fight);
    return true;
  }
  if (fight.pendingCastSchool === SCHOOL_SORC && flat === 30) {
    if (key < "1" || key > "9") return false;
    let steps = key.charCodeAt(0) - 0x30;
    const { dx, dy } = facingDelta(fight.session);
    while (steps-- > 0) {
      fight.session.x = ((fight.session.x | 0) + dx) & 0x0f;
      fight.session.y = ((fight.session.y | 0) + dy) & 0x0f;
    }
    fight.session.scriptAbort = 1;
    fight.session.posChangedLatch = 0;
    fight.pendingCastFlat = -1;
    fight.pendingCastSchool = -1;
    fight.spellActed = 1;
    setStatus(fight, "Teleport.");
    api.beginActionAck(fight);
    return true;
  }
  if (fight.pendingCastSchool === SCHOOL_SORC && flat === 15) {
    if (fight.castAux < 0) {
      if (uk < "A" || uk > "E") return false;
      fight.castAux = uk.charCodeAt(0) - 0x41;
      setStatus(fight, "(1-4)?");
      return true;
    }
    if (key < "1" || key > "4") return false;
    const row = key.charCodeAt(0) - 0x31;
    const idx = fight.castAux * 4 + row;
    fight.session.y = 0xff;
    fight.session.x = 0xff;
    fight.session.screen = fight.flyScreens[idx] ?? 0;
    fight.session.posChangedLatch = 1;
    fight.session.scriptAbort = 1;
    fight.castAux = -1;
    fight.pendingCastFlat = -1;
    fight.pendingCastSchool = -1;
    fight.spellActed = 1;
    setStatus(fight, "Fly.");
    api.beginActionAck(fight);
    return true;
  }
  if (fight.pendingCastSchool === SCHOOL_CLER && flat === 43) {
    if (key < "1" || key > "5") return false;
    fight.session.screen = key.charCodeAt(0) - 0x31;
    fight.session.x = 0xff;
    fight.session.y = 0xff;
    fight.session.posChangedLatch = 1;
    fight.session.scriptAbort = 1;
    fight.pendingCastFlat = -1;
    fight.pendingCastSchool = -1;
    fight.spellActed = 1;
    setStatus(fight, "Town Portal.");
    api.beginActionAck(fight);
    return true;
  }
  if (key < "1" || key > String(pc)) return false;
  const partySlot = key.charCodeAt(0) - 0x31;
  fight.pendingCastFlat = -1;
  fight.pendingCastSchool = -1;
  fight.spellActed = 1;
  const caster = partyMember(fight, fight.activePartySlot);
  if (flat === 8) applyHeroismToPartySlot(fight, partySlot);
  else if (flat === 28) applyFrenzyToPartySlot(fight, api, partySlot);
  else if (flat === 3) applyHealCa58(fight, partySlot, 8, "First Aid");
  else if (flat === 5) {
    const lv = (caster?.level | 0) > 0 ? caster.level | 0 : 1;
    let heal = 0;
    for (let i = 0; i < lv; i++) heal += rngRange(fight, 1, 10);
    applyHealCa58(fight, partySlot, heal, "Power Cure");
  } else if (flat === 7) applyHealCa58(fight, partySlot, 15, "Cure Wounds");
  else if (flat === 16) applyCureMaskToPartySlot(fight, partySlot, 0x77, false, "Cure Poison");
  else if (flat === 22) applyCureMaskToPartySlot(fight, partySlot, 0x7b, false, "Cure Disease");
  else if (flat === 30) applyCureMaskToPartySlot(fight, partySlot, 0, true, "Remove Condition");
  else if (flat === 23) applyRestoreAlignmentToPartySlot(fight, partySlot);
  else if (flat === 32) applyRejuvenateToPartySlot(fight, partySlot);
  else if (flat === 39) applyRaiseDeadToPartySlot(fight, partySlot);
  else if (flat === 33) applyStoneToFleshToPartySlot(fight, partySlot);
  else if (flat === 46) applyResurrectionToPartySlot(fight, partySlot);
  api.beginActionAck(fight);
  return true;
}

function sessionSetLloyd(fight) {
  const s = fight.session;
  s.lloydScreen = s.screen | 0;
  s.lloydCoord = ((s.y & 0x0f) << 4) | (s.x & 0x0f);
}

function sessionRecallLloyd(fight) {
  const s = fight.session;
  s.screen = s.lloydScreen | 0;
  const packed = s.lloydCoord | 0;
  s.x = packed & 0x0f;
  s.y = (packed >> 4) & 0x0f;
  s.posChangedLatch = 1;
  s.scriptAbort = 1;
}

export function tickItemPick(fight, api, key) {
  const flat = fight.pendingCastFlat | 0;
  const uk = key.toUpperCase();
  if (flat === -2) {
    let backpack = false;
    let slot = -1;
    if (key >= "1" && key <= "6") {
      slot = key.charCodeAt(0) - 0x31;
      backpack = false;
    } else if (uk >= "A" && uk <= "F") {
      slot = uk.charCodeAt(0) - 0x41;
      backpack = true;
    } else return false;
    fight.pendingCastFlat = -1;
    const ok = applyItemUse(fight, api, fight.activePartySlot, backpack, slot);
    if (!ok) {
      api.beginActionAck(fight);
      return true;
    }
    if (
      fight.state === PICK.PartyPick ||
      fight.state === PICK.ItemPick ||
      fight.state === PICK.CastTarget
    )
      return true;
    api.beginActionAck(fight);
    return true;
  }
  if (uk < "A" || uk > "F") return false;
  const packSlot = uk.charCodeAt(0) - 0x41;
  const rec = partyMember(fight, fight.activePartySlot);
  if (!rec?.backpack?.[packSlot]?.id) return false;
  const school = fight.pendingCastSchool | 0;
  fight.pendingCastFlat = -1;
  fight.pendingCastSchool = -1;
  fight.spellActed = 1;
  if (flat === 34) applyRechargeToBackpackSlot(fight, packSlot);
  else if (flat === 37) applyDuplicationFromBackpackSlot(fight, packSlot);
  else if (flat === 47 && school === SCHOOL_SORC) applyEnchantToBackpackSlot(fight, packSlot);
  else if (flat === 47) applyUncurseToBackpackSlot(fight, packSlot);
  api.beginActionAck(fight);
  return true;
}

export function applyItemUse(fight, api, partySlot, backpack, slotIndex) {
  const rec = partyMember(fight, partySlot);
  if (!rec || slotIndex < 0 || slotIndex >= 6) {
    setStatus(fight, "No power.");
    return false;
  }
  let itemId = 0;
  let flagsBefore = 0;
  let slotRef = null;
  if (backpack) {
    slotRef = rec.backpack?.[slotIndex];
    if (!slotRef?.id || (slotRef.id | 0) === 0xff) {
      setStatus(fight, "No power.");
      return false;
    }
    if (!(slotRef.charges | 0)) {
      setStatus(fight, "No charges.");
      return false;
    }
    itemId = slotRef.id | 0;
    flagsBefore = slotRef.flags | 0;
    slotRef.charges = ((slotRef.charges | 0) - 1) & 0xff;
    if (!(slotRef.charges | 0)) {
      slotRef.id = 0xff;
      slotRef.flags = 0;
    }
  } else {
    slotRef = rec.equipped?.[slotIndex];
    if (!slotRef?.id) {
      setStatus(fight, "No power.");
      return false;
    }
    if (slotRef.charges != null && !(slotRef.charges | 0)) {
      setStatus(fight, "No charges.");
      return false;
    }
    itemId = slotRef.id | 0;
    flagsBefore = slotRef.flags | 0;
    if (fight.state !== "Inactive" && !fight.explorationCast && slotRef.charges != null) {
      slotRef.charges = ((slotRef.charges | 0) - 1) & 0xff;
      if (!(slotRef.charges | 0)) {
        slotRef.id = 0xff;
        slotRef.flags = 0;
      }
    }
  }
  const effect = itemEffectByte(fight.manifest, itemId);
  if (!effect) {
    setStatus(fight, "No power.");
    return false;
  }
  fight.activePartySlot = partySlot;
  if (effect >= 0x80) {
    const code = (effect & 0x7f) - 1;
    if (code < 0) {
      setStatus(fight, "No power.");
      return false;
    }
    const flat = code >= 0x30 ? code - 0x30 : code;
    fight.forceCastSchool = code >= 0x30 ? SCHOOL_CLER : SCHOOL_SORC;
    fight.skipCastCost = true;
    resolvePlayerCast(fight, api, flat);
    fight.forceCastSchool = -1;
    fight.skipCastCost = false;
    return true;
  }
  /* 0xF4DE: amount = flags + lo nibble; kind = (effect>>4)&7.
   * kind0 writes hi byte of hp_current (+$74) — CombatSession.cpp:2385. */
  const amount = ((flagsBefore + (effect & 0x0f)) & 0xff) >>> 0;
  const kind = (effect >> 4) & 7;
  if (kind === 0) {
    const hp = rec.hpCurrent | 0;
    const hi = Math.min(255, ((hp >> 8) & 0xff) + amount);
    rec.hpCurrent = ((hi << 8) | (hp & 0xff)) & 0xffff;
  } else {
    const fields = [null, "might", "speed", "accuracy", "alignmentBase", "level", "spellLevel", "spMax"];
    const field = fields[kind];
    if (field) {
      const sum = (rec[field] | 0) + amount;
      rec[field] = sum > 255 ? 255 : sum;
    }
  }
  setStatus(fight, "Item used.");
  return true;
}

/**
 * CombatSession::castSpellFromSheet @ 2238 — explore/combat sheet Cast entry.
 */
export function castSpellFromSheet(fight, api, partySlot, flat0) {
  if (!fight || partySlot < 0 || partySlot >= fight.partyCount) return;
  fight.explorationCast = fight.state === "Inactive";
  fight.activePartySlot = partySlot;
  fight.forceCastSchool = -1;
  fight.skipCastCost = false;
  if (fight.explorationCast) fight.state = "AwaitingCommand";
  resolvePlayerCast(fight, api, flat0);
  if (fight.explorationCast) {
    if (
      fight.state === "AwaitingActionAck" ||
      fight.state === "AwaitingCommand" ||
      fight.state === "Inactive"
    ) {
      fight.state = "Inactive";
      fight.explorationCast = false;
    }
  }
}

/** CombatSession::tickSheetCastAux @ 2260. */
export function tickSheetCastAux(fight, api, key) {
  if (!fight?.explorationCast) return false;
  if (key === "\u001b") {
    fight.pendingCastFlat = -1;
    fight.pendingCastSchool = -1;
    fight.castAux = -1;
    fight.forceCastSchool = -1;
    fight.skipCastCost = false;
    fight.state = "Inactive";
    fight.explorationCast = false;
    setStatus(fight, "");
    return true;
  }
  const uk = String(key || "").toUpperCase();
  let progressed = false;
  if (fight.state === "AwaitingPartyPick") progressed = tickPartyPick(fight, api, key);
  else if (fight.state === "AwaitingItemPick") progressed = tickItemPick(fight, api, uk);
  else if (fight.state === "AwaitingCastTarget") progressed = tickCastTarget(fight, api, uk);
  if (fight.state === "AwaitingActionAck" || fight.state === "AwaitingCommand") {
    fight.state = "Inactive";
    fight.explorationCast = false;
    fight.forceCastSchool = -1;
    fight.skipCastCost = false;
    fight.activePartySlot = -1;
  }
  return progressed;
}

/** Walker session party / quest stub — mirrors game EventVmHelpers + roster fields (subset). */
"use strict";

import { MAGE_GUILD_MEMBER_MASK } from "./townTables.js";
import { resolveMemberFieldOffset } from "./eventFieldMap.js";

const TOWN_TRAIN_INDEX = { 0: 1, 1: 5, 2: 2, 3: 4, 4: 2 };
const DONATION_GOLD = { 0: 20, 1: 250, 2: 40, 3: 120, 4: 40 };
const DONATION_BITS = { 0: 1, 1: 2, 2: 4, 3: 8, 4: 16 };
const BACKPACK_SLOTS = 6;
const CLASS_NAMES = ["Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Robber", "Ninja", "Barbarian"];

/** Normalize backpack to six roster slots (SoA id/charges/flags). */
export function normalizeMemberBackpack(member) {
  const src = member.backpack ?? [];
  member.backpack = Array.from({ length: BACKPACK_SLOTS }, (_, i) => ({
    id: src[i]?.id ?? 0,
    charges: src[i]?.charges ?? 0,
    flags: src[i]?.flags ?? 0,
  }));
  return member;
}

/** @param {object} raw exported from manifest.defaultParty.members[] */
export function memberFromExport(raw) {
  return normalizeMemberBackpack({
    rosterIndex: raw.rosterIndex ?? 0,
    name: raw.name ?? "Hero",
    sex: raw.sex ?? 0,
    alignmentBase: raw.alignmentBase ?? 0,
    race: raw.race ?? 0,
    classId: raw.classId ?? 0,
    level: raw.level ?? 1,
    spellLevel: raw.spellLevel ?? 0,
    gold: raw.gold ?? 0,
    gems: raw.gems ?? 0,
    experience: raw.experience ?? 0,
    hpCurrent: raw.hpCurrent ?? 1,
    hpMax: raw.hpMax ?? 1,
    hpAux: raw.hpAux ?? raw.hpMax ?? 1,
    spCurrent: raw.spCurrent ?? 0,
    spMax: raw.spMax ?? 0,
    condition: raw.condition ?? 0,
    armorClass: raw.armorClass ?? 0,
    age: raw.age ?? 0,
    food: raw.food ?? 0,
    might: raw.might ?? 10,
    speed: raw.speed ?? 10,
    accuracy: raw.accuracy ?? 10,
    luck: raw.luck ?? 10,
    thievery: raw.thievery ?? 0,
    unlockSkill: raw.unlockSkill ?? 0,
    endurance: raw.endurance ?? 14,
    intelligence: raw.intelligence ?? 10,
    personality: raw.personality ?? 10,
    homeTown: raw.homeTown ?? 0,
    classQuestGuildMask: raw.classQuestGuildMask ?? 0,
    /** Packed secondary-skill / title nibbles @ roster+0x50 (ASM selector 0x6D). */
    skillPack: raw.skillPack ?? 0,
    spellBook: raw.spellBook ?? new Array(12).fill(0),
    pubMeal: raw.pubMeal ?? 0,
    equipped: raw.equipped ?? [],
    backpack: raw.backpack ?? [],
  });
}

/** Sorcerer flat spell index 0..47 in roster spellbook bytes @ +0x51 region. */
export function memberSpellKnown(member, flatIndex) {
  const book = member.spellBook ?? [];
  const byte = flatIndex >> 3;
  const bit = flatIndex & 7;
  return ((book[byte] ?? 0) & (1 << bit)) !== 0;
}

export function memberGrantSpell(member, flatIndex) {
  if (flatIndex < 0 || flatIndex > 47) return false;
  if (!member.spellBook?.length) member.spellBook = new Array(12).fill(0);
  const byte = flatIndex >> 3;
  member.spellBook[byte] = (member.spellBook[byte] ?? 0) | (1 << (flatIndex & 7));
  return true;
}

/**
 * Mage-guild shop-open membership gate, ASM-confirmed at 0x1E410:
 * (record+0x79 class_quest_guild_mask) & MAGE_GUILD_MEMBER_MASK[mapId] != 0.
 */
export function memberGuildMember(member, mapId) {
  const mask = MAGE_GUILD_MEMBER_MASK[mapId] ?? 0;
  return mask !== 0 && ((member.classQuestGuildMask ?? 0) & mask) !== 0;
}

/** Read a roster record byte by offset (mirrors eventVmApplyPartyByteOp field access). */
export function getMemberFieldByte(member, offset) {
  switch (offset) {
    case 0x0b:
      return member.homeTown ?? 0;
    case 0x50:
      return member.skillPack ?? 0;
    case 0x79:
      return member.classQuestGuildMask ?? 0;
    default:
      if (!member.rawFields) member.rawFields = {};
      return member.rawFields[offset] ?? 0;
  }
}

/** Write a roster record byte by offset (full byte replace @ 0x1A1D0 for home town). */
export function setMemberFieldByte(member, offset, value) {
  const v = value & 0xff;
  switch (offset) {
    case 0x0b:
      member.homeTown = v;
      return;
    case 0x50:
      member.skillPack = v;
      return;
    case 0x79:
      member.classQuestGuildMask = v;
      return;
    default:
      if (!member.rawFields) member.rawFields = {};
      member.rawFields[offset] = v;
  }
}

/** ASM @ 0x1A1B2: record+0x0B <- map+1 for each active party member (clears bit 7). */
export function applyInnRegistry(session, mapId) {
  if (mapId < 0 || mapId >= 5) return;
  const home = (mapId + 1) & 0xff;
  for (const m of ensureParty(session)) {
    setMemberFieldByte(m, 0x0b, home);
  }
}

export function memberHasSkillId(member, skillId) {
  const id = skillId & 0x0f;
  if (id < 1 || id > 15) return false;
  const packed = member.skillPack ?? 0;
  return (packed & 0x0f) === id || ((packed >> 4) & 0x0f) === id;
}

export function memberGrantSkillId(member, skillId) {
  const id = skillId & 0x0f;
  if (id < 1 || id > 15) return;
  let packed = member.skillPack ?? 0;
  if ((packed & 0x0f) === 0) {
    packed = (packed & 0xf0) | id;
  } else if (((packed >> 4) & 0x0f) === 0) {
    packed = (packed & 0x0f) | (id << 4);
  } else {
    return;
  }
  member.skillPack = packed & 0xff;
}

/** Both skill-pack nibbles occupied (rosterSkillSlotFull low+high). */
export function memberSkillSlotsFull(member) {
  const packed = member.skillPack ?? 0;
  const lo = packed & 0x0f;
  const hi = (packed >> 4) & 0x0f;
  return lo >= 1 && lo <= 15 && hi >= 1 && hi <= 15;
}

/** @param {object | null} manifest */
export function partyFromManifest(manifest) {
  const dp = manifest?.defaultParty;
  if (!dp?.members?.length) return null;
  return dp.members.map(memberFromExport);
}

/** Fallback single-member stub when roster.dat is absent. */
export function createDefaultMember(overrides = {}) {
  return normalizeMemberBackpack({
    rosterIndex: 0,
    name: "Knight",
    classId: 0,
    level: 5,
    gold: 5000,
    hpCurrent: 60,
    hpMax: 60,
    hpAux: 80,
    spCurrent: 0,
    spMax: 0,
    condition: 0,
    experience: 50000,
    endurance: 14,
    intelligence: 10,
    personality: 10,
    spellLevel: 0,
    backpack: [],
    ...overrides,
  });
}

/** @param {object} session */
export function ensureParty(session) {
  if (!session.party?.length) {
    session.party = [createDefaultMember({ gold: session.gold ?? 5000, level: session.level ?? 5 })];
    session.rosterSlots = [0];
    session.partyCount = 1;
  }
  return session.party;
}

/** @param {object} session @param {number} partySlot 0..partyCount-1 */
export function getPartyMember(session, partySlot = 0) {
  const party = ensureParty(session);
  if (partySlot < 0 || partySlot >= party.length) return party[0];
  return party[partySlot];
}

/** @deprecated use getPartyMember(session, 0) */
export function getDefaultMember(session) {
  return getPartyMember(session, 0);
}

export function sessionPartyGoldTotal(session) {
  return ensureParty(session).reduce((sum, m) => sum + (m.gold | 0), 0);
}

/** @param {object} session */
export function syncSessionGoldFromParty(session) {
  const party = ensureParty(session);
  session.gold = sessionPartyGoldTotal(session);
  session.level = party[0]?.level ?? session.level ?? 1;
  session.hp = party[0]?.hpCurrent ?? session.hp ?? 0;
  session.maxHp = party[0]?.hpMax ?? session.maxHp ?? 0;
  session.partyCount = party.length;
}

/** @param {object} session — editGold UI writes member 0 + keeps total in sync */
export function syncPartyFromSessionGold(session) {
  const m = getPartyMember(session, 0);
  if (session.gold != null) m.gold = session.gold | 0;
  if (session.level != null) m.level = session.level | 0;
  if (session.hp != null) m.hpCurrent = session.hp | 0;
  if (session.maxHp != null) {
    m.hpMax = session.maxHp | 0;
    if (m.hpAux < m.hpMax) m.hpAux = m.hpMax;
  }
}

/** @param {object} member */
export function memberBackpackFreeSlot(member) {
  normalizeMemberBackpack(member);
  for (let i = 0; i < BACKPACK_SLOTS; i++) {
    if (!member.backpack[i].id) return i;
  }
  return -1;
}

/** @param {object} member @param {number} id @param {number} charges @param {number} flags @param {number} slot */
export function memberGiveItem(member, id, charges = 0, flags = 0, slot = -1) {
  if (!id) return false;
  normalizeMemberBackpack(member);
  const idx = slot >= 0 ? slot : memberBackpackFreeSlot(member);
  if (idx < 0 || idx >= BACKPACK_SLOTS) return false;
  member.backpack[idx] = { id, charges, flags };
  return true;
}

export function memberHasItem(member, id, consume = false) {
  if (!id) return false;
  normalizeMemberBackpack(member);
  for (let i = 0; i < BACKPACK_SLOTS; i++) {
    const slot = member.backpack[i];
    if (slot.id === id) {
      if (consume) {
        slot.id = 0;
        slot.charges = 0;
        slot.flags = 0;
      }
      return true;
    }
  }
  return false;
}

/**
 * OP_16 @ 0x16520: count item in equipped (+0x28) and backpack (+0x3A) slots.
 * @param {object} member
 * @param {number} itemId
 */
export function memberCountItem(member, itemId) {
  if (!itemId) return 0;
  let count = 0;
  normalizeMemberBackpack(member);
  for (let i = 0; i < BACKPACK_SLOTS; i++) {
    if (member.backpack[i].id === itemId) count++;
  }
  const eq = member.equipped ?? [];
  for (let i = 0; i < BACKPACK_SLOTS; i++) {
    if (eq[i]?.id === itemId) count++;
  }
  return count;
}

/** OP_16: match count on the first party member with any hit (asm 0x16520). */
export function sessionCountPartyItem(session, itemId) {
  for (const m of ensureParty(session)) {
    const count = memberCountItem(m, itemId);
    if (count > 0) return count;
  }
  return 0;
}

/**
 * OP_32 @ 0x17190 -> 0x04614: sum nibbles of record+0x50 equal to id over
 * living members (condition < 0x81).
 */
export function sessionCountPartyNibbleMatches(session, id) {
  let count = 0;
  const want = id & 0x0f;
  for (const m of ensureParty(session)) {
    if ((m.condition ?? 0) >= 0x81) continue;
    const packed = m.skillPack ?? 0;
    if ((packed & 0x0f) === want) count++;
    if (((packed >> 4) & 0x0f) === want) count++;
  }
  return count;
}

/** roster_count_living_chars @ 0x47A2 / OP_31 abort gate 0x47EC: (condition & 0xE0) == 0. */
export function sessionCountLivingPartyMembers(session) {
  let count = 0;
  for (const m of ensureParty(session)) {
    if (((m.condition ?? 0) & 0xe0) === 0) count++;
  }
  return count;
}

/** Arena Games ticket item ids (0xD0..0xD3 = Green/Yellow/Red/Black), asm 0x9DAE-0x9DC6. */
export const ARENA_TICKET_LO = 0xd0;
export const ARENA_TICKET_HI = 0xd3;
export const ARENA_TICKET_COLOR_NAMES = ["Green", "Yellow", "Red", "Black"];

/**
 * asm 0x9D9C-0x9DDA: scan every party member's BACKPACK ONLY (not equipped
 * slots), member-major then backpack-slot-minor order, first match wins.
 * @param {object[]} party
 */
export function findArenaTicket(party) {
  for (let m = 0; m < party.length; m++) {
    const member = party[m];
    normalizeMemberBackpack(member);
    for (let slot = 0; slot < BACKPACK_SLOTS; slot++) {
      const id = member.backpack[slot]?.id ?? 0;
      if (id >= ARENA_TICKET_LO && id <= ARENA_TICKET_HI) {
        return { found: true, color: id - ARENA_TICKET_LO, memberIndex: m, backpackSlot: slot };
      }
    }
  }
  return { found: false, color: 0, memberIndex: -1, backpackSlot: -1 };
}

/** asm 0x9E40 (thunk -$7F26): remove the ticket located by findArenaTicket. */
export function consumeArenaTicket(party, ticket) {
  if (!ticket?.found) return;
  const member = party[ticket.memberIndex];
  if (!member) return;
  normalizeMemberBackpack(member);
  member.backpack[ticket.backpackSlot] = { id: 0, charges: 0, flags: 0 };
}

/** @param {object | null} manifest @param {number} id */
export function itemNameFromManifest(manifest, id) {
  if (!id) return "(empty)";
  const raw = manifest?.items?.[id];
  if (raw) return String(raw).trim() || `item #${id}`;
  return `item #${id}`;
}

/** @param {object | null} manifest @param {number} id */
export function itemGoldFromManifest(manifest, id) {
  if (!manifest?.itemsGold || id < 0 || id >= manifest.itemsGold.length) return 0;
  return manifest.itemsGold[id] | 0;
}

/** Quest / engine var ids (OP_17 / OP_1A group byte). */
const VAR_OFFSETS = {
  0x23: "levitate",
  0x27: "talisman0",
  0x28: "talisman1",
  0x29: "talisman2",
  0x2a: "talisman3",
  0x2b: "classQuestCnt",
  0x2c: "questCounterB",
  0x32: "guardianCave",
  0x33: "tundraLever",
  0x3b: "xabranGate",
  0x3c: "dawnGate",
  0x3d: "periodB",
  0x3e: "periodA",
  0x84: "eraLow",
};

export const ITEM_GOLD_GOBLET = 0xe0;
export const ITEM_FE_FARTHING = 0xd4;

/** @returns {object} */
export function createSessionState(overrides = {}) {
  const { manifest, party: partyOverride, rosterSlots: slotsOverride, ...rest } = overrides;
  const fromManifest = manifest ? partyFromManifest(manifest) : null;
  const party = partyOverride ?? fromManifest ?? [createDefaultMember()];
  const rosterSlots =
    slotsOverride ?? (fromManifest ? party.map((m) => m.rosterIndex ?? 0) : [0]);
  const totalGold = party.reduce((sum, m) => sum + (m.gold | 0), 0);
  return {
    gold: rest.gold ?? totalGold,
    gems: rest.gems ?? party.reduce((n, m) => n + (m.gems | 0), 0),
    day: 1,
    year: 1,
    era: 0,
    hp: party[0]?.hpCurrent ?? 0,
    maxHp: party[0]?.hpMax ?? 0,
    level: party[0]?.level ?? 1,
    party,
    rosterSlots,
    partyCount: party.length,
    cond: 0,
    combatVictory: false,
    scriptCounter: 0,
    flags: {},
    partyFields: { 0x76: 0x01 },
    inventory: [],
    clearedTiles: new Map(),
    questDonationBits: 0,
    selectedMember: 0,
    foundItemBuffer: null,
    inputBuffer: "",
    titleNibbleCount: 0,
    ...rest,
  };
}

export function trainingCostPerChar(level, screenId) {
  const idx = TOWN_TRAIN_INDEX[screenId] ?? 1;
  return level * idx * 50;
}

export function healingCostPerChar(level, screenId) {
  const idx = TOWN_TRAIN_INDEX[screenId] ?? 1;
  return level * idx * 10;
}

export function donationGold(screenId) {
  return DONATION_GOLD[screenId] ?? 20;
}

function resolveFieldOffset(selector) {
  return resolveMemberFieldOffset(selector);
}

export function sessionAddGold(session, amount) {
  const m = getPartyMember(session, 0);
  m.gold = Math.max(0, (m.gold | 0) + (amount | 0));
  syncSessionGoldFromParty(session);
}

export function sessionDeductGold(session, amount) {
  let need = amount | 0;
  if (sessionPartyGoldTotal(session) < need) return false;
  for (const m of ensureParty(session)) {
    if (need <= 0) break;
    const take = Math.min(m.gold | 0, need);
    m.gold -= take;
    need -= take;
  }
  syncSessionGoldFromParty(session);
  return true;
}

export function sessionAddGems(session, n) {
  session.gems = Math.max(0, (session.gems | 0) + (n | 0));
}

/** @param {object} session @param {number} id @param {boolean} consume */
export function sessionHasItem(session, id, consume = false) {
  if (!id) return false;
  for (const m of ensureParty(session)) {
    if (memberHasItem(m, id, consume)) return true;
  }
  const stack = session.inventory?.find((s) => s.id === id && s.count > 0);
  if (!stack) return false;
  if (consume) {
    stack.count--;
    if (stack.count <= 0) {
      session.inventory = session.inventory.filter((s) => s.count > 0);
    }
  }
  return true;
}

/** @returns {boolean} placed on a member backpack (true) or overflow buffer (false) */
export function sessionGiveItem(session, id, charges = 0, flags = 0) {
  if (!id) return false;
  for (const m of ensureParty(session)) {
    if (memberBackpackFreeSlot(m) >= 0) {
      memberGiveItem(m, id, charges, flags);
      return true;
    }
  }
  session.foundItemBuffer = { id, charges, flags };
  return false;
}

/** OP_2A: fill found-item buffer only (mm2_found_items_op2a_fill — no party deposit). */
export function sessionApplyTreasure(session, treasure) {
  if (!treasure) return;
  session.foundItemBuffer = {
    gold: treasure.gold ?? 0,
    gems: treasure.gems ?? 0,
    items: [...(treasure.items ?? [])],
  };
}

export function sessionLoadVar(session, group) {
  if (group <= 0x17) {
    return session.flags[group] ?? 0;
  }
  const key = VAR_OFFSETS[group];
  if (key === "eraLow") return session.era & 0xff;
  if (key === "classQuestCnt") return session.flags[0x2b] ?? 0;
  if (key === "questCounterB") return session.flags[0x2c] ?? 0;
  return session.flags[group] ?? 0;
}

export function sessionStoreVar(session, group, val) {
  if (group <= 0x17) {
    session.flags[group] = val & 0xff;
    return;
  }
  const key = VAR_OFFSETS[group];
  if (key === "eraLow") session.era = val & 0xff;
  else session.flags[group] = val & 0xff;
}

/**
 * OP_15 (test) / OP_18 (masked write) on per-member roster fields.
 * @returns {boolean} cond result for test-only
 */
/** OP_15/18 @ 0x16426 — member_spec 0=all (N..1), 1..8=one, 9=selected. */
export function sessionApplyPartyByteOp(
  session,
  memberSpec,
  selector,
  val,
  { masked = false, andM = 0xff, orM = 0, testOnly = false } = {}
) {
  const off = resolveFieldOffset(selector);
  if (off == null) return false;

  const party = ensureParty(session);
  const partyN = party.length;
  session.cond = 0;

  let spec = memberSpec & 0xff;
  let effectiveOr = orM;
  if (spec >= 0x80) {
    effectiveOr = session.cond; /* incoming was cleared; bit7 path uses saved — approx */
    spec &= 0x7f;
  }

  const slots = [];
  if (spec === 0) {
    for (let m = partyN; m >= 1; m--) slots.push(m);
  } else if (spec === 9) {
    const sel = (session.selectedMember ?? 0) + 1;
    if (sel >= 1 && sel <= partyN) slots.push(sel);
  } else {
    let one = spec;
    if (one > partyN) one = 1;
    if (one >= 1 && one <= partyN) slots.push(one);
  }

  let cond = 0;
  for (const slot1 of slots) {
    const member = party[slot1 - 1];
    if (!member) continue;
    let byteVal = getMemberFieldByte(member, off);
    if (masked) {
      byteVal = (byteVal & andM) | effectiveOr;
      setMemberFieldByte(member, off, byteVal);
    } else if (testOnly) {
      let piece = byteVal;
      if (val !== 0) piece &= val;
      cond |= piece;
    }
  }
  if (testOnly) {
    session.cond = cond & 0xff;
    return cond !== 0;
  }
  return masked;
}

/** OP_1F / OP_20 gold or HP on stub party. */
export function sessionApplyPartyEffect(session, args, subtract = false) {
  const selector = args[0] ?? 0;
  const value24 = (args[2] ?? 0) | ((args[3] ?? 0) << 8) | ((args[4] ?? 0) << 16);
  session.cond = 1;
  if (selector === 0x3c || selector === 0x3d) {
    const amount = value24 & 0xffffffff;
    if (subtract) {
      if (!sessionDeductGold(session, amount)) {
        session.cond = 0;
        return;
      }
    } else {
      sessionAddGold(session, amount);
    }
    return;
  }
  if (selector === 0x02 || selector === 0x03) {
    const amount = value24 & 0xff;
    if (subtract) {
      if (session.hp < amount) session.cond = 0;
      else session.hp -= amount;
    } else {
      session.hp = Math.min(session.maxHp, session.hp + amount);
    }
  }
}

export function sessionPartyGemsTotal(session) {
  return ensureParty(session).reduce((sum, m) => sum + (m.gems | 0), 0);
}

/** OP_24 → 0x6ACE: deduct amount from party gold pool; remainder on first member. */
export function sessionTryPayGold(session, amount) {
  const need = amount | 0;
  const party = ensureParty(session);
  const total = sessionPartyGoldTotal(session);
  if (total < need) return false;
  let remain = total - need;
  let pooled = false;
  for (const m of party) {
    m.gold = pooled ? 0 : remain;
    pooled = true;
    remain = 0;
  }
  syncSessionGoldFromParty(session);
  return pooled || need === 0;
}

/** OP_25 → 0x6B9A: same for gems (+$5C). */
export function sessionTryPayGems(session, amount) {
  const need = amount | 0;
  const party = ensureParty(session);
  let total = 0;
  for (const m of party) total += m.gems | 0;
  /* Also honor session.gems aggregate if members lack per-char gems. */
  if (total === 0 && (session.gems | 0) > 0) total = session.gems | 0;
  if (total < need) return false;
  let remain = total - need;
  let pooled = false;
  for (const m of party) {
    m.gems = pooled ? 0 : remain;
    pooled = true;
    remain = 0;
  }
  session.gems = party.reduce((s, m) => s + (m.gems | 0), 0);
  return pooled || need === 0;
}

/** @deprecated OP_25 is gems try-pay, not ticket/key codes. */
export function sessionCheckCode16(session, code) {
  return sessionTryPayGems(session, code);
}

export function sessionClearTileFlag(session, screenId, x, y, maps) {
  const pos = (y << 4) | x;
  const key = `${screenId}:${pos}`;
  session.clearedTiles.set(key, true);
  const sc = maps?.screens?.[screenId];
  if (sc?.collision && pos >= 0 && pos < sc.collision.length) {
    sc.collision[pos] = sc.collision[pos] & ~0x80;
  }
}

export function isTileCleared(session, screenId, pos) {
  return session.clearedTiles.has(`${screenId}:${pos}`);
}

/** @param {object} session @param {object | null} manifest */
export function formatSessionPartyPanel(session, manifest) {
  syncSessionGoldFromParty(session);
  const party = ensureParty(session);
  const lines = [
    `Party (${party.length}) — total gold ${sessionPartyGoldTotal(session)} gp`,
    `Day ${session.day}  Gems ${session.gems}  VM cond=${session.cond}`,
  ];
  for (let i = 0; i < party.length; i++) {
    const m = party[i];
    const cls = CLASS_NAMES[m.classId] ?? `#${m.classId}`;
    lines.push(
      `${i + 1}. ${m.name} ${cls} L${m.level}  ${m.hpCurrent}/${m.hpMax} HP  ${m.gold} gp` +
        (m.condition ? `  cond=0x${(m.condition & 0xff).toString(16)}` : "")
    );
    const bp = m.backpack
      .filter((s) => s.id)
      .map((s) => {
        const name = itemNameFromManifest(manifest, s.id);
        return s.charges ? `${name}(${s.charges})` : name;
      })
      .join(", ");
    if (bp) lines.push(`   ${bp}`);
  }
  if (session.questDonationBits) {
    lines.push(`Temple donate bits: 0x${(session.questDonationBits & 0xff).toString(16)}`);
  }
  if (session.inventory.length) {
    const inv = session.inventory
      .map((s) => {
        const name = manifest?.items?.[s.id]?.trim?.() ?? manifest?.items?.[s.id];
        const label = name ? String(name).trim() : `#${s.id}`;
        return s.count > 1 ? `${label}×${s.count}` : label;
      })
      .join(", ");
    lines.push(`Items: ${inv}`);
  } else {
    lines.push("Items: (empty)");
  }
  const bits = Object.entries(session.partyFields)
    .filter(([, v]) => v)
    .map(([k, v]) => `@${k}=0x${(v & 0xff).toString(16)}`);
  if (bits.length) lines.push(`Fields: ${bits.join(" ")}`);
  const flagKeys = Object.keys(session.flags);
  if (flagKeys.length) {
    lines.push(
      `Vars: ${flagKeys.map((k) => `[${k}]=${session.flags[k]}`).join(" ")}`
    );
  }
  if (session.clearedTiles.size) {
    lines.push(`Cleared tiles: ${session.clearedTiles.size}`);
  }
  return lines.join("\n");
}

export function formatSessionInventoryShort(session, manifest) {
  if (!session.inventory.length) return "none";
  return session.inventory
    .map((s) => {
      const raw = manifest?.items?.[s.id];
      const name = raw ? String(raw).trim() : `#${s.id}`;
      return s.count > 1 ? `${name}×${s.count}` : name;
    })
    .join(", ");
}

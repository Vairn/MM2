/** Walker session party / quest stub — mirrors game EventVmHelpers + roster fields (subset). */
"use strict";

/** @typedef {{ id: number, count: number, charges?: number, flags?: number }} InvStack */

/** Field selector → record byte offset (from game/include/mm2/events/EventFieldMap.h). */
const FIELD_OFFSET = {
  0x6d: 0x50,
  0x74: 0x79,
  0x76: 0x76,
  0x7b: 0x7b,
};

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
    endurance: raw.endurance ?? 14,
    intelligence: raw.intelligence ?? 10,
    personality: raw.personality ?? 10,
    equipped: raw.equipped ?? [],
    backpack: raw.backpack ?? [],
  });
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
  const sel = selector & 0x7f;
  if (FIELD_OFFSET[sel] != null) return FIELD_OFFSET[sel];
  if (sel <= 0x17) return null;
  return sel <= 0x55 ? sel : null;
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

export function sessionApplyTreasure(session, treasure) {
  if (!treasure) return;
  if (treasure.gold > 0) sessionAddGold(session, treasure.gold);
  if (treasure.gems > 0) sessionAddGems(session, treasure.gems);
  for (const it of treasure.items ?? []) {
    if (it.id) sessionGiveItem(session, it.id, it.charges ?? 0, it.flags ?? 0);
  }
  session.foundItemBuffer = {
    gold: treasure.gold,
    gems: treasure.gems,
    items: treasure.items ?? [],
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
 * OP_15 (test) / OP_18 (masked write) on stub party fields.
 * @returns {boolean} cond result for test-only
 */
export function sessionApplyPartyByteOp(
  session,
  count,
  selector,
  val,
  { masked = false, andM = 0xff, orM = 0, testOnly = false } = {}
) {
  const off = resolveFieldOffset(selector);
  if (off == null) return false;

  let byteVal = session.partyFields[off] ?? 0;
  if (testOnly) {
    session.cond = (byteVal & val) !== 0 ? 1 : 0;
    return session.cond === 1;
  }
  if (masked) {
    byteVal = (byteVal & andM) | orM;
    session.partyFields[off] = byteVal & 0xff;
    session.cond = 1;
    return true;
  }
  return false;
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

export function sessionCheckCode16(session, code) {
  if (code === 0) return true;
  if (code <= 3) return sessionHasItem(session, 208 + code, false);
  if (code >= 0x10 && code <= 0x13) return sessionHasItem(session, 0x70 + code, false);
  return false;
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

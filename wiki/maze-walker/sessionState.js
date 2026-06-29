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
  return {
    gold: 5000,
    gems: 10,
    day: 1,
    year: 1,
    era: 0,
    hp: 100,
    maxHp: 100,
    level: 5,
    cond: 0,
    combatVictory: false,
    scriptCounter: 0,
    /** @type {Record<number, number>} event var bank 0x00..0x17 */
    flags: {},
    /** @type {Record<number, number>} roster byte offsets for stub party member */
    partyFields: { 0x76: 0x01 },
    /** @type {InvStack[]} */
    inventory: [],
    /** @type {Map<string, boolean>} key `${screen}:${pos}` */
    clearedTiles: new Map(),
    questDonationBits: 0,
    selectedMember: 0,
    foundItemBuffer: null,
    inputBuffer: "",
    titleNibbleCount: 0,
    ...overrides,
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

export function sessionPartyGoldTotal(session) {
  return session.gold | 0;
}

export function sessionAddGold(session, amount) {
  session.gold = Math.max(0, (session.gold | 0) + (amount | 0));
}

export function sessionDeductGold(session, amount) {
  const need = amount | 0;
  if (session.gold < need) return false;
  session.gold -= need;
  return true;
}

export function sessionAddGems(session, n) {
  session.gems = Math.max(0, (session.gems | 0) + (n | 0));
}

/** @param {object} session @param {number} id @param {boolean} consume */
export function sessionHasItem(session, id, consume = false) {
  if (!id) return false;
  const stack = session.inventory.find((s) => s.id === id && s.count > 0);
  if (!stack) return false;
  if (consume) {
    stack.count--;
    if (stack.count <= 0) {
      session.inventory = session.inventory.filter((s) => s.count > 0);
    }
  }
  return true;
}

/** @returns {boolean} placed on member (true) or overflow buffer (false) */
export function sessionGiveItem(session, id, charges = 0, flags = 0) {
  if (!id) return false;
  const cap = 6;
  const total = session.inventory.reduce((n, s) => n + s.count, 0);
  if (total >= cap) {
    session.foundItemBuffer = { id, charges, flags };
    return false;
  }
  const existing = session.inventory.find((s) => s.id === id);
  if (existing) existing.count++;
  else session.inventory.push({ id, count: 1, charges, flags });
  return true;
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
      if (session.gold < amount) {
        session.cond = 0;
        return;
      }
      session.gold -= amount;
    } else {
      session.gold = Math.min(0xffffffff, session.gold + amount);
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
  const lines = [
    `Gold: ${session.gold}  Gems: ${session.gems}`,
    `HP: ${session.hp}/${session.maxHp}  L${session.level}  Day ${session.day}`,
    `cond=${session.cond}  combatWin=${session.combatVictory ? "Y" : "n"}`,
  ];
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

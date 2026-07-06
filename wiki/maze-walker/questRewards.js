/** Quest transaction leaves for walker (Feldecarb, Nordon goblet). */
"use strict";

import {
  sessionHasItem,
  sessionGiveItem,
  sessionAddGold,
  memberGrantSpell,
  getPartyMember,
  ITEM_FE_FARTHING,
  ITEM_GOLD_GOBLET,
} from "./sessionState.js";

export const ITEM_CASTLE_KEY = 0xd5;
export const SPELL_EAGLE_EYE_FLAT = 7; /* S2/1 sorcerer, 0-based flat @ roster spellbook */
export const NORDON_GOBLET_XP = 2000;
export const NORDON_GOBLET_GOLD = 1000;

export { ITEM_FE_FARTHING, ITEM_GOLD_GOBLET };

/** Feldecarb Fountain: consume Fe Farthing, grant Castle Key. */
export function applyFeldecarbFountain(session) {
  if (!sessionHasItem(session, ITEM_FE_FARTHING, true)) {
    return { ok: false, reason: "noFarthing" };
  }
  const placed = sessionGiveItem(session, ITEM_CASTLE_KEY, 0, 0);
  return { ok: true, placed };
}

/** Nordon goblet turn-in: XP + Eagle Eye + gold (FAQ / loc 60 str[11]). */
export function applyNordonGobletReturn(session, memberSlot = 0) {
  const member = getPartyMember(session, memberSlot);
  member.experience = (member.experience | 0) + NORDON_GOBLET_XP;
  memberGrantSpell(member, SPELL_EAGLE_EYE_FLAT);
  sessionAddGold(session, NORDON_GOBLET_GOLD);
  return { ok: true, member };
}

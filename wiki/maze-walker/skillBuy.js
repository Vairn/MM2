/** OP_0E default-range skill vendors — mirrors game/src/events/EventSkillBuy.cpp */
"use strict";

import {
  sessionPartyGoldTotal,
  memberGrantSkillId,
  memberHasSkillId,
  memberSkillSlotsFull,
  getPartyMember,
} from "./sessionState.js";

/** @typedef {{ execSelector: number, skillId: number, goldCost: number, intro: string|null, memberAlreadySelected: boolean }} SkillBuyOffer */

/** @type {SkillBuyOffer[]} */
export const SKILL_BUY_OFFERS = [
  {
    execSelector: 0x38,
    skillId: 12,
    goldCost: 750,
    intro:
      "Tired of getting lost? For just 750 gold,\nChris will teach you how to use a sextant\nproperly. Learn (y/n)?",
    memberAlreadySelected: false,
  },
  {
    execSelector: 0x3e,
    skillId: 15,
    goldCost: 500,
    intro:
      "\"I am Sgt. Stephen Pain. For 500 gold join\nmy private hell and increase your pathetic\nstamina.\" Train (y/n)?",
    memberAlreadySelected: false,
  },
  {
    execSelector: 0x3f,
    skillId: 8,
    goldCost: 500,
    intro:
      "The enormous gladiator Spartacus will\nmercilessly beat you for 500 gold. You will\nbe stronger. Pay the brute (y/n)?",
    memberAlreadySelected: false,
  },
  {
    execSelector: 0x40,
    skillId: 1,
    goldCost: 500,
    intro:
      "Learn the ancient secrets every weapon hides.\nThe master of accuracy charges 500 gold.\nDo you pay (y/n)?",
    memberAlreadySelected: false,
  },
  {
    execSelector: 0x4d,
    skillId: 5,
    goldCost: 500,
    intro:
      "Rinaldo, the ultimate ambassador, will instruct\nyou in the gentle art of diplomacy for 500 gold.\nLearn (y/n)?",
    memberAlreadySelected: false,
  },
  {
    execSelector: 0x4e,
    skillId: 6,
    goldCost: 200,
    intro:
      "Lucky Spade, the best gambler alive, will teach\nyou his system for 200 gold. Learn (y/n)?",
    memberAlreadySelected: false,
  },
  {
    execSelector: 0x4f,
    skillId: 14,
    goldCost: 300,
    intro:
      "Sly, a seedy looking rogue, will teach you how\n to pickpocket for 300 gold. Pay (y/n)?",
    memberAlreadySelected: false,
  },
  {
    execSelector: 0x52,
    skillId: 7,
    goldCost: 1000,
    intro: null,
    memberAlreadySelected: true,
  },
];

/** @param {number} sel */
export function skillBuyOfferForExecSelector(sel) {
  return SKILL_BUY_OFFERS.find((o) => o.execSelector === (sel & 0xff)) ?? null;
}

function skillSlotsFull(member) {
  return memberSkillSlotsFull(member);
}

/**
 * Grant skill after member pick (eventApplySkillBuy).
 * Inline event.dat scripts check party gold via OP_24 but do not deduct in bytecode.
 * @returns {{ ok: boolean, reason?: string }}
 */
export function applySkillBuy(session, memberSlot, skillId, goldCost) {
  const member = getPartyMember(session, memberSlot);
  if (skillSlotsFull(member)) {
    return { ok: false, reason: "full" };
  }
  if (memberHasSkillId(member, skillId)) {
    return { ok: false, reason: "known" };
  }
  if (sessionPartyGoldTotal(session) < goldCost) {
    return { ok: false, reason: "gold" };
  }
  memberGrantSkillId(member, skillId);
  return { ok: true };
}

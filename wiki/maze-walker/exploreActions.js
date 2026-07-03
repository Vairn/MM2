/** Bash / Unlock — ports game/src/gameplay/ExploreActions.cpp + GameSession handlers. */
"use strict";

import { MAP_GRID } from "./view3d.js";

const VIS_DIR_SHIFT = [6, 4, 2, 0];
const COLL_WALL_BIT = [0x01, 0x04, 0x10, 0x40];

export const WALL_OPEN = 0;
export const WALL_DOOR = 2;

export const ObstructionMsg = {
  Solid: "Solid!",
  Locked: "Locked!",
  NotLocked: "Not Locked!",
  Success: "Success!",
};

/** rng(min,max) @ 0x22BC6 — deterministic LCG matching game/src/gameplay/ExploreActions.cpp */
export class ExploreRng {
  constructor(seed = 0x1234abcd) {
    this.state = seed ? seed >>> 0 : 1;
  }

  raw15() {
    this.state = (Math.imul(this.state, 1664525) + 1013904223) >>> 0;
    return (this.state >>> 17) & 0x7fff;
  }

  range(lo, hi) {
    if (hi <= lo) return lo;
    const span = hi - lo + 1;
    const scale = (0x8000 / span) | 0;
    let q = scale ? (this.raw15() / scale) | 0 : 0;
    if (q >= span) q = span - 1;
    return lo + q;
  }
}

export function forwardVisualField(visual, x, y, facing) {
  const cell = visual[y * MAP_GRID + x] & 0xff;
  const shift = VIS_DIR_SHIFT[facing & 3];
  return (cell >> shift) & 3;
}

function collisionDirForFacing(facing) {
  return (3 - (facing & 3)) & 3;
}

export function doorLockedAt(collision, x, y, facing) {
  const cd = collisionDirForFacing(facing);
  const cell = collision[y * MAP_GRID + x] & 0xff;
  return (cell & COLL_WALL_BIT[cd]) !== 0;
}

export function clearDoorLock(collision, x, y, facing) {
  const idx = y * MAP_GRID + x;
  const cd = collisionDirForFacing(facing);
  collision[idx] = (collision[idx] & ~COLL_WALL_BIT[cd]) & 0xff;
}

export function unlockSkillForMember(member) {
  const at1e = member.unlockSkill ?? 0;
  if (at1e > 0) return at1e;
  return member.thievery ?? 0;
}

export function bestPartyUnlockSkill(party) {
  let best = 0;
  for (const m of party ?? []) {
    best = Math.max(best, unlockSkillForMember(m));
  }
  return best;
}

export function bashDoorRoll(mightSum, doorStrength, roll10_109, trapD100) {
  let strength = mightSum & 0xff;
  const roll10 = (roll10_109 & 0xff) / 10 | 0;
  let success;
  if (roll10 === 5) {
    success = true;
  } else {
    strength = (strength + roll10) & 0xff;
    success = strength >= (doorStrength & 0xff);
  }
  if (!success) return { outcome: "locked", msg: ObstructionMsg.Locked, clearsLock: false };
  if ((trapD100 & 0xff) < 0x33) return { outcome: "trap", msg: null, clearsLock: false };
  return { outcome: "opened", msg: null, clearsLock: true };
}

export function unlockDoorRoll(thievery, lockD100, trapByte, trapD100) {
  const lock = lockD100 & 0xff;
  const thief = thievery & 0xff;
  if (lock < 0x60 && thief >= lock) {
    return { outcome: "success", msg: ObstructionMsg.Success, clearsLock: true };
  }
  if ((trapByte & 0xff) < (trapD100 & 0xff)) {
    return { outcome: "trap", msg: null, clearsLock: false };
  }
  return { outcome: "locked", msg: ObstructionMsg.Locked, clearsLock: false };
}

export function partyMightSum(party) {
  let sum = 0;
  const n = Math.min(party?.length ?? 0, 2);
  for (let i = 0; i < n; ++i) {
    sum += party[i]?.might ?? 0;
  }
  return sum;
}

export function applyDoorTrapDamage(party, rng) {
  if (!party?.length) return null;
  const slot = rng.range(0, party.length - 1);
  const member = party[slot];
  const damage = rng.range(1, 100);
  const hp = member.hpCurrent ?? 0;
  member.hpCurrent = hp > damage ? hp - damage : 0;
  return { slot, member, damage };
}

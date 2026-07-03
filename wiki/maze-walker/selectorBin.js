/** OP_0E default-path selector binning @ asm 0x15EDC — mirrors EventVmHelpers.cpp */
"use strict";

/** @type {[number, number, number, number][]} */
const SELECTOR_BINS = [
  [0x09, 0x10, 0x3c, 0x08],
  [0x11, 0x37, 0x3d, 0x10],
  [0x38, 0x4b, 0x3e, 0x37],
  [0x4c, 0x54, 0x3f, 0x4b],
  [0x56, 0x5b, 0x40, 0x55],
  [0x5c, 0x5e, 0x41, 0x5b],
  [0x65, 0x69, 0x42, 0x64],
  [0x6a, 0x7c, 0x43, 0x69],
  [0x97, 0x98, 0x44, 0x96],
  [0xe3, 0xf3, 0x45, 0xe2],
  [0xf4, 0xfb, 0x46, 0xf3],
];

/** @param {number} sel */
export function binExecSelector(sel) {
  const s = sel & 0xff;
  for (const [lo, hi, cat, sub] of SELECTOR_BINS) {
    if (s >= lo && s <= hi) return { category: cat, index: s - sub };
  }
  return null;
}

/*
 * CORRECTED 2026-07: a prior pass claimed the OP_0E DEFAULT-range dispatch
 * (0x15EDC bins the selector to category 0x3C..0x46, then 0x160AE calls thunk
 * -$7DFA) always reaches 0x9D76, the "Arena Games" ticket-combat-reward
 * engine. That was backwards. Byte-reading the A4 vtable trampoline table
 * directly out of EXTRACTED/ghidra/mm2_data_00.bin (file offset 0x7FFE+disp)
 * settles it:
 *   -$7DBE (file off 0x0240): 4EF9 00009D76  -> 0x09D76  Arena Games engine
 *   -$7DFA (file off 0x0204): 4EF9 000092F2  -> 0x092F2  event_dat_loader (!)
 * So explicit OP_0E selector 0x08 (thunk -$7DBE) is the SOLE path into the
 * Arena Games engine; the default-range dispatch instead re-enters the SAME
 * event_dat_loader used to load a normal on-map location (0x92F2), with a
 * synthetic category/index — plausibly to run one of event.dat's non-map
 * "string bank" pseudo-records (e.g. decoder location 60, "Nordon/Nordonna/
 * Corak"). That reinvocation mechanism is not reverse-engineered yet. The
 * earlier (wrong) claim made the walker run the ticket-check for every
 * default-range selector, including totally unrelated quest/reward tiles
 * (Olympic trials, post-victory combat tiers, the game-start Corak
 * monologue, ...), all of which incorrectly demanded a ticket.
 *
 * When explicit selector 0x08 IS reached (e.g. Middlegate's arena-entrance
 * tile), the engine unconditionally:
 *   1) scans every party member's BACKPACK ONLY (record+0x3A..0x3F, NOT
 *      equipped slots) for a ticket item 0xD0..0xD3 (asm 0x9D9C-0x9DDA);
 *   2) on miss: "Sorry, but you must have a ticket to compete in these
 *      games." (str @ code 0xA082/0xA0A7, byte-exact) — no combat;
 *   3) on hit: consumes the ticket (thunk -$7F26), shows "The games master
 *      accepts your ticket.  Let the battle begin!" (0xA0BF/0xA0E5), and
 *      arms a FIXED encounter (same battle-slot array + combat thunk as
 *      OP_12) with monster type = ((color*3 + area[screen])*16) + rng(1,16)
 *      (asm 0x9E86-0x9EC2, area table @ data hunk 0xE74);
 *   4) on victory: grants gold from a 4(color) x 3(town tier) table (data
 *      hunk 0xE7A) to the first eligible party member and prints "Winner,
 *      you receive N gold" (0xA0FC/0xA111) — plus a documented ROM bug that
 *      corrupts record+0x79 (doc 36-class-quest-hp-bug.md, not replicated).
 *
 * Explicit selector 0x07 (general store, thunk -$7DB8 -> 0xA62C, also
 * byte-verified) does NOT reach this engine either — a distinct fixed
 * address.
 */

/** Data hunk 0xE74 (A4-$718A): per-screen difficulty add-in, indexed by map
 *  screen 0..4 (Middlegate/Atlantium/Tundara/Vulcania/Sandsobar). */
export const ARENA_AREA_INDEX = [0, 2, 0, 0, 1];

/** Data hunk 0xE7A (A4-$7184): 4 ticket colors x 3 town tiers (tier =
 *  min(screen,2): Middlegate=0, Atlantium=1, everyone else=2). */
export const ARENA_GOLD_REWARD = [
  [200, 1500, 500],
  [2000, 5000, 3000],
  [7000, 15000, 10000],
  [20000, 50000, 30000],
];

/** @param {number} color 0..3 @param {number} screen 0..4 */
export function arenaGoldReward(color, screen) {
  const c = Math.max(0, Math.min(3, color | 0));
  const tier = Math.max(0, Math.min(2, screen | 0));
  return ARENA_GOLD_REWARD[c][tier];
}

/** asm 0x9E86-0x9EC2: fixed-encounter monster-type id. `roll1to16` must be
 *  in [1,16] (asm rng(1,16) via thunk -$7BB4). */
export function arenaMonsterType(color, screen, roll1to16) {
  const c = Math.max(0, Math.min(3, color | 0));
  const area = ARENA_AREA_INDEX[screen] ?? ARENA_AREA_INDEX[0];
  const roll = Math.max(1, Math.min(16, roll1to16 | 0));
  return ((c * 3 + area) * 16 + roll) & 0xff;
}

/** @param {number} sel @param {number} [mapId] */
export function describeExecSelector(sel, mapId = -1) {
  const s = sel & 0xff;
  if (s === 0x08) {
    return "Arena Games ticket engine\nOP_0E 0x08 → -$7DBE → 0x9D76";
  }
  const bin = binExecSelector(s);
  const cat = bin
    ? `category 0x${bin.category.toString(16)}, slot ${bin.index}`
    : `selector 0x${s.toString(16)}`;
  return `Default-range dispatch (${cat})\nOP_0E default @ 0x15EDC → -$7DFA → 0x92F2 (event_dat_loader, NOT the arena engine)`;
}

/** Only explicit OP_0E selector 0x08 (thunk -$7DBE) reaches the Arena Games
 *  ticket engine (0x9D76) — corrected 2026-07, see the trace note above.
 *  Default-range selectors (isTownServiceSelector) do NOT. */
export function isEngineCombatSelector(sel, _mapId = -1) {
  return (sel & 0xff) === 0x08;
}

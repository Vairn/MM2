/** Event.dat script VM for the map walker — mirrors game/src/events/EventRuntime.cpp + session stub. */
"use strict";

import {
  createSessionState,
  sessionPartyGoldTotal,
  sessionDeductGold,
  sessionApplyTreasure,
  sessionGiveItem,
  sessionConsumeBackpackItem,
  sessionLoadVar,
  sessionStoreVar,
  sessionApplyPartyByteOp,
  sessionApplyPartyEffect,
  sessionCheckCode16,
  sessionTryPayGold,
  sessionTryPayGems,
  sessionClearTileFlag,
  sessionAddGold,
  sessionCountPartyItem,
  sessionCountPartyNibbleMatches,
  sessionCountLivingPartyMembers,
  sessionSearchPayoff,
  sessionOp31IterateDamage,
  sessionSlideTrapHalve,
  sessionSeedFixedEncounter,
  sessionSeedTileAmbientEncounter,
  syncSessionGoldFromParty,
  getPartyMember,
  ensureParty,
} from "./sessionState.js";

import {
  runTempleService,
  runTrainingService,
  runSmithService,
  runTavernService,
  runGuildService,
  runInnRegistry,
  runArenaTicketSelector,
  promptSelectMember,
  svcGeneralStoreConvert,
  svcFoodEncodePurchase,
  svcDrinkEncodePurchase,
  svcQuestLordArm,
  svcQuestCompleteReward,
  svcQuestBusy,
} from "./townServices.js";

import { binExecSelector } from "./selectorBin.js";
import { runCombatContract } from "./combatSession.js";
import { waitSpaceWithScriptedKey } from "./scriptedKey.js";
import { resolveOp0bSprite, signEnvIdForScreen } from "./serviceSignResolver.js";
import { runOp0eFdPrintChrome } from "./op0eFdChrome.js";
import { TOWN_NAMES } from "./townTables.js";

/** @deprecated use runDefaultRangeOverlay — kept for tests referencing combat bytes. */
const LOC_61_COMBAT_OVERLAY = {
  22: [0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00],
  23: [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
  24: [0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
  25: [0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
};

/**
 * Resolve default-range overlay slot (no side effects).
 * LE string-anchor + pool_seek decoding is done at export time (export_map_walker.py).
 */
function resolveOverlaySlot(ctx, sel) {
  const bin = binExecSelector(sel);
  if (!bin) return null;

  const overlays = ctx.overlays ?? {};
  const loc = overlays[String(bin.category)] ?? overlays[bin.category];
  let slot = loc?.slots?.[String(bin.index)] ?? loc?.slots?.[bin.index];

  /* Fallback: legacy hardcoded loc-61 combat bytes if bundle lacks overlays. */
  if (!slot && bin.category === 0x3d && LOC_61_COMBAT_OVERLAY[bin.index]) {
    slot = {
      kind: "bytecode",
      nodes: [{ op: 0x12, args: LOC_61_COMBAT_OVERLAY[bin.index], pseudo: "encounter_setup(...)" }],
    };
  }
  if (!slot) return null;
  return { bin, loc, slot };
}

/**
 * OP_0E default-range @ 0x15EDC / 0x160A2 — queue only (do NOT run VM here).
 * Mirrors EventRuntime::runDefaultRangeOverlay → QUEUED_EVENT_ID = index.
 * @returns {boolean}
 */
function queueDefaultRangeOverlay(ctx, sel) {
  const resolved = resolveOverlaySlot(ctx, sel);
  if (!resolved) return false;
  const { bin, loc, slot } = resolved;
  const session = ctx.session;
  session.queuedEventId = bin.index & 0xff;
  /* AST walker: stash slot (C++ memcpy's overlay raw into work_buf). */
  ctx._queuedOverlay = { bin, loc, slot };
  ctx.note?.(
    `queue overlay cat 0x${bin.category.toString(16)} idx ${bin.index} (run after abort)`
  );
  return true;
}

/**
 * Scanner epilogue @ 0x176B6 — EventRuntime::runQueuedDispatch.
 * Runs the queued overlay after the current script aborts.
 * @returns {Promise<boolean>}
 */
async function runQueuedDispatch(ctx) {
  const session = ctx.session;
  if ((session.queuedEventId ?? 0xff) === 0xff) return false;

  const q = ctx._queuedOverlay;
  session.queuedEventId = 0xff;
  ctx._queuedOverlay = null;
  if (!q) return false;

  const { bin, loc, slot } = q;
  session.scriptAbort = 0;

  if (slot.kind === "text") {
    const text = ctx.resolveEventText?.(slot.text) ?? slot.text;
    await ctx.waitForSpace(text, ctx.sprite, 0x0e);
    ctx.note(`overlay cat 0x${bin.category.toString(16)} idx ${bin.index}: text`);
    return true;
  }

  if (slot.kind === "bytecode" && slot.nodes?.length) {
    const inner = {
      ...ctx,
      evtData: { nodes: slot.nodes },
      strings: loc?.strings ?? [],
      _queuedOverlay: null,
      _isQueuedOverlay: true,
    };
    const result = await runEventScript(inner);
    if (result.teleported) {
      ctx.teleported = true;
      ctx.ended = true;
    }
    if (result.ended && !result.teleported) ctx.ended = true;
    ctx.note(
      `overlay cat 0x${bin.category.toString(16)} idx ${bin.index}: VM (${slot.nodes.length} nodes)`
    );
    return true;
  }
  return false;
}

/** @deprecated name — queues then caller drains via runQueuedDispatch. */
async function runDefaultRangeOverlay(ctx, sel) {
  return queueDefaultRangeOverlay(ctx, sel);
}

/** Shared prompt-combat after GS seed — CombatSession enter/finish contract. */
async function promptCombatOutcome(ctx, enc, op) {
  return runCombatContract(ctx, enc, op);
}

/** Shared OP_12 fixed-encounter flow (map scripts + loc-61 overlays). */
async function runOp12Combat(ctx, args) {
  const { session, manifest, note, onSessionChange } = ctx;
  sessionSeedFixedEncounter(session, args, false);
  const enc = describeEncounter(manifest, 0x12, args, "");
  ctx.sprite = ctx.sprite ?? enc.sprite ?? null;
  const outcome = await runCombatContract(ctx, enc, 0x12);
  if (outcome === "victory") {
    /* latch already set inside combatFinishVictory */
  } else if (outcome === "refused") {
    note(`${enc.heading}: enter refused`);
  } else {
    ctx.ended = true;
  }
  onSessionChange?.(session);
}

/**
 * Tile-flag ambient fight @ asm 0x176F2: collision 0x80 with no event.dat triplet.
 * Mirrors game eventRunTileAmbientEncounter + eventVmConsumeTileEncounterFlag.
 */
export async function runTileAmbientEncounter(ctx) {
  const { session, manifest, onSessionChange, maps, screenId, tileX, tileY } = ctx;
  const notes = [];
  sessionSeedTileAmbientEncounter(session);
  const enc = describeEncounter(manifest, 0x13, [], "");
  const outcome = await runCombatContract(ctx, enc, 0x13);
  if (outcome === "victory") notes.push("Tile-flag encounter: victory latch set");
  else if (outcome === "refused") notes.push("Tile-flag encounter: enter refused (live=0)");
  else notes.push(`Tile-flag encounter: ${outcome}`);
  sessionClearTileFlag(session, screenId, tileX, tileY, maps);
  onSessionChange?.(session);
  return { won: outcome === "victory", notes };
}

/** Token-skip byte lengths for opcodes 0x00..0x32, read byte-exact from the
 * ROM's opcode_len_tbl @ A4-$6CC8 (data hunk file offset 0x1336; ground
 * truth: tools/dump_event_token_table.py / EXTRACTED/event_token_len_table.json).
 * Used ONLY by the skip-token walker (OP_10/OP_11/OP_2B) to advance PAST a
 * token it does not execute — distinct from (and, at 2 opcodes, DIFFERENT
 * than) "1 + argc":
 *   - op 0x00 (invalid): ROM skip delta is 0, not 1.
 *   - op 0x25 (check-code16): the handler reads 2 argument bytes when
 *     EXECUTED (own length 3, see OP_ARGC), but the ROM's skip-table entry
 *     is only 2 (opcode + 1 byte) — a genuine ROM quirk. A skip walk that
 *     passes OVER an OP_25 token (rather than executing it) under-counts by
 *     one byte in the original game, desyncing the token stream by 1.
 * Mirrors game/src/events/EventVmHelpers.cpp `eventVmTokenDelta`.
 */
export const OP_TOKEN_DELTA = [
  0, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 3, 3, 2, 2, 1, 2, 2, 13, 11, 1, 4, 3, 3, 5, 5, 3,
  2, 2, 2, 2, 7, 7, 4, 3, 3, 3, 2, 1, 1, 3, 1, 15, 2, 2, 3, 3, 1, 11, 4, 2,
];

export const OP_ARGC = {
  0x00: 0, 0x01: 1, 0x02: 1, 0x03: 1, 0x04: 1, 0x05: 1, 0x06: 1, 0x07: 0, 0x08: 0,
  0x09: 0, 0x0a: 0, 0x0b: 2, 0x0c: 2, 0x0d: 1, 0x0e: 1, 0x0f: 0, 0x10: -1, 0x11: -1,
  0x12: 12, 0x13: 10, 0x14: 0, 0x15: 3, 0x16: 2, 0x17: 2, 0x18: 4, 0x19: 4, 0x1a: 2,
  0x1b: 1, 0x1c: 1, 0x1d: 1, 0x1e: 1, 0x1f: 6, 0x20: 6, 0x21: 3, 0x22: 2, 0x23: 2,
  0x24: 2, 0x25: 2, 0x26: 0, 0x27: 0, 0x28: 2, 0x29: 0, 0x2a: 14, 0x2b: -1, 0x2c: 1,
  0x2d: 2, 0x2e: 2, 0x2f: 0, 0x30: 10, 0x31: 3, 0x32: 1,
};

/** Opcode implementation status for audit report (C++ remake fidelity).
 * Keep aligned with EXTRACTED/docs/56-event-system-remaining-gaps.md §3. */
export const OPCODE_STATUS = {
  0x00: "real", 0x01: "real", 0x02: "real", 0x03: "real", 0x04: "real",
  0x05: "real", 0x06: "real", 0x07: "real", 0x08: "real", 0x09: "real",
  0x0a: "real", 0x0b: "real", 0x0c: "real",
  0x0d: "real", /* presentation-only; no GS writes */
  0x0e: "real", /* dispatch + hosted engines; town shop leaves = C++ town engines */
  0x0f: "real", 0x10: "real", 0x11: "real",
  /* OP_12/13: enter/finishVictory/finishLeave contract = CombatSession public API. */
  0x12: "real", 0x13: "real", 0x14: "real", 0x15: "real", 0x16: "real", 0x17: "real",
  0x18: "real", 0x19: "real", 0x1a: "real", 0x1b: "real", 0x1c: "real",
  0x1d: "real", 0x1e: "real", /* presentation/timing only */
  0x1f: "real", 0x20: "real", 0x21: "real", 0x22: "real", 0x23: "real",
  0x24: "real", 0x25: "real", 0x26: "real", 0x27: "real", 0x28: "real", 0x29: "real",
  0x2a: "real", 0x2b: "real", 0x2c: "real", 0x2d: "real", 0x2e: "real", 0x2f: "real",
  0x30: "real", 0x31: "real", 0x32: "real",
};

/** Intra-map portal XY tables @ A4-$70D0 (data 0xF2E) — EventTownServices.cpp. */
const PORTAL_SRC_X = [1, 1, 4, 4, 7, 7, 5, 10, 13, 13];
const PORTAL_SRC_Y = [13, 2, 5, 10, 2, 13, 10, 10, 2, 13];
const PORTAL_DST_X = [1, 1, 4, 4, 7, 7, 10, 10, 13, 13];
const PORTAL_DST_Y = [14, 1, 4, 11, 1, 14, 4, 11, 1, 14];
/** Found-item ranges @ -$70A8 / -$70A5 for selectors 0x81..0x83. */
const FOUND_ITEM_SPAN = [13, 6, 7];
const FOUND_ITEM_BASE = [66, 92, 127];

const SELECTOR_SPRITES = {
  0x01: "68_anm", 0x02: "67_anm", 0x03: "63_anm", 0x04: "66_anm",
  0x05: "37_anm", 0x06: "62_anm", 0x07: null, 0x08: null,
};

/** PlayTownServiceUi::serviceTitle — menu chrome only (not a pre-event prompt). */
const MENU_KIND_TITLE = {
  0x02: "Training",
  0x03: "Tavern",
  0x04: "Temple",
  0x05: "Mage Guild",
  0x06: "Blacksmith",
};

function townName(screenId) {
  return TOWN_NAMES[screenId] ?? TOWN_NAMES[0] ?? "Town";
}

const TAVERN_INTRO = [
  "A low mumble emerges from the middle\nof the road crowd.  Amber, a sultry\nwaitress asks, \"Do you wish to order\nfrom our vast menu of drinks (y/n)?\"",
  "The barkeep Rowena boasts, \"We have\nthe best brewmeister in the land!\nInterested (y/n)?\"",
  "Belinthra, the bawdy proprietress,\nraises her tankard of ale in a toast\nand booms, \"Would you like a sampling\nof my stuff (y/n)?\"",
  "Raising a cup of mead, Gabrielle, the\ngorgeous barmaid winks seductively\nwhile softly asking \"Would you like\n to use my services (y/n)?\"",
  "Amidst bloodthirsty brawling stands\nluscious Lucindra, the barmaid.\nBottles fly through the air as she\nasks, \"Can I do ya something' (y/n)?\"",
];

const TEMPLE_INTRO = [
  "A slim cleric in a cowled robe peers\nat you and asks in a serene voice,\n\"May I aid you, travelers (y/n)?\"",
  "An initiate tends embers of saffron\nin an ornate alterbowl.  He offers\nassistance.  Accept (y/n)?",
  "A drumbeat and candlelight lend the\ntemple an aura of tranquility.  A\npriest offers help.  Accept (y/n)?",
  "Tending the smoke of the lava fire,\n a curate breaks his silent prayer.\n\"May I offer you our services (y/n)?\"",
  "Ambergris and woodcedar incense burns\non the altar as the priest chants.\nHe asks, \"Do you need any help (y/n)?\"",
];

const SMITH_INTRO = [
  "The burly blacksmith Svendegard busily\nshapes a deadly sword in the forge.\nDo you care to see his work (y/n)?",
  "The famous blacksmith Morgan\nDrewnhald asks if you wish to view\nweapons he has made available for\nsale.  View (y/n)?",
  "Flames leap out from the brick\nsmelter as the sweaty smith says,\n\"Do you wish to see my humble\nwares (y/n)?\"",
  "A friendly smith wipes his hands on a\nworn, leather apron.  He shakes your\nhand and asks, \"Care to see my wares\n(y/n)?\"",
  "A tough old blacksmith glares at\nyour party and shouts \"Hey, only\ncowards browse!\"  View wares (y/n)?",
];

const MAGE_GUILD_INTRO = [
  "Sages in multi-hued robes congregate\nin the hall.  The archmage offers\nspells for sale.  Interested (y/n)?",
  "The meeting shifts towards entropy as\nyou step in.  A cabalist approaches\nyou with a spell list.  Buy (y/n)?",
  "Lounging next to a roaring fire which\nburns no wood, mystics offer spells.\nBuy (y/n)?",
  "Magicians clad in furry robes sip\nwine and chat softly.  Listen (y/n)?",
  "Sorcerers sort phials of sands on\nthe shelves.  A man barks, \"Spells\n(y/n)?\"",
];

/** exe-only innkeeper greets @ 0x19D00..0x1A200 (doc 29), map-index ordered. */
const INN_INTRO = [
  "A jolly old innkeeper waves his quill\nin the air as he asks, \"Will you sign\n the registry (y/n)?\"",
  "The well-groomed whiskers of the old\nconcierge twitch as he proclaims, \"We\nhave the finest suites.  Sign\nin (y/n)?\"",
  "Ordigon, the elderly innkeeper, rubs\nhis bushy white mustache and says,\n\"We have cozy, warm beds.  Sign-in\n(y/n)?\"",
  "The aging host of the sleezy inn\nleers at you from behind his desk and\nasks, \"I suppose your party only wants\none room.  Stay (y/n)?\"",
  "The proprietor blows a pile of dust\noff the register and asks eagerly,\n\"Can I sign you in (y/n)?\"",
];

const OP0D_SEQUENCE_NAMES = [
  "gated intro (enable -$79AF)",
  "sequence 1", "sequence 2", "sequence 3", "sequence 4",
  "sequence 5", "sequence 6", "sequence 7", "sequence 8",
  "pre-transition redraw (exit-flag bit0)",
];

export function itemName(manifest, id) {
  if (!id || id <= 0) return null;
  const items = manifest?.items;
  if (Array.isArray(items) && id < items.length && items[id]) {
    return String(items[id]).trim();
  }
  return `item #${id}`;
}

export function monsterName(manifest, id) {
  const idNum = Number(id);
  if (!Number.isFinite(idNum) || idNum <= 0 || idNum >= 256) return null;
  const mons = manifest?.monsters;
  if (!Array.isArray(mons)) return null;
  const byIdx = mons[idNum];
  if (byIdx && byIdx.index === idNum && byIdx.name) return String(byIdx.name).trim();
  const found = mons.find((m) => m.index === idNum);
  return found?.name ? String(found.name).trim() : null;
}

function monsterDef(manifest, id) {
  const idNum = Number(id);
  if (!Number.isFinite(idNum) || idNum <= 0 || idNum >= 256) return null;
  const mons = manifest?.monsters;
  if (!Array.isArray(mons)) return null;
  const byIdx = mons[idNum];
  if (byIdx && byIdx.index === idNum) return byIdx;
  return mons.find((m) => m.index === idNum) ?? null;
}

function parseEncounterIdsFromPseudo(pseudo, isOp13) {
  if (!pseudo) return [];
  const re = isOp13
    ? /encounter_setup_b\(data=([0-9A-Fa-f\s]+)/
    : /encounter_setup\(monsters=([0-9A-Fa-f\s]+)/;
  const m = pseudo.match(re);
  if (!m) return [];
  const ids = [];
  for (const tok of m[1].trim().split(/\s+/)) {
    const v = parseInt(tok, 16);
    if (Number.isFinite(v) && v > 0 && v < 256) ids.push(v);
  }
  return ids;
}

function encounterSlotIds(op, args, pseudo) {
  const ids = [];
  for (let j = 0; j < 10; j++) {
    const mId = Number(args?.[j] ?? 0);
    if (Number.isFinite(mId) && mId > 0 && mId < 256) ids.push(mId);
  }
  if (!ids.length) return parseEncounterIdsFromPseudo(pseudo, op === 0x13);
  return ids;
}

function formatEncounterNameList(mNames) {
  const nameCounts = {};
  for (const name of mNames) nameCounts[name] = (nameCounts[name] || 0) + 1;
  return Object.entries(nameCounts)
    .map(([name, count]) => (count > 1 ? `${count} ${name}` : name))
    .join("\n");
}

export function describeEncounter(manifest, op, args, pseudo = "") {
  const isRandom = op === 0x13;
  /* Heading is for map-sidebar / notes only — never combat chrome (CombatSession has none). */
  const heading = isRandom ? "Random encounter" : "Fixed encounter";
  const ids = encounterSlotIds(op, args, pseudo);
  const mNames = [];
  let sprite = null;
  for (const mId of ids) {
    const name = monsterName(manifest, mId);
    if (name) {
      mNames.push(name);
      if (!sprite) {
        const mDef = monsterDef(manifest, mId);
        if (mDef?.sprite) {
          sprite = { sheet: `${String(mDef.sprite).padStart(2, "0")}_anm`, frame: "0" };
        }
      }
    }
  }
  /* Combat uses monster names alone (right column); keep a plain list for callers. */
  let text;
  if (mNames.length) text = formatEncounterNameList(mNames);
  else if (isRandom && ids.length) {
    text = ids.map((id) => id.toString(16).toUpperCase().padStart(2, "0")).join(" ");
  } else if (ids.length) {
    text = ids.map((id) => id.toString(16).toUpperCase().padStart(2, "0")).join(" ");
  } else text = "";
  return { text, sprite, heading, names: mNames };
}

export function formatEncounterFlowLine(manifest, node) {
  const pseudo = node.pseudo || (node.op === 0x13 ? "encounter_setup_b(...)" : "encounter_setup(...)");
  const { text, names } = describeEncounter(manifest, node.op, node.args, pseudo);
  if (names.length) return `${pseudo} — ${formatEncounterNameList(names).replace(/\n/g, ", ")}`;
  if (text) return `${pseudo} — ${text}`;
  return pseudo;
}

export function decodeTreasure2A(args) {
  if (!args || args.length < 14) return null;
  const gold = args[0] | (args[1] << 8) | (args[2] << 16);
  const gems = args[3] | (args[4] << 8);
  const items = [];
  for (let i = 0; i < 3; i++) {
    const base = 5 + i * 3;
    const id = args[base];
    if (id) items.push({ id, charges: args[base + 1], flags: args[base + 2] });
  }
  return { gold, gems, items };
}

function decodeU16Gold(args) {
  if (args.length < 2) return 0;
  return args[0] | (args[1] << 8);
}

function decodeU16Code25(args) {
  if (args.length < 2) return 0;
  return (args[0] << 8) | args[1];
}

/** Decode OP_30 expected bytes for display (trim trailing spaces). */
function decodeOp30Answer(args) {
  let s = "";
  for (const b of args) {
    const c = (0x11a - b) & 0xff;
    s += c >= 0x20 && c <= 0x7e ? String.fromCharCode(c) : "?";
  }
  return s.trimEnd();
}

/**
 * OP_30 @ 0x17034: compare 10 space-padded input bytes vs (0x11A - expected[i]).
 * input must already be uppercase (toupper via -$7B78 in ASM).
 */
function checkOp30Password(inputBuf, expectedArgs) {
  if (!inputBuf || expectedArgs.length < 10) return false;
  for (let i = 0; i < 10; i++) {
    const got = (inputBuf.charCodeAt(i) || 0x20) & 0xff;
    const want = (0x11a - (expectedArgs[i] & 0xff)) & 0xff;
    if (got !== want) return false;
  }
  return true;
}

/**
 * OP_0C @ 0x15E12 bit6 / high remaps before map load.
 * Without a seeded rng, roll=1 (same fallback as C++ when rng unbound).
 */
function remapOp0cDest(destScreen, destTile, rng = null) {
  let dest = destScreen & 0xff;
  let tile = destTile & 0xff;
  const roll = (lo, hi) => {
    if (typeof rng === "function") return rng(lo, hi) | 0;
    return lo;
  };
  if ((dest & 0x40) !== 0) {
    let d = (roll(1, 20) + 5) & 0xff;
    if (d >= 0x11) d = (d + 0x10) & 0xff;
    d |= 0x80;
    dest = d;
  }
  if (dest >= 0x80) {
    tile = roll(1, 255) & 0xff;
  }
  dest &= 0x3f;
  return { dest, tile };
}

function townIntroSlot(screenId) {
  return screenId >= 0 && screenId < 5 ? screenId : 0;
}

function signSpriteFromPseudo(pseudo) {
  const m = pseudo?.match(/\[(\d+)\.anm\]/);
  return m ? { sheet: `${m[1]}_anm`, frame: "0" } : null;
}

function formatTreasureMessage(treasure, manifest, resolveEventText, session) {
  sessionApplyTreasure(session, treasure);
  const parts = ["Treasure! (Search to collect)"];
  if (treasure.gold > 0) parts.push(`Gold: ${treasure.gold} gp`);
  if (treasure.gems > 0) parts.push(`Gems: ${treasure.gems}`);
  for (const it of treasure.items) {
    parts.push(itemName(manifest, it.id) ?? `item #${it.id}`);
  }
  return resolveEventText(parts.join("\n"));
}

function formatGiveItemMessage(args, manifest, resolveEventText, session) {
  let id = args[1] ?? 0;
  if ((args[0] ?? 0) >= 0x80) id = session.cond & 0xff;
  const placed = sessionGiveItem(session, id, args[2] ?? 0, args[3] ?? 0);
  session.cond = placed ? 1 : 0;
  const name = itemName(manifest, id);
  if (placed) return resolveEventText(`Received: ${name}`);
  return resolveEventText(`Received: ${name}\n(stored in found-item buffer — backpacks full)`);
}

function isTownServiceSelector(sel) {
  return (
    (sel >= 0x09 && sel <= 0x10) || (sel >= 0x11 && sel <= 0x37) ||
    (sel >= 0x38 && sel <= 0x4b) || (sel >= 0x4c && sel <= 0x54) ||
    (sel >= 0x56 && sel <= 0x5b) || (sel >= 0x5c && sel <= 0x5e) ||
    (sel >= 0x65 && sel <= 0x69) || (sel >= 0x6a && sel <= 0x7c) ||
    (sel >= 0x97 && sel <= 0x98) || (sel >= 0xe3 && sel <= 0xf3) ||
    (sel >= 0xf4 && sel <= 0xfb)
  );
}

function skipTokenNodes(nodes, fromIndex, count) {
  return fromIndex + count;
}

function op2dCheckMemberAttr(session, arg1, arg2) {
  const useRace = (arg1 & 0x80) !== 0;
  const useSex = (arg1 & 0x40) !== 0;
  const useClass = !useRace && !useSex;
  const anyMode = (arg1 & 0x20) !== 0;
  const val1 = arg1 & 0x0f;
  const val2 = (arg1 & 0xe0) === 0 ? arg2 & 0x0f : val1;
  const party = session.party ?? [];
  let match = false;
  for (const rec of party) {
    match = false;
    if (useClass) match = rec.classId === val1;
    if (useSex) match = rec.sex === val1;
    if (useRace) match = rec.race === val1;
    if (!match) {
      if (useClass) match = rec.classId === val2;
      if (useSex) match = rec.sex === val2;
      if (useRace) match = rec.race === val2;
    }
    if (anyMode ? match : !match) break;
  }
  return match ? 1 : 0;
}

async function runTownService(ctx, sel, title, sprite) {
  ctx.title = title;
  ctx.sprite = sprite;
  const { waitForSpace, screenId, promptYesNo, promptMenuKey, session, note, maps } = ctx;
  const slot = townIntroSlot(screenId);
  const rng = (lo, hi) => {
    if (typeof ctx.rng === "function") return ctx.rng(lo, hi) | 0;
    return lo; /* C++ unbound-rng fallback */
  };
  const promptHexDigit = async (prompt) => {
    const keys = "0123456789abcdef";
    while (true) {
      const pick = await (ctx.promptMenuKey || promptMenuKey)(
        prompt,
        keys,
        sprite,
        0x0e
      );
      if (pick == null) return null;
      const ch = String(pick).toLowerCase();
      const digit = keys.indexOf(ch);
      if (digit >= 0 && digit <= 0xf) return digit;
    }
  };

  if (sel === 0x01) {
    /* Inn @ 0x1A132: Y/N → registry + Goto Town epilogue (not Rest submenu). */
    const yes = await promptYesNo(INN_INTRO[slot], sprite, 0x0e);
    if (!yes) {
      ctx.note("Inn registry declined");
      return;
    }
    await runInnRegistry(ctx);
    session.pendingEventLatch = 1;
    ctx.onSessionChange?.(session);
    return;
  }
  if (sel === 0x02) {
    /* Bound Training menu @ EventTownServices — no showServiceTitle when UI runs. */
    await runTrainingService(ctx);
    return;
  }
  if (sel === 0x04) {
    await runTempleService(ctx);
    return;
  }
  if (sel === 0x06) {
    await runSmithService(ctx);
    return;
  }
  if (sel === 0x07) {
    /* General store -$7DB8 → 0xA62C: 100gp + convert +$50 skill nibbles. */
    const yes = await promptYesNo(
      "A greedy gnome offers to convert your\nsecondary skills for 100 gold.\nBuy (y/n)?",
      sprite,
      0x0e
    );
    if (!yes) {
      ctx.note("General store declined");
      return;
    }
    const memSlot = await promptSelectMember(ctx);
    if (memSlot == null) return;
    const member = getPartyMember(session, memSlot);
    const r = svcGeneralStoreConvert(member);
    syncSessionGoldFromParty(session);
    await waitForSpace(r.message, sprite, 0x0e);
    note(`General store: ${r.converted ? "converted" : r.paid ? "paid" : "no-op"}`);
    ctx.onSessionChange?.(session);
    return;
  }
  if (sel === 0x64) {
    /* Circus 0xDF04 — attr pick; win if +$7D bit1 else Cupie Doll roll. */
    const yes = await promptYesNo(
      "Cheerfully striped tents. Game booths\nline the circus grounds. Play (y/n)?",
      sprite,
      0x0e
    );
    if (!yes) {
      ctx.note("Circus declined");
      return;
    }
    /* ASM 0xDD18 menu 1..6 → might/per/acc/end/speed/luck (FAQ lists 7 incl. Shell Game). */
    const pick = await promptMenuKey(
      "1) Might  2) Personality  3) Accuracy\n4) Endurance  5) Speed  6) Luck\n1–6 — Esc cancel",
      "123456",
      sprite,
      0x0e
    );
    if (!pick) return;
    const attr = parseInt(pick, 10) - 1;
    const keys = ["might", "personality", "accuracy", "endurance", "speed", "luck"];
    let won = false;
    for (const m of ensureParty(session)) {
      const bit = (m.classQuestBits ?? m.rawBytes?.[0x7d] ?? 0) & 0x02;
      if (bit) {
        won = true;
        if (m.rawBytes) m.rawBytes[0x7d] &= ~0x02;
        m.classQuestBits = (m.classQuestBits ?? 0) & ~0x02;
        const k = keys[attr] ?? "intelligence";
        let v = (m[k] | 0);
        v = v > 0x5a ? 0x64 : Math.min(100, v + 10);
        m[k] = v;
      }
    }
    if (won) {
      await waitForSpace("You win a prize!", sprite, 0x0e);
    } else {
      const roll =
        typeof ctx.rng === "function" ? ctx.rng(1, 0xfe) : 1 + Math.floor(Math.random() * 0xfe);
      if (roll <= 0x7f) {
        for (const m of ensureParty(session)) {
          const bp = m.backpack ?? [];
          const empty = bp.findIndex((s) => !s?.id);
          if (empty >= 0) {
            bp[empty] = { id: 0xda, charges: 0, flags: 0 };
            m.backpack = bp;
            await waitForSpace("You receive a Cupie Doll!", sprite, 0x0e);
            note("Circus: Cupie Doll 0xDA");
            ctx.onSessionChange?.(session);
            return;
          }
        }
      }
      await waitForSpace("Sorry, you lose.", sprite, 0x0e);
    }
    note("Circus game resolved");
    ctx.onSessionChange?.(session);
    return;
  }
  if (sel === 0x03) {
    const introSlot = townIntroSlot(ctx.screenId);
    const yes = await ctx.promptYesNo(TAVERN_INTRO[introSlot], sprite, 0x0e);
    if (!yes) {
      ctx.note(`Tavern: declined`);
      return;
    }
    await runTavernService(ctx);
    return;
  }
  if (sel === 0x05) {
    /* Bound Mage Guild menu — C++ runBoundMenu skips hall intro when UI is bound. */
    await runGuildService(ctx);
    return;
  }
  if (sel === 0x08) {
    /* Arena Games ticket engine (0x08 -> thunk -$7DBE -> 0x9D76). */
    await runArenaTicketSelector(ctx, sel);
    return;
  }
  if (sel === 0x7e) {
    /* Free teleport UI -$7DB2 → 0xD576: hex X then Y. */
    const x = await promptHexDigit("What is the magical location:\n       X ( 0-15 ) ?");
    if (x == null) return;
    const y = await promptHexDigit("What is the magical location:\n       Y ( 0-15 ) ?");
    if (y == null) return;
    ctx.onTeleport?.(screenId, x, y);
    session.pendingEventLatch = 1;
    ctx.teleported = true;
    note(`OP_0E 0x7E free teleport → (${x},${y})`);
    ctx.onSessionChange?.(session);
    return;
  }
  if (sel === 0x7f) {
    /* Combat seed -$7DAC → 0xD634: type = (rng(1,16)-1)+(Y<<4); OP_12 seed. */
    const roll = rng(1, 16);
    const mon = ((roll - 1) + ((ctx.tileY & 0xf) << 4)) & 0xff;
    const party = ensureParty(session);
    const block = new Array(12).fill(0);
    for (let i = 0; i < party.length && i < 10; i++) block[i] = mon;
    session.monsterCount = 0; /* asm clr -$77BE before enter @ EventTownServices */
    sessionSeedFixedEncounter(session, block, false);
    const enc = describeEncounter(ctx.manifest, 0x12, block, "");
    const outcome = await runCombatContract(ctx, enc, 0x0e);
    if (outcome === "victory") {
      note(`OP_0E 0x7F combat seed mon=0x${mon.toString(16)} — victory`);
    } else {
      note(`OP_0E 0x7F combat seed mon=0x${mon.toString(16)} — ${outcome}`);
    }
    ctx.onSessionChange?.(session);
    return;
  }
  if (sel === 0x80) {
    /* Intra-map portal -$7DA6 → 0xD6A4 + slide-trap halve. */
    const cx = ctx.tileX & 0xff;
    const cy = ctx.tileY & 0xff;
    let hit = -1;
    for (let i = 0; i < 10; i++) {
      if (PORTAL_SRC_X[i] === cx && PORTAL_SRC_Y[i] === cy) {
        hit = i;
        break;
      }
    }
    if (hit < 0) {
      note("OP_0E 0x80 portal — no XY match");
      return;
    }
    sessionClearTileFlag(session, screenId, cx, cy, maps);
    const dx = PORTAL_DST_X[hit];
    const dy = PORTAL_DST_Y[hit];
    ctx.onTeleport?.(screenId, dx, dy);
    session.pendingEventLatch = 1;
    session.exitFlags = (session.exitFlags | 5) & 0xff;
    await waitForSpace("Magical slide trap!", sprite, 0x0e);
    sessionSlideTrapHalve(session);
    ctx.teleported = true;
    note(`OP_0E 0x80 portal (${cx},${cy}) → (${dx},${dy}) + slide trap`);
    ctx.onSessionChange?.(session);
    return;
  }
  if (sel === 0x81 || sel === 0x82 || sel === 0x83) {
    /* Found-item -$7DA0 → 0xD89C(arg 0/1/2). */
    const arg = sel - 0x81;
    const span = FOUND_ITEM_SPAN[arg];
    const base = FOUND_ITEM_BASE[arg];
    const roll = rng(1, span);
    const itemId = (roll + base - 1) & 0xff;
    const placed = sessionGiveItem(session, itemId, 0, 0);
    if (placed) {
      sessionClearTileFlag(session, screenId, ctx.tileX, ctx.tileY, maps);
      session.exitFlags = (session.exitFlags | 2) & 0xff;
      const name = itemName(ctx.manifest, itemId) ?? `item #${itemId}`;
      await waitForSpace(`You have found a ${name}`, sprite, 0x0e);
      note(`OP_0E 0x${sel.toString(16)} found ${name}`);
    } else {
      note(`OP_0E 0x${sel.toString(16)} give failed (id=${itemId})`);
    }
    ctx.onSessionChange?.(session);
    return;
  }
  if (sel === 0xc9 || sel === 0xca) {
    /* 0x19AB4/0x19AC4 → 0x1980A: 0x193AC reward+apply, busy gate, else A–D. */
    const drink = sel === 0xca;
    const party = ensureParty(session);
    const done = svcQuestCompleteReward(party, drink, ctx.manifest);
    syncSessionGoldFromParty(session);
    if (done.activity > 0) {
      const msg =
        done.membersRewarded > 0
          ? `You have done everyone a great service\nand you shall be rewarded.\n  ${done.xpEach} experience points!`
          : "Quest progress applied.";
      await waitForSpace(msg, sprite, 0x0e);
      note(drink ? "OP_0E 0xCA quest complete/apply" : "OP_0E 0xC9 quest complete/apply");
      ctx.onSessionChange?.(session);
      return;
    }
    if (svcQuestBusy(party, drink)) {
      await waitForSpace(
        drink
          ? "Your party has already been quested\nto seek out the foe."
          : "Your party has already been quested\nto seek out the item.",
        sprite,
        0x0e
      );
      note(drink ? "OP_0E 0xCA quest busy" : "OP_0E 0xC9 quest busy");
      return;
    }
    const intro = drink
      ? "Heads of monstrous beasts adorn the\nwalls in this room.  Will you gather\nmore trophies for Lord Slayer (y/n)?"
      : "The huge chamber is overstocked with\nmany unusual items.  Lord Hoardall\nbegs your party for a favor.  Will\nyou gather more items for him (y/n)?";
    const yes = await ctx.promptYesNo(intro, sprite, 0x0e);
    if (!yes) {
      note(drink ? "OP_0E 0xCA declined" : "OP_0E 0xC9 declined");
      return;
    }
    const lord = drink ? "Slayer" : "Hoardall";
    const menu =
      `At what level of difficulty do you\nwish to aid Lord ${lord}?\n` +
      `A) Page's Quest\nB) Squire's Quest\nC) Knight's Quest\nD) Lord's Quest\n` +
      `${lord} (A-D)?`;
    while (true) {
      const pick = await (ctx.promptMenuKey || promptMenuKey)(menu, "abcd", sprite, 0x0e);
      const ch = (pick || "").toUpperCase();
      if (!ch) {
        await waitForSpace("Then begone, knave!", sprite, 0x0e);
        note(drink ? "OP_0E 0xCA Esc" : "OP_0E 0xC9 Esc");
        return;
      }
      if (ch >= "A" && ch <= "C") {
        if (drink) svcDrinkEncodePurchase(party, ch.charCodeAt(0) - 65);
        else svcFoodEncodePurchase(party, ch.charCodeAt(0) - 65);
        await waitForSpace(
          "The quest I have decided upon for your\nparty, is to seek the target.",
          sprite,
          0x0e
        );
        note(drink ? "OP_0E 0xCA drink encode" : "OP_0E 0xC9 food encode");
        break;
      }
      if (ch === "D") {
        const armed = svcQuestLordArm(party, drink);
        if (armed < 0) {
          /* ASM returns -1 → re-prompt A–D. */
          note("OP_0E lord arm (none) — re-prompt");
          continue;
        }
        await waitForSpace("Lord's Quest accepted.", sprite, 0x0e);
        note(`OP_0E lord arm (${armed})`);
        break;
      }
    }
    ctx.onSessionChange?.(session);
    return;
  }
  if (sel === 0xfd) {
    /* 0x161B2 + GameSession FdPrintChrome hosted pages / fight / WAFE. */
    await runOp0eFdPrintChrome(ctx);
    return;
  }
  if (isTownServiceSelector(sel)) {
    /* Skill vendors + quests: queue overlay (C++ runDefaultRangeOverlay);
     * drained by runQueuedDispatch after OP_0E abort — do not run inline. */
    if (queueDefaultRangeOverlay(ctx, sel)) {
      return;
    }
    if (sel >= 0x26 && sel <= 0x29) {
      await waitForSpace(
        "Combat could not start.\n(monsters.dat missing or overlay failed)",
        sprite,
        0x0e
      );
      return;
    }
    /* C++ default fallback: showServiceTitle(townName) — never "Town service". */
    await waitForSpace(townName(screenId), sprite, 0x0e);
    ctx.note(`exec_selector(0x${sel.toString(16)}) — default-range overlay missing`);
    return;
  }
}

/**
 * @param {object} ctx
 * @returns {Promise<{teleported:boolean, ended:boolean, aborted:boolean, notes:string[], trace:object[]}>}
 */
export async function runEventScript(ctx) {
  const {
    evtData,
    strings,
    screenId,
    tileX,
    tileY,
    manifest,
    maps,
    session: sessionIn,
    resolveEventText,
    waitForSpace,
    promptYesNo,
    promptAnswer,
    promptMenuKey,
    promptCombatResult,
    onViewportOverlay,
    onTeleport,
    onPatchTile,
    onSessionChange,
    onVmStep,
  } = ctx;

  const session = sessionIn ?? createSessionState();
  ctx.session = session;
  /* scanAndRun clears EXIT_FLAGS; queued overlay resume must not. */
  if (!ctx._isQueuedOverlay) {
    session.exitFlags = 0;
    session.scriptAbort = 0;
  }
  if (session.queuedEventId == null) session.queuedEventId = 0xff;

  const notes = [];
  /** @type {object[]} */
  const trace = [];
  let lastSignSprite = null;
  let teleported = false;
  let ended = false;
  let aborted = false;
  let pendingPromptText = null;
  let pendingPromptOp = null;

  const nodes = evtData?.nodes ?? [];
  let i = 0;

  const note = (msg) => notes.push(msg);

  const vmStep = (nodeIndex, op, detail = "") => {
    trace.push({
      nodeIndex,
      op,
      cond: session.cond,
      detail,
    });
    onVmStep?.({ nodeIndex, op, cond: session.cond, detail, trace });
    onSessionChange?.(session);
  };

  ctx.note = note;
  ctx.waitForSpace = waitForSpace;
  ctx.promptYesNo = promptYesNo;
  ctx.promptAnswer = promptAnswer;
  ctx.promptMenuKey = promptMenuKey;
  ctx.screenId = screenId;
  ctx.manifest = manifest;
  ctx.onSessionChange = onSessionChange;
  ctx.teleported = false;
  ctx.ended = false;

  const strAt = (idx) => (idx < strings.length ? resolveEventText(strings[idx]) : "");

  const nextOp = (from) => {
    for (let j = from; j < nodes.length; j++) {
      if (nodes[j].op === -1) continue;
      return nodes[j].op;
    }
    return null;
  };

  const syncCond = () => {
    ctx.cond = session.cond;
  };

  while (i < nodes.length) {
    const nodeIndex = i;
    const node = nodes[i];
    i++;

    if (node.op === -1) {
      vmStep(nodeIndex, -1, "text_record");
      await waitForSpace(node.text);
      pendingPromptText = null;
      continue;
    }

    const op = node.op;
    const args = node.args || [];

    if (op >= 0x01 && op <= 0x06) {
      const strIdx = args[0];
      vmStep(nodeIndex, op, `str[${strIdx}]`);
      if (strIdx < strings.length) {
        let text = strAt(strIdx);
        /* OP_04/05/06 @ 0x15A00/0x15A52/0x15B24: skip draw when -$79E1 != 0. */
        const cantSee = (session.cantSeeFlag | 0) !== 0;
        if (op === 0x06) {
          /* 0x15AFE: rewrite '-' → '{' before glyph draw. */
          text = text.replace(/-/g, "{");
        }
        if (op === 0x04) {
          if (!cantSee) {
            onViewportOverlay?.({ type: "door", text });
          }
        } else if (op === 0x05) {
          if (!cantSee) onViewportOverlay?.({ type: "wall", text });
        } else if (op === 0x06) {
          if (!cantSee) onViewportOverlay?.({ type: "signpost", text });
        } else if (op >= 0x01 && op <= 0x03) {
          /* EXIT_FLAGS: OP_01 bit0, OP_02 bit1, OP_03 bits 0+1. */
          if (op === 0x01) session.exitFlags = (session.exitFlags | 1) & 0xff;
          else if (op === 0x02) session.exitFlags = (session.exitFlags | 2) & 0xff;
          else session.exitFlags = (session.exitFlags | 3) & 0xff;
          const followedByYn = nextOp(i) === 0x09 || nextOp(i) === 0x0a;
          if (followedByYn) {
            pendingPromptText = text;
            pendingPromptOp = op;
            ctx.onDraw?.();
          } else {
            await waitForSpace(text, null, op);
            pendingPromptText = null;
          }
        } else {
          await waitForSpace(text, null, op);
          pendingPromptText = null;
        }
      }
      continue;
    }

    switch (op) {
      case 0x07:
        vmStep(nodeIndex, op, "wait space");
        await waitForSpace(pendingPromptText ?? "", null, pendingPromptOp ?? 0x07);
        pendingPromptText = null;
        break;

      case 0x08:
        /* OP_08 @ 0x15D26: -$71DC←$FD then SPACE via scripted buffer. */
        session.scriptedKeyMode = 0xfd;
        vmStep(nodeIndex, op, "wait space (scripted $FD)");
        await waitSpaceWithScriptedKey(ctx, pendingPromptText ?? "", null, pendingPromptOp ?? 0x08);
        pendingPromptText = null;
        break;

      case 0x09:
      case 0x0a: {
        /* OP_0A: -$71DC←$FD then same Y/N as OP_09 (scripted poll in walker keys). */
        if (op === 0x0a) session.scriptedKeyMode = 0xfd;
        vmStep(nodeIndex, op, op === 0x0a ? "y/n (scripted $FD)" : "y/n prompt");
        const promptText = pendingPromptText ?? "";
        pendingPromptText = null;
        session.cond = (await promptYesNo(promptText, null, pendingPromptOp ?? op)) ? 1 : 0;
        syncCond();
        note(`Y/N → cond=${session.cond}`);
        break;
      }

      case 0x0b: {
        /* OP_0B @ 0x15DB0: ServiceSignResolver → .anm only; EXIT_FLAGS bit2.
         * No text capture, no SPACE wait — C++ showOp0B continues into OP_0E. */
        vmStep(nodeIndex, op, node.pseudo || "service_sign");
        session.exitFlags = (session.exitFlags | 4) & 0xff;
        session.signEnvId = signEnvIdForScreen(screenId);
        const strIdx = args[0] ?? 0;
        const placement = args[1] ?? 0;
        let sprite = resolveOp0bSprite(session, screenId, strIdx);
        if (!sprite) sprite = signSpriteFromPseudo(node.pseudo);
        lastSignSprite = sprite;
        if (sprite) {
          sprite.placement = placement;
          ctx.onViewportOverlay?.(sprite);
        }
        break;
      }

      case 0x0c: {
        /* event_op0c_map_transition @ 0x15E12 — bit6 / >=$80 remaps before load. */
        const remapped = remapOp0cDest(args[0] ?? 0, args[1] ?? 0, ctx.rng);
        const dest = remapped.dest;
        const tile = remapped.tile;
        vmStep(nodeIndex, op, `→ screen ${dest}`);
        const ty = (tile >> 4) & 0xf;
        const tx = tile & 0xf;
        const destName = maps?.screens?.[dest]?.name ?? `screen ${dest}`;
        onTeleport?.(dest, tx, ty);
        session.pendingEventLatch = 1;
        /* endScript after transition: clear EXIT_FLAGS / SCRIPT_ABORT. */
        session.exitFlags = 0;
        session.scriptAbort = 0;
        teleported = true;
        ended = true;
        note(`Map transition → ${destName} (${tx},${ty})` +
          (((args[0] ?? 0) & 0x40) ? " [bit6 remap]" : ""));
        return { teleported, ended, aborted, notes, trace };
      }

      case 0x0d: {
        const idx = args[0] ?? 0;
        const name = OP0D_SEQUENCE_NAMES[idx] ?? `sequence #${idx}`;
        vmStep(nodeIndex, op, name);
        note(`OP_0D play_sequence(#${idx}) — ${name} (presentation only, no GS writes)`);
        break;
      }

      case 0x0e: {
        /* event_op0e_selector_dispatch @ 0x160C2: SCRIPT_ABORT=1 at entry so
         * the current script ends after the selector returns. */
        const sel = (args[0] ?? 0) & 0xff;
        vmStep(nodeIndex, op, `selector 0x${sel.toString(16)}`);
        const slot = townIntroSlot(screenId);
        const fallbackSheet = SELECTOR_SPRITES[sel];
        const sprite = lastSignSprite ?? (fallbackSheet ? { sheet: fallbackSheet, frame: "0" } : null);
        lastSignSprite = null;
        /* C++ service_title_ is never written (OP_0B is sign-only). Fallback =
         * townName; menu chrome uses MENU_KIND_TITLE — never "Town service". */
        const title = MENU_KIND_TITLE[sel] || townName(screenId);
        aborted = true; /* ASM sets -$79EA at OP_0E entry */

        ctx.tileX = tileX;
        ctx.tileY = tileY;
        ctx.onTeleport = onTeleport;

        if (sel === 0x04) {
          const yes = await promptYesNo(TEMPLE_INTRO[slot], sprite, op);
          if (!yes) {
            note(`Temple: declined`);
            ended = true;
            return { teleported, ended, aborted, notes, trace };
          }
        }

        if (sel === 0x06) {
          const yes = await promptYesNo(SMITH_INTRO[slot], sprite, op);
          if (!yes) {
            note(`Blacksmith: declined`);
            ended = true;
            return { teleported, ended, aborted, notes, trace };
          }
        }

        /* 0x09–0x10 / 0x11+ default-range → runDefaultRangeOverlay via
         * isTownServiceSelector (enroll/locksmith/portal/quests). No FAQ stubs. */
        const serviceHandled =
          sel === 0x01 ||
          sel === 0x02 ||
          sel === 0x03 ||
          sel === 0x04 ||
          sel === 0x05 ||
          sel === 0x06 ||
          sel === 0x64 ||
          sel === 0x07 ||
          sel === 0x08 ||
          sel === 0x7e ||
          sel === 0x7f ||
          sel === 0x80 ||
          sel === 0x81 ||
          sel === 0x82 ||
          sel === 0x83 ||
          sel === 0xc9 ||
          sel === 0xca ||
          sel === 0xfd ||
          isTownServiceSelector(sel);

        if (serviceHandled) {
          await runTownService(ctx, sel, title, sprite);
          /* abortScript @ 0x17540: clear SCRIPT_ABORT; keep EXIT_FLAGS (no $171AC). */
          session.scriptAbort = 0;
          /* Scanner epilogue @ 0x176B6: drain OP_0E default-range queue. */
          if (await runQueuedDispatch(ctx)) {
            if (ctx.teleported) teleported = true;
          }
          if (ctx.teleported) teleported = true;
          ended = true;
          onSessionChange?.(session);
          return { teleported, ended, aborted, notes, trace };
        }

        await waitForSpace(`(selector 0x${sel.toString(16)})`, sprite, op);
        note(`exec_selector(0x${sel.toString(16)}) — unhandled`);
        session.scriptAbort = 0;
        ended = true;
        return { teleported, ended, aborted, notes, trace };
      }

      case 0x0f:
        /* endScript: EXIT_FLAGS=0, SCRIPT_ABORT=0, then overlay restore. */
        vmStep(nodeIndex, op, "end_script");
        session.exitFlags = 0;
        session.scriptAbort = 0;
        if (await runQueuedDispatch(ctx)) {
          if (ctx.teleported) teleported = true;
        }
        ended = true;
        return { teleported, ended, aborted, notes, trace };

      case 0x10: {
        const n = args[0] ?? 0;
        const take = session.cond !== 0;
        vmStep(nodeIndex, op, take ? `skip ${n} (cond=1)` : `no skip (cond=0)`);
        if (take) i = skipTokenNodes(nodes, i, n);
        note(`OP_10 if cond: skip ${n} → node ${i}`);
        break;
      }

      case 0x11: {
        const n = args[0] ?? 0;
        const take = session.cond === 0;
        vmStep(nodeIndex, op, take ? `skip ${n} (!cond)` : `no skip (cond=1)`);
        if (take) i = skipTokenNodes(nodes, i, n);
        note(`OP_11 if !cond: skip ${n} → node ${i}`);
        break;
      }

      case 0x12:
      case 0x13: {
        vmStep(nodeIndex, op, node.pseudo || "encounter");
        if (op === 0x12) {
          await runOp12Combat(ctx, args);
          if (ctx.ended) {
            ended = true;
            aborted = true;
            session.scriptAbort = 0;
            return { teleported, ended, aborted, notes, trace };
          }
          break;
        }
        /* OP_13: eventRunFixedEncounter variant_b=true — mode 0, clear tail. */
        sessionSeedFixedEncounter(session, args, true);
        const enc = describeEncounter(manifest, op, args, node.pseudo);
        const sprite = lastSignSprite ?? enc.sprite;
        lastSignSprite = null;
        ctx.sprite = sprite;
        const outcome = await runCombatContract(ctx, enc, op);
        if (outcome === "victory") {
          note(`${enc.heading}: victory latch (seeded-random)`);
        } else if (outcome === "refused") {
          note(`${enc.heading}: enter refused (live=0)`);
        } else {
          note(`${enc.heading}: ${outcome} — script ends`);
          ended = true;
          aborted = true;
          return { teleported, ended, aborted, notes, trace };
        }
        onSessionChange?.(session);
        break;
      }

      case 0x14: {
        vmStep(nodeIndex, op, `clear flag @ (${tileX},${tileY})`);
        sessionClearTileFlag(session, screenId, tileX, tileY, maps);
        note(`OP_14 cleared event flag on tile (${tileX},${tileY})`);
        onSessionChange?.(session);
        break;
      }

      case 0x15: {
        const count = args[0] ?? 0;
        const selector = args[1] ?? 0;
        const val = args[2] ?? 0;
        sessionApplyPartyByteOp(session, count, selector, val, { testOnly: true });
        syncCond();
        vmStep(nodeIndex, op, `test field 0x${selector.toString(16)} & ${val} → cond=${session.cond}`);
        note(`OP_15 party test → cond=${session.cond}`);
        break;
      }

      case 0x18: {
        const count = args[0] ?? 0;
        const selector = args[1] ?? 0;
        const andM = args[2] ?? 0xff;
        const orM = args[3] ?? 0;
        sessionApplyPartyByteOp(session, count, selector, 0, { masked: true, andM, orM });
        syncCond();
        vmStep(nodeIndex, op, `masked set 0x${selector.toString(16)} (${andM.toString(16)},${orM.toString(16)})`);
        note(`OP_18 masked party field 0x${selector.toString(16)}`);
        break;
      }

      case 0x1f:
      case 0x20: {
        sessionApplyPartyEffect(session, args, op === 0x20);
        syncCond();
        vmStep(nodeIndex, op, `${node.pseudo || "party_effect"} → cond=${session.cond}`);
        note(`${node.pseudo || `OP_${op.toString(16)}`} → cond=${session.cond}`);
        break;
      }

      case 0x2e: {
        /* OP_2E @ 0x16F50 — class-gated OR into member+(arg1-0x6E)+0x51. */
        let arg1 = args[0] ?? 0;
        const arg2 = args[1] ?? 0;
        let clsA = 4;
        let clsB = 2;
        if (arg1 >= 0x80) {
          clsA = 3;
          clsB = 1;
          arg1 &= 0x7f;
        }
        const fieldOff = ((arg1 - 0x6e) & 0xff) + 0x51;
        for (const m of session.party ?? []) {
          if (m.classId === clsA || m.classId === clsB) {
            const raw = m.rawBytes ?? null;
            if (raw && fieldOff >= 0 && fieldOff < raw.length) {
              raw[fieldOff] |= arg2 & 0xff;
            } else if (fieldOff === 0x51) {
              m.classQuestBits = (m.classQuestBits ?? 0) | (arg2 & 0xff);
            }
          }
        }
        vmStep(nodeIndex, op, node.pseudo || "set_attr_bit");
        note(`${node.pseudo || "OP_2E"} class-gated OR @ +0x${fieldOff.toString(16)}`);
        break;
      }

      case 0x16: {
        const want = args[1] ?? 0;
        const cond = sessionCountPartyItem(session, want);
        session.cond = cond;
        syncCond();
        vmStep(nodeIndex, op, `item #${want} → cond=${cond}`);
        note(`OP_16 item #${want} (${itemName(manifest, want)}) → cond=${cond}`);
        break;
      }

      case 0x17: {
        const group = args[0] ?? 0;
        session.cond = sessionLoadVar(session, group);
        syncCond();
        vmStep(nodeIndex, op, `load_var(${group})=${session.cond}`);
        note(`OP_17 load_var group=0x${group.toString(16)} → cond=${session.cond}`);
        break;
      }

      case 0x19: {
        vmStep(nodeIndex, op, `give item #${args[1] ?? 0}`);
        await waitForSpace(formatGiveItemMessage(args, manifest, resolveEventText, session), null, op);
        syncCond();
        note(`OP_19 give → cond=${session.cond}`);
        onSessionChange?.(session);
        break;
      }

      case 0x1a: {
        const group = args[0] ?? 0;
        const val = args[1] ?? 0;
        sessionStoreVar(session, group, val);
        vmStep(nodeIndex, op, `store_var(${group})=${val}`);
        note(`OP_1A store_var group=0x${group.toString(16)} ← ${val}`);
        onSessionChange?.(session);
        break;
      }

      case 0x1b: {
        const threshold = args[0] ?? 0;
        if (session.cond < threshold) session.cond = 0;
        syncCond();
        vmStep(nodeIndex, op, `cond >= ${threshold} → ${session.cond}`);
        break;
      }

      case 0x1c: {
        /* 0x16742: -$7BB4(1,u8) → raw roll into cond (not boolean). */
        const hi = args[0] ?? 0;
        let roll = 1;
        if (typeof ctx.rng === "function") roll = ctx.rng(1, hi) | 0;
        else if (ctx.rng && typeof ctx.rng.range === "function") roll = ctx.rng.range(1, hi) | 0;
        session.cond = roll & 0xff;
        syncCond();
        vmStep(nodeIndex, op, `rng(1,${hi}) → ${session.cond}`);
        note(`OP_1C rng_roll(1,${hi}) → cond=${session.cond}`);
        break;
      }

      case 0x1d:
        vmStep(nodeIndex, op, `audio_wait(${args[0] ?? 0})`);
        note(`OP_1D -$7E84→0x6798 audio_wait((arg*7)+1) — presentation only`);
        break;

      case 0x1e:
        vmStep(nodeIndex, op, `delay ${args[0] ?? 0}`);
        note(`OP_1E -$7BC0/0x22B4A delay + -$7BD2/0x22586 poll — skipped (timing)`);
        break;

      case 0x21: {
        const pos = args[0] ?? 0;
        const ty = (pos >> 4) & 0xf;
        const tx = pos & 0xf;
        onPatchTile?.(ty, tx, args[1], args[2]);
        session.exitFlags = (session.exitFlags | 4) & 0xff; /* 0x16A34 bit2 */
        vmStep(nodeIndex, op, `patch (${ty},${tx})`);
        note(`OP_21 set_tile (${ty},${tx}) vis=${args[1]} col=${args[2]}`);
        break;
      }

      case 0x22: {
        const lo = args[0] ?? 0;
        const hi = args[1] ?? 0;
        const era = session.era & 0xff;
        session.cond = era >= lo && era <= hi ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `era ${era} in [${lo}..${hi}] → ${session.cond}`);
        note(`OP_22 era gate → cond=${session.cond}`);
        break;
      }

      case 0x23: {
        const a1 = args[0] ?? 0;
        const a2 = args[1] ?? 0;
        const day = session.day & 0xff;
        if (a1 === 0xb5) session.cond = day & 1 ? 1 : 0;
        else if (a1 === 0xb6) session.cond = day & 1 ? 0 : 1;
        else session.cond = day >= a1 && day <= a2 ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `day ${day} → cond=${session.cond}`);
        note(`OP_23 day gate → cond=${session.cond}`);
        break;
      }

      case 0x24: {
        const need = decodeU16Gold(args);
        session.cond = sessionTryPayGold(session, need) ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `gold try-pay ${need} → ${session.cond}`);
        note(`OP_24 gold pool-pay ≥${need} → cond=${session.cond}`);
        break;
      }

      case 0x25: {
        const need = decodeU16Code25(args);
        session.cond = sessionTryPayGems(session, need) ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `gems try-pay ${need} → ${session.cond}`);
        note(`OP_25 gems pool-pay ≥${need} → cond=${session.cond}`);
        break;
      }

      case 0x26:
      case 0x27: {
        /* OP_26/27 @ 0x16BC0: digit 1..party → cond/-$5D42/-$5D3F; dead ≥$81 re-prompt;
         * ESC → SCRIPT_ABORT. No invented prompt string (script text already shown). */
        vmStep(nodeIndex, op, op === 0x26 ? "select member skill" : "select member");
        while (true) {
          const slot0 = await promptSelectMember(ctx);
          if (slot0 == null) {
            aborted = true;
            ended = true;
            note(`OP_${op.toString(16)} ESC — script abort`);
            return { teleported, ended, aborted, notes, trace };
          }
          const m = ensureParty(session)[slot0];
          if (m && (m.condition ?? 0) >= 0x81) {
            note(`OP_${op.toString(16)} reject dead slot ${slot0 + 1}`);
            continue;
          }
          session.selectedMember = slot0 + 1; /* A4-$5D42 1-based */
          session.savedCond = slot0 + 1;
          session.cond = slot0 + 1;
          syncCond();
          note(`OP_${op.toString(16)} select member → slot ${slot0 + 1}`);
          break;
        }
        break;
      }

      case 0x28: {
        /* OP_28 @ 0x16C86: discard arg0; backpack-only consume always. */
        const itemId = args[1] ?? 0;
        const has = sessionConsumeBackpackItem(session, itemId);
        session.cond = has ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `consume backpack ${itemId} → ${session.cond}`);
        note(`OP_28 take ${itemName(manifest, itemId)} → cond=${session.cond}`);
        onSessionChange?.(session);
        break;
      }

      case 0x29:
        vmStep(nodeIndex, op, "force_abort");
        aborted = true;
        ended = true;
        note("OP_29 script abort");
        return { teleported, ended, aborted, notes, trace };

      case 0x2a: {
        vmStep(nodeIndex, op, "treasure");
        const treasure = decodeTreasure2A(args);
        const text = treasure
          ? formatTreasureMessage(treasure, manifest, resolveEventText, session)
          : "Treasure!";
        await waitForSpace(text, { sheet: "70_anm", frame: "0" }, op);
        note(`OP_2A treasure: +${treasure?.gold ?? 0} gp, +${treasure?.gems ?? 0} gems`);
        onSessionChange?.(session);
        break;
      }

      case 0x2b: {
        const n = args[0] ?? 0;
        if (session.combatVictory) {
          vmStep(nodeIndex, op, `skip ${n} (victory latch)`);
          i = skipTokenNodes(nodes, i, n);
          note(`OP_2B skip ${n} — combat victory`);
        } else {
          vmStep(nodeIndex, op, "no skip (no victory)");
          note(`OP_2B skip ${n} — victory latch clear`);
        }
        break;
      }

      case 0x2c: {
        const add = args[0] ?? 0;
        session.scriptCounter = ((session.scriptCounter | 0) + add) & 0xffff;
        session.exitFlags = (session.exitFlags | 1) & 0xff; /* 0x16D98 bit0 */
        vmStep(nodeIndex, op, `counter += ${add} → ${session.scriptCounter}`);
        note(`OP_2C counter → ${session.scriptCounter}`);
        break;
      }

      case 0x2d: {
        const arg1 = args[0] ?? 0;
        const arg2 = args[1] ?? 0;
        session.cond = op2dCheckMemberAttr(session, arg1, arg2);
        syncCond();
        vmStep(nodeIndex, op, `member attr 0x${arg1.toString(16)}/0x${arg2.toString(16)} → ${session.cond}`);
        note(`OP_2D member check → cond=${session.cond}`);
        break;
      }

      case 0x2f: {
        /* event_op2f @ 0x16FEA: prompts via -$7F92 into A4-$5C50 (10 chars, space-padded). */
        vmStep(nodeIndex, op, "answer prompt");
        const ask = typeof promptAnswer === "function" ? promptAnswer : null;
        if (ask) {
          session.inputBuffer = await ask("?", null, 0x2f);
        } else {
          session.inputBuffer = "          ";
          note("OP_2F: no promptAnswer bound — empty buffer");
        }
        note(`OP_2F input="${(session.inputBuffer || "").trimEnd()}"`);
        break;
      }

      case 0x30: {
        /* event_op30_answer_check @ 0x17034: 10-byte compare vs input buffer. */
        const answer = decodeOp30Answer(args);
        session.cond = checkOp30Password(session.inputBuffer || "", args) ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `password "${answer}" → ${session.cond}`);
        note(`OP_30 password "${answer}" → cond=${session.cond}`);
        break;
      }

      case 0x31: {
        /* EXIT_FLAGS bit1; 0x4952 HP damage (out-flags 0); living abort. */
        const memberSpec = args[0] ?? 0;
        const value = ((args[1] ?? 0) | ((args[2] ?? 0) << 8)) & 0xffff;
        const n = sessionOp31IterateDamage(session, memberSpec, value);
        session.exitFlags = (session.exitFlags | 2) & 0xff;
        if (sessionCountLivingPartyMembers(session) === 0) {
          aborted = true;
          vmStep(nodeIndex, op, "living abort");
          note("OP_31: no living party → SCRIPT_ABORT (0x47EC)");
          return { teleported, ended, aborted, notes, trace };
        }
        vmStep(nodeIndex, op, `damage ${value} → ${n} target(s)`);
        note(`OP_31: damage ${value} on ${n} member(s)`);
        onSessionChange?.(session);
        break;
      }

      case 0x32: {
        const id = args[0] ?? 0;
        session.cond = sessionCountPartyNibbleMatches(session, id);
        syncCond();
        vmStep(nodeIndex, op, `nibble id=0x${id.toString(16)} → count=${session.cond}`);
        note(`OP_32 title/skill nibble count → cond=${session.cond}`);
        break;
      }

      case 0x00:
        /* Invalid opcode @ 0x1748C → SCRIPT_ABORT. */
        aborted = true;
        ended = true;
        vmStep(nodeIndex, op, "invalid → abort");
        note("OP_00 invalid opcode → SCRIPT_ABORT");
        return { teleported, ended, aborted, notes, trace };

      default:
        /* Opcodes ≥ 0x33: C++ aborts + endScript. */
        if (op >= 0x33) {
          aborted = true;
          ended = true;
          vmStep(nodeIndex, op, "out of range → abort");
          note(`Opcode 0x${op.toString(16)} ≥ 0x33 → SCRIPT_ABORT`);
          return { teleported, ended, aborted, notes, trace };
        }
        vmStep(nodeIndex, op, "unhandled");
        note(`Unhandled opcode 0x${op.toString(16)}`);
        break;
    }
  }

  /* Natural script fall-through: same epilogue as C++ runVmLoop — drain queue. */
  if ((session.queuedEventId ?? 0xff) !== 0xff) {
    if (await runQueuedDispatch(ctx)) {
      if (ctx.teleported) teleported = true;
      ended = true;
    }
  }

  return { teleported, ended, aborted, notes, trace };
}

/** Scanner era gate: first op 0x22 always runs; else era must match attrib+0x0F. */
export function eraGateAllowsEvent(session, screen, evtData) {
  const first = evtData?.nodes?.[0];
  if (first && (first.op & 0xff) === 0x22) return true;
  if (screen?.eraGate == null) return true; /* bundle without eraGate: open */
  return (session.era & 0xff) === (screen.eraGate & 0xff);
}

export { createSessionState, checkOp30Password, remapOp0cDest };

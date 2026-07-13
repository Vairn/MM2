/** Event.dat script VM for the map walker — mirrors game/src/events/EventRuntime.cpp + session stub. */
"use strict";

import {
  createSessionState,
  sessionPartyGoldTotal,
  sessionDeductGold,
  sessionApplyTreasure,
  sessionGiveItem,
  sessionHasItem,
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
  syncSessionGoldFromParty,
  getPartyMember,
  ensureParty,
} from "./sessionState.js";

import {
  runTempleService,
  runTrainingService,
  runSmithService,
  runInnService,
  runTavernService,
  runGuildService,
  runInnRegistry,
  runDeferredServiceMenu,
  runArenaTicketSelector,
  promptSelectMember,
  svcGeneralStoreConvert,
  svcApplyDrinkEncoding,
  svcApplyFoodEncoding,
  svcFoodEncodePurchase,
  svcDrinkEncodePurchase,
  svcQuestLordArm,
  svcQuestCompleteReward,
  svcQuestBusy,
} from "./townServices.js";

import { binExecSelector } from "./selectorBin.js";

/** @deprecated use runDefaultRangeOverlay — kept for tests referencing combat bytes. */
const LOC_61_COMBAT_OVERLAY = {
  22: [0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00],
  23: [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
  24: [0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
  25: [0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
};

/**
 * Default-range overlay VM re-entry (asm runDefaultRangeOverlay @ EventRuntime.cpp).
 * Uses WALKER_OVERLAYS from export_map_walker.py when ctx.overlays is set.
 * @returns {Promise<boolean>}
 */
async function runDefaultRangeOverlay(ctx, sel) {
  const bin = binExecSelector(sel);
  if (!bin) return false;

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
  if (!slot) return false;

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

/** Shared OP_12 fixed-encounter flow (map scripts + loc-61 overlays). */
async function runOp12Combat(ctx, args) {
  const { session, manifest, waitForSpace, promptYesNo, promptCombatResult, note, onSessionChange } =
    ctx;
  const enc = describeEncounter(manifest, 0x12, args, "");
  const sprite = ctx.sprite ?? enc.sprite;
  const modalText = `${enc.text}\n\nV = victory (continue script)\nN = flee (abort)`;
  await waitForSpace(modalText, sprite, 0x12);
  const won = promptCombatResult
    ? await promptCombatResult(enc)
    : await promptYesNo("Did the party win?", sprite, 0x12);
  if (won) {
    session.combatVictory = true;
    /* 0x1243e: clr.b -$794C — do not invent post-fight $FE. */
    session.foundItemSentinel = 0;
    note(`${enc.heading}: victory latch set`);
  } else {
    note(`${enc.heading}: fled — script ends`);
    ctx.ended = true;
  }
  onSessionChange?.(session);
}

/**
 * Tile-flag ambient fight @ asm 0x176F2: collision 0x80 with no event.dat triplet.
 * Mirrors game eventRunTileAmbientEncounter + eventVmConsumeTileEncounterFlag.
 */
export async function runTileAmbientEncounter(ctx) {
  const {
    session,
    manifest,
    waitForSpace,
    promptYesNo,
    promptCombatResult,
    onSessionChange,
    maps,
    screenId,
    tileX,
    tileY,
  } = ctx;
  const notes = [];
  const enc = describeEncounter(manifest, 0x13, [], "");
  const modalText = `${enc.text}\n\n(tile-flag @ 0x176F2 — random monsters)\n\nV = victory\nN = flee`;
  await waitForSpace(modalText, enc.sprite, 0x13);
  const won = promptCombatResult
    ? await promptCombatResult(enc)
    : await promptYesNo("Did the party win?", enc.sprite, 0x13);
  if (won) {
    session.combatVictory = true;
    session.foundItemSentinel = 0; /* 0x1243e */
    notes.push("Tile-flag encounter: victory latch set");
  } else {
    notes.push("Tile-flag encounter: fled");
  }
  sessionClearTileFlag(session, screenId, tileX, tileY, maps);
  onSessionChange?.(session);
  return { won, notes };
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
  0x00: "real", 0x01: "partial", 0x02: "partial", 0x03: "partial", 0x04: "partial",
  0x05: "partial", 0x06: "partial", 0x07: "real", 0x08: "partial", 0x09: "real",
  0x0a: "partial", 0x0b: "partial", 0x0c: "real",
  0x0d: "real", /* presentation-only; no GS writes */
  0x0e: "partial", /* dispatch verified; selector engines separate */
  0x0f: "real", 0x10: "real", 0x11: "real",
  0x12: "partial", 0x13: "partial", 0x14: "real", 0x15: "real", 0x16: "real", 0x17: "real",
  0x18: "real", 0x19: "real", 0x1a: "real", 0x1b: "real", 0x1c: "real",
  0x1d: "real", 0x1e: "real", /* presentation/timing only */
  0x1f: "real", 0x20: "real", 0x21: "real", 0x22: "real", 0x23: "real",
  0x24: "real", 0x25: "real", 0x26: "partial", 0x27: "partial", 0x28: "real", 0x29: "real",
  0x2a: "real", 0x2b: "real", 0x2c: "real", 0x2d: "real", 0x2e: "real", 0x2f: "real",
  0x30: "real", 0x31: "real", 0x32: "real",
  /* OP_04/05/06: can't-see gate -$79E1 + OP_06 '-'→'{' are GS/logic-exact;
   * remaining Partial = glyph/centering chrome only. */
};

const SELECTOR_SPRITES = {
  0x01: "68_anm", 0x02: "67_anm", 0x03: "63_anm", 0x04: "66_anm",
  0x05: "37_anm", 0x06: "62_anm", 0x07: null, 0x08: null,
};

const SELECTOR_LABEL = {
  0x01: "Inn", 0x02: "Training hall", 0x03: "Tavern", 0x04: "Temple",
  0x05: "Mages guild", 0x06: "Blacksmith", 0x07: "General store", 0x08: "Arena / special shop",
  0x64: "Circus",
};

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
  let text;
  if (mNames.length) text = `${heading}:\n${formatEncounterNameList(mNames)}`;
  else if (isRandom && ids.length) {
    text = `${heading} (seed: ${ids.map((id) => id.toString(16).toUpperCase().padStart(2, "0")).join(" ")})`;
  } else if (ids.length) {
    text = `${heading}:\n${ids.map((id) => id.toString(16).toUpperCase().padStart(2, "0")).join(" ")}`;
  } else text = `${heading}!`;
  return { text, sprite, heading, names: mNames };
}

export function formatEncounterFlowLine(manifest, node) {
  const pseudo = node.pseudo || (node.op === 0x13 ? "encounter_setup_b(...)" : "encounter_setup(...)");
  const { text, names } = describeEncounter(manifest, node.op, node.args, pseudo);
  if (names.length) return `${pseudo} — ${formatEncounterNameList(names).replace(/\n/g, ", ")}`;
  if (text.includes("(seed:")) return `${pseudo} — ${text}`;
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

function captureServiceTitle(text, buf) {
  if (!text) return buf.title || "";
  const line = String(text).split("\n")[0].trim();
  return line || buf.title || "";
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
  const { waitForSpace, screenId, promptYesNo, promptMenuKey, session, note } = ctx;
  const slot = townIntroSlot(screenId);

  if (sel === 0x01) {
    const yes = await promptYesNo(INN_INTRO[slot], sprite, 0x0e);
    if (!yes) {
      ctx.note("Inn registry declined");
      return;
    }
    await runInnRegistry(ctx);
    await runInnService(ctx);
    return;
  }
  if (sel === 0x02) {
    await waitForSpace(title, sprite, 0x0e);
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
    const slot = await promptSelectMember(ctx);
    if (slot == null) return;
    const member = getPartyMember(session, slot);
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
      const roll = Math.floor(Math.random() * 0xfe) + 1;
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
    const slot = townIntroSlot(ctx.screenId);
    const yes = await ctx.promptYesNo(TAVERN_INTRO[slot], sprite, 0x0e);
    if (!yes) {
      ctx.note(`${title}: declined`);
      return;
    }
    await runTavernService(ctx);
    return;
  }
  if (sel === 0x05) {
    const slot = townIntroSlot(ctx.screenId);
    const yes = await ctx.promptYesNo(MAGE_GUILD_INTRO[slot], sprite, 0x0e);
    if (!yes) {
      ctx.note(`${title}: declined`);
      return;
    }
    await runGuildService(ctx);
    return;
  }
  if (sel === 0x08) {
    /* Arena Games ticket engine (0x08 -> thunk -$7DBE -> 0x9D76). CORRECTED
     * 2026-07: byte-verified against the A4 vtable trampoline table in
     * mm2_data_00.bin — explicit selector 0x08 is the SOLE path into 0x9D76,
     * not the default-range dispatch (that thunk, -$7DFA, actually resolves
     * to event_dat_loader / 0x92F2 — see selectorBin.js). */
    await runArenaTicketSelector(ctx, sel);
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
    const pick = await (ctx.promptMenuKey || promptMenuKey)(menu, "abcd", sprite, 0x0e);
    const ch = (pick || "").toUpperCase();
    if (ch >= "A" && ch <= "C") {
      if (drink) svcDrinkEncodePurchase(party, ch.charCodeAt(0) - 65);
      else svcFoodEncodePurchase(party, ch.charCodeAt(0) - 65);
      note(drink ? "OP_0E 0xCA drink encode" : "OP_0E 0xC9 food encode");
    } else if (ch === "D") {
      const armed = svcQuestLordArm(party, drink);
      note(armed < 0 ? "OP_0E lord arm (none)" : `OP_0E lord arm (${armed})`);
    }
    ctx.onSessionChange?.(session);
    return;
  }
  if (isTownServiceSelector(sel)) {
    /* Skill vendors + quests: real overlay bytecode (no EventSkillBuy hijack). */
    if (await runDefaultRangeOverlay(ctx, sel)) {
      if (ctx.teleported) return;
      return;
    }
    await waitForSpace(title, sprite, 0x0e);
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

  const notes = [];
  /** @type {object[]} */
  const trace = [];
  let serviceTitle = "";
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
            serviceTitle = captureServiceTitle(text, { title: serviceTitle });
          }
        } else if (op === 0x05) {
          if (!cantSee) onViewportOverlay?.({ type: "wall", text });
        } else if (op === 0x06) {
          if (!cantSee) onViewportOverlay?.({ type: "signpost", text });
        } else if (op >= 0x01 && op <= 0x03) {
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
      case 0x08:
        vmStep(nodeIndex, op, "wait space");
        await waitForSpace(pendingPromptText ?? "", null, pendingPromptOp ?? 0x07);
        pendingPromptText = null;
        break;

      case 0x09:
      case 0x0a: {
        vmStep(nodeIndex, op, "y/n prompt");
        const promptText = pendingPromptText ?? "";
        pendingPromptText = null;
        session.cond = (await promptYesNo(promptText, null, pendingPromptOp ?? op)) ? 1 : 0;
        syncCond();
        note(`Y/N → cond=${session.cond}`);
        break;
      }

      case 0x0b: {
        vmStep(nodeIndex, op, node.pseudo || "service_sign");
        const sprite = signSpriteFromPseudo(node.pseudo);
        lastSignSprite = sprite;
        const title = serviceTitle || "Service";
        await waitForSpace(title, sprite, 0x0b);
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
        const title = serviceTitle || SELECTOR_LABEL[sel] || "Town service";
        aborted = true; /* ASM sets -$79EA at OP_0E entry */

        ctx.tileX = tileX;
        ctx.tileY = tileY;
        ctx.onTeleport = onTeleport;

        if (sel === 0x04) {
          const yes = await promptYesNo(TEMPLE_INTRO[slot], sprite, op);
          if (!yes) {
            note(`${title}: declined`);
            ended = true;
            return { teleported, ended, aborted, notes, trace };
          }
        }

        if (sel === 0x06) {
          const yes = await promptYesNo(SMITH_INTRO[slot], sprite, op);
          if (!yes) {
            note(`${title}: declined`);
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
          sel === 0xc9 ||
          sel === 0xca ||
          isTownServiceSelector(sel);

        if (serviceHandled) {
          if (sel === 0x02) {
            await waitForSpace(title, sprite, op);
          }
          await runTownService(ctx, sel, title, sprite);
          if (ctx.teleported) {
            teleported = true;
          }
          ended = true;
          onSessionChange?.(session);
          return { teleported, ended, aborted, notes, trace };
        }

        await waitForSpace(`${title}\n(selector 0x${sel.toString(16)})`, sprite, op);
        note(`exec_selector(0x${sel.toString(16)}) — unhandled`);
        ended = true;
        return { teleported, ended, aborted, notes, trace };
      }

      case 0x0f:
        vmStep(nodeIndex, op, "end_script");
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
          if (ctx.ended) return { teleported, ended, aborted, notes, trace };
          break;
        }
        const enc = describeEncounter(manifest, op, args, node.pseudo);
        const sprite = lastSignSprite ?? enc.sprite;
        lastSignSprite = null;
        const modalText = `${enc.text}\n\nV = victory (continue script)\nN = flee (abort)`;
        await waitForSpace(modalText, sprite, op);
        const won = promptCombatResult
          ? await promptCombatResult(enc)
          : await promptYesNo("Did the party win?", sprite, op);
        if (won) {
          session.combatVictory = true;
          session.foundItemSentinel = 0; /* 0x1243e */
          note(`${enc.heading}: victory latch set`);
        } else {
          note(`${enc.heading}: fled — script ends`);
          ended = true;
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
          session.selectedMember = slot0;
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
        const has = sessionHasItem(session, itemId, true);
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
        let value = ((args[1] ?? 0) | ((args[2] ?? 0) << 8)) & 0xffff;
        if (memberSpec >= 0x80) value = session.cond & 0xff;
        const party = session.party ?? [];
        const targets = [];
        const spec = memberSpec & 0x7f;
        if (spec === 0) {
          for (let i = 0; i < party.length; i++) targets.push(i);
        } else {
          /* ASM 0x1710A: only spec==9 → selected (-$5D42); 1..8 = member slots. */
          let slot = spec === 9 ? (session.selectedMember ?? 1) : spec;
          if (slot >= 1 && slot <= party.length) targets.push(slot - 1);
        }
        for (const i of targets) {
          const m = party[i];
          if (!m || (m.condition ?? 0) >= 0x80) continue;
          m.condition = (m.condition ?? 0) & 0xef;
          const hp = m.hpMax ?? m.hp ?? 0;
          if ((m.condition ?? 0) & 0x40) {
            m.condition = 0x81;
            m.hpMax = 0;
            m.hp = 0;
          } else if (value >= hp) {
            m.condition = (m.condition ?? 0) | 0x40;
            m.hpMax = 0;
            m.hp = 0;
          } else {
            m.hpMax = hp - value;
            if (m.hp != null) m.hp = Math.max(0, (m.hp | 0) - value);
          }
        }
        if (sessionCountLivingPartyMembers(session) === 0) {
          aborted = true;
          vmStep(nodeIndex, op, "living abort");
          note("OP_31: no living party → SCRIPT_ABORT (0x47EC)");
          return { teleported, ended, aborted, notes, trace };
        }
        vmStep(nodeIndex, op, `damage ${value} → ${targets.length} target(s)`);
        note(`OP_31: damage ${value} on ${targets.length} member(s)`);
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

      default:
        vmStep(nodeIndex, op, "unhandled");
        note(`Unhandled opcode 0x${op.toString(16)}`);
        break;
    }
  }

  return { teleported, ended, aborted, notes, trace };
}

export { createSessionState, checkOp30Password, remapOp0cDest };

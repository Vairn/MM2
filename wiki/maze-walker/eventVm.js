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
  sessionClearTileFlag,
  healingCostPerChar,
  trainingCostPerChar,
  donationGold,
  ITEM_GOLD_GOBLET,
  ITEM_FE_FARTHING,
} from "./sessionState.js";

/** Token byte lengths for opcodes 0x00..0x32 (A4-$6CC8). */
export const OP_TOKEN_DELTA = [
  1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 3, 3, 2, 2, 1, 2, 2, 13, 11, 1, 4, 3, 3, 5, 5, 3,
  2, 2, 2, 2, 7, 7, 4, 3, 3, 3, 3, 1, 1, 3, 1, 15, 2, 2, 3, 3, 1, 11, 4, 2,
];

export const OP_ARGC = {
  0x00: 0, 0x01: 1, 0x02: 1, 0x03: 1, 0x04: 1, 0x05: 1, 0x06: 1, 0x07: 0, 0x08: 0,
  0x09: 0, 0x0a: 0, 0x0b: 2, 0x0c: 2, 0x0d: 1, 0x0e: 1, 0x0f: 0, 0x10: -1, 0x11: -1,
  0x12: 12, 0x13: 10, 0x14: 0, 0x15: 3, 0x16: 2, 0x17: 2, 0x18: 4, 0x19: 4, 0x1a: 2,
  0x1b: 1, 0x1c: 1, 0x1d: 1, 0x1e: 1, 0x1f: 6, 0x20: 6, 0x21: 3, 0x22: 2, 0x23: 2,
  0x24: 2, 0x25: 2, 0x26: 0, 0x27: 0, 0x28: 2, 0x29: 0, 0x2a: 14, 0x2b: -1, 0x2c: 1,
  0x2d: 2, 0x2e: 2, 0x2f: 0, 0x30: 10, 0x31: 3, 0x32: 1,
};

/** Opcode implementation status for audit report. */
export const OPCODE_STATUS = {
  0x00: "real", 0x01: "real", 0x02: "real", 0x03: "real", 0x04: "real", 0x05: "real", 0x06: "real",
  0x07: "real", 0x08: "real", 0x09: "real", 0x0a: "real", 0x0b: "real", 0x0c: "real",
  0x0d: "partial", 0x0e: "partial", 0x0f: "real", 0x10: "real", 0x11: "real",
  0x12: "partial", 0x13: "partial", 0x14: "real", 0x15: "real", 0x16: "real", 0x17: "real",
  0x18: "real", 0x19: "real", 0x1a: "real", 0x1b: "real", 0x1c: "partial", 0x1d: "partial",
  0x1e: "partial", 0x1f: "partial", 0x20: "partial", 0x21: "real", 0x22: "real", 0x23: "real",
  0x24: "real", 0x25: "real", 0x26: "partial", 0x27: "partial", 0x28: "real", 0x29: "real",
  0x2a: "real", 0x2b: "real", 0x2c: "real", 0x2d: "partial", 0x2e: "partial", 0x2f: "real",
  0x30: "partial", 0x31: "partial", 0x32: "partial",
};

const SELECTOR_SPRITES = {
  0x01: "68_anm", 0x02: "67_anm", 0x03: "63_anm", 0x04: "66_anm",
  0x05: "37_anm", 0x06: "62_anm", 0x07: null, 0x08: null,
};

const SELECTOR_LABEL = {
  0x01: "Inn", 0x02: "Training hall", 0x03: "Tavern", 0x04: "Temple",
  0x05: "Mages guild", 0x06: "Blacksmith", 0x07: "General store", 0x08: "Arena / special shop",
  0x64: "Portal travel",
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

const MAGE_GUILD_INTRO = [
  "Sages in multi-hued robes congregate\nin the hall.  The archmage offers\nspells for sale.  Interested (y/n)?",
  "The meeting shifts towards entropy as\nyou step in.  A cabalist approaches\nyou with a spell list.  Buy (y/n)?",
  "Lounging next to a roaring fire which\nburns no wood, mystics offer spells.\nBuy (y/n)?",
  "Magicians clad in furry robes sip\nwine and chat softly.  Listen (y/n)?",
  "Sorcerers sort phials of sands on\nthe shelves.  A man barks, \"Spells\n(y/n)?\"",
];

const BLACKSMITH_INTRO = [
  "The burly blacksmith Svendegard busily\nshapes a deadly sword in the forge.\nDo you care to see his work (y/n)?",
  "The famous blacksmith Morgan\nDrewnhald asks if you wish to view\nweapons he has made available for\nsale.  View (y/n)?",
  "Flames leap out from the brick\nsmelter as the sweaty smith says,\n\"Do you wish to see my humble\nwares (y/n)?\"",
  "A friendly smith wipes his hands on a\nworn, leather apron.  He shakes your\nhand and asks, \"Care to see my wares\n(y/n)?\"",
  "A tough old blacksmith glares at\nyour party and shouts \"Hey, only\ncowards browse!\"  View wares (y/n)?",
];

const POORMANS_PORTAL =
  "A uniformed brownie stands before a shimmering energy field offering " +
  "instantaneous travel to Sandsobar for 10 gold. Travel (y/n)?";

const OP0D_SEQUENCE_NAMES = [
  "gated intro (enable -$79AF)",
  "sequence 1", "sequence 2", "sequence 3", "sequence 4",
  "sequence 5", "sequence 6", "sequence 7", "sequence 8",
  "pre-transition redraw (exit-flag bit0)",
];

const SERVICE_MENUS = {
  0x03: {
    title: "Slaughtered Lamb — menu",
    lines: [
      "A) feeding frenzy  B) drink  C) specialties",
      "D) tip the waitress  E) listen for rumors",
      "(Food/drink costs RNG-gated in engine — not simulated)",
    ],
  },
  0x04: {
    title: "Gateway Temple — menu",
    lines: [
      "A) Restore Cond  B) Restore Algn  C) Donations",
      "D) cleric spell  E) cleric spell  F) cleric spell",
    ],
    costs: (screenId, session) => {
      const heal = healingCostPerChar(session.level, screenId);
      const donate = donationGold(screenId);
      return [`Heal (est.): ${heal} gp/char L${session.level}`, `Donate: ${donate} gp`];
    },
  },
  0x05: {
    title: "Mage guild — menu",
    lines: ["A) spell  B) spell  C) spell  D) spell", "(Membership + spell stock per town — engine)"],
  },
  0x06: {
    title: "Blacksmith — menu",
    lines: [
      "A) Weapons  B) Today's Specials  C) Armor",
      "D) Misc  E) Sell  F) Identify",
      "(Buy loop uses items.dat gold @ 0x1BF16 — not simulated)",
    ],
  },
  0x01: {
    title: "Inn — menu",
    lines: ["Rest / dismiss roster members (engine @ -$7CD4)", "(Rest HP/SP + day advance — not simulated)"],
  },
  0x02: {
    title: "Training hall — menu",
    lines: ["Level-up from XP when threshold met (engine @ 0x1C898 path)"],
    costs: (screenId, session) => [
      `Training fee formula: level×town×50 = ${trainingCostPerChar(session.level, screenId)} gp`,
    ],
  },
  0x07: { title: "General store", lines: ["Category shop engine @ 0x9D76 (deferred)"] },
  0x08: { title: "Arena / special shop", lines: ["Category shop @ 0x9D76 (deferred)"] },
};

const GOBLET_QUEST_NO_FARTHING = "Fool, you have no farthing to flick!";
const GOBLET_QUEST_PROMPT =
  "Nordon eyes your pack. \"Return my stolen goblet\nand flick a farthing in yonder fountain (y/n)?\"";
const GOBLET_QUEST_NO_GOBLET =
  "Nordon scowls. \"I see no Gold Goblet — seek the\ngoblins in Middlegate Cavern (7,0).\"";
const GOBLET_QUEST_SUCCESS =
  "Nordon beams! \"Ah, my goblet! Take 2000 experience,\nthe spell Eagle Eye, and search for 1000 gold!\"";

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

function decodeOp30Answer(args) {
  let s = "";
  for (const b of args) {
    const c = (0x11a - b) & 0xff;
    s += c >= 0x20 && c <= 0x7e ? String.fromCharCode(c) : "?";
  }
  return s.trimEnd();
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
  const parts = ["Treasure!"];
  if (treasure.gold > 0) parts.push(`Gold: +${treasure.gold} (party ${session.gold})`);
  if (treasure.gems > 0) parts.push(`Gems: +${treasure.gems} (party ${session.gems})`);
  for (const it of treasure.items) {
    const placed = sessionGiveItem(session, it.id, it.charges, it.flags);
    parts.push(`${itemName(manifest, it.id)}${placed ? "" : " → found-item buffer"}`);
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

async function showServiceMenu(ctx, sel, title, sprite) {
  const { screenId, session, waitForSpace } = ctx;
  const menu = SERVICE_MENUS[sel];
  const lines = [`${title}`, ""];
  if (menu) {
    lines.push(...menu.lines);
    if (menu.costs) lines.push("", ...menu.costs(screenId, session));
  } else {
    lines.push(`Service selector 0x${sel.toString(16)}`, "(Shop engine @ -$7DFA / 0x9D76 — menu not decoded)");
  }
  lines.push("", "(Walker: menu display only — pick character / buy not ported)");
  await waitForSpace(lines.join("\n"), sprite, 0x0e);
}

async function runGobletQuest(ctx, title, sprite) {
  const { manifest, session, waitForSpace, promptYesNo, strings, resolveEventText } = ctx;
  await waitForSpace(title, sprite, 0x0e);
  if (!sessionHasItem(session, ITEM_GOLD_GOBLET, false)) {
    const msg = strings?.[20] ? resolveEventText(strings[20]) : GOBLET_QUEST_NO_GOBLET;
    await waitForSpace(msg, sprite, 0x0e);
    ctx.note("Goblet quest: no Gold Goblet (item 224 / 0xE0) in inventory");
    return;
  }
  const yes = await promptYesNo(GOBLET_QUEST_PROMPT, sprite, 0x0e);
  if (!yes) {
    ctx.note("Goblet quest declined");
    return;
  }
  if (!sessionHasItem(session, ITEM_FE_FARTHING, false)) {
    const msg = strings?.[20] ? resolveEventText(strings[20]) : GOBLET_QUEST_NO_FARTHING;
    await waitForSpace(msg, sprite, 0x0e);
    ctx.note("Goblet quest: need Fe Farthing (item 212 / 0xD4) — Olympic Trial / temple donations");
    return;
  }
  sessionHasItem(session, ITEM_FE_FARTHING, true);
  sessionHasItem(session, ITEM_GOLD_GOBLET, true);
  sessionAddRewardStub(session);
  await waitForSpace(GOBLET_QUEST_SUCCESS, sprite, 0x0e);
  ctx.note("Goblet quest: turned in goblet + farthing — +1000 gold (stub reward)");
}

function sessionAddRewardStub(session) {
  session.gold += 1000;
  session.flags[0x2b] = (session.flags[0x2b] ?? 0) | 1;
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
        const text = strAt(strIdx);
        if (op === 0x04) {
          onViewportOverlay?.({ type: "door", text });
          serviceTitle = captureServiceTitle(text, { title: serviceTitle });
        } else if (op === 0x05) {
          onViewportOverlay?.({ type: "wall", text });
        } else if (op === 0x06) {
          onViewportOverlay?.({ type: "signpost", text });
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
        vmStep(nodeIndex, op, `→ screen ${args[0]}`);
        const dest = args[0];
        const tile = args[1];
        const ty = (tile >> 4) & 0xf;
        const tx = tile & 0xf;
        const destName = maps?.screens?.[dest]?.name ?? `screen ${dest}`;
        onTeleport?.(dest, tx, ty);
        teleported = true;
        ended = true;
        note(`Map transition → ${destName} (${tx},${ty})`);
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
        const sel = args[0];
        vmStep(nodeIndex, op, `selector 0x${sel.toString(16)}`);
        const slot = townIntroSlot(screenId);
        const fallbackSheet = SELECTOR_SPRITES[sel];
        const sprite = lastSignSprite ?? (fallbackSheet ? { sheet: fallbackSheet, frame: "0" } : null);
        lastSignSprite = null;
        const title = serviceTitle || SELECTOR_LABEL[sel] || "Town service";

        if (sel === 0x11 && screenId === 0) {
          const yes = await promptYesNo(POORMANS_PORTAL, sprite, op);
          if (yes) {
            if (!sessionDeductGold(session, 10)) {
              await waitForSpace("Not enough gold!", sprite, op);
              note("Poorman's Portal: insufficient gold");
              break;
            }
            onTeleport?.(4, 0x61 & 0xf, (0x61 >> 4) & 0xf);
            teleported = true;
            ended = true;
            note("Poorman's Portal → Sandsobar (−10 gp)");
            onSessionChange?.(session);
            return { teleported, ended, aborted, notes, trace };
          }
          note("Poorman's Portal declined");
          break;
        }

        if (sel === 0x0a) {
          await runGobletQuest(ctx, title, sprite);
          break;
        }

        if (sel === 0x03) {
          const yes = await promptYesNo(TAVERN_INTRO[slot], sprite, op);
          if (yes) await showServiceMenu(ctx, sel, title, sprite);
          else note(`${title}: declined`);
          break;
        }
        if (sel === 0x04) {
          const yes = await promptYesNo(TEMPLE_INTRO[slot], sprite, op);
          if (yes) await showServiceMenu(ctx, sel, title, sprite);
          else note(`${title}: declined`);
          break;
        }
        if (sel === 0x05) {
          const yes = await promptYesNo(MAGE_GUILD_INTRO[slot], sprite, op);
          if (yes) await showServiceMenu(ctx, sel, title, sprite);
          else note(`${title}: declined`);
          break;
        }
        if (sel === 0x06) {
          const yes = await promptYesNo(BLACKSMITH_INTRO[slot], sprite, op);
          if (yes) await showServiceMenu(ctx, sel, title, sprite);
          else note(`${title}: declined`);
          break;
        }
        if (sel === 0x01 || sel === 0x02) {
          await waitForSpace(title, sprite, op);
          await showServiceMenu(ctx, sel, title, sprite);
          break;
        }
        if (sel === 0x07 || sel === 0x08 || isTownServiceSelector(sel)) {
          await waitForSpace(`${title}`, sprite, op);
          await showServiceMenu(ctx, sel, title, sprite);
          note(`${SELECTOR_LABEL[sel] || title} (0x${sel.toString(16)})`);
          break;
        }

        await waitForSpace(`${title}\n(selector 0x${sel.toString(16)})`, sprite, op);
        note(`exec_selector(0x${sel.toString(16)}) — unhandled`);
        break;
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
        vmStep(nodeIndex, op, node.pseudo || "set_attr_bit");
        note(`${node.pseudo || "OP_2E"} — class-gated OR (needs roster classes)`);
        break;
      }

      case 0x16: {
        const want = args[1] ?? 0;
        let cond = 0;
        if (sessionHasItem(session, want, false)) cond = 1;
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
        const q = args[0] ?? 0;
        session.cond = 0;
        syncCond();
        vmStep(nodeIndex, op, `engine_query(${q}) → 0 (stub)`);
        note(`OP_1C engine_query(${q}) — runtime -$7BB4 not ported (cond=0)`);
        break;
      }

      case 0x1d:
        vmStep(nodeIndex, op, `engine_call(${args[0] ?? 0})`);
        note(`OP_1D engine_call(${args[0] ?? 0}) — presentation only`);
        break;

      case 0x1e:
        vmStep(nodeIndex, op, `delay ${args[0] ?? 0}`);
        note(`OP_1E delay(${args[0] ?? 0}) — skipped (timing)`);
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
        session.cond = sessionPartyGoldTotal(session) >= need ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `gold ${session.gold} >= ${need} → ${session.cond}`);
        note(`OP_24 gold check ≥${need} → cond=${session.cond}`);
        break;
      }

      case 0x25: {
        const code = decodeU16Code25(args);
        session.cond = sessionCheckCode16(session, code) ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `code 0x${code.toString(16)} → ${session.cond}`);
        note(`OP_25 code 0x${code.toString(16)} → cond=${session.cond}`);
        break;
      }

      case 0x26:
        vmStep(nodeIndex, op, "select member skill");
        await waitForSpace("Who will learn this skill (1-8)?", null, op);
        session.selectedMember = 1;
        note("OP_26 select member — stub picks slot 1 (no roster UI)");
        break;

      case 0x27:
        vmStep(nodeIndex, op, "select member");
        await waitForSpace("Select a party member (1-8):", null, op);
        session.selectedMember = 1;
        note("OP_27 select member — stub picks slot 1");
        break;

      case 0x28: {
        const probe = args[0] ?? 0;
        const itemId = args[1] ?? 0;
        const consume = probe === 0;
        const has = sessionHasItem(session, itemId, consume);
        session.cond = has ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `${itemName(manifest, itemId)} ${consume ? "consume" : "check"} → ${session.cond}`);
        note(`OP_28 ${consume ? "take" : "probe"} ${itemName(manifest, itemId)} → cond=${session.cond}`);
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

      case 0x2d:
        session.cond = 0;
        syncCond();
        vmStep(nodeIndex, op, "member attr (needs roster)");
        note("OP_2D member attr check — no roster (cond=0)");
        break;

      case 0x2f:
        session.inputBuffer = "";
        vmStep(nodeIndex, op, "clear input");
        break;

      case 0x30: {
        const answer = decodeOp30Answer(args);
        session.cond = 0;
        syncCond();
        vmStep(nodeIndex, op, `password "${answer}"`);
        note(`OP_30 password "${answer}" — no input buffer (cond=0)`);
        break;
      }

      case 0x31:
        vmStep(nodeIndex, op, "party engine op");
        note("OP_31 party iterate engine call — deferred (-$7F08)");
        break;

      case 0x32: {
        const id = args[0] ?? 0;
        session.cond = session.titleNibbleCount === id ? 1 : 0;
        syncCond();
        vmStep(nodeIndex, op, `title nibble id=0x${id.toString(16)} → ${session.cond}`);
        note(`OP_32 title nibble count — stub cond=${session.cond}`);
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

export { createSessionState };

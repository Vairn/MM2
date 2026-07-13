/**
 * OP_0E 0xFD endgame print-chrome — mirrors GameSession FdPrintChrome stages
 * + EventTownServices case 0xFD + eventVmFillOp0eFdStrTables.
 */
"use strict";

import { scriptedKeyReset } from "./scriptedKey.js";
import { sessionSeedOp0eFdEncounter } from "./sessionState.js";
import { runCombatContract } from "./combatSession.js";

/** DATA hunk bank offsets — EventVmHelpers.h kStrBankOffs. */
const STR_BANK_OFFS = [0x0000, 0x063c, 0x0f5c, 0x1286, 0x1844, 0x1e80];
const STR_BANK_SPAN = 0x924;

/**
 * Decode str.dat bank into NUL-terminated C-strings (same as eventVmDecodeStrBank
 * + eventVmNextStrBankCString walk).
 * @param {Uint8Array|ArrayBuffer} strDat
 * @param {number} bankIndex
 * @returns {string[]}
 */
export function decodeStrBankStrings(strDat, bankIndex) {
  const raw = strDat instanceof Uint8Array ? strDat : new Uint8Array(strDat);
  if (bankIndex < 0 || bankIndex >= 4) return [];
  const off = STR_BANK_OFFS[bankIndex];
  const buf = new Uint8Array(STR_BANK_SPAN);
  for (let i = 0; i < STR_BANK_SPAN; i++) {
    const src = off + i;
    let c = 0;
    if (src < raw.length) {
      c = (raw[src] + 0x1c) & 0xff;
      if (c === 0x1d) c = 0;
    }
    buf[i] = c;
  }
  const out = [];
  let cur = 0;
  while (cur < STR_BANK_SPAN) {
    const start = cur;
    while (cur < STR_BANK_SPAN && buf[cur] !== 0) cur++;
    const slice = buf.subarray(start, cur);
    out.push(String.fromCharCode(...slice));
    cur++; /* past NUL */
  }
  return out;
}

/**
 * Fill session.fdPtrTables from bank-3 linear strings (counts match C++ fill).
 * @param {object} session
 * @param {Uint8Array|null} strDat
 */
export function fillOp0eFdStrTables(session, strDat) {
  const counts = [4, 4, 14, 4, 11, 10];
  session.fdPtrTables = counts.map(() => []);
  if (!strDat) {
    session.scriptedKeyMode = 0xfd;
    return;
  }
  const strings = decodeStrBankStrings(strDat, 3);
  let cursor = 0;
  for (let t = 0; t < counts.length; t++) {
    const n = counts[t];
    const rows = [];
    for (let i = 0; i < n; i++) {
      rows.push(strings[cursor++] ?? "");
    }
    session.fdPtrTables[t] = rows;
  }
  session.scriptedKeyMode = 0xfd;
  session.monsterSlots = new Array(10).fill(0);
}

function formatPtrTable(rows) {
  return (rows ?? []).filter((s) => s && s.length).join("\n");
}

function loadFdPage(session, stage) {
  const tables = session.fdPtrTables ?? [];
  switch (stage) {
    case 0:
      return formatPtrTable(tables[0]) || "(continue)";
    case 2: {
      const a = formatPtrTable(tables[1]);
      return (
        "A vaccuum tube sucks the party into\n" +
        "        the control room...\n" +
        (a || "")
      );
    }
    case 4:
      return (
        "         Thank you.\n" +
        "Recalculating trajectory now...\n" +
        "Error, Error!  Computer malfunction.\n" +
        "    Internal program override..."
      );
    case 5: {
      const a = formatPtrTable(tables[2]);
      const b = formatPtrTable(tables[3]);
      if (a && b) return `${a}\n${b}`;
      return a || b || "(continue)";
    }
    case 6: {
      const a = formatPtrTable(tables[4]);
      const b = formatPtrTable(tables[5]);
      if (a && b) return `${a}\n${b}`;
      return a || b || "(continue)";
    }
    default:
      return "";
  }
}

function passwordOk(typed) {
  const kWafe = "WAFE      ";
  if (!typed) return false;
  for (let i = 0; i < 10; i++) {
    if ((typed[i] ?? " ") !== kWafe[i]) return false;
  }
  return true;
}

/**
 * Full OP_0E 0xFD hosted flow (print → fight → WAFE → pages / Death Strikes).
 * @param {object} ctx event VM ctx (+ optional strDat / promptAnswer)
 */
export async function runOp0eFdPrintChrome(ctx) {
  const {
    session,
    waitForSpace,
    promptAnswer,
    note,
    onSessionChange,
    onTeleport,
    maps,
    sprite,
    strDat,
  } = ctx;

  fillOp0eFdStrTables(session, strDat ?? session.strDat ?? null);
  scriptedKeyReset(session);

  const abort = session.scriptAbort ?? 1;
  if (abort === 2) {
    const dest = session.savedTownId ?? 0;
    const entry = maps?.screens?.[dest]?.entry ?? [0, 0];
    onTeleport?.(dest, entry[0] ?? 0, entry[1] ?? 0);
    session.pendingEventLatch = 1;
    ctx.teleported = true;
    note?.("OP_0E 0xFD abort=2 Goto Town");
    onSessionChange?.(session);
    return;
  }
  if (abort === 3) {
    session.op0eFdCtr = ((session.op0eFdCtr ?? 0) + 1) & 0xffff;
    const dest = session.savedTownId ?? 0;
    const entry = maps?.screens?.[dest]?.entry ?? [0, 0];
    await waitForSpace("     Death Strikes!", sprite, 0x0e);
    onTeleport?.(dest, entry[0] ?? 0, entry[1] ?? 0);
    session.pendingEventLatch = 1;
    ctx.teleported = true;
    note?.("OP_0E 0xFD abort=3 Death Strikes → Goto Town");
    onSessionChange?.(session);
    return;
  }

  /* Stage 0: PTR0 then fight seed (GameSession::maybeOpenFdPrintChrome → startOp0eFdEncounter). */
  await waitForSpace(loadFdPage(session, 0), sprite, 0x0e);
  sessionSeedOp0eFdEncounter(session);
  session.scriptAbort = 1;
  const slots = (session.monsterSlots ?? []).slice(0, 5);
  const enc = {
    heading: "Endgame encounter",
    text: `Endgame encounter (mode $83):\n${slots
      .map((id) => id.toString(16).toUpperCase().padStart(2, "0"))
      .join(" ")}`,
    names: [],
    sprite: null,
  };
  const outcome = await runCombatContract(ctx, enc, 0x0e);

  if (outcome !== "victory") {
    session.scriptAbort = 1;
    note?.("OP_0E 0xFD fight non-victory → SCRIPT_ABORT=1");
    onSessionChange?.(session);
    return;
  }

  /* Victory → stage 2 control room + WAFE entry. */
  await waitForSpace(loadFdPage(session, 2), sprite, 0x0e);

  let stage = 3;
  while (stage === 3) {
    const typed = promptAnswer
      ? await promptAnswer("Enter preamble:", sprite, 0x0e)
      : "WAFE      ";
    const trimmed = (typed ?? "").replace(/ /g, "");
    if (typed == null || trimmed.length === 0) {
      /* ESC / empty → Death Strikes path (GameSession fd stage 3 escape). */
      session.scriptAbort = 3;
      session.op0eFdCtr = ((session.op0eFdCtr ?? 0) + 1) & 0xffff;
      await waitForSpace("     Death Strikes!", sprite, 0x0e);
      const dest = session.savedTownId ?? 0;
      const entry = maps?.screens?.[dest]?.entry ?? [0, 0];
      onTeleport?.(dest, entry[0] ?? 0, entry[1] ?? 0);
      session.pendingEventLatch = 1;
      ctx.teleported = true;
      note?.("OP_0E 0xFD WAFE Esc → Death Strikes");
      onSessionChange?.(session);
      return;
    }
    if (passwordOk(typed)) {
      stage = 4;
      break;
    }
    await waitForSpace("Incorrect!", sprite, 0x0e);
    note?.("OP_0E 0xFD WAFE incorrect");
    onSessionChange?.(session);
    return;
  }

  await waitForSpace(loadFdPage(session, 4), sprite, 0x0e);
  await waitForSpace(loadFdPage(session, 5), sprite, 0x0e);
  await waitForSpace(loadFdPage(session, 6), sprite, 0x0e);
  session.scriptAbort = 2;
  note?.("OP_0E 0xFD print chrome complete → SCRIPT_ABORT=2 (Goto Town)");
  const dest = session.savedTownId ?? 0;
  const entry = maps?.screens?.[dest]?.entry ?? [0, 0];
  onTeleport?.(dest, entry[0] ?? 0, entry[1] ?? 0);
  session.pendingEventLatch = 1;
  ctx.teleported = true;
  onSessionChange?.(session);
}

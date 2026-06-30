/** In-game Quick Ref + character sheet — ports InGameCharacterSheet.cpp + play_layout.h. */
"use strict";

import { drawChar, drawGlyph } from "./ui.js";
import { itemNameFromManifest } from "./sessionState.js";
import { fillCellRect, drawPlayModalBackdrop } from "./playScreen.js";

const CLASS_NAMES = ["Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Robber", "Ninja", "Barbarian"];
const ALIGN_NAMES = ["Good", "Neut", "Evil"];
const RACE_NAMES = ["Human", "Elf", "Dwarf", "Gnome", "Half-Orc"];
const PACK_LETTERS = "ABCDEF";

const L = {
  overlayRow: 1,
  overlayCol: 1,
  overlayW: 38,
  overlayH: 23,
  quickHeader1: 0x01,
  quickHeader2: 0x0c,
  quickRow1Base: 0x03,
  quickRow2Base: 0x0e,
  quickColIndex: 0x01,
  quickColHpSlash: 0x14,
  quickColSpSlash: 0x20,
  quickColLvl: 0x08,
  quickColSL: 0x0a,
  quickColAge: 0x0e,
  quickColAC: 0x12,
  quickColGems: 0x18,
  quickColFood: 0x1c,
  quickColCond: 0x20,
  sheetHeaderCol: 0x02,
  sheetStatRowBase: 0x04,
  sheetStatColLeft: 0x02,
  sheetStatColMid: 0x09,
  sheetSlashCol: 0x12,
  sheetStatColCost: 0x18,
  sheetStatColRight: 0x1a,
  sheetDividerRow: 0x0c,
  sheetEquipRowBase: 0x0d,
  sheetEquipCol: 0x02,
  sheetBackpackCol: 0x14,
  sheetFooterRow1: 0x14,
  sheetFooterRow2: 0x15,
  sheetFooterCmdRow3: 0x16,
  sheetFooterCol: 0x02,
};

function drawCellText(ctx, row, col, text, r = 255, g = 255, b = 255) {
  let x = col * 8;
  const y = row * 8;
  for (let i = 0; i < text.length; i++) {
    drawChar(ctx, x, y, text[i], r, g, b);
    x += 8;
  }
}

function drawBorderIntegratedTextAt(ctx, row, col, text, r = 255, g = 255, b = 255) {
  fillCellRect(ctx, col, row, text.length, 1);
  drawCellText(ctx, row, col, text, r, g, b);
}

function drawBorderIntegratedText(ctx, row, borderCol, borderW, text, r = 255, g = 255, b = 255) {
  if (text.length <= 0 || text.length > borderW - 2) return;
  const textCol = borderCol + Math.floor((borderW - text.length) / 2);
  fillCellRect(ctx, textCol, row, text.length, 1);
  drawCellText(ctx, row, textCol, text, r, g, b);
}

function formatSlashStatCurrent(value, fieldWidth) {
  return String(value).padStart(fieldWidth, " ");
}

function drawSlashStat(ctx, row, labelCol, slashCol, label, current, max) {
  const labelLen = label.length;
  const fieldWidth = slashCol - (labelCol + labelLen);
  if (fieldWidth <= 0) return;
  drawCellText(ctx, row, labelCol, label);
  drawCellText(ctx, row, labelCol + labelLen, formatSlashStatCurrent(current, fieldWidth));
  drawCellText(ctx, row, slashCol, "/");
  drawCellText(ctx, row, slashCol + 1, String(max));
}

function drawQuickRefSlashPair(ctx, row, slashCol, current, max) {
  const valWidth = 5;
  drawCellText(ctx, row, slashCol - valWidth, formatSlashStatCurrent(current, valWidth));
  drawCellText(ctx, row, slashCol, "/");
  drawCellText(ctx, row, slashCol + 1, String(max));
}

function drawCellHorizRule(ctx, row, col, len, r = 200, g = 200, b = 200) {
  for (let i = 0; i < len; i++) {
    drawGlyph(ctx, (col + i) * 8, row * 8, "-".charCodeAt(0), r, g, b);
  }
}

function drawDoubleSectionHeader(ctx, row, col, widthCells, label1, label2, r = 200, g = 200, b = 200) {
  const l1 = label1.length;
  const l2 = label2.length;
  if (widthCells <= l1 + l2) {
    drawCellText(ctx, row, col, label1, r, g, b);
    return;
  }
  const pad = widthCells - l1 - l2;
  const leftRule = Math.floor(pad / 4);
  const rightRule = Math.floor(pad / 4);
  const midRule = pad - leftRule - rightRule;
  let cx = col;
  drawCellHorizRule(ctx, row, cx, leftRule, r, g, b);
  cx += leftRule;
  drawCellText(ctx, row, cx, label1, r, g, b);
  cx += l1;
  drawCellHorizRule(ctx, row, cx, midRule, r, g, b);
  cx += midRule;
  drawCellText(ctx, row, cx, label2, r, g, b);
  cx += l2;
  drawCellHorizRule(ctx, row, cx, rightRule, r, g, b);
}

function overlayBottomRow() {
  return L.overlayRow + L.overlayH - 1;
}

function drawModalEscFooter(ctx) {
  drawBorderIntegratedText(
    ctx,
    overlayBottomRow(),
    L.overlayCol,
    L.overlayW,
    "( 'ESC' to go back )",
    180,
    180,
    180
  );
}

function drawSheetEscFooter(ctx) {
  drawBorderIntegratedTextAt(
    ctx,
    overlayBottomRow(),
    L.sheetFooterCol,
    "( 'ESC' to go back )",
    180,
    180,
    180
  );
}

function className(id) {
  return CLASS_NAMES[id] ?? "?";
}

function alignHeaderName(id) {
  return ALIGN_NAMES[id] ?? "?";
}

function raceHeaderName(id) {
  return RACE_NAMES[id] ?? "?";
}

function conditionName(c) {
  if (c >= 0x80) return "Dead";
  switch (c) {
    case 0:
      return "Good";
    case 1:
      return "Cursed";
    case 2:
    case 3:
      return "Silenced";
    default:
      return c >= 4 ? "Poisoned" : "?";
  }
}

function itemLabel(manifest, itemId) {
  if (!itemId) return "";
  return itemNameFromManifest(manifest, itemId);
}

/** @param {CanvasRenderingContext2D} ctx @param {object} session @param {object|null} manifest */
export function renderQuickRef(ctx, session, manifest) {
  drawPlayModalBackdrop(ctx);

  drawCellText(
    ctx,
    L.quickHeader1,
    L.quickColIndex,
    "#     Name    Hit Points  Spell Points",
    255,
    255,
    128
  );
  drawCellText(
    ctx,
    L.quickHeader2,
    L.quickColIndex,
    "# Lvl SL AC Age Gems  Food Condition",
    255,
    255,
    128
  );

  const party = session.party ?? [];
  for (let i = 0; i < party.length && i < 8; i++) {
    const rec = party[i];
    const name = (rec.name ?? "").slice(0, 11).padEnd(11, " ");
    const row1 = L.quickRow1Base + i;
    drawCellText(ctx, row1, L.quickColIndex, `${i + 1})  ${name}`);
    drawQuickRefSlashPair(ctx, row1, L.quickColHpSlash, rec.hpCurrent ?? 0, rec.hpMax ?? 0);
    drawQuickRefSlashPair(ctx, row1, L.quickColSpSlash, rec.spCurrent ?? 0, rec.spMax ?? 0);

    const row2 = L.quickRow2Base + i;
    const gray = [200, 200, 200];
    drawCellText(ctx, row2, L.quickColIndex, String(i + 1), ...gray);
    drawCellText(ctx, row2, L.quickColLvl, String(rec.level ?? 0), ...gray);
    drawCellText(ctx, row2, L.quickColSL, String(rec.spellLevel ?? 0), ...gray);
    drawCellText(ctx, row2, L.quickColAC, String(rec.armorClass ?? 0), ...gray);
    drawCellText(ctx, row2, L.quickColAge, String(rec.age ?? 0), ...gray);
    drawCellText(ctx, row2, L.quickColGems, String(rec.gems ?? 0), ...gray);
    drawCellText(ctx, row2, L.quickColFood, String(rec.food ?? 0), ...gray);
    drawCellText(ctx, row2, L.quickColCond, conditionName(rec.condition ?? 0), ...gray);
  }

  drawModalEscFooter(ctx);
}

/** @param {CanvasRenderingContext2D} ctx @param {object} session @param {number} partySlot @param {object|null} manifest */
export function renderCharacterSheet(ctx, session, partySlot, manifest) {
  const party = session.party ?? [];
  const rec = party[partySlot];
  drawPlayModalBackdrop(ctx);

  if (!rec) {
    drawCellText(ctx, 2, 2, "No character selected.");
    drawSheetEscFooter(ctx);
    return;
  }

  const dispChar = String.fromCharCode("1".charCodeAt(0) + partySlot);
  const name = (rec.name ?? "").slice(0, 11);
  const header = `${dispChar}) ${name}: ${rec.sex ? "F" : "M"} ${alignHeaderName(rec.alignmentBase ?? 0)} ${raceHeaderName(rec.race ?? 0)} ${className(rec.classId ?? 0)}`;
  drawBorderIntegratedTextAt(ctx, L.overlayRow, L.sheetHeaderCol, header);

  const r0 = L.sheetStatRowBase;
  drawCellText(ctx, r0 + 0, L.sheetStatColLeft, `Lvl=${rec.level ?? 0}`);
  drawCellText(ctx, r0 + 1, L.sheetStatColLeft, `Mgt=${rec.might ?? 0}`);
  drawCellText(ctx, r0 + 2, L.sheetStatColLeft, `Int=${rec.intelligence ?? 0}`);
  drawCellText(ctx, r0 + 3, L.sheetStatColLeft, `Per=${rec.personality ?? 0}`);
  drawCellText(ctx, r0 + 4, L.sheetStatColLeft, `End=${rec.endurance ?? 0}`);
  drawCellText(ctx, r0 + 5, L.sheetStatColLeft, `Spd=${rec.speed ?? 0}`);
  drawCellText(ctx, r0 + 6, L.sheetStatColLeft, `Acy=${rec.accuracy ?? 0}`);
  drawCellText(ctx, r0 + 7, L.sheetStatColLeft, `Lck=${rec.luck ?? 0}`);

  drawSlashStat(ctx, r0 + 0, L.sheetStatColMid, L.sheetSlashCol, "HP=", rec.hpCurrent ?? 0, rec.hpMax ?? 0);
  drawSlashStat(ctx, r0 + 1, L.sheetStatColMid, L.sheetSlashCol, "SP=", rec.spCurrent ?? 0, rec.spMax ?? 0);
  drawCellText(ctx, r0 + 3, L.sheetStatColMid, `AC=${rec.armorClass ?? 0}`);
  drawCellText(ctx, r0 + 3, L.sheetSlashCol + 1, `SL=${rec.spellLevel ?? 0}`);

  drawCellText(ctx, r0 + 0, L.sheetStatColRight, `Age=${rec.age ?? 0}`);
  drawCellText(ctx, r0 + 1, L.sheetStatColRight, `Exp=${rec.experience ?? 0}`);
  drawCellText(ctx, r0 + 3, L.sheetStatColCost, `Gold=${rec.gold ?? 0}`);
  drawCellText(ctx, r0 + 4, L.sheetStatColCost, `Gems=${rec.gems ?? 0}`);
  drawCellText(ctx, r0 + 5, L.sheetStatColCost, `Food=${rec.food ?? 0}`);
  drawCellText(ctx, r0 + 6, L.sheetStatColRight, `Cond= ${conditionName(rec.condition ?? 0)}`);

  const dividerW = L.overlayCol + L.overlayW - 2 - L.sheetEquipCol + 1;
  drawDoubleSectionHeader(ctx, L.sheetDividerRow, L.sheetEquipCol, dividerW, " Equipped ", " Backpack ");

  const equipped = rec.equipped ?? [];
  const backpack = rec.backpack ?? [];
  for (let i = 0; i < 6; i++) {
    const row = L.sheetEquipRowBase + i;
    const eq = equipped[i];
    let eline;
    if (eq?.id) {
      const iname = itemLabel(manifest, eq.id).slice(0, 10).padEnd(10, " ");
      eline = `${i + 1}) ${iname}`;
    } else {
      eline = `${i + 1})`;
    }
    drawCellText(ctx, row, L.sheetEquipCol, eline, 220, 220, 220);

    const bp = backpack[i];
    if (bp?.id) {
      const iname = itemLabel(manifest, bp.id).slice(0, 10).padEnd(10, " ");
      eline = `${PACK_LETTERS[i]}) ${iname}`;
    } else {
      eline = `${PACK_LETTERS[i]})`;
    }
    drawCellText(ctx, row, L.sheetBackpackCol, eline, 220, 220, 220);
  }

  fillCellRect(ctx, L.sheetFooterCol, L.sheetFooterRow1 - 1, L.overlayW - 2, 4);
  drawCellText(
    ctx,
    L.sheetFooterRow1,
    L.sheetFooterCol,
    "'Q' Quick Ref  'C' Cast    'D' Drop",
    180,
    180,
    180
  );
  drawCellText(
    ctx,
    L.sheetFooterRow2,
    L.sheetFooterCol,
    "'E' Equip      'G' Gather  'R' Remove",
    180,
    180,
    180
  );
  drawCellText(
    ctx,
    L.sheetFooterCmdRow3,
    L.sheetFooterCol,
    "'S' Share  'T' Trade  'U' Use",
    180,
    180,
    180
  );
  drawSheetEscFooter(ctx);
}

/** In-game Quick Ref + character sheet — ports InGameCharacterSheet.cpp + play_layout.h. */
"use strict";

import { drawChar, drawGlyph } from "./ui.js";
import { itemNameFromManifest, memberSpellKnown } from "./sessionState.js";
import { fillCellRect, drawPlayModalBackdrop } from "./playScreen.js";
import { kSorcererSpells, kClericSpells } from "./combatSpells.js";

const CLASS_NAMES = ["Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Robber", "Ninja", "Barbarian"];
const ALIGN_NAMES = ["Good", "Neut", "Evil"];
const RACE_NAMES = ["Human", "Elf", "Dwarf", "Gnome", "Half-Orc"];
const PACK_LETTERS = "ABCDEF";
const SPELLS_PER_LEVEL = [7, 7, 6, 6, 5, 5, 4, 4, 4];
const SPELL_LEVELS = 9;

/** Sheet sub-modes — InGameCharacterSheet.h SheetSubMode. */
export const SheetSubMode = {
  Normal: "Normal",
  SpellBook: "SpellBook",
  CastPicker: "CastPicker",
  UsePick: "UsePick",
};

export const CastPromptPhase = {
  Level: "Level",
  Number: "Number",
};

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

/** SpellSchool for class — InGameCharacterSheet / SpellBook.h. */
export function spellSchoolForClass(classId) {
  const c = classId | 0;
  if (c === 2 || c === 4) return 0; /* sorc */
  if (c === 1 || c === 3) return 1; /* cler */
  return -1;
}

function schoolSpellTable(school) {
  if (school === 0) return kSorcererSpells;
  if (school === 1) return kClericSpells;
  return null;
}

function spellFlatFromLevelNumber(level, number) {
  if (level < 1 || level > SPELL_LEVELS || number < 1) return -1;
  const maxN = SPELLS_PER_LEVEL[level - 1];
  if (number > maxN) return -1;
  let base = 0;
  for (let i = 0; i < level - 1; i++) base += SPELLS_PER_LEVEL[i];
  return base + (number - 1);
}

/** @returns {object} fresh SheetSession */
export function createSheetSession(partySlot = 0) {
  return {
    partySlot: partySlot | 0,
    subMode: SheetSubMode.Normal,
    castPhase: CastPromptPhase.Level,
    castLevel: 0,
    castSpellFlat: -1,
    pendingUseSlot: -1,
    statusLine: "",
  };
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
  const gray = [200, 200, 200];
  for (let i = 0; i < party.length && i < 8; i++) {
    const rec = party[i];
    if (!rec) continue;
    const row1 = L.quickRow1Base + i;
    const name = (rec.name ?? "").slice(0, 11);
    drawCellText(ctx, row1, L.quickColIndex, `${i + 1})  ${name.padEnd(11)}`);
    drawQuickRefSlashPair(ctx, row1, L.quickColHpSlash, rec.hpCurrent ?? 0, rec.hpMax ?? 0);
    drawQuickRefSlashPair(ctx, row1, L.quickColSpSlash, rec.spCurrent ?? 0, rec.spMax ?? 0);

    const row2 = L.quickRow2Base + i;
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

function renderSpellBook(ctx, rec) {
  const school = spellSchoolForClass(rec.classId);
  if (school < 0) return;

  const kWinRow = 10;
  const kWinCol = 7;
  const kWinW = 29;
  const kWinH = 14;
  const kTitleRow = kWinRow + 1;
  const kHeaderRow = kWinRow + 3;
  const kGridRowBase = kWinRow + 4;
  const kLvlCol = kWinCol + 2;
  const kMarkColBase = kWinCol + 5;

  fillCellRect(ctx, kWinCol, kWinRow, kWinW, kWinH);
  /* Blue fill approx — playScreen fill is black; draw solid via cell chars. */
  for (let r = 0; r < kWinH; r++) {
    for (let c = 0; c < kWinW; c++) {
      /* leave backdrop; yellow border via glyphs */
    }
  }
  /* Yellow console box border */
  for (let c = 0; c < kWinW; c++) {
    drawGlyph(ctx, (kWinCol + c) * 8, kWinRow * 8, 0x2d, 255, 255, 0);
    drawGlyph(ctx, (kWinCol + c) * 8, (kWinRow + kWinH - 1) * 8, 0x2d, 255, 255, 0);
  }
  for (let r = 0; r < kWinH; r++) {
    drawGlyph(ctx, kWinCol * 8, (kWinRow + r) * 8, 0x7c, 255, 255, 0);
    drawGlyph(ctx, (kWinCol + kWinW - 1) * 8, (kWinRow + r) * 8, 0x7c, 255, 255, 0);
  }

  drawBorderIntegratedText(ctx, kTitleRow, kWinCol, kWinW, "Spell Book", 255, 255, 128);
  drawCellText(ctx, kHeaderRow, kLvlCol, "Lvl 1 2 3 4 5 6 7", 255, 255, 255);

  let flat = 0;
  for (let level = 1; level <= SPELL_LEVELS; level++) {
    const row = kGridRowBase + (level - 1);
    drawCellText(ctx, row, kLvlCol, String(level), 255, 255, 255);
    const slots = SPELLS_PER_LEVEL[level - 1];
    for (let slot = 0; slot < slots; slot++) {
      if (memberSpellKnown(rec, flat)) {
        const col = kMarkColBase + slot * 2;
        drawGlyph(ctx, col * 8, row * 8, 0x17, 255, 255, 255);
      }
      flat++;
    }
  }
}

function renderCastPicker(ctx, rec, sheetSession) {
  renderSpellBook(ctx, rec);
  const kPromptRow = 0x16;
  if (sheetSession.castPhase === CastPromptPhase.Level) {
    drawCellText(ctx, kPromptRow, 0x02, " Spell Level: ", 255, 255, 255);
  } else {
    drawCellText(ctx, kPromptRow, 0x02, ` Spell Level: ${sheetSession.castLevel}`, 255, 255, 255);
    drawCellText(ctx, kPromptRow, 0x0c, "Number: ", 255, 255, 255);
  }
}

/**
 * @param {CanvasRenderingContext2D} ctx
 * @param {object} session
 * @param {number} partySlot
 * @param {object|null} manifest
 * @param {{ combatMode?: boolean, sheetSession?: object }} [opts]
 */
export function renderCharacterSheet(ctx, session, partySlot, manifest, opts = {}) {
  const combatMode = !!opts.combatMode;
  const sheetSession = opts.sheetSession ?? null;
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
      const iname = itemLabel(manifest, eq.id);
      eline = `${i + 1}) ${iname}`;
    } else {
      eline = `${i + 1})`;
    }
    drawCellText(ctx, row, L.sheetEquipCol, eline, 220, 220, 220);

    const bp = backpack[i];
    if (bp?.id) {
      const iname = itemLabel(manifest, bp.id);
      eline = `${PACK_LETTERS[i]}) ${iname}`;
    } else {
      eline = `${PACK_LETTERS[i]})`;
    }
    drawCellText(ctx, row, L.sheetBackpackCol, eline, 220, 220, 220);
  }

  fillCellRect(ctx, L.sheetFooterCol, L.sheetFooterRow1 - 1, L.overlayW - 2, 4);
  /* InGameCharacterSheet.cpp:500 — combat footer is V-only; Cast/Use are exploration. */
  if (combatMode) {
    drawCellText(ctx, L.sheetFooterRow1, L.sheetFooterCol, "'V' View spell book", 180, 180, 180);
  } else {
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
  }

  if (sheetSession?.statusLine) {
    drawCellText(ctx, L.sheetFooterRow1 - 1, L.sheetFooterCol, sheetSession.statusLine, 255, 255, 128);
  }

  drawSheetEscFooter(ctx);

  if (sheetSession?.subMode === SheetSubMode.SpellBook) {
    renderSpellBook(ctx, rec);
  } else if (sheetSession?.subMode === SheetSubMode.CastPicker) {
    renderCastPicker(ctx, rec, sheetSession);
  }
}

/**
 * InGameCharacterSheet::handleKey — combat_mode matches C++ (V spellbook;
 * C/U Cast/Use are exploration-only and break in combat).
 * @returns {"none"|"close"|"cast"|"use"}
 */
export function handleSheetKey(key, sheetSession, session, combatMode) {
  const party = session.party ?? [];
  const rec = party[sheetSession.partySlot];
  if (!rec) return "close";

  const upper = String(key || "").toUpperCase();

  if (sheetSession.subMode === SheetSubMode.UsePick) {
    sheetSession.subMode = SheetSubMode.Normal;
    if (upper >= "1" && upper <= "6") {
      sheetSession.pendingUseSlot = upper.charCodeAt(0) - 0x31;
      sheetSession.statusLine = "Using...";
      return "use";
    }
    if (upper >= "A" && upper <= "F") {
      sheetSession.pendingUseSlot = 6 + (upper.charCodeAt(0) - 0x41);
      sheetSession.statusLine = "Using...";
      return "use";
    }
    sheetSession.pendingUseSlot = -1;
    sheetSession.statusLine = "";
    return "none";
  }

  if (sheetSession.subMode === SheetSubMode.SpellBook) {
    return "none";
  }

  if (sheetSession.subMode === SheetSubMode.CastPicker) {
    if (upper >= "1" && upper <= "9") {
      const digit = upper.charCodeAt(0) - 0x30;
      if (sheetSession.castPhase === CastPromptPhase.Level) {
        const maxSl = rec.spellLevel | 0;
        if (digit < 1 || digit > maxSl || digit > SPELL_LEVELS) return "none";
        sheetSession.castLevel = digit;
        sheetSession.castPhase = CastPromptPhase.Number;
        return "none";
      }
      const flat = spellFlatFromLevelNumber(sheetSession.castLevel, digit);
      if (flat < 0 || !memberSpellKnown(rec, flat)) return "none";
      sheetSession.castSpellFlat = flat;
      sheetSession.subMode = SheetSubMode.Normal;
      sheetSession.castPhase = CastPromptPhase.Level;
      const school = spellSchoolForClass(rec.classId);
      const table = schoolSpellTable(school);
      sheetSession.statusLine = table?.[flat]?.name ? `Cast ${table[flat].name}.` : "Cast.";
      return "cast";
    }
    return "none";
  }

  switch (upper) {
    case "V":
      if (combatMode) {
        const school = spellSchoolForClass(rec.classId);
        if (school < 0) sheetSession.statusLine = "No spell book.";
        else {
          sheetSession.subMode = SheetSubMode.SpellBook;
          sheetSession.statusLine = "";
        }
      }
      break;
    case "C": {
      /* Combat mode: break — InGameCharacterSheet.cpp:1059. Combat 'C' is CombatSession. */
      if (combatMode) break;
      const school = spellSchoolForClass(rec.classId);
      if (school < 0 || !(rec.spellLevel | 0)) {
        sheetSession.statusLine = "No spell book.";
        break;
      }
      sheetSession.subMode = SheetSubMode.CastPicker;
      sheetSession.castPhase = CastPromptPhase.Level;
      sheetSession.castLevel = 0;
      sheetSession.castSpellFlat = -1;
      sheetSession.statusLine = "";
      break;
    }
    case "U":
      /* Combat mode: break — InGameCharacterSheet.cpp:1119. */
      if (combatMode) break;
      sheetSession.subMode = SheetSubMode.UsePick;
      sheetSession.pendingUseSlot = -1;
      sheetSession.statusLine = "Use which? (1-6/A-F)";
      break;
    default:
      break;
  }
  return "none";
}

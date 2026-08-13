/** Play-screen chrome — ports game/src/gfx/PlayScreenChrome.cpp (ASM @ 0x60F4). */
"use strict";

import { drawGlyph, drawChar } from "./ui.js";

export const SCREEN_W = 320;
export const SCREEN_H = 200;

const BR = 255;
const BG = 0;
const BB = 0;

const GL = {
  TL: 0x01,
  TR: 0x02,
  BL: 0x03,
  BR: 0x04,
  HSEG: 0x05,
  HCAPL: 0x06,
  HCAPR: 0x07,
  VCAPB: 0x08,
  VCAPT: 0x09,
  VSEG: 0x0b,
};

function glyphAt(ctx, col, row, glyph) {
  drawGlyph(ctx, col * 8, row * 8, glyph, BR, BG, BB);
}

export function fillCellRect(ctx, col, row, w, h) {
  ctx.fillStyle = "#000";
  ctx.fillRect(col * 8, row * 8, w * 8, h * 8);
}

/** Modal overlay frame — drawPlayModalBackdrop @ PlayScreenChrome.cpp */
export function drawPlayModalBackdrop(ctx) {
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
  consoleBox(ctx, 1, 1, 38, 23);
  fillCellRect(ctx, 2, 2, 36, 21);
}

function consoleBox(ctx, col, row, w, h) {
  const col1 = col + w - 1;
  const row1 = row + h - 1;
  glyphAt(ctx, col, row, GL.TL);
  glyphAt(ctx, col1, row, GL.TR);
  glyphAt(ctx, col, row1, GL.BL);
  glyphAt(ctx, col1, row1, GL.BR);
  for (let c = col + 1; c < col1; c++) {
    glyphAt(ctx, c, row, GL.HSEG);
    glyphAt(ctx, c, row1, GL.HSEG);
  }
  for (let r = row + 1; r < row1; r++) {
    glyphAt(ctx, col, r, GL.VSEG);
    glyphAt(ctx, col1, r, GL.VSEG);
  }
}

export const PARTY_NAME_MAX = 11;
const PARTY_PREFIX_LEN = 4; /* " N) " @ 0x6150 */
const PARTY_SLOT_ROW_BASE = 0x13;
const PARTY_SLOT_COL_LEFT = 0x01;
const PARTY_SLOT_COL_RIGHT = 0x14;
const PARTY_SLOT_CLEAR_WIDTH = 0x13;

/** @returns {string} format_party_status_line @ PartyStatusFormat.cpp / 0x6150 */
export function formatPartyStatusLine(slotIndex, name, hp) {
  const slotDigit = slotIndex >= 0 && slotIndex < 8 ? slotIndex + 1 : 1;
  /* win_print(name) until NUL — no 11-pad; '/' and HP shift with name length. */
  const nameField = (name ?? "").slice(0, PARTY_NAME_MAX);
  /* 0x6266..0x629A: left-aligned 3-cell field (trailing spaces), not padStart. */
  const hpField = hp > 999 ? "+++" : String(hp).padEnd(3, " ");
  return ` ${slotDigit}) ${nameField} /${hpField}`;
}

/** @deprecated use PARTY_NAME_MAX — kept for callers that still import the old name */
export const PARTY_NAME_WIDTH = PARTY_NAME_MAX;

/** @param {object} session */
export function buildPlayPartySlots(session) {
  const slots = Array.from({ length: 8 }, () => ({ present: false }));
  const party = session?.party ?? [];
  for (let i = 0; i < party.length && i < 8; i++) {
    const m = party[i];
    slots[i] = {
      present: true,
      name: m.name ?? "",
      /* draw_party_status_panel @ 0x624C: roster +$5E (hp_max), not +$74. */
      hp: m.hpMax ?? 0,
      badCondition: (m.condition ?? 0) !== 0,
    };
  }
  return slots;
}

function drawCellText(ctx, col, row, text, r = 255, g = 255, b = 255) {
  let x = col * 8;
  const y = row * 8;
  for (let i = 0; i < text.length; i++) {
    drawChar(ctx, x, y, text[i], r, g, b);
    x += 8;
  }
}

function hLine(ctx, col0, col1, row) {
  glyphAt(ctx, col0, row, GL.HCAPL);
  for (let col = col0 + 1; col < col1; col++) glyphAt(ctx, col, row, GL.HSEG);
  glyphAt(ctx, col1, row, GL.HCAPR);
}

function vLine(ctx, row0, row1, col) {
  glyphAt(ctx, col, row0, GL.VCAPT);
  for (let row = row0 + 1; row < row1; row++) glyphAt(ctx, col, row, GL.VSEG);
  glyphAt(ctx, col, row1, GL.VCAPB);
}

function outerFrame(ctx) {
  const col1 = 39;
  const row1 = 23;
  glyphAt(ctx, 0, 0, GL.TL);
  glyphAt(ctx, col1, 0, GL.TR);
  glyphAt(ctx, 0, row1, GL.BL);
  glyphAt(ctx, col1, row1, GL.BR);
  for (let col = 1; col < col1; col++) {
    glyphAt(ctx, col, 0, GL.HSEG);
    glyphAt(ctx, col, row1, GL.HSEG);
  }
  for (let row = 1; row < row1; row++) {
    glyphAt(ctx, 0, row, GL.VSEG);
    glyphAt(ctx, col1, row, GL.VSEG);
  }
}

function playScreenInteriorFills(ctx) {
  fillCellRect(ctx, 1, 1, 26, 15);
  fillCellRect(ctx, 28, 1, 10, 15);
  fillCellRect(ctx, 1, 0x11, 38, 6);
}

export function drawPlayScreenChrome(ctx) {
  playScreenInteriorFills(ctx);
  outerFrame(ctx);
  hLine(ctx, 0, 0x27, 0x10);
  hLine(ctx, 0, 0x27, 0x12);
  vLine(ctx, 0, 0x10, 0x1b);
  vLine(ctx, 0x10, 0x12, 0x0c);
  vLine(ctx, 0x10, 0x12, 0x15);
  vLine(ctx, 0x10, 0x12, 0x1f);
}

export function drawPlayViewportDivider(ctx) {
  vLine(ctx, 0, 0x10, 0x1b);
}

export function drawPlayStatusBar(ctx, day, year, facingKey, newGame) {
  const row = 0x11;
  drawString(ctx, newGame ? "'O' Options" : "'P' Protect", 1 * 8, row * 8);
  drawString(ctx, `Day=${day}`, 0x0d * 8, row * 8);
  drawString(ctx, `Year=${year}`, 0x16 * 8, row * 8);
  drawString(ctx, `Face=${facingKey}`, 0x20 * 8, row * 8);
}

function drawString(ctx, text, px, py) {
  let x = px;
  for (let i = 0; i < text.length; i++) {
    drawChar(ctx, x, py, text[i]);
    x += 8;
  }
}

export function drawPlayPartyPanel(ctx, slots) {
  for (let i = 0; i < 8; i++) {
    const row = PARTY_SLOT_ROW_BASE + Math.floor(i / 2);
    const col = i & 1 ? PARTY_SLOT_COL_RIGHT : PARTY_SLOT_COL_LEFT;
    const s = slots[i];
    /* Empty only: -$7F62 @ 0x6178. Occupied overwrite in place (clear width
     * 0x13 from col 1 would erase left slot's last HP cell at col 0x14). */
    if (!s?.present) {
      fillCellRect(ctx, col, row, PARTY_SLOT_CLEAR_WIDTH, 1);
      continue;
    }

    const line = formatPartyStatusLine(i, s.name, s.hp);
    const prefix = line.slice(0, PARTY_PREFIX_LEN);
    const slash = line.indexOf(" /", PARTY_PREFIX_LEN);
    const nameField = slash >= 0 ? line.slice(PARTY_PREFIX_LEN, slash) : line.slice(PARTY_PREFIX_LEN);
    const tail = slash >= 0 ? line.slice(slash) : "";

    drawCellText(ctx, col, row, prefix);
    if (s.badCondition) {
      drawCellText(ctx, col + PARTY_PREFIX_LEN, row, nameField, 255, 80, 80);
    } else {
      drawCellText(ctx, col + PARTY_PREFIX_LEN, row, nameField);
    }
    drawCellText(ctx, col + PARTY_PREFIX_LEN + nameField.length, row, tail);
  }
}

const COMMAND_REF = [
  "  OPTIONS  ",
  "\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05",
  "\x18 Forward  ",
  "\x19 Move Back",
  "\x1a Turn Left",
  "\x1b Turn Rght",
  "B Bash Door",
  "C Controls ",
  "D Dismiss  ",
  "E Exchange ",
  "Q Quick Ref",
  "R Rest     ",
  "S Search   ",
  "U Unlock   ",
  "# View Char",
];

export function drawPlayRightColumn(ctx, panel) {
  if (panel === "protect") {
    hLine(ctx, 0x1b, 0x27, 0x09);
    drawString(ctx, "Light     )", 0x1c * 8, 0x0a * 8);
    drawString(ctx, "Magic     %", 0x1c * 8, 0x0b * 8);
    drawString(ctx, "Forces    %", 0x1c * 8, 0x0c * 8);
    return;
  }
  for (let i = 0; i < COMMAND_REF.length; i++) {
    let x = 0x1c * 8;
    const y = (1 + i) * 8;
    for (let j = 0; j < COMMAND_REF[i].length; j++) {
      const code = COMMAND_REF[i].charCodeAt(j);
      if (code >= 32) drawGlyph(ctx, x, y, code, 255, 255, 255);
      x += 8;
    }
  }
}

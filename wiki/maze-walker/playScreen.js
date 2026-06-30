/** Play-screen chrome — ports game/src/gfx/PlayScreenChrome.cpp (ASM @ 0x60F4). */
"use strict";

import { drawGlyph, drawChar } from "./ui.js?v=20260630-slower-anims-2";

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

function fillCellRect(ctx, col, row, w, h) {
  ctx.fillStyle = "#000";
  ctx.fillRect(col * 8, row * 8, w * 8, h * 8);
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
    const row = 0x13 + Math.floor(i / 2);
    const col = i & 1 ? 0x14 : 0x01;
    fillCellRect(ctx, col, row, 0x13, 1);
    const s = slots[i];
    if (!s?.present) continue;
    drawString(ctx, s.line || s.name || "", col * 8, row * 8);
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

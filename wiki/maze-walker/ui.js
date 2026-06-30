/** MM2 8×8 font UI — ports game/src/gfx/ScreenCompositor.cpp + EventTextView.cpp. */
"use strict";

import { formatEncounterFlowLine } from "./eventVm.js";

/** @type {number[][] | null} */
let font = null;

export function initFont(fontData) {
  font = fontData;
}

function glyphRows(code) {
  if (!font || code < 0 || code >= font.length) return null;
  return font[code];
}

export function drawGlyph(ctx, px, py, code, r = 255, g = 255, b = 255) {
  const rows = glyphRows(code);
  if (!rows) return;
  ctx.fillStyle = `rgb(${r | 0},${g | 0},${b | 0})`;
  for (let row = 0; row < 8; row++) {
    const bits = rows[row] & 0xff;
    for (let col = 0; col < 8; col++) {
      if ((bits >> col) & 1) ctx.fillRect(px + col, py + row, 1, 1);
    }
  }
}

export function drawChar(ctx, px, py, ch, r = 255, g = 255, b = 255) {
  const code = ch.charCodeAt(0);
  if (code < 32 || code > 127) return;
  drawGlyph(ctx, px, py, code, r, g, b);
}

export function drawString(ctx, text, px, py, maxW = Infinity) {
  let x = px;
  let y = py;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch === "\n") {
      x = px;
      y += 8;
      continue;
    }
    if (x + 8 > px + maxW) break;
    drawChar(ctx, x, y, ch);
    x += 8;
  }
}

/** Red console box (modal popups). */
export function drawBox(ctx, x, y, w, h) {
  ctx.fillStyle = "#000";
  ctx.fillRect(x, y, w, h);
  const br = 255;
  const bg = 0;
  const bb = 0;
  const cols = Math.floor(w / 8);
  const rows = Math.floor(h / 8);
  const col0 = Math.floor(x / 8);
  const row0 = Math.floor(y / 8);
  drawConsoleBox(ctx, row0, col0, cols, rows, br, bg, bb);
}

function drawConsoleBox(ctx, row, col, wCells, hCells, r, g, b) {
  if (wCells < 2 || hCells < 2) return;
  const GL = {
    TL: 0x10,
    TR: 0x11,
    BL: 0x12,
    BR: 0x13,
    TOP: 0x0e,
    BOT: 0x0f,
    LEFT: 0x14,
    RIGHT: 0x15,
  };
  const x0 = col * 8;
  const y0 = row * 8;
  drawGlyph(ctx, x0, y0, GL.TL, r, g, b);
  for (let c = 1; c < wCells - 1; c++) drawGlyph(ctx, x0 + c * 8, y0, GL.TOP, r, g, b);
  drawGlyph(ctx, x0 + (wCells - 1) * 8, y0, GL.TR, r, g, b);
  for (let rr = 1; rr < hCells - 1; rr++) {
    const yy = y0 + rr * 8;
    drawGlyph(ctx, x0, yy, GL.LEFT, r, g, b);
    drawGlyph(ctx, x0 + (wCells - 1) * 8, yy, GL.RIGHT, r, g, b);
  }
  const yBot = y0 + (hCells - 1) * 8;
  drawGlyph(ctx, x0, yBot, GL.BL, r, g, b);
  for (let c = 1; c < wCells - 1; c++) drawGlyph(ctx, x0 + c * 8, yBot, GL.BOT, r, g, b);
  drawGlyph(ctx, x0 + (wCells - 1) * 8, yBot, GL.BR, r, g, b);
}

function glyphAt(ctx, col, row, glyph, r, g, b) {
  drawGlyph(ctx, col * 8, row * 8, glyph, r, g, b);
}

function visibleLen(s, start = 0) {
  let n = 0;
  while (start + n < s.length && s[start + n] !== "\n") n++;
  return n;
}

function centeredCol(startCol, endCol, text, lineStart) {
  const width = endCol - startCol + 1;
  const len = visibleLen(text, lineStart);
  let col = startCol + Math.floor((width - len) / 2);
  return Math.max(startCol, col);
}

/** Cell-addressed text draw — mirrors EventTextView textAt / ScreenCompositor::drawText. */
function textAtCell(ctx, col, row, text, r = 255, g = 255, b = 255) {
  let c = col;
  let r0 = row;
  for (let i = 0; i < text.length; i++) {
    if (text[i] === "\n") {
      c = col;
      r0++;
      continue;
    }
    drawChar(ctx, c * 8, r0 * 8, text[i], r, g, b);
    c++;
  }
}

function printWrapped(ctx, startCol, startRow, endCol, endRow, text, center = false, r = 255, g = 255, b = 255) {
  let col = startCol;
  let row = startRow;
  if (center && text.length) col = centeredCol(startCol, endCol, text, 0);

  for (let i = 0; i < text.length; i++) {
    if (text[i] === "\n") {
      row++;
      if (row > endRow) break;
      col = center && text[i + 1] ? centeredCol(startCol, endCol, text, i + 1) : startCol;
      continue;
    }
    drawChar(ctx, col * 8, row * 8, text[i], r, g, b);
    col++;
    if (col > endCol) {
      col = startCol;
      row++;
      if (row > endRow) break;
    }
  }
}

function fillSignInteriorRow(ctx, row) {
  ctx.fillStyle = "#000";
  ctx.fillRect(8 * 8, row * 8, 11 * 8, 8);
}

/** OP_06 outdoor signpost @ 0x15AEE — gold frame + post, white centered text. */
export function drawSignpost(ctx, text) {
  const br = 255;
  const bg = 204;
  const bb = 0;
  const GL = {
    TOP: 0x0e,
    BOT: 0x0f,
    TL: 0x10,
    TR: 0x11,
    BL: 0x12,
    BR: 0x13,
    LEFT: 0x14,
    RIGHT: 0x15,
    SOLID: 0x7e,
  };

  for (let col = 8; col <= 18; col++) glyphAt(ctx, col, 7, GL.TOP, br, bg, bb);
  glyphAt(ctx, 7, 7, GL.TL, br, bg, bb);
  glyphAt(ctx, 19, 7, GL.TR, br, bg, bb);

  for (let row = 8; row <= 9; row++) {
    glyphAt(ctx, 7, row, GL.LEFT, br, bg, bb);
    fillSignInteriorRow(ctx, row);
    glyphAt(ctx, 19, row, GL.RIGHT, br, bg, bb);
  }

  for (let col = 8; col <= 18; col++) glyphAt(ctx, col, 10, GL.BOT, br, bg, bb);
  glyphAt(ctx, 7, 10, GL.BL, br, bg, bb);
  glyphAt(ctx, 19, 10, GL.BR, br, bg, bb);

  for (let row = 11; row <= 13; row++) {
    glyphAt(ctx, 12, row, GL.RIGHT, br, bg, bb);
    glyphAt(ctx, 13, row, GL.SOLID, br, bg, bb);
    glyphAt(ctx, 14, row, GL.LEFT, br, bg, bb);
  }
  glyphAt(ctx, 12, 14, GL.BR, br, bg, bb);
  glyphAt(ctx, 14, 14, GL.BL, br, bg, bb);
  glyphAt(ctx, 13, 14, GL.SOLID, br, bg, bb);

  const rewritten = text.replace(/-/g, "{");
  printWrapped(ctx, 8, 8, 18, 9, rewritten, true);
}

/**
 * Normalize event.dat string bytes for display — mirrors EventTextView::copyResolvedText
 * (@ → newline @ 0x158C8) plus doc/export " / " breaks when strings were not pre-decoded.
 */
export function resolveEventText(raw) {
  if (!raw) return "";
  return String(raw)
    .replace(/\r\n/g, "\n")
    .replace(/@/g, "\n")
    .replace(/ \/ /g, "\n");
}

function countPopupLines(text) {
  let lines = 1;
  let col = 0;
  for (let i = 0; i < text.length; i++) {
    if (text[i] === "\n") {
      lines++;
      col = 0;
      continue;
    }
    col++;
    if (col >= 20) {
      lines++;
      col = 0;
    }
  }
  return lines;
}

/** OP_04 @ 0x159F4 — centered door nameplate, row 3 (cursor((28-len)/2,3), win_print). */
export function drawDoorLabel(ctx, text) {
  const len = visibleLen(text);
  const col = Math.floor((28 - len) / 2);
  textAtCell(ctx, col, 3, text);
}

/** OP_05 @ 0x15A46 — borderless wall inscription (popup A). */
export function drawWallText(ctx, text) {
  const lines = countPopupLines(text);
  let row = lines / 2 < 5 ? 4 - Math.floor(lines / 2) : 0;
  row = 3 + row;
  printWrapped(ctx, 4, row, 24, 13, text, true);
}

/** @typedef {{ type: 'door'|'wall'|'signpost', text: string } | null} ViewportOverlay */

export function drawViewportOverlay(ctx, overlay) {
  if (!overlay) return;
  switch (overlay.type) {
    case "door":
      drawDoorLabel(ctx, overlay.text);
      break;
    case "wall":
      drawWallText(ctx, overlay.text);
      break;
    case "signpost":
      drawSignpost(ctx, overlay.text);
      break;
    default:
      break;
  }
}

/** True when an exported event script uses OP_05 (popup A wall text). */
export function eventUsesWallText(evtData) {
  if (!evtData?.nodes?.length) return false;
  return evtData.nodes.some((n) => n.op === 0x05);
}

/** Collect tile positions (y<<4|x) that host wall-text events on this screen. */
export function wallTextTilePositions(sc) {
  const out = new Set();
  if (!sc?.events) return out;
  for (const evtData of Object.values(sc.events)) {
    if (!eventUsesWallText(evtData)) continue;
    for (const trig of evtData.triggers) out.add(trig.pos);
  }
  return out;
}

/** Minimap icon: small white inscription lines (wall text). */
export function drawMinimapWallText(ctx, cx, cy, tw, th) {
  ctx.fillStyle = "rgba(255,255,255,0.85)";
  const lx = cx + Math.floor(tw * 0.2);
  const ly = cy + Math.floor(th * 0.25);
  const lw = Math.max(4, Math.floor(tw * 0.6));
  ctx.fillRect(lx, ly, lw, 1);
  ctx.fillRect(lx, ly + 3, lw, 1);
  ctx.fillRect(lx, ly + 6, Math.floor(lw * 0.7), 1);
}

/** True when an exported event script begins with OP_06 (Popup B signpost). */
export function eventUsesSignpost(evtData) {
  if (!evtData?.nodes?.length) return false;
  return evtData.nodes.some((n) => n.op === 0x06);
}

/** Collect tile positions (y<<4|x) that host signpost events on this screen. */
export function signpostTilePositions(sc) {
  const out = new Set();
  if (!sc?.events) return out;
  for (const evtData of Object.values(sc.events)) {
    if (!eventUsesSignpost(evtData)) continue;
    for (const trig of evtData.triggers) out.add(trig.pos);
  }
  return out;
}

/** Minimap icon: tiny gold sign-board + post. */
export function drawMinimapSignpost(ctx, cx, cy, tw, th) {
  const bx = cx + Math.floor(tw * 0.15);
  const by = cy + Math.floor(th * 0.1);
  const bw = Math.max(4, Math.floor(tw * 0.7));
  const bh = Math.max(3, Math.floor(th * 0.35));
  ctx.fillStyle = "rgb(255,204,0)";
  ctx.fillRect(bx, by, bw, bh);
  ctx.fillRect(cx + Math.floor(tw / 2) - 1, by + bh, 2, Math.max(3, Math.floor(th * 0.45)));
}

/** Tile indices (y<<4|x) that have an event.dat trigger on this screen. */
export function scriptedEventPositions(sc) {
  const out = new Set();
  if (!sc?.events) return out;
  for (const evtData of Object.values(sc.events)) {
    for (const trig of evtData.triggers) out.add(trig.pos);
  }
  return out;
}

/** Walkable floor: at least one edge is not a solid wall (map.dat page 1). */
export function tileWalkable(collisionByte) {
  if (((collisionByte >> 0) & 3) !== 1) return true;
  if (((collisionByte >> 2) & 3) !== 1) return true;
  if (((collisionByte >> 4) & 3) !== 1) return true;
  if ((collisionByte & 0x40) === 0) return true;
  return false;
}

/**
 * Per-screen step random encounters (attrib +0x09 roll @ asm 0x10A2).
 * Town interiors keep a rate byte but the cavern/dungeon/castle paths gate the roll.
 */
export function screenHasAmbientSteps(sc) {
  if (!sc || sc.outdoor) return false;
  const rate = sc.encounter?.stepRate ?? 0;
  if (rate <= 0) return false;
  const env = sc.env || "";
  return env === "cavern" || env === "dungeon" || env === "castle" || env === "other";
}

/** Collision 0x80 with no event.dat script — map-flagged ambient encounter tiles. */
export function collisionAmbientPositions(sc) {
  const scripted = scriptedEventPositions(sc);
  const out = new Set();
  if (!sc?.collision) return out;
  for (let idx = 0; idx < sc.collision.length; idx++) {
    if ((sc.collision[idx] & 0x80) && !scripted.has(idx)) out.add(idx);
  }
  return out;
}

/** Walkable tile eligible for ambient step roll (no scripted trigger on tile). */
export function isAmbientStepTile(sc, idx) {
  if (!screenHasAmbientSteps(sc)) return false;
  if (!sc.collision || idx < 0 || idx >= sc.collision.length) return false;
  if (!tileWalkable(sc.collision[idx])) return false;
  return !scriptedEventPositions(sc).has(idx);
}

/** Sidebar / status line for ambient step encounters. */
export function formatAmbientStepMessage(sc) {
  const rate = sc?.encounter?.stepRate ?? 0;
  if (rate <= 0) return null;
  return `Ambient random encounter (1-in-${rate} step)`;
}

/** @typedef {'sign'|'wall'|'door'|'encFixed'|'encRandom'} MinimapMarkerKind */

function pseudoLineSummary(line) {
  if (!line) return null;
  const stripped = line.replace(/^\s*\d+:\s*/, "").trim();
  if (!stripped || stripped.startsWith("if ") || stripped.startsWith("# skip")) return null;
  if (stripped.startsWith("text_record:")) return "Text record";
  const fn = stripped.match(/^([a-z_][a-z0-9_]*)\s*\(/i);
  if (fn) return fn[1].replace(/_/g, " ");
  return stripped.length > 48 ? `${stripped.slice(0, 45)}…` : stripped;
}

function execSelectorLabel(pseudo) {
  const m = pseudo?.match(/exec_selector\(0x[0-9A-Fa-f]+\)\s*(?:#\s*(.+))?/);
  if (m?.[1]) return m[1].trim().replace(/_/g, " ");
  return "Town service";
}

function serviceSignLabel(pseudo) {
  const m = pseudo?.match(/service_sign\([^)]*\)/);
  if (m) return "Shop sign";
  return "Sign";
}

/** Numbered decompiled flow for the events sidebar (all nodes in script order). */
export function formatEventScriptFlow(evtData, manifest = null, vmTrace = null) {
  if (!evtData) return "No events.";
  const lines = [];
  const nodes = evtData.nodes;
  const traceByNode = new Map();
  if (vmTrace?.length) {
    for (const t of vmTrace) traceByNode.set(t.nodeIndex, t);
  }
  if (nodes?.length) {
    for (let i = 0; i < nodes.length; i++) {
      const n = nodes[i];
      const num = String(i).padStart(2, "0");
      let line;
      if (n.op === -1) {
        const preview = String(n.text || "").replace(/\n/g, " / ").slice(0, 72);
        line = `${num}: text_record: "${preview}"`;
      } else if ((n.op === 0x12 || n.op === 0x13) && manifest) {
        line = `${num}: ${formatEncounterFlowLine(manifest, n)}`;
      } else {
        line = `${num}: ${n.pseudo || `OP_${n.op.toString(16).toUpperCase()}`}`;
      }
      const tr = traceByNode.get(i);
      if (tr) {
        const branch =
          tr.detail && (tr.detail.includes("skip") || tr.detail.includes("cond"))
            ? ` ← ${tr.detail}`
            : tr.detail
              ? ` ← ${tr.detail}`
              : "";
        line = `▶ ${line}${branch} [cond=${tr.cond}]`;
      }
      lines.push(line);
    }
    return lines.join("\n");
  }
  const script = evtData.script || [];
  for (let i = 0; i < script.length; i++) {
    lines.push(`${String(i).padStart(2, "0")}: ${script[i]}`);
  }
  return lines.length ? lines.join("\n") : "(empty script)";
}

/** First user-visible opcode label for sidebar (never OP_12/13 — use encounterLabelForEvent). */
const SIDEBAR_OP_PRIORITY = [0x06, 0x05, 0x04, 0x0e, 0x0b, 0x01, 0x02, 0x03, 0x0c, 0x2a];

function opcodeDisplayLabel(n) {
  switch (n.op) {
    case 0x04:
      return "Door text";
    case 0x05:
      return "Wall text";
    case 0x06:
      return "Signpost";
    case 0x01:
      return "Show text";
    case 0x02:
      return "Text block";
    case 0x03:
      return "Show text";
    case 0x0E:
      return execSelectorLabel(n.pseudo);
    case 0x0B:
      return serviceSignLabel(n.pseudo);
    case 0x0C:
      return "Map transition";
    case 0x2A:
      return "Treasure";
    default:
      return null;
  }
}

/**
 * Human-readable event type for sidebar / status (non-encounter scripts).
 * @param {object} evtData
 * @returns {string | null}
 */
export function primaryEventLabelForEvent(evtData) {
  if (!evtData?.nodes?.length) {
    return pseudoLineSummary(evtData?.script?.[0]);
  }
  let bestLabel = null;
  let bestPri = SIDEBAR_OP_PRIORITY.length;
  for (const n of evtData.nodes) {
    if (n.op === 0x12 || n.op === 0x13) continue;
    const pri = SIDEBAR_OP_PRIORITY.indexOf(n.op);
    if (pri < 0) continue;
    const label = opcodeDisplayLabel(n);
    if (label && pri < bestPri) {
      bestPri = pri;
      bestLabel = label;
    }
  }
  return bestLabel ?? pseudoLineSummary(evtData.script?.[0]);
}

/** Sidebar / status title — encounter only for OP_12/OP_13, else decompile label. */
export function eventTitleForEvent(evtData) {
  return encounterLabelForEvent(evtData) ?? primaryEventLabelForEvent(evtData) ?? "Event script";
}

/** True when an event script uses OP_12 (fixed) or OP_13 (seeded-random) encounter setup. */
export function eventUsesEncounter(evtData) {
  if (!evtData?.nodes?.length) return false;
  return evtData.nodes.some((n) => n.op === 0x12 || n.op === 0x13);
}

/** @typedef {'fixed'|'random'} EncounterKind */

/**
 * Label for sidebar / status — OP_13 = seeded-random, OP_12 = fixed monster list.
 * @param {object} evtData
 * @returns {EncounterKind | null}
 */
export function encounterKindForEvent(evtData) {
  if (!evtData?.nodes?.length) return null;
  if (evtData.nodes.some((n) => n.op === 0x13)) return "random";
  if (evtData.nodes.some((n) => n.op === 0x12)) return "fixed";
  return null;
}

/** Human-readable encounter type for UI lists. */
export function encounterLabelForEvent(evtData) {
  const kind = encounterKindForEvent(evtData);
  if (kind === "random") return "Random encounter";
  if (kind === "fixed") return "Fixed encounter";
  return null;
}

/** Marker kind for one event script (ignores control-flow-only ops). */
function eventMinimapKind(evtData) {
  if (!evtData?.nodes?.length) return null;
  for (const n of evtData.nodes) {
    if (n.op === 0x06) return "sign";
    if (n.op === 0x05) return "wall";
    if (n.op === 0x04) return "door";
  }
  const enc = encounterKindForEvent(evtData);
  if (enc === "random") return "encRandom";
  if (enc === "fixed") return "encFixed";
  return null;
}

const MINIMAP_KIND_PRIORITY = {
  sign: 0,
  wall: 1,
  door: 2,
  encFixed: 3,
  encRandom: 3,
};

/**
 * Per-tile minimap marker with priority: signpost > wall > door > encounter > none.
 * Encounter icons only when the tile's events include OP_12/OP_13 and no higher-priority UI op.
 * @returns {Map<number, MinimapMarkerKind>}
 */
export function buildMinimapMarkers(sc) {
  /** @type {Map<number, MinimapMarkerKind>} */
  const out = new Map();
  if (!sc?.events) return out;
  for (const evtData of Object.values(sc.events)) {
    const kind = eventMinimapKind(evtData);
    if (!kind) continue;
    for (const trig of evtData.triggers) {
      const prev = out.get(trig.pos);
      if (prev === undefined || MINIMAP_KIND_PRIORITY[kind] < MINIMAP_KIND_PRIORITY[prev]) {
        out.set(trig.pos, kind);
      } else if (
        kind.startsWith("enc") &&
        prev.startsWith("enc") &&
        kind === "encRandom" &&
        prev === "encFixed"
      ) {
        out.set(trig.pos, kind);
      }
    }
  }
  return out;
}

/**
 * Collect tile positions (y<<4|x) that host OP_12/OP_13 encounters (respects UI priority).
 * @returns {Map<number, EncounterKind>}
 */
export function encounterTilePositions(sc) {
  /** @type {Map<number, EncounterKind>} */
  const out = new Map();
  for (const [pos, kind] of buildMinimapMarkers(sc)) {
    if (kind === "encFixed") out.set(pos, "fixed");
    else if (kind === "encRandom") out.set(pos, "random");
  }
  return out;
}

/** Minimap icon: small door nameplate (OP_04). */
export function drawMinimapDoor(ctx, cx, cy, tw, th) {
  const bx = cx + Math.floor(tw * 0.15);
  const by = cy + Math.floor(th * 0.15);
  const bw = Math.max(4, Math.floor(tw * 0.7));
  const bh = Math.max(2, Math.floor(th * 0.25));
  ctx.fillStyle = "rgba(180,140,90,0.9)";
  ctx.fillRect(bx, by, bw, bh);
  ctx.fillStyle = "rgba(90,60,30,0.85)";
  ctx.fillRect(bx, by + bh - 1, bw, 1);
}

/** Draw the highest-priority minimap marker for a tile. */
export function drawMinimapMarker(ctx, cx, cy, tw, th, kind) {
  switch (kind) {
    case "sign":
      drawMinimapSignpost(ctx, cx, cy, tw, th);
      break;
    case "wall":
      drawMinimapWallText(ctx, cx, cy, tw, th);
      break;
    case "door":
      drawMinimapDoor(ctx, cx, cy, tw, th);
      break;
    case "encFixed":
      drawMinimapEncounter(ctx, cx, cy, tw, th, "fixed");
      break;
    case "encRandom":
      drawMinimapEncounter(ctx, cx, cy, tw, th, "random");
      break;
    default:
      break;
  }
}

/**
 * Ambient encounter territory — dotted overlay (step zone) or solid dot (collision 0x80 only).
 * @param {{ flagged?: boolean }} opts — flagged = map collision event bit without script
 */
export function drawMinimapAmbient(ctx, cx, cy, tw, th, { flagged = false } = {}) {
  const mx = cx + Math.floor(tw / 2);
  const my = cy + Math.floor(th / 2);
  ctx.fillStyle = flagged ? "rgba(200,70,200,0.9)" : "rgba(200,70,200,0.35)";
  if (flagged) {
    ctx.beginPath();
    ctx.arc(mx, my, Math.max(2, Math.floor(Math.min(tw, th) * 0.18)), 0, Math.PI * 2);
    ctx.fill();
    return;
  }
  const r = Math.max(1, Math.floor(Math.min(tw, th) * 0.12));
  for (let dy = -1; dy <= 1; dy++) {
    for (let dx = -1; dx <= 1; dx++) {
      if ((dx + dy) & 1) continue;
      ctx.beginPath();
      ctx.arc(mx + dx * r * 2, my + dy * r * 2, r, 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

/** Minimap icon: orange X (fixed) or magenta asterisk (random) — distinct from sign/wall/event dot. */
export function drawMinimapEncounter(ctx, cx, cy, tw, th, kind = "fixed") {
  const mx = cx + Math.floor(tw / 2);
  const my = cy + Math.floor(th / 2);
  const r = Math.max(3, Math.floor(Math.min(tw, th) * 0.28));
  ctx.strokeStyle = kind === "random" ? "rgba(255,90,255,0.95)" : "rgba(255,120,40,0.95)";
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  if (kind === "random") {
    for (let i = 0; i < 4; i++) {
      const a = (Math.PI / 4) + (i * Math.PI / 2);
      ctx.moveTo(mx + Math.cos(a) * r, my + Math.sin(a) * r);
      ctx.lineTo(mx - Math.cos(a) * r, my - Math.sin(a) * r);
    }
  } else {
    ctx.moveTo(mx - r, my - r);
    ctx.lineTo(mx + r, my + r);
    ctx.moveTo(mx + r, my - r);
    ctx.lineTo(mx - r, my + r);
  }
  ctx.stroke();
}

// --- Narrative modal text (OP_01/02/03) — ports EventTextView.cpp, doc 44 ---

const CHROME_R = 255;
const CHROME_G = 128;
const CHROME_B = 128;

const kGlyphHSeg = 0x05;
const kGlyphHCapL = 0x06;
const kGlyphHCapR = 0x07;
const kGlyphVSeg = 0x0b;

function clearCells(ctx, x1, y1, x2, y2) {
  ctx.fillStyle = "#000";
  ctx.fillRect(x1 * 8, y1 * 8, (x2 - x1 + 1) * 8, (y2 - y1 + 1) * 8);
}

function hLineChrome(ctx, col0, col1, row) {
  glyphAt(ctx, col0, row, kGlyphHCapL, CHROME_R, CHROME_G, CHROME_B);
  for (let col = col0 + 1; col < col1; col++) {
    glyphAt(ctx, col, row, kGlyphHSeg, CHROME_R, CHROME_G, CHROME_B);
  }
  glyphAt(ctx, col1, row, kGlyphHCapR, CHROME_R, CHROME_G, CHROME_B);
}

function chromeMsgTop(ctx) {
  glyphAt(ctx, 0, 18, kGlyphVSeg, CHROME_R, CHROME_G, CHROME_B);
  glyphAt(ctx, 39, 18, kGlyphVSeg, CHROME_R, CHROME_G, CHROME_B);
  glyphAt(ctx, 12, 16, kGlyphHSeg, CHROME_R, CHROME_G, CHROME_B);
  glyphAt(ctx, 21, 16, kGlyphHSeg, CHROME_R, CHROME_G, CHROME_B);
  glyphAt(ctx, 31, 16, kGlyphHSeg, CHROME_R, CHROME_G, CHROME_B);
}

/** OP_01 — single centered line on row 17 + row-18 divider (EventTextView::showOp01). */
export function drawOp01Narrative(ctx, text) {
  hLineChrome(ctx, 0, 39, 18);
  chromeMsgTop(ctx);
  clearCells(ctx, 1, 17, 38, 17);
  printWrapped(ctx, 1, 17, 38, 17, text, true);
}

/** OP_02 — left-justified block rows 19..22 (EventTextView::showOp02). */
export function drawOp02Narrative(ctx, text) {
  clearCells(ctx, 1, 19, 38, 22);
  printWrapped(ctx, 1, 19, 38, 22, text, false);
}

/** OP_03 — tall block rows 17..22 (EventTextView::showOp03). */
export function drawOp03Narrative(ctx, text) {
  chromeMsgTop(ctx);
  clearCells(ctx, 1, 17, 38, 22);
  printWrapped(ctx, 1, 17, 38, 22, text, false);
}

/** OP_07 space prompt — row 23 col 9 (EventTextView::draw space_prompt_). */
export function drawSpacePromptRow23(ctx) {
  textAtCell(ctx, 9, 23, "('Space' to continue)");
}

/**
 * Draw narrative/event modal text in the lower console band (NOT on the 3D viewport).
 * @param {CanvasRenderingContext2D} ctx
 * @param {{ op?: number, text: string, showSpace?: boolean }} opts
 */
export function drawNarrativePanel(ctx, { op = 0x01, text, showSpace = false, vmOp = null }) {
  const layoutOp = resolveNarrativeLayoutOp(text, vmOp ?? op);
  switch (layoutOp) {
    case 0x02:
      drawOp02Narrative(ctx, text);
      break;
    case 0x03:
      drawOp03Narrative(ctx, text);
      break;
    default:
      drawOp01Narrative(ctx, text);
      break;
  }
  if (showSpace) drawSpacePromptRow23(ctx);
}

/** Wrapped line count in the narrative band (cols 1..38). */
function countNarrativeLines(text) {
  let lines = 1;
  let col = 0;
  for (let i = 0; i < text.length; i++) {
    if (text[i] === "\n") {
      lines++;
      col = 0;
      continue;
    }
    col++;
    if (col >= 38) {
      lines++;
      col = 0;
    }
  }
  return lines;
}

/**
 * Map event VM opcode + text to OP_01/02/03 layout (screen-global rows 17..23).
 * VM opcodes like OP_0E/0x0B are not layout ops — service intros use OP_02 @ rows 19..22
 * (EventTownServices.cpp showServiceIntro / showServiceTitle).
 * @param {string} text
 * @param {number | null | undefined} vmOp — raw script opcode, or null to infer from text only
 */
export function resolveNarrativeLayoutOp(text, vmOp) {
  if (vmOp >= 0x01 && vmOp <= 0x03) return vmOp;
  if (vmOp === 0x0b || vmOp === 0x0e || vmOp === 0x12 || vmOp === 0x13) return 0x02;
  if (!text) return 0x01;
  if (countNarrativeLines(text) > 4) return 0x03;
  if (text.includes("\n") || text.length > 38) return 0x02;
  return 0x01;
}

/** Pick OP_01 vs OP_02/03 from text alone (no VM opcode). */
export function inferNarrativeOp(text) {
  return resolveNarrativeLayoutOp(text, null);
}

/** ASM-faithful 3D frustum engine — ports tools/view3d_trace.py + view3d_indoor StitchedVisual. */
"use strict";

export const MAP_GRID = 16;
export const MAP_PAGE_SIZE = 256;
export const MAP_SCREENS = 60;
export const VIEW_W = 208;
export const VIEW_H = 120;
export const ORIGIN_X = 8;
export const SKY_Y = 8;
export const FLOOR_Y = 68;

const BUNDLE_N = [0, 1, 255, 0, 1, 0];
const BUNDLE_E = [1, 0, 0, 1, 0, 255];
const BUNDLE_S = [0, 255, 1, 0, 255, 0];
const BUNDLE_W = [255, 0, 0, 255, 0, 1];
const BUNDLES = [BUNDLE_N, BUNDLE_E, BUNDLE_S, BUNDLE_W];

const S_F20 = 0,
  S_F1F = 1,
  S_F1E = 2,
  S_F1D = 3,
  S_F1C = 4,
  S_F1B = 5,
  S_F1A = 6,
  S_F19 = 7;
const S_F18 = 8,
  S_F17 = 9,
  S_F16 = 10,
  S_F15 = 11,
  S_F14 = 12,
  S_F13 = 13,
  S_F12 = 14,
  S_F11 = 15;
const S_F10 = 16,
  S_F0F = 17,
  S_F0E = 18,
  S_F0D = 19;

export const FACE = ["N", "E", "S", "W"];
export const STEP_DX = [0, 1, 0, -1];
export const STEP_DY = [1, 0, -1, 0];

export const K_CARTO_TILE_FALLBACK = [
  0x00, 0x05, 0x06, 0x05, 0x03, 0x0b, 0x0d, 0x0b, 0x04, 0x0c, 0x0e, 0x0c, 0x03, 0x0b, 0x0d, 0x0b,
  0x01, 0x0f, 0x11, 0x0f, 0x07, 0x13, 0x16, 0x13, 0x09, 0x15, 0x19, 0x15, 0x07, 0x13, 0x16, 0x13,
  0x02, 0x10, 0x12, 0x10, 0x08, 0x14, 0x18, 0x14, 0x0a, 0x17, 0x1a, 0x17, 0x08, 0x14, 0x18, 0x14,
  0x01, 0x0f, 0x11, 0x0f, 0x07, 0x13, 0x16, 0x13, 0x09, 0x15, 0x19, 0x15, 0x07, 0x13, 0x16, 0x13,
];

function sb(bundle, i) {
  const v = bundle[i] & 0xff;
  return v > 127 ? v - 256 : v;
}

export function setFacing(facing) {
  const i = facing & 3;
  const masks = [0xc0, 0x30, 0x0c, 0x03];
  const d1 = masks[i];
  const shFwd = d1 === 0x03 ? 0 : 6 - 2 * i;
  const shD4 = (shFwd + 2) & 7;
  const shD5 = shFwd === 0 ? 6 : shFwd - 2;
  const d5 = d1 === 0x03 ? 0xc0 : (d1 >> 2) & 0xff;
  const d4 = d1 === 0xc0 ? 0x03 : (d1 << 2) & 0xff;
  return { mask: d1, shift: shFwd, mask_d4: d4, mask_d5: d5, shift_d4: shD4, shift_d5: shD5 };
}

function putSlot(slots, idx, cell, mask, shift) {
  if ((cell & mask) === 0) return;
  const code = (cell >> shift) & 3;
  if (code === 0) return;
  slots[idx] = code;
}

export function buildFrustum(hood, f) {
  const slots = new Array(20).fill(0);
  const d1 = f.mask;
  const d4 = f.mask_d4;
  const d5 = f.mask_d5;
  const shFwd = f.shift;
  const shD4 = f.shift_d4;
  const shD5 = f.shift_d5;
  const b = (i) => (i >= 0 && i < hood.length ? hood[i] : 0);

  putSlot(slots, S_F20, b(0), d4, shD4);
  if (slots[S_F20] === 0) putSlot(slots, S_F14, b(4), d1, shFwd);
  putSlot(slots, S_F1C, b(0), d5, shD5);
  if (slots[S_F1C] === 0) putSlot(slots, S_F10, b(8), d1, shFwd);

  if (b(0) & d1) {
    putSlot(slots, S_F18, b(0), d1, shFwd);
    if (slots[S_F20] === 0 && slots[S_F14] === 0) putSlot(slots, S_F13, b(5), d1, shFwd);
    if (slots[S_F1C] === 0 && slots[S_F10] === 0) putSlot(slots, S_F0F, b(9), d1, shFwd);
  } else {
    if (b(1) & d4) putSlot(slots, S_F1F, b(1), d4, shD4);
    else putSlot(slots, S_F13, b(5), d1, shFwd);
    if (b(1) & d5) putSlot(slots, S_F1B, b(1), d5, shD5);
    else putSlot(slots, S_F0F, b(9), d1, shFwd);
    putSlot(slots, S_F17, b(1), d1, shFwd);
    if (slots[S_F1F] === 0 && slots[S_F13] === 0) putSlot(slots, S_F12, b(6), d1, shFwd);
    if (slots[S_F1B] === 0 && slots[S_F0F] === 0) putSlot(slots, S_F0E, b(10), d1, shFwd);

    if ((b(1) & d1) === 0) {
      if (b(2) & d4) putSlot(slots, S_F1E, b(2), d4, shD4);
      else putSlot(slots, S_F12, b(6), d1, shFwd);
      if (b(2) & d5) putSlot(slots, S_F1A, b(2), d5, shD5);
      else putSlot(slots, S_F0E, b(10), d1, shFwd);
      putSlot(slots, S_F16, b(2), d1, shFwd);

      if ((b(2) & d1) === 0) {
        if (b(3) & d4) putSlot(slots, S_F1D, b(3), d4, shD4);
        else putSlot(slots, S_F11, b(7), d1, shFwd);
        if (b(3) & d5) putSlot(slots, S_F19, b(3), d5, shD5);
        else putSlot(slots, S_F0D, b(11), d1, shFwd);
        putSlot(slots, S_F15, b(3), d1, shFwd);
      }
    }
  }

  function norm(a, bb) {
    if (slots[a] !== 0 && slots[bb] === 2) slots[bb] = 1;
  }
  norm(S_F20, S_F13);
  norm(S_F1C, S_F0F);
  norm(S_F1F, S_F12);
  norm(S_F1B, S_F0E);
  return slots;
}

export function collectBlits(slots) {
  const frontY = [22, 40, 54, 62];
  const frontX = [32, 64, 88, 104];
  const leftX1 = [8, 32, 64, 88],
    leftY1 = [8, 22, 40, 54];
  const leftX2 = [8, 8, 40, 88],
    leftY2 = [22, 40, 54, 62];
  const rightX1 = [192, 160, 136, 120],
    rightY1 = [8, 22, 40, 54];
  const rightX2 = [192, 160, 136, 120],
    rightY2 = [22, 40, 54, 62];
  const leftBase = [12, 14, 2, 3];
  const rightBase = [13, 15, 2, 3];
  const blits = [];

  function paint(kind, paintDepth, code) {
    if (code === 0) return;
    const depth = (paintDepth & 0x7f) - 1;
    if (depth < 0 || depth >= 4) return;
    const mirror = paintDepth >= 0x80;
    const isDoor = code === 2;
    if (kind === "front") {
      blits.push({
        kind,
        depth,
        frame: depth + (isDoor ? 0x10 : 0),
        x: frontX[depth],
        y: frontY[depth] + (depth === 0 ? 1 : 0),
        code,
      });
    } else if (kind === "left") {
      if (mirror) {
        blits.push({
          kind,
          depth,
          frame: leftBase[depth] + (isDoor ? 0x10 : 0),
          x: leftX2[depth],
          y: leftY2[depth],
          code,
        });
      } else {
        blits.push({
          kind,
          depth,
          frame: depth + 4 + (isDoor ? 0x10 : 0),
          x: leftX1[depth],
          y: leftY1[depth],
          code,
        });
      }
    } else if (mirror) {
      blits.push({
        kind,
        depth,
        frame: rightBase[depth] + (isDoor ? 0x10 : 0),
        x: rightX2[depth],
        y: rightY2[depth],
        code,
      });
    } else {
      blits.push({
        kind,
        depth,
        frame: depth + 8 + (isDoor ? 0x10 : 0),
        x: rightX1[depth],
        y: rightY1[depth],
        code,
      });
    }
  }

  const s = slots;
  paint("left", 4, s[S_F1D]);
  paint("left", 0x84, s[S_F11]);
  paint("right", 4, s[S_F19]);
  paint("right", 0x84, s[S_F0D]);
  paint("front", 4, s[S_F15]);
  paint("left", 3, s[S_F1E]);
  paint("left", 0x83, s[S_F12]);
  paint("right", 3, s[S_F1A]);
  paint("right", 0x83, s[S_F0E]);
  paint("front", 3, s[S_F16]);
  paint("left", 2, s[S_F1F]);
  paint("left", 0x82, s[S_F13]);
  paint("right", 2, s[S_F1B]);
  paint("right", 0x82, s[S_F0F]);
  paint("front", 2, s[S_F17]);
  paint("left", 1, s[S_F20]);
  paint("left", 0x81, s[S_F14]);
  paint("right", 1, s[S_F1C]);
  paint("right", 0x81, s[S_F10]);
  paint("front", 1, s[S_F18]);
  return blits;
}

/** Stitched page-0 sampler (view3d_indoor.StitchedVisual). */
export class StitchedVisual {
  constructor(screens, screenId) {
    this.screen = screenId;
    const sc = screens[screenId];
    this.center = sc.visual;
    this.north = this._page(screens, sc.neighbors[0]);
    this.east = this._page(screens, sc.neighbors[1]);
    this.south = this._page(screens, sc.neighbors[2]);
    this.west = this._page(screens, sc.neighbors[3]);
  }

  _page(screens, idx) {
    if (idx >= 0 && idx < screens.length) return screens[idx].visual;
    return new Array(MAP_PAGE_SIZE).fill(0);
  }

  at(x, y) {
    let page = this.center;
    let lx = x;
    let ly = y;
    if (x > 0x0f && x < 0x14) {
      page = this.east;
      lx = x - 0x10;
    } else if (x >= 0xfc) {
      page = this.west;
      lx = x - 0xf0;
    }
    if (y >= MAP_GRID) {
      page = this.north;
      ly = y - MAP_GRID;
    } else if (y < 0) {
      page = this.south;
      ly = y + MAP_GRID;
    }
    if (lx < 0 || ly < 0 || lx >= MAP_GRID || ly >= MAP_GRID) return 0;
    return page[(ly << 4) | lx];
  }
}

export function refreshHood(grid, px, py, facing) {
  const hood = new Array(13).fill(0);
  const b = BUNDLES[facing & 3];
  const dx = sb(b, 0);
  const dy = sb(b, 1);

  function row(sx, sy, outOff) {
    let x = sx;
    let y = sy;
    for (let i = 0; i < 5; i++) {
      hood[outOff + i] = grid.at(x, y);
      x += dx;
      y += dy;
    }
  }

  row(px, py, 0);
  row(px + sb(b, 2), py + sb(b, 3), 4);
  row(px + sb(b, 4), py + sb(b, 5), 8);
  return hood;
}

export function buildIndoorScene(grid, x, y, facing) {
  const hood = refreshHood(grid, x, y, facing);
  const slots = buildFrustum(hood, setFacing(facing));
  return { hood, slots, blits: collectBlits(slots) };
}

export function stepParty(facing, x, y, screen, screens) {
  x += STEP_DX[facing & 3];
  y += STEP_DY[facing & 3];
  const rec = screens[screen];
  const n = rec.neighbors;
  if (x < 0) {
    if (n[3] >= 0 && n[3] < MAP_SCREENS) {
      screen = n[3];
      x = 15;
    }
  } else if (x >= MAP_GRID) {
    if (n[1] >= 0 && n[1] < MAP_SCREENS) {
      screen = n[1];
      x = 0;
    }
  }
  if (y < 0) {
    if (n[2] >= 0 && n[2] < MAP_SCREENS) {
      screen = n[2];
      y = 15;
    }
  } else if (y >= MAP_GRID) {
    if (n[0] >= 0 && n[0] < MAP_SCREENS) {
      screen = n[0];
      y = 0;
    }
  }
  return {
    screen,
    x: Math.max(0, Math.min(15, x)),
    y: Math.max(0, Math.min(15, y)),
  };
}

export function selectIndoorSheets(env) {
  if (env === "cavern") return { walls: "cave_32", floor: "cavef_32", torch: "cavet_32", sky: "sky_32" };
  if (env === "castle")
    return { walls: "castle_32", floor: "castlef_32", torch: "castlet_32", sky: "sky_32" };
  // town + other interior defaults
  return { walls: "town_32", floor: "townf_32", torch: "townt_32", sky: "sky_32" };
}

export function roofAt(sc, x, y) {
  // attrib.dat +0x20..+0x3F — tile t = y*16+x, byte 0x20+(t>>3), bit t&7 (MapSection.cpp)
  const tile = y * MAP_GRID + x;
  const byteIdx = tile >> 3;
  const bit = tile & 7;
  if (!sc.roof || byteIdx >= sc.roof.length) return false;
  return ((sc.roof[byteIdx] >> bit) & 1) !== 0;
}

export function skyFrameFor(sc, x, y) {
  return roofAt(sc, x, y) ? 1 : 0;
}

export function cartoFrame(screenId, visual, outdoor, cartoTable) {
  if (screenId === 41) return 8;
  if (screenId === 42 || screenId === 43) return 4;
  if (screenId === 44) return 5;
  if (outdoor) return visual & 0x1f;
  return cartoTable[(visual >> 2) & 0x3f];
}

export function tileLabel(x, y) {
  return `${String.fromCharCode(97 + x)}${y + 1}`;
}

export function torchBlitFor(wb) {
  if (wb.code !== 3 || wb.depth > 2) return null;
  if (wb.kind === "front") {
    return { frame: 0x12 + wb.depth * 3, x: [105, 108, 107][wb.depth], y: [44, 52, 60][wb.depth] };
  }
  if (wb.kind === "left") {
    if ([12, 14, 2, 3].includes(wb.frame)) {
      if (wb.depth === 0) return null;
      return { frame: 0x12 + wb.depth * 3, x: [8, 16, 64][wb.depth], y: [44, 52, 60][wb.depth] };
    }
    return { frame: wb.depth * 3, x: [8, 43, 73][wb.depth], y: [49, 55, 59][wb.depth] };
  }
  if (wb.kind === "right") {
    if ([13, 15, 2, 3].includes(wb.frame)) {
      if (wb.depth === 0) return null;
      return { frame: 0x12 + wb.depth * 3, x: [202, 199, 152][wb.depth], y: [44, 52, 60][wb.depth] };
    }
    return { frame: 9 + wb.depth * 3, x: [196, 166, 142][wb.depth], y: [49, 55, 59][wb.depth] };
  }
  return null;
}

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
const VIS_DIR_SHIFT = [6, 4, 2, 0];

function collisionFieldWall(cell, dirIdx) {
  const d = dirIdx & 3;
  if (d === 3) return (cell & 0x40) !== 0;
  return ((cell >> (d * 2)) & 1) !== 0;
}

function movementToCollisionDir(facing) {
  return (3 - (facing & 3)) & 3;
}

/** Block step using map.dat page 1; outdoor impassable terrain uses page-0 high bits. */
export function movementBlocked(sc, x, y, facing, terrainLookup = null) {
  const d = facing & 3;
  const cd = movementToCollisionDir(d);
  const col = sc.collision;
  const idx = y * MAP_GRID + x;
  if (collisionFieldWall(col[idx], cd)) return true;
  let nx = x + STEP_DX[d];
  let ny = y + STEP_DY[d];
  if (nx < 0 || ny < 0 || nx >= MAP_GRID || ny >= MAP_GRID) return false;
  const back = movementToCollisionDir((d + 2) & 3);
  const destIdx = ny * MAP_GRID + nx;
  if (collisionFieldWall(col[destIdx], back)) return true;
  if (sc.outdoor) {
    const destV = sc.visual[destIdx];
    if (terrainLookup) {
      const id = destV & 0x1f;
      const tClass = terrainLookup[id];
      // 1=mountain, 3=solid trees
      if (tClass === 1 || tClass === 3) return true;
      // 4=deep water, swamp, snow, desert, etc. Only block deep water (ID 10).
      if (tClass === 4 && id === 10) return true;
    } else {
      if ((destV & 0x60) === 0x60 || (destV & 0x80) !== 0) return true;
    }
  }
  return false;
}

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
    // Door (2) beside an open side → plain wall (1) @0x2B6A..0x2BBC.
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
  /** Populated at paint time for field===3 only (ASM -$4F62/-$4F76/-$4F8A). */
  const torchBlits = [];

  function paint(latX, paintDepth, code) {
    if (code === 0) return;
    const depth = (paintDepth & 0x7f) - 1;
    if (depth < 0 || depth >= 4) return;
    const kind = latX === 0 ? "front" : latX < 0 ? "left" : "right";
    const mirror = latX === -2 || latX === 2;
    const isDoor = code === 2;
    let entry;
    if (latX === 0) {
      entry = {
        latX,
        latRow: -depth,
        kind,
        depth,
        frame: depth + (isDoor ? 0x10 : 0),
        x: frontX[depth],
        y: frontY[depth] + (depth === 0 ? 1 : 0),
        code,
      };
    } else if (latX === -2) {
      entry = {
        latX,
        latRow: -depth,
        kind,
        depth,
        mirror: true,
        frame: leftBase[depth] + (isDoor ? 0x10 : 0),
        x: leftX2[depth],
        y: leftY2[depth],
        code,
      };
    } else if (latX === -1) {
      entry = {
        latX,
        latRow: -depth,
        kind,
        depth,
        mirror: false,
        frame: depth + 4 + (isDoor ? 0x10 : 0),
        x: leftX1[depth],
        y: leftY1[depth],
        code,
      };
    } else if (latX === 1) {
      entry = {
        latX,
        latRow: -depth,
        kind,
        depth,
        mirror: false,
        frame: depth + 8 + (isDoor ? 0x10 : 0),
        x: rightX1[depth],
        y: rightY1[depth],
        code,
      };
    } else {
      entry = {
        latX,
        latRow: -depth,
        kind,
        depth,
        mirror: true,
        frame: rightBase[depth] + (isDoor ? 0x10 : 0),
        x: rightX2[depth],
        y: rightY2[depth],
        code,
      };
    }
    blits.push(entry);
    if (code === 3 && depth <= 2) {
      torchBlits.push(entry);
    }
  }

  const s = slots;
  paint(-1, 4, s[S_F1D]);
  paint(-2, 0x84, s[S_F11]);
  paint(1, 4, s[S_F19]);
  paint(2, 0x84, s[S_F0D]);
  paint(0, 4, s[S_F15]);
  paint(-1, 3, s[S_F1E]);
  paint(-2, 0x83, s[S_F12]);
  paint(1, 3, s[S_F1A]);
  paint(2, 0x83, s[S_F0E]);
  paint(0, 3, s[S_F16]);
  paint(-1, 2, s[S_F1F]);
  paint(-2, 0x82, s[S_F13]);
  paint(1, 2, s[S_F1B]);
  paint(2, 0x82, s[S_F0F]);
  paint(0, 2, s[S_F17]);
  paint(-1, 1, s[S_F20]);
  paint(-2, 0x81, s[S_F14]);
  paint(1, 1, s[S_F1C]);
  paint(2, 0x81, s[S_F10]);
  paint(0, 1, s[S_F18]);
  return { blits, torchBlits };
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
  const { blits, torchBlits } = collectBlits(slots);
  return { hood, slots, blits, torchBlits };
}

export function stepParty(facing, x, y, screen, screens, terrainLookup = null, noclip = false) {
  const rec = screens[screen];
  if (!noclip && movementBlocked(rec, x, y, facing, terrainLookup)) {
    return { screen, x, y };
  }
  x += STEP_DX[facing & 3];
  y += STEP_DY[facing & 3];
  const n = rec.neighbors;
  const nScreens = screens.length;
  if (x < 0) {
    if (n[3] >= 0 && n[3] < nScreens) {
      screen = n[3];
      x = 15;
    }
  } else if (x >= MAP_GRID) {
    if (n[1] >= 0 && n[1] < nScreens) {
      screen = n[1];
      x = 0;
    }
  }
  if (y < 0) {
    if (n[2] >= 0 && n[2] < nScreens) {
      screen = n[2];
      y = 15;
    }
  } else if (y >= MAP_GRID) {
    if (n[0] >= 0 && n[0] < nScreens) {
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

export function selectIndoorSheets(env, gfxMode = "amiga", wallset = "auto") {
  const s = sheetSuffix(gfxMode);
  const pick = (base) => ({
    walls: `${base.walls}${s}`,
    floor: `${base.floor}${s}`,
    torch: `${base.torch}${s}`,
    sky: `${base.sky}${s}`,
  });
  if (wallset !== "auto" && INDOOR_WALLSETS[wallset]) return pick(INDOOR_WALLSETS[wallset]);
  if (env === "cavern") return pick(INDOOR_WALLSETS.cavern);
  if (env === "castle") return pick(INDOOR_WALLSETS.castle);
  // town + other interior defaults
  return pick(INDOOR_WALLSETS.town);
}

/** Manual wallset override for the indoor 3D picker (`auto` = follow attrib env). */
export const INDOOR_WALLSET_IDS = ["auto", "town", "cavern", "castle"];

export const INDOOR_WALLSETS = {
  town: { walls: "town", floor: "townf", torch: "townt", sky: "sky" },
  cavern: { walls: "cave", floor: "cavef", torch: "cavet", sky: "sky" },
  castle: { walls: "castle", floor: "castlef", torch: "castlet", sky: "sky" },
};

/** Amiga `.32` keys vs PC `.4`/`.16` exports (`town_cga`, `town_ega`, …). */
export const GFX_MODES = ["amiga", "cga", "ega"];

export function sheetSuffix(gfxMode = "amiga") {
  if (gfxMode === "cga" || gfxMode === "ega") return `_${gfxMode}`;
  return "_32";
}

export function remapSheetKey(key32, gfxMode = "amiga") {
  if (!key32 || gfxMode === "amiga") return key32;
  if (key32.endsWith("_32")) return `${key32.slice(0, -3)}${sheetSuffix(gfxMode)}`;
  if (key32.endsWith("_anm")) return `${key32.slice(0, -4)}${sheetSuffix(gfxMode)}`;
  return key32;
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
  if (screenId >= 41 && screenId <= 44) return visual & 0x1f;
  if (outdoor) return visual & 0x1f;
  return cartoTable[(visual >> 2) & 0x3f];
}

export function tileLabel(x, y) {
  return `${String.fromCharCode(97 + x)}${y + 1}`;
}

/** key_read_3d @0x1E9CE: torch frame += phase from A4-$667A (0..2, wraps at 3). */
export const TORCH_PHASE_COUNT = 3;

/**
 * PC TOWNT/CAVET/CASTLET (.4/.16) lay out 12-frame side blocks (left 0–11,
 * right 12–23, front/far 24–35). Amiga townt.32 uses 9-frame blocks
 * (0–8, 9–17, 18–26) with the same logical index math from -$4F62 tables.
 */
export function remapTorchOverlayFrame(frame, gfxMode = "amiga") {
  if (gfxMode === "amiga") return frame;
  if (frame > 26) return frame;
  return Math.floor(frame / 9) * 12 + (frame % 9);
}

function inferLatX(wb) {
  if (wb.latX != null) return wb.latX;
  if (wb.kind === "front") return 0;
  if (wb.kind === "left") return wb.mirror ? -2 : -1;
  return wb.mirror ? 2 : 1;
}

/** Mirrors game/src/gfx/View3D.cpp view3dTorchBlitFor (+ PC frame remap). */
export function torchBlitFor(wb, phase = 0, gfxMode = "amiga") {
  /* map.dat page-0 field 3 = wall+torch overlay slot (NEVER door=2). Caller passes torchBlits[] (code===3) only. */
  if (wb.code !== 3 || wb.depth > 2) return null;
  const flicker = ((phase % TORCH_PHASE_COUNT) + TORCH_PHASE_COUNT) % TORCH_PHASE_COUNT;
  const baseFrame = wb.frame;
  const latX = inferLatX(wb);
  const mapFrame = (frame) => remapTorchOverlayFrame(frame, gfxMode);

  if (latX === 0) {
    return {
      frame: mapFrame(0x12 + wb.depth * 3 + flicker),
      x: [105, 108, 107][wb.depth],
      y: [44, 52, 60][wb.depth],
    };
  }
  if (latX === -1) {
    if (
      baseFrame === 12 ||
      baseFrame === 14 ||
      baseFrame === 2 ||
      baseFrame === 3
    ) {
      if (wb.depth === 0) return null;
      return {
        frame: mapFrame(0x12 + wb.depth * 3 + flicker),
        x: [8, 16, 64][wb.depth],
        y: [44, 52, 60][wb.depth],
      };
    }
    return {
      frame: mapFrame(wb.depth * 3 + flicker),
      x: [8, 43, 73][wb.depth],
      y: [49, 55, 59][wb.depth],
    };
  }
  if (latX === -2) {
    if (wb.depth === 0) return null;
    return {
      frame: mapFrame(0x12 + wb.depth * 3 + flicker),
      x: [8, 16, 64][wb.depth],
      y: [44, 52, 60][wb.depth],
    };
  }
  if (latX === 1) {
    if (
      baseFrame === 13 ||
      baseFrame === 15 ||
      baseFrame === 2 ||
      baseFrame === 3
    ) {
      if (wb.depth === 0) return null;
      return {
        frame: mapFrame(0x12 + wb.depth * 3 + flicker),
        x: [202, 199, 152][wb.depth],
        y: [44, 52, 60][wb.depth],
      };
    }
    return {
      frame: mapFrame(9 + wb.depth * 3 + flicker),
      x: [196, 166, 142][wb.depth],
      y: [49, 55, 59][wb.depth],
    };
  }
  if (latX === 2) {
    if (wb.depth === 0) return null;
    return {
      frame: mapFrame(0x12 + wb.depth * 3 + flicker),
      x: [202, 199, 152][wb.depth],
      y: [44, 52, 60][wb.depth],
    };
  }
  return null;
}

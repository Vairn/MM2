/** Outdoor first-person scene — ports tools/view3d_outdoor.py. */
"use strict";

import { MAP_GRID, MAP_PAGE_SIZE, MAP_SCREENS, ORIGIN_X, STEP_DX, STEP_DY } from "./view3d.js?v=20260630-slower-anims-2";

const BUNDLE_N = [0, 1, 255, 0, 1, 0];
const BUNDLE_E = [1, 0, 0, 1, 0, 255];
const BUNDLE_S = [0, 255, 1, 0, 255, 0];
const BUNDLE_W = [255, 0, 0, 255, 0, 1];
const BUNDLES = [BUNDLE_N, BUNDLE_E, BUNDLE_S, BUNDLE_W];

const HORIZON_SHEETS = ["outdoor1_32", "outdoor2_32", "outdoor3_32"];

const MAP_TERRAIN_ID_BIOME = { 2: "ocean_32", 3: "ocean_32" };
const SECTOR_LABEL_BIOME = { 0xc3: "ocean_32" };
const BIOME_BY_AREA = { 41: "swamp_32", 42: "swamp_32", 43: "swamp_32", 44: "swamp_32" };
const BIOME_SHEET_REMAP = { desert_32: "ocean_32", ocean_32: "tundra_32", tundra_32: "desert_32" };

const OUTDOOR_DECOR_Y = [0x80 - 20, 0x80 - 35, 0x80 - 50, 0x80 - 60];
const OUTDOOR_DECOR_X = 8;
const OUTDOOR_DECOR_X_112 = 0x70;
const OUTDOOR_DECOR_X_BDE = [184, 160, 136, 112];

const OUTDOOR_L1_Y = [21, 21, 42, 50];
const OUTDOOR_L1_X = [40, 40, 64, 88];
const OUTDOOR_L1_FRAME = [0, 0, 1, 2];
const OUTDOOR_L2_Y = [36, 46, 50, 58];
const OUTDOOR_L2_X = [8, 16, 32, 88];
const OUTDOOR_L2_FRAME = [4, 5, 2, 3];
const OUTDOOR_L3_Y = [36, 46, 50, 58];
const OUTDOOR_L3_X = [176, 152, 136, 120];
const OUTDOOR_L3_FRAME = [6, 7, 2, 3];

function sb(bundle, i) {
  const v = bundle[i] & 0xff;
  return v > 127 ? v - 256 : v;
}

function facingMask(facing) {
  return [0xc0, 0x30, 0x0c, 0x03][facing & 3];
}

function remapBiome(sheet) {
  return BIOME_SHEET_REMAP[sheet] || sheet;
}

/** Stitched outdoor grid (view3d_outdoor.StitchedMap). */
export class StitchedOutdoor {
  constructor(screens, screenId) {
    this.screen = screenId;
    this.screens = screens;
    this.pageByScreen = { [screenId]: screens[screenId].visual };
    const rec = screens[screenId];
    for (let slot = 0; slot < 4; slot++) {
      const n = this._neighborId(rec, slot);
      if (n >= 0 && n < screens.length) this.pageByScreen[n] = screens[n].visual;
    }
  }

  _neighborId(rec, slot) {
    const n = rec.neighbors[slot];
    return n >= 0 && n < this.screens.length ? n : rec.id;
  }

  _neighborOf(screen, slot) {
    const n = this.screens[screen].neighbors[slot];
    return n >= 0 && n < this.screens.length ? n : screen;
  }

  _resolve(x, y) {
    let screen = this.screen;
    let lx = x;
    let ly = y;
    if (lx < 0) {
      screen = this._neighborOf(screen, 3);
      lx += MAP_GRID;
    } else if (lx >= MAP_GRID) {
      screen = this._neighborOf(screen, 1);
      lx -= MAP_GRID;
    }
    if (ly < 0) {
      screen = this._neighborOf(screen, 2);
      ly += MAP_GRID;
    } else if (ly >= MAP_GRID) {
      screen = this._neighborOf(screen, 0);
      ly -= MAP_GRID;
    }
    const page = this.pageByScreen[screen] || new Array(MAP_PAGE_SIZE).fill(0);
    return { page, screen, lx, ly };
  }

  screenIdAt(x, y) {
    return this._resolve(x, y).screen;
  }

  at(x, y) {
    const { page, lx, ly } = this._resolve(x, y);
    if (lx < 0 || ly < 0 || lx >= MAP_GRID || ly >= MAP_GRID) return 0;
    return page[(ly << 4) | lx];
  }
}

function refreshOutdoorHood(grid, px, py, facing) {
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

function processTerrainRows(rows, currentCell, facing, lookup) {
  const c6 = rows[0].map((b) => lookup[b & 0x1f]);
  const c2 = rows[1].map((b) => lookup[b & 0x1f]);
  const be = rows[2].map((b) => lookup[b & 0x1f]);
  const near = c6[0];
  if (near >= 1 && near <= 3) {
    const fwd = facingMask(facing);
    const left = fwd === 0xc0 ? 0x03 : (fwd << 2) & 0xff;
    const right = fwd === 0x03 ? 0xc0 : fwd >> 2;
    if ((currentCell & left & 0x55) !== 0) c2[0] = near;
    if ((currentCell & right & 0x55) !== 0) be[0] = near;
    if ((currentCell & fwd & 0x55) === 0) c6[0] = 0;
  }
  return [c6, c2, be];
}

export function buildSectorLabelBiomes(screens) {
  const out = { ...SECTOR_LABEL_BIOME };
  for (const rec of screens) {
    if (!rec.outdoor) continue;
    const label = rec.sector;
    if (label in out) continue;
    const sf = rec.surface !== undefined ? rec.surface : 0;
    if (sf === 0xcc) out[label] = "ocean_32";
    else if (sf === 0x99) out[label] = "tundra_32";
    else if (sf === 0xbb) out[label] = "swamp_32";
    else out[label] = "desert_32";
  }
  return out;
}

function biomeForCell(mapByte, sectorLabel, sectorTable) {
  const tid = mapByte & 0x1f;
  if (tid in MAP_TERRAIN_ID_BIOME) return remapBiome(MAP_TERRAIN_ID_BIOME[tid]);
  return remapBiome(sectorTable[sectorLabel] || "desert_32");
}

function columnBiomes(grid, px, py, facing, screens, sectorTable) {
  const b = BUNDLES[facing & 3];
  const dx = sb(b, 0);
  const dy = sb(b, 1);
  const out = [];
  for (let col = 0; col < 4; col++) {
    const wx = px + col * dx;
    const wy = py + col * dy;
    const sid = grid.screenIdAt(wx, wy);
    const cell = grid.at(wx, wy);
    const label = screens[sid].sector;
    out.push(biomeForCell(cell, label, sectorTable));
  }
  return out;
}

function buildDecorBlits(c6, laneL2, laneL3, colBiomes) {
  const blits = [];
  for (let col = 3; col >= 0; col--) {
    const biome = colBiomes[col] || "desert_32";
    const mainA = c6[col] > 3;
    const mainB = laneL2[col] > 3;
    const mainC = laneL3[col] > 3;
    const y = OUTDOOR_DECOR_Y[col];
    if (mainA) {
      blits.push({ sheet: biome, frame: col, x: OUTDOOR_DECOR_X, y });
      if (mainB) blits.push({ sheet: biome, frame: col + 12, x: OUTDOOR_DECOR_X, y });
      if (mainC)
        blits.push({ sheet: biome, frame: col + 16, x: OUTDOOR_DECOR_X_BDE[col] - ORIGIN_X, y });
    } else {
      if (mainB) blits.push({ sheet: biome, frame: col + 4, x: OUTDOOR_DECOR_X, y });
      if (mainC) blits.push({ sheet: biome, frame: col + 8, x: OUTDOOR_DECOR_X_112, y });
    }
  }
  return blits;
}

function horizonIndex(terrainClass) {
  if (terrainClass <= 0 || terrainClass > 3) return 0xff;
  return terrainClass - 1;
}

function horizonSprite(idx, frame, x, y) {
  if (idx === 0xff || idx < 0 || idx >= HORIZON_SHEETS.length) return null;
  return { sheet: HORIZON_SHEETS[idx], frame, x, y };
}

function buildHorizonBlits(c6, laneL2, laneL3) {
  const l1 = c6.slice(0, 4).map((c) => horizonIndex(c));
  const l2 = laneL2.slice(0, 4).map((c) => horizonIndex(c));
  const l3 = laneL3.slice(0, 4).map((c) => horizonIndex(c));
  let start = 4;
  for (let col = 0; col < 4; col++) {
    if (l1[col] !== 0xff) {
      start = col;
      break;
    }
  }
  let pivot = start === 3 && l1[start] === 0xff ? 3 : start;
  const blits = [];

  if (start < 4 && l1[start] !== 0xff) {
    const b = horizonSprite(l1[start], OUTDOOR_L1_FRAME[start], OUTDOOR_L1_X[start], OUTDOOR_L1_Y[start]);
    if (b) blits.push(b);
  } else if (start === 4) pivot = 3;

  function passL2(col, piv) {
    const special = col !== 0 && col === piv && l1[col] !== 0xff;
    if (special && l2[col - 1] !== 0xff) l2[col] = 0xff;
    if (l2[col] === 0xff) return;
    let frame = OUTDOOR_L2_FRAME[col];
    let x = OUTDOOR_L2_X[col];
    let y = OUTDOOR_L2_Y[col];
    if (special && l2[piv] !== 0xff) {
      frame = OUTDOOR_L2_FRAME[piv - 1];
      x = OUTDOOR_L2_X[piv - 1];
      y = OUTDOOR_L1_Y[piv];
    }
    if (col === 1 && l2[0] === 0xff) x = 8;
    if (col === 1 || (col === 2 && col === piv)) x = 8;
    const b = horizonSprite(l2[col], frame, x, y);
    if (b) blits.push(b);
  }

  function passL3(col, piv) {
    const special = col !== 0 && col === piv && l1[col] !== 0xff;
    if (special && l3[col - 1] !== 0xff) l3[col] = 0xff;
    if (l3[col] === 0xff) return;
    let frame = OUTDOOR_L3_FRAME[col];
    let x = OUTDOOR_L3_X[col];
    let y = OUTDOOR_L3_Y[col];
    if (special && l3[piv] !== 0xff) {
      frame = OUTDOOR_L3_FRAME[piv - 1];
      x = OUTDOOR_L3_X[piv - 1];
      y = OUTDOOR_L1_Y[piv];
    }
    if (col === 1 && l3[0] === 0xff) x = col === piv ? 0xb0 : 0x98;
    const b = horizonSprite(l3[col], frame, x, y);
    if (b) blits.push(b);
  }

  for (let col = pivot; col >= 0; col--) passL2(col, pivot);
  for (let col = pivot; col >= 0; col--) passL3(col, pivot);
  return blits;
}

export function buildOutdoorScene(grid, px, py, facing, screens, terrainLookup) {
  const hood = refreshOutdoorHood(grid, px, py, facing);
  const rows = [hood.slice(0, 5), hood.slice(4, 9), hood.slice(8, 13)];
  const [c6, laneD0, laneCc] = processTerrainRows(rows, grid.at(px, py), facing, terrainLookup);
  const laneL2 = laneD0;
  const laneL3 = laneCc;
  const sectorTable = buildSectorLabelBiomes(screens);
  const colBio = columnBiomes(grid, px, py, facing, screens, sectorTable);
  return {
    decor: buildDecorBlits(c6, laneL2, laneL3, colBio),
    horizon: buildHorizonBlits(c6, laneL2, laneL3),
  };
}

export { STEP_DX, STEP_DY };

/** MM2 first-person map walker — ports view3d_indoor.py / view3d_outdoor.py / MapSection. */
import {
  MAP_GRID,
  VIEW_W,
  VIEW_H,
  ORIGIN_X,
  SKY_Y,
  FLOOR_Y,
  FACE,
  StitchedVisual,
  buildIndoorScene,
  stepParty,
  selectIndoorSheets,
  skyFrameFor,
  cartoFrame,
  tileLabel,
  torchBlitFor,
  K_CARTO_TILE_FALLBACK,
} from "./view3d.js";
import { StitchedOutdoor, buildOutdoorScene } from "./outdoor3d.js";

const SCALE = 3;
const MINI_TW = 14;
const MINI_TH = 11;

const state = {
  screen: 0,
  x: 7,
  y: 6,
  facing: 0,
};

/** @type {{ version:number, screens: object[] } | null} */
let maps = null;
/** @type {object | null} */
let manifest = null;
/** @type {Map<string, HTMLImageElement>} */
const images = new Map();

const viewCanvas = document.getElementById("view");
const miniCanvas = document.getElementById("minimap");
const viewCtx = viewCanvas.getContext("2d");
const miniCtx = miniCanvas.getContext("2d");
const statusEl = document.getElementById("status");
const screenSelect = document.getElementById("screenSelect");
const locName = document.getElementById("locName");
const locTile = document.getElementById("locTile");
const locScreen = document.getElementById("locScreen");
const locFacing = document.getElementById("locFacing");
const locMode = document.getElementById("locMode");
const locCeiling = document.getElementById("locCeiling");

function cartoTable() {
  return manifest?.cartoTile || K_CARTO_TILE_FALLBACK;
}

function terrainLookup() {
  return manifest?.terrainLookup || new Array(256).fill(0);
}

async function loadJson(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`${url}: ${res.status}`);
  return res.json();
}

async function loadSpriteTable(spriteTable) {
  await Promise.all(
    Object.entries(spriteTable).map(
      ([key, src]) =>
        new Promise((resolve, reject) => {
          const img = new Image();
          img.onload = () => {
            images.set(key, img);
            resolve();
          };
          img.onerror = () => reject(new Error(key));
          img.src = src;
        })
    )
  );
}

async function loadSpritesFromPaths() {
  if (!manifest?.sheets) return;
  const tasks = [];
  for (const [key, sheet] of Object.entries(manifest.sheets)) {
    for (const [frameStr, fr] of Object.entries(sheet.frames)) {
      const cacheKey = `${key}:${frameStr}`;
      if (images.has(cacheKey) || !fr.path) continue;
      tasks.push(
        new Promise((resolve, reject) => {
          const img = new Image();
          img.onload = () => {
            images.set(cacheKey, img);
            resolve();
          };
          img.onerror = () => reject(new Error(fr.path));
          img.src = fr.path;
        })
      );
    }
  }
  await Promise.all(tasks);
}

async function loadData() {
  try {
    const bundle = await import("./walker-bundle.js");
    if (bundle.WALKER_MAPS && bundle.WALKER_MANIFEST && bundle.WALKER_SPRITES) {
      return {
        maps: bundle.WALKER_MAPS,
        manifest: bundle.WALKER_MANIFEST,
        sprites: bundle.WALKER_SPRITES,
      };
    }
  } catch {
    /* fall through to split JSON + PNG layout (--split) */
  }
  return {
    maps: await loadJson("maps.json"),
    manifest: await loadJson("sprites.json"),
    sprites: null,
  };
}

function spriteImg(sheetKey, frame) {
  return images.get(`${sheetKey}:${frame}`);
}

function blit(ctx, sheetKey, frame, x, y) {
  const img = spriteImg(sheetKey, frame);
  if (!img) return;
  ctx.drawImage(img, x, y);
}

function blitTransparent(ctx, sheetKey, frame, x, y) {
  const img = spriteImg(sheetKey, frame);
  if (!img) return;
  ctx.drawImage(img, x, y);
}

function renderIndoorView(ctx, sc, scene) {
  const sheets = selectIndoorSheets(sc.env);
  const skyFrame = skyFrameFor(sc, state.x, state.y);
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, VIEW_W, VIEW_H);
  blitTransparent(ctx, sheets.floor, "0", ORIGIN_X, FLOOR_Y);
  blitTransparent(ctx, sheets.sky, String(skyFrame), ORIGIN_X, SKY_Y);
  for (const b of scene.blits) {
    blitTransparent(ctx, sheets.walls, String(b.frame), b.x, b.y);
    const tb = torchBlitFor(b);
    if (tb) blitTransparent(ctx, sheets.torch, String(tb.frame), tb.x, tb.y);
  }
}

function renderOutdoorView(ctx, scene) {
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, VIEW_W, VIEW_H);
  blitTransparent(ctx, "outf_32", "0", ORIGIN_X, FLOOR_Y);
  blitTransparent(ctx, "sky_32", "0", ORIGIN_X, SKY_Y);
  for (const b of scene.decor) blitTransparent(ctx, b.sheet, String(b.frame), b.x, b.y);
  for (const b of scene.horizon) blitTransparent(ctx, b.sheet, String(b.frame), b.x, b.y);
}

function renderMinimap(sc) {
  const w = MAP_GRID * MINI_TW;
  const h = MAP_GRID * MINI_TH;
  miniCanvas.width = w;
  miniCanvas.height = h;
  miniCtx.fillStyle = "#14141e";
  miniCtx.fillRect(0, 0, w, h);
  const sheetKey = sc.outdoor ? "outb_32" : "townb_32";
  const table = cartoTable();

  for (let sy = 0; sy < MAP_GRID; sy++) {
    const diskY = MAP_GRID - 1 - sy;
    for (let sx = 0; sx < MAP_GRID; sx++) {
      const idx = (diskY << 4) | sx;
      const px = sx * MINI_TW;
      const py = sy * MINI_TH;
      miniCtx.fillStyle = "#444450";
      miniCtx.fillRect(px, py, MINI_TW, MINI_TH);
      const frame = cartoFrame(state.screen, sc.visual[idx], sc.outdoor, table);
      const img = spriteImg(sheetKey, String(frame));
      if (img) miniCtx.drawImage(img, px, py, MINI_TW, MINI_TH);
      if (sc.collision[idx] & 0x80) {
        miniCtx.fillStyle = "rgba(255,65,65,0.85)";
        miniCtx.beginPath();
        miniCtx.arc(px + MINI_TW / 2, py + MINI_TH / 2, 2, 0, Math.PI * 2);
        miniCtx.fill();
      }
    }
  }

  const px = state.x * MINI_TW;
  const py = (MAP_GRID - 1 - state.y) * MINI_TH;
  miniCtx.strokeStyle = "rgba(255,220,0,0.85)";
  miniCtx.lineWidth = 1;
  miniCtx.strokeRect(px + 0.5, py + 0.5, MINI_TW - 1, MINI_TH - 1);
  miniCtx.fillStyle = "rgba(255,220,0,0.25)";
  miniCtx.fillRect(px + 1, py + 1, MINI_TW - 2, MINI_TH - 2);
  const dirFrame = [0x20, 0x22, 0x21, 0x23][state.facing & 3];
  const arr = spriteImg(sheetKey, String(dirFrame));
  if (arr) miniCtx.drawImage(arr, px, py, MINI_TW, MINI_TH);
  miniCtx.strokeStyle = "#787887";
  miniCtx.strokeRect(0, 0, w, h);
}

function draw() {
  if (!maps || !manifest) return;
  const sc = maps.screens[state.screen];
  viewCanvas.width = VIEW_W * SCALE;
  viewCanvas.height = VIEW_H * SCALE;
  viewCtx.imageSmoothingEnabled = false;
  viewCtx.setTransform(SCALE, 0, 0, SCALE, 0, 0);

  if (sc.outdoor) {
    const grid = new StitchedOutdoor(maps.screens, state.screen);
    const scene = buildOutdoorScene(grid, state.x, state.y, state.facing, maps.screens, terrainLookup());
    renderOutdoorView(viewCtx, scene);
    locMode.textContent = "outdoor 3D";
  } else {
    const grid = new StitchedVisual(maps.screens, state.screen);
    const scene = buildIndoorScene(grid, state.x, state.y, state.facing);
    renderIndoorView(viewCtx, sc, scene);
    locMode.textContent = `${sc.env} interior`;
  }

  renderMinimap(sc);
  locName.textContent = sc.name;
  locScreen.textContent = String(state.screen);
  locTile.textContent = `${tileLabel(state.x, state.y)} (${state.x},${state.y})`;
  locFacing.textContent = FACE[state.facing];
  if (locCeiling) {
    locCeiling.textContent = sc.outdoor
      ? "open sky"
      : skyFrameFor(sc, state.x, state.y) ? "ceiling (sky.32 f1)" : "open (sky.32 f0)";
  }
}

function stepForward() {
  const next = stepParty(state.facing, state.x, state.y, state.screen, maps.screens);
  applyMove(next);
}

function stepBackward() {
  const next = stepParty((state.facing + 2) & 3, state.x, state.y, state.screen, maps.screens);
  applyMove(next);
}

function applyMove(next) {
  const crossed = next.screen !== state.screen;
  state.screen = next.screen;
  state.x = next.x;
  state.y = next.y;
  screenSelect.value = String(state.screen);
  statusEl.textContent = crossed
    ? `Entered screen ${next.screen} — ${maps.screens[next.screen].name}`
    : `Moved to ${tileLabel(state.x, state.y)}`;
  draw();
}

function turnLeft() {
  state.facing = (state.facing + 3) & 3;
  statusEl.textContent = `Facing ${FACE[state.facing]}`;
  draw();
}

function turnRight() {
  state.facing = (state.facing + 1) & 3;
  statusEl.textContent = `Facing ${FACE[state.facing]}`;
  draw();
}

function jumpEntry() {
  const sc = maps.screens[state.screen];
  state.x = sc.entry[0];
  state.y = sc.entry[1];
  statusEl.textContent = `Entry ${tileLabel(state.x, state.y)}`;
  draw();
}

function bindControls() {
  document.getElementById("btnEntry").addEventListener("click", jumpEntry);
  document.getElementById("btnForward").addEventListener("click", stepForward);
  document.getElementById("btnBack").addEventListener("click", stepBackward);
  document.getElementById("btnLeft").addEventListener("click", turnLeft);
  document.getElementById("btnRight").addEventListener("click", turnRight);

  screenSelect.addEventListener("change", () => {
    state.screen = Number(screenSelect.value);
    jumpEntry();
  });

  window.addEventListener("keydown", (ev) => {
    if (ev.target instanceof HTMLSelectElement) return;
    switch (ev.key) {
      case "ArrowUp":
      case "w":
      case "W":
        ev.preventDefault();
        stepForward();
        break;
      case "ArrowDown":
      case "s":
      case "S":
        ev.preventDefault();
        stepBackward();
        break;
      case "ArrowLeft":
      case "a":
      case "A":
        ev.preventDefault();
        turnLeft();
        break;
      case "ArrowRight":
      case "d":
      case "D":
        ev.preventDefault();
        turnRight();
        break;
      default:
        break;
    }
  });
}

async function init() {
  bindControls();
  try {
    const data = await loadData();
    maps = data.maps;
    manifest = data.manifest;
    if (data.sprites) await loadSpriteTable(data.sprites);
    else await loadSpritesFromPaths();
  } catch (err) {
    document.getElementById("loadError").hidden = false;
    document.getElementById("loadError").textContent =
      `Could not load walker data (${err.message}). Run: python tools/export_map_walker.py`;
    statusEl.textContent = "No data.";
    return;
  }

  for (const sc of maps.screens) {
    const opt = document.createElement("option");
    opt.value = String(sc.id);
    opt.textContent = `${String(sc.id).padStart(2, "0")} — ${sc.name}`;
    screenSelect.appendChild(opt);
  }

  state.screen = 0;
  jumpEntry();
  statusEl.textContent = "W/↑ forward · S/↓ back · A/D turn · like view3d_indoor.py";
}

init();

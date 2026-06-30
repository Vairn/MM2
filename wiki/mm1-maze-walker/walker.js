/** MM1 map walker — indoor dungeons + converted overland (MM2 outdoor 3D). */
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
  tileLabel,
  torchBlitFor,
} from "./view3d.js";
import { StitchedOutdoor, buildOutdoorScene } from "./outdoor3d.js";

const SCALE = 3;
const MINI_TW = 14;
const MINI_TH = 11;

const state = {
  screen: 0,
  x: 8,
  y: 8,
  facing: 0,
};

let maps = null;
let manifest = null;
const images = new Map();

const viewCanvas = document.getElementById("view");
const miniCanvas = document.getElementById("minimap");
const viewCtx = viewCanvas.getContext("2d");
const miniCtx = miniCanvas.getContext("2d");
const statusEl = document.getElementById("status");
const screenSelect = document.getElementById("screenSelect");
const locName = document.getElementById("locName");
const locSlug = document.getElementById("locSlug");
const locTile = document.getElementById("locTile");
const locScreen = document.getElementById("locScreen");
const locFacing = document.getElementById("locFacing");
const locMode = document.getElementById("locMode");
const chkNoclip = document.getElementById("chkNoclip");

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
    /* split layout */
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

const WALL_DIR_SHIFT = [6, 4, 2, 0]; // N, E, S, W — matches view3d DIR_PAIR_SHIFT / ScummVM DIRMASK_* 

function wallNibble(v, dir) {
  return (v >> WALL_DIR_SHIFT[dir & 3]) & 3;
}

const WALL_MINI_COLORS = [
  null,
  "rgba(110,110,125,0.95)",
  "rgba(220,176,56,0.95)",
  "rgba(96,196,112,0.95)",
];

function renderWallMinimap(sc) {
  const w = MAP_GRID * MINI_TW;
  const h = MAP_GRID * MINI_TH;
  miniCanvas.width = w;
  miniCanvas.height = h;
  miniCtx.fillStyle = "#14141e";
  miniCtx.fillRect(0, 0, w, h);
  const wallW = Math.max(1, Math.floor(MINI_TW / 5));

  for (let sy = 0; sy < MAP_GRID; sy++) {
    const diskY = MAP_GRID - 1 - sy;
    for (let sx = 0; sx < MAP_GRID; sx++) {
      const idx = (diskY << 4) | sx;
      const v = sc.visual[idx];
      const c = sc.collision[idx];
      const px = sx * MINI_TW;
      const py = sy * MINI_TH;
      const blocked = [0, 1, 2, 3].some((d) => (c >> (d * 2)) & 1);
      miniCtx.fillStyle = blocked ? "#3a2828" : "#524038";
      miniCtx.fillRect(px, py, MINI_TW, MINI_TH);
      for (let d = 0; d < 4; d++) {
        const code = wallNibble(v, d);
        const col = WALL_MINI_COLORS[code];
        if (!col) continue;
        miniCtx.fillStyle = col;
        if (d === 0) miniCtx.fillRect(px, py, MINI_TW, wallW); // N
        if (d === 1) miniCtx.fillRect(px + MINI_TW - wallW, py, wallW, MINI_TH); // E
        if (d === 2) miniCtx.fillRect(px, py + MINI_TH - wallW, MINI_TW, wallW); // S
        if (d === 3) miniCtx.fillRect(px, py, wallW, MINI_TH); // W
      }
      if (c & 0x80) {
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
  miniCtx.strokeStyle = "#787887";
  miniCtx.strokeRect(0, 0, w, h);
}

function renderMinimap(sc) {
  // MM1 page 0 is MapWalls on every screen (dungeons and overland).
  renderWallMinimap(sc);
}

function draw() {
  if (!maps || !manifest) return;
  const sc = maps.screens[state.screen];
  viewCanvas.width = VIEW_W * SCALE;
  viewCanvas.height = VIEW_H * SCALE;
  viewCtx.imageSmoothingEnabled = false;
  viewCtx.setTransform(SCALE, 0, 0, SCALE, 0, 0);

  if (sc.outdoor && !sc.mapWalls) {
    const grid = new StitchedOutdoor(maps.screens, state.screen);
    const scene = buildOutdoorScene(
      grid,
      state.x,
      state.y,
      state.facing,
      maps.screens,
      terrainLookup()
    );
    renderOutdoorView(viewCtx, scene);
    locMode.textContent = sc.conversion
      ? `overland (${sc.conversion})`
      : "overland (MM2 terrain)";
  } else if (sc.outdoor && sc.mapWalls) {
    const grid = new StitchedVisual(maps.screens, state.screen);
    const scene = buildIndoorScene(grid, state.x, state.y, state.facing);
    renderIndoorView(viewCtx, sc, scene);
    const bio = sc.sectorLabel || sc.biome || "overland";
    locMode.textContent = `overland ${bio} (MM1 MapWalls)`;
  } else {
    const grid = new StitchedVisual(maps.screens, state.screen);
    const scene = buildIndoorScene(grid, state.x, state.y, state.facing);
    renderIndoorView(viewCtx, sc, scene);
    locMode.textContent = `${sc.env} interior (MM2 walls)`;
  }

  renderMinimap(sc);
  locName.textContent = sc.name;
  locSlug.textContent = sc.slug ? `${sc.slug}.ovr` : "—";
  locScreen.textContent = String(state.screen);
  locTile.textContent = `${tileLabel(state.x, state.y)} (${state.x},${state.y})`;
  locFacing.textContent = FACE[state.facing];
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

function stepForward() {
  applyMove(
    stepParty(state.facing, state.x, state.y, state.screen, maps.screens, chkNoclip.checked)
  );
}

function stepBackward() {
  applyMove(
    stepParty(
      (state.facing + 2) & 3,
      state.x,
      state.y,
      state.screen,
      maps.screens,
      chkNoclip.checked
    )
  );
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

  miniCanvas.addEventListener("click", (ev) => {
    const rect = miniCanvas.getBoundingClientRect();
    const scaleX = miniCanvas.width / rect.width;
    const scaleY = miniCanvas.height / rect.height;
    const x = (ev.clientX - rect.left) * scaleX;
    const y = (ev.clientY - rect.top) * scaleY;
    const tileX = Math.floor(x / MINI_TW);
    const tileY = MAP_GRID - 1 - Math.floor(y / MINI_TH);
    if (tileX >= 0 && tileX < MAP_GRID && tileY >= 0 && tileY < MAP_GRID) {
      state.x = tileX;
      state.y = tileY;
      statusEl.textContent = `Teleported to ${tileLabel(state.x, state.y)}`;
      draw();
    }
  });
  miniCanvas.style.cursor = "crosshair";

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
      `Could not load walker data (${err.message}). Run: python tools/export_mm1_map_walker.py`;
    statusEl.textContent = "No data.";
    return;
  }

  for (const sc of maps.screens) {
    const opt = document.createElement("option");
    opt.value = String(sc.id);
    const tag = sc.outdoor ? " [outdoor]" : "";
    opt.textContent = `${String(sc.id).padStart(2, "0")} — ${sc.name}${tag}`;
    screenSelect.appendChild(opt);
  }

  state.screen = 0;
  jumpEntry();
  statusEl.textContent = "W/↑ forward · overland uses MM1 MAZEDATA walls";
}

init();

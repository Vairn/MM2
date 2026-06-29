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
  TORCH_PHASE_COUNT,
  K_CARTO_TILE_FALLBACK,
} from "./view3d.js";
import { StitchedOutdoor, buildOutdoorScene } from "./outdoor3d.js";
import {
  initFont,
  resolveEventText,
  drawViewportOverlay,
  drawNarrativePanel,
  resolveNarrativeLayoutOp,
  buildMinimapMarkers,
  drawMinimapMarker,
  eventTitleForEvent,
  scriptedEventPositions,
  collisionAmbientPositions,
  screenHasAmbientSteps,
  isAmbientStepTile,
  formatAmbientStepMessage,
  drawMinimapAmbient,
  formatEventScriptFlow,
} from "./ui.js";
import { runEventScript } from "./eventVm.js";
import {
  createSessionState,
  formatSessionPartyPanel,
  isTileCleared as sessionTileCleared,
} from "./sessionState.js";
import {
  SCREEN_W,
  SCREEN_H,
  drawPlayScreenChrome,
  drawPlayViewportDivider,
  drawPlayStatusBar,
  drawPlayPartyPanel,
  drawPlayRightColumn,
} from "./playScreen.js";

const SCALE = 3;
const MINI_TW = 14;
const MINI_TH = 11;

/** Indoor torch flicker: one phase step per poll of key_read_3d @0x1E9CE (~vblank). */
const TORCH_TICKS_PER_PHASE = 2;
/** ViewportAnmOverlay flipbook fallback when no sequence table (62.anm etc.). */
const ANM_FLIPBOOK_DELAY = 5;

let animTick = 0;
let animRafId = 0;
let lastAnimTorchPhase = -1;
let lastAnimSpriteKey = "";

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
const uiCanvas = document.getElementById("uiLayer");
const miniCanvas = document.getElementById("minimap");
const viewCtx = viewCanvas.getContext("2d");
const uiCtx = uiCanvas.getContext("2d");
const miniCtx = miniCanvas.getContext("2d");
const statusEl = document.getElementById("status");
const screenSelect = document.getElementById("screenSelect");
const locName = document.getElementById("locName");
const locTile = document.getElementById("locTile");
const locScreen = document.getElementById("locScreen");
const locFacing = document.getElementById("locFacing");
const locMode = document.getElementById("locMode");
const locCeiling = document.getElementById("locCeiling");
const eventScriptEl = document.getElementById("eventScript");
const partyStateEl = document.getElementById("partyState");
const editGoldEl = document.getElementById("editGold");
const editDayEl = document.getElementById("editDay");
const chkNoclip = document.getElementById("chkNoclip");

/** @type {ReturnType<createSessionState>} */
let session = createSessionState();
/** @type {object[] | null} */
let lastVmTrace = null;
/** @type {number | null} */
let lastVmCond = null;

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
  if (!manifest) return;
  const tasks = [];
  const loadFromMap = (mapObj) => {
    if (!mapObj) return;
    for (const [key, sheet] of Object.entries(mapObj)) {
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
  };
  loadFromMap(manifest.sheets);
  loadFromMap(manifest.anm);
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

function torchPhase() {
  return Math.floor(animTick / TORCH_TICKS_PER_PHASE) % TORCH_PHASE_COUNT;
}

function anmFrameCount(sheetKey) {
  const entry = manifest?.anm?.[sheetKey];
  if (!entry?.frames) return 0;
  return Object.keys(entry.frames).length;
}

/** Cycle composed .anm cel index (ViewportAnmOverlay flipbook / sequence fallback). */
function resolveAnmFrame(sheetKey, baseFrame = 0) {
  const n = anmFrameCount(sheetKey);
  if (n <= 1) return String(baseFrame);
  const step = Math.floor(animTick / ANM_FLIPBOOK_DELAY) % n;
  return String(step);
}

function renderIndoorView(ctx, sc, scene, phase) {
  const sheets = selectIndoorSheets(sc.env);
  const skyFrame = skyFrameFor(sc, state.x, state.y);
  blitTransparent(ctx, sheets.floor, "0", ORIGIN_X, FLOOR_Y);
  blitTransparent(ctx, sheets.sky, String(skyFrame), ORIGIN_X, SKY_Y);
  for (const b of scene.blits) {
    blitTransparent(ctx, sheets.walls, String(b.frame), b.x, b.y);
    const tb = torchBlitFor(b, phase);
    if (tb) blitTransparent(ctx, sheets.torch, String(tb.frame), tb.x, tb.y);
  }
}

function renderOutdoorView(ctx, scene) {
  blitTransparent(ctx, "outf_32", "0", ORIGIN_X, FLOOR_Y);
  blitTransparent(ctx, "sky_32", "0", ORIGIN_X, SKY_Y);
  for (const b of scene.decor) blitTransparent(ctx, b.sheet, String(b.frame), b.x, b.y);
  for (const b of scene.horizon) blitTransparent(ctx, b.sheet, String(b.frame), b.x, b.y);
}

function triggerMatchesFacing(flags, facing) {
  const facingBit = 0x80 >> (facing & 3);
  return (flags & facingBit) !== 0 || (flags & 0xf0) === 0xf0;
}

/** Preview OP_04/05/06 overlay when standing on a trigger tile (matches facing). */
function eventOverlayPreviewAt(sc, x, y, facing) {
  if (!sc?.events) return null;
  const pos = (y << 4) | x;
  for (const evtData of Object.values(sc.events)) {
    for (const trig of evtData.triggers) {
      if (trig.pos !== pos || !triggerMatchesFacing(trig.flags, facing)) continue;
      for (const node of evtData.nodes ?? []) {
        if (node.op === 0x04) {
          const strIdx = node.args[0];
          if (strIdx < sc.strings.length) {
            return { type: "door", text: resolveEventText(sc.strings[strIdx]) };
          }
        }
        if (node.op === 0x05) {
          const strIdx = node.args[0];
          if (strIdx < sc.strings.length) {
            return { type: "wall", text: resolveEventText(sc.strings[strIdx]) };
          }
        }
        if (node.op === 0x06) {
          const strIdx = node.args[0];
          if (strIdx < sc.strings.length) {
            return { type: "signpost", text: resolveEventText(sc.strings[strIdx]) };
          }
        }
      }
    }
  }
  return null;
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
  const minimapMarkers = buildMinimapMarkers(sc);
  const ambientFlagged = collisionAmbientPositions(sc);
  const scriptedTiles = scriptedEventPositions(sc);
  const showStepZone = screenHasAmbientSteps(sc);

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
      const marker = minimapMarkers.get(idx);
      if (marker) {
        drawMinimapMarker(miniCtx, px, py, MINI_TW, MINI_TH, marker);
      } else if (ambientFlagged.has(idx)) {
        drawMinimapAmbient(miniCtx, px, py, MINI_TW, MINI_TH, { flagged: true });
      } else if (showStepZone && isAmbientStepTile(sc, idx)) {
        drawMinimapAmbient(miniCtx, px, py, MINI_TW, MINI_TH, { flagged: false });
      } else if (sc.collision[idx] & 0x80 && scriptedTiles.has(idx)) {
        if (sessionTileCleared(session, state.screen, idx)) {
          miniCtx.fillStyle = "rgba(120,120,120,0.55)";
          miniCtx.beginPath();
          miniCtx.arc(px + MINI_TW / 2, py + MINI_TH / 2, 2, 0, Math.PI * 2);
          miniCtx.fill();
        } else {
          miniCtx.fillStyle = "rgba(255,65,65,0.85)";
          miniCtx.beginPath();
          miniCtx.arc(px + MINI_TW / 2, py + MINI_TH / 2, 2, 0, Math.PI * 2);
          miniCtx.fill();
        }
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

/** @type {{ type: 'text'|'prompt', layoutOp: number, vmOp: number | null, text: string, showSpace: boolean, resolve: Function, sprite?: object } | null} */
let uiState = null;

function drawUI() {
  uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);
  if (!uiState) {
    uiCanvas.style.pointerEvents = "none";
    return;
  }
  uiCanvas.style.pointerEvents = "auto";

  if (uiState.type === "text" || uiState.type === "prompt") {
    /* Narrative modal — lower console band rows 17..23 (EventTextView / doc 44),
     * NOT centered on the 208×120 viewport. */
    drawNarrativePanel(uiCtx, {
      op: uiState.layoutOp,
      vmOp: uiState.vmOp,
      text: uiState.text,
      showSpace: uiState.type === "text" && uiState.showSpace,
    });
  }
}

function updateUI() {
  drawUI();
  if (uiState) {
    requestAnimationFrame(updateUI);
  }
}

function animNeedsRedraw() {
  if (!maps) return false;
  const sc = maps.screens[state.screen];
  const tp = torchPhase();
  if (!sc.outdoor && tp !== lastAnimTorchPhase) return true;
  if (uiState?.sprite) {
    const key = `${uiState.sprite.sheet}:${resolveAnmFrame(uiState.sprite.sheet, Number(uiState.sprite.frame) || 0)}`;
    if (key !== lastAnimSpriteKey) return true;
  }
  return false;
}

function syncAnimCache() {
  if (!maps) return;
  const sc = maps.screens[state.screen];
  lastAnimTorchPhase = sc.outdoor ? -1 : torchPhase();
  if (uiState?.sprite) {
    lastAnimSpriteKey = `${uiState.sprite.sheet}:${resolveAnmFrame(
      uiState.sprite.sheet,
      Number(uiState.sprite.frame) || 0
    )}`;
  } else {
    lastAnimSpriteKey = "";
  }
}

function startAnimLoop() {
  if (animRafId) return;
  const loop = () => {
    animTick++;
    if (animNeedsRedraw()) {
      syncAnimCache();
      draw();
    }
    animRafId = requestAnimationFrame(loop);
  };
  animRafId = requestAnimationFrame(loop);
}

function waitForSpace(text, sprite = null, vmOp = null) {
  const resolved = resolveEventText(text);
  return new Promise(resolve => {
    uiState = {
      type: "text",
      layoutOp: resolveNarrativeLayoutOp(resolved, vmOp),
      vmOp,
      text: resolved,
      showSpace: true,
      resolve,
      sprite,
    };
    draw();
    updateUI();
  });
}

function promptCombatResult(enc) {
  const text = `${enc.heading}!\n\nPress V = victory (set combat latch)\nPress N = flee (abort script)`;
  return new Promise((resolve) => {
    uiState = {
      type: "prompt",
      layoutOp: 0x02,
      vmOp: 0x12,
      text,
      showSpace: false,
      resolve: (v) => resolve(v),
      combatPrompt: true,
    };
    draw();
    updateUI();
  });
}

function syncPartyEditorsFromSession() {
  if (editGoldEl) editGoldEl.value = String(session.gold);
  if (editDayEl) editDayEl.value = String(session.day);
  if (partyStateEl) partyStateEl.textContent = formatSessionPartyPanel(session, manifest);
}

function applyPartyEditorsToSession() {
  if (editGoldEl) session.gold = Math.max(0, parseInt(editGoldEl.value, 10) || 0);
  if (editDayEl) session.day = Math.max(1, parseInt(editDayEl.value, 10) || 1);
  syncPartyEditorsFromSession();
  draw();
}

function promptYesNo(text, sprite = null, vmOp = null) {
  const resolved = resolveEventText(text);
  return new Promise(resolve => {
    uiState = {
      type: "prompt",
      layoutOp: resolveNarrativeLayoutOp(resolved, vmOp),
      vmOp,
      text: resolved,
      showSpace: false,
      resolve,
      sprite,
    };
    draw();
    updateUI();
  });
}

function handleUIKey(ev) {
  if (!uiState) return false;
  if (uiState.type === "text" && (ev.code === "Space" || ev.key === "Enter")) {
    const resolve = uiState.resolve;
    uiState = null;
    uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);
    resolve();
    draw();
    return true;
  }
  if (uiState.type === "prompt") {
    if (uiState.combatPrompt) {
      if (ev.key.toLowerCase() === "v") {
        const resolve = uiState.resolve;
        uiState = null;
        uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);
        resolve(true);
        draw();
        return true;
      }
      if (ev.key.toLowerCase() === "n" || ev.code === "Space" || ev.key === "Escape") {
        const resolve = uiState.resolve;
        uiState = null;
        uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);
        resolve(false);
        draw();
        return true;
      }
      return true;
    }
    if (ev.key.toLowerCase() === "y") {
      const resolve = uiState.resolve;
      uiState = null;
      uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);
      resolve(true);
      draw();
      return true;
    }
    if (ev.key.toLowerCase() === "n" || ev.code === "Space" || ev.key === "Escape") {
      const resolve = uiState.resolve;
      uiState = null;
      uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);
      resolve(false);
      draw();
      return true;
    }
  }
  return true; // block other input while UI is active
}
function drawPlayHud(ctx) {
  drawPlayViewportDivider(ctx);
  drawPlayStatusBar(ctx, 1, 1, FACE[state.facing], true);
  drawPlayPartyPanel(ctx, []);
  drawPlayRightColumn(ctx, "options");
}

function draw() {
  if (!maps || !manifest) return;
  const sc = maps.screens[state.screen];
  viewCanvas.width = SCREEN_W * SCALE;
  viewCanvas.height = SCREEN_H * SCALE;
  viewCtx.imageSmoothingEnabled = false;
  viewCtx.setTransform(SCALE, 0, 0, SCALE, 0, 0);

  uiCanvas.width = SCREEN_W * SCALE;
  uiCanvas.height = SCREEN_H * SCALE;
  uiCtx.imageSmoothingEnabled = false;
  uiCtx.setTransform(SCALE, 0, 0, SCALE, 0, 0);
  uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);

  drawPlayScreenChrome(viewCtx);

  if (sc.outdoor) {
    const grid = new StitchedOutdoor(maps.screens, state.screen);
    const scene = buildOutdoorScene(grid, state.x, state.y, state.facing, maps.screens, terrainLookup());
    renderOutdoorView(viewCtx, scene);
    locMode.textContent = "outdoor 3D";
  } else {
    const grid = new StitchedVisual(maps.screens, state.screen);
    const scene = buildIndoorScene(grid, state.x, state.y, state.facing);
    renderIndoorView(viewCtx, sc, scene, torchPhase());
    locMode.textContent = `${sc.env} interior`;
  }

  /* OP_04/05/06 persistent viewport text — on viewCtx so modal uiLayer clears do not erase it. */
  drawViewportOverlay(
    viewCtx,
    viewportOverlay ?? eventOverlayPreviewAt(sc, state.x, state.y, state.facing)
  );

  /* OP_0B / encounter sprites over the 3D hood while narrative text sits in the lower band. */
  if (uiState?.sprite) {
    const frame = resolveAnmFrame(uiState.sprite.sheet, Number(uiState.sprite.frame) || 0);
    const img = spriteImg(uiState.sprite.sheet, frame);
    if (img) {
      const sx = 8 + Math.floor((208 - img.width) / 2);
      const sy = 8 + Math.floor((120 - img.height) / 2);
      viewCtx.drawImage(img, sx, sy);
    }
  }

  drawPlayHud(viewCtx);

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

  // Update events sidebar + standing-tile event hint
  let eventText = "No events.";
  let standingEventHint = null;
  const pos = (state.y << 4) | state.x;
  const tileEvents = [];
  if (sc.events) {
    for (const [evtId, evtData] of Object.entries(sc.events)) {
      for (const trig of evtData.triggers) {
        if (trig.pos === pos) {
          tileEvents.push({ id: evtId, data: evtData, flags: trig.flags });
        }
      }
    }
    if (tileEvents.length > 0) {
      eventText = tileEvents.map(e => {
        const flagStr = `0x${e.flags.toString(16).padStart(2, '0').toUpperCase()}`;
        const title = `--- ${eventTitleForEvent(e.data)} — Event ${e.id} (cond: ${flagStr}) ---`;
        const flow = formatEventScriptFlow(e.data, manifest, lastVmTrace);
        return `${title}\n${flow}`;
      }).join('\n\n');
      for (const e of tileEvents) {
        if (!triggerMatchesFacing(e.flags, state.facing)) continue;
        standingEventHint = eventTitleForEvent(e.data);
        break;
      }
    }
  }
  if (tileEvents.length === 0) {
    const ambientMsg = formatAmbientStepMessage(sc);
    if (ambientMsg && isAmbientStepTile(sc, pos)) {
      eventText = ambientMsg;
      if (!standingEventHint) standingEventHint = "Ambient random encounter";
    } else if (ambientMsg && collisionAmbientPositions(sc).has(pos)) {
      eventText = `${ambientMsg}\n(collision event flag, no script)`;
      if (!standingEventHint) standingEventHint = "Ambient random encounter";
    }
  }
  eventScriptEl.textContent = eventText;
  if (lastVmCond != null && !isEventRunning) {
    eventScriptEl.textContent = `${eventText}\n\n--- VM ---\nlast cond=${lastVmCond}`;
  }
  if (lastEventNotes.length && !uiState && !isEventRunning) {
    const noteBlock = lastEventNotes.map(n => `[walker] ${n}`).join("\n");
    const base = eventScriptEl.textContent;
    if (base !== "No events.") {
      eventScriptEl.textContent = `${base}\n\n--- last run ---\n${noteBlock}`;
    } else if (noteBlock) {
      eventScriptEl.textContent = `--- last run ---\n${noteBlock}`;
    }
  }
  syncPartyEditorsFromSession();
  if (standingEventHint && !uiState && !isEventRunning) {
    statusEl.textContent = `${standingEventHint} at ${tileLabel(state.x, state.y)} — step or turn to trigger`;
  }

  if (uiState) drawUI();
}

function stepForward() {
  const next = stepParty(state.facing, state.x, state.y, state.screen, maps.screens, terrainLookup(), chkNoclip.checked);
  applyMove(next);
}

function stepBackward() {
  const next = stepParty((state.facing + 2) & 3, state.x, state.y, state.screen, maps.screens, terrainLookup(), chkNoclip.checked);
  applyMove(next);
}

function applyMove(next) {
  const crossed = next.screen !== state.screen;
  state.screen = next.screen;
  state.x = next.x;
  state.y = next.y;
  viewportOverlay = null;
  lastEventNotes = [];
  screenSelect.value = String(state.screen);
  statusEl.textContent = crossed
    ? `Entered screen ${next.screen} — ${maps.screens[next.screen].name}`
    : `Moved to ${tileLabel(state.x, state.y)}`;
  draw();
  checkEvents();
}

function turnLeft() {
  state.facing = (state.facing + 3) & 3;
  viewportOverlay = null;
  statusEl.textContent = `Facing ${FACE[state.facing]}`;
  draw();
  checkEvents();
}

function turnRight() {
  state.facing = (state.facing + 1) & 3;
  viewportOverlay = null;
  statusEl.textContent = `Facing ${FACE[state.facing]}`;
  draw();
  checkEvents();
}

function jumpEntry() {
  const sc = maps.screens[state.screen];
  state.x = sc.entry[0];
  state.y = sc.entry[1];
  viewportOverlay = null;
  statusEl.textContent = `Entry ${tileLabel(state.x, state.y)}`;
  draw();
  checkEvents();
}

let isEventRunning = false;
/** @type {import('./ui.js').ViewportOverlay} */
let viewportOverlay = null;
/** @type {string[]} */
let lastEventNotes = [];

async function runEvent(evtData, strings) {
  isEventRunning = true;
  lastVmTrace = [];
  try {
    const result = await runEventScript({
      evtData,
      strings,
      screenId: state.screen,
      tileX: state.x,
      tileY: state.y,
      manifest,
      maps,
      session,
      resolveEventText,
      waitForSpace,
      promptYesNo,
      promptCombatResult,
      onDraw: () => draw(),
      onSessionChange: () => syncPartyEditorsFromSession(),
      onVmStep: ({ trace, cond }) => {
        lastVmTrace = trace;
        lastVmCond = cond;
        draw();
      },
      onViewportOverlay: (overlay) => {
        viewportOverlay = overlay;
        draw();
      },
      onTeleport: (dest, tx, ty) => {
        state.screen = dest;
        state.x = tx;
        state.y = ty;
        viewportOverlay = null;
        session.combatVictory = false;
        screenSelect.value = String(state.screen);
        statusEl.textContent = `Entered ${maps.screens[dest]?.name ?? `screen ${dest}`}`;
        draw();
      },
      onPatchTile: (ty, tx, visual, collision) => {
        const sc = maps.screens[state.screen];
        if (!sc) return;
        const idx = (ty << 4) | tx;
        if (visual != null && sc.visual) sc.visual[idx] = visual;
        if (collision != null && sc.collision) sc.collision[idx] = collision;
      },
    });
    lastEventNotes = result.notes;
    lastVmTrace = result.trace ?? lastVmTrace;
    lastVmCond = session.cond;
    syncPartyEditorsFromSession();
    if (result.teleported) {
      checkEvents();
    } else if (result.notes.length) {
      statusEl.textContent = result.notes[result.notes.length - 1];
    }
    draw();
  } finally {
    isEventRunning = false;
  }
}

function checkEvents() {
  if (!maps) return;
  const sc = maps.screens[state.screen];
  if (!sc.events) return;

  const pos = (state.y << 4) | state.x;

  for (const [evtId, evtData] of Object.entries(sc.events)) {
    for (const trig of evtData.triggers) {
      if (trig.pos === pos) {
        if (triggerMatchesFacing(trig.flags, state.facing)) {
          // Use setTimeout to allow the canvas to render before showing alerts
          setTimeout(() => runEvent(evtData, sc.strings), 10);
          return;
        }
      }
    }
  }
}

function bindControls() {
  document.getElementById("btnEntry").addEventListener("click", () => { if (!uiState && !isEventRunning) jumpEntry(); });
  document.getElementById("btnForward").addEventListener("click", () => { if (!uiState && !isEventRunning) stepForward(); });
  document.getElementById("btnBack").addEventListener("click", () => { if (!uiState && !isEventRunning) stepBackward(); });
  document.getElementById("btnLeft").addEventListener("click", () => { if (!uiState && !isEventRunning) turnLeft(); });
  document.getElementById("btnRight").addEventListener("click", () => { if (!uiState && !isEventRunning) turnRight(); });

  uiCanvas.addEventListener("click", (ev) => {
    if (!uiState) return;
    if (uiState.type === "text") {
      const resolve = uiState.resolve;
      uiState = null;
      uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);
      resolve();
      draw();
    } else if (uiState.type === "prompt") {
      const rect = uiCanvas.getBoundingClientRect();
      const x = ev.clientX - rect.left;
      const resolve = uiState.resolve;
      uiState = null;
      uiCtx.clearRect(0, 0, SCREEN_W, SCREEN_H);
      resolve(x < rect.width / 2);
      draw();
    }
  });

  if (editGoldEl) editGoldEl.addEventListener("change", applyPartyEditorsToSession);
  if (editDayEl) editDayEl.addEventListener("change", applyPartyEditorsToSession);

  miniCanvas.addEventListener("click", (ev) => {
    if (uiState) return;
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
      viewportOverlay = null;
      statusEl.textContent = `Teleported to ${tileLabel(state.x, state.y)}`;
      draw();
      checkEvents();
    }
  });
  
  miniCanvas.style.cursor = "crosshair";

  screenSelect.addEventListener("change", () => {
    if (uiState || isEventRunning) {
      screenSelect.value = String(state.screen);
      return;
    }
    state.screen = Number(screenSelect.value);
    jumpEntry();
  });

  window.addEventListener("keydown", (ev) => {
    if (ev.target instanceof HTMLSelectElement) return;
    if (handleUIKey(ev)) return;
    if (isEventRunning) return;
    
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
    if (manifest.font) initFont(manifest.font);
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
  syncPartyEditorsFromSession();
  jumpEntry();
  syncAnimCache();
  startAnimLoop();
  statusEl.textContent = "W/↑ forward · S/↓ back · A/D turn · like view3d_indoor.py";
}

init();

/** MM2 map maze walker — 16×16 screens from map.dat collision geometry. */

const GRID = 16;
const CELL = 32;

/** @typedef {{ id:number, name:string, outdoor:boolean, entry:number[], neighbors:number[], visual:number[], collision:number[] }} Screen */

/** @type {{ version:number, screens: Screen[] } | null} */
let data = null;

const state = {
  screen: 0,
  x: 7,
  y: 6,
  facing: 0, // 0=N 1=E 2=S 3=W
};

const canvas = document.getElementById("map");
const ctx = canvas.getContext("2d");
const statusEl = document.getElementById("status");
const screenSelect = document.getElementById("screenSelect");
const locName = document.getElementById("locName");
const locTile = document.getElementById("locTile");
const locScreen = document.getElementById("locScreen");
const locFacing = document.getElementById("locFacing");
const locEvent = document.getElementById("locEvent");

function tileLabel(x, y) {
  return `${String.fromCharCode(97 + x)}${y + 1}`;
}

function screenRow(y) {
  return GRID - 1 - y;
}

function idx(x, y) {
  return y * GRID + x;
}

function wallN(c) {
  return c & 1;
}
function wallE(c) {
  return (c >> 2) & 1;
}
function wallS(c) {
  return (c >> 4) & 1;
}
function wallW(c) {
  return (c >> 6) & 1;
}

function visualField(v, dir) {
  return (v >> (dir * 2)) & 3;
}

/** @param {Screen} sc */
function canExit(sc, x, y, dx, dy) {
  const nx = x + dx;
  const ny = y + dy;
  if (nx >= 0 && nx < GRID && ny >= 0 && ny < GRID) {
    return canMove(sc, x, y, nx, ny);
  }
  const c = sc.collision[idx(x, y)];
  if (dy > 0) return !wallN(c);
  if (dy < 0) return !wallS(c);
  if (dx > 0) return !wallE(c);
  if (dx < 0) return !wallW(c);
  return false;
}

/** @param {Screen} sc */
function canMove(sc, x, y, nx, ny) {
  const c = sc.collision[idx(x, y)];
  const nc = sc.collision[idx(nx, ny)];
  const v = sc.visual[idx(x, y)];
  const nv = sc.visual[idx(nx, ny)];

  if (ny > y) {
    if (wallN(c) || wallS(nc)) return false;
    if (visualField(v, 0) === 1 && visualField(nv, 2) === 1) return false;
    return true;
  }
  if (ny < y) {
    if (wallS(c) || wallN(nc)) return false;
    if (visualField(v, 2) === 1 && visualField(nv, 0) === 1) return false;
    return true;
  }
  if (nx > x) {
    if (wallE(c) || wallW(nc)) return false;
    if (visualField(v, 1) === 1 && visualField(nv, 3) === 1) return false;
    return true;
  }
  if (nx < x) {
    if (wallW(c) || wallE(nc)) return false;
    if (visualField(v, 3) === 1 && visualField(nv, 1) === 1) return false;
    return true;
  }
  return false;
}

/** @param {Screen} sc */
function stepEdge(sc, x, y) {
  let screen = state.screen;
  let nx = x;
  let ny = y;
  const n = sc.neighbors;

  if (nx < 0) {
    if (n[3] >= 0 && n[3] < data.screens.length) {
      screen = n[3];
      nx = GRID - 1;
    }
  } else if (nx >= GRID) {
    if (n[1] >= 0 && n[1] < data.screens.length) {
      screen = n[1];
      nx = 0;
    }
  }
  if (ny < 0) {
    if (n[2] >= 0 && n[2] < data.screens.length) {
      screen = n[2];
      ny = GRID - 1;
    }
  } else if (ny >= GRID) {
    if (n[0] >= 0 && n[0] < data.screens.length) {
      screen = n[0];
      ny = 0;
    }
  }

  nx = Math.max(0, Math.min(GRID - 1, nx));
  ny = Math.max(0, Math.min(GRID - 1, ny));
  return { screen, x: nx, y: ny };
}

function tryMove(dx, dy, facing) {
  if (!data) return;
  const sc = data.screens[state.screen];
  const nx = state.x + dx;
  const ny = state.y + dy;

  if (!canExit(sc, state.x, state.y, dx, dy)) {
    statusEl.textContent = "Blocked by wall.";
    state.facing = facing;
    draw();
    return;
  }

  const next = stepEdge(sc, nx, ny);
  const crossed = next.screen !== state.screen;
  state.screen = next.screen;
  state.x = next.x;
  state.y = next.y;
  state.facing = facing;
  screenSelect.value = String(state.screen);
  statusEl.textContent = crossed
    ? `Entered screen ${next.screen} — ${data.screens[next.screen].name}`
    : `Moved to ${tileLabel(state.x, state.y)}`;
  draw();
}

function jumpEntry() {
  if (!data) return;
  const sc = data.screens[state.screen];
  state.x = sc.entry[0];
  state.y = sc.entry[1];
  statusEl.textContent = `Spawn at entry ${tileLabel(state.x, state.y)}`;
  draw();
}

function draw() {
  if (!data) return;
  const sc = data.screens[state.screen];
  canvas.width = GRID * CELL;
  canvas.height = GRID * CELL;

  for (let sy = 0; sy < GRID; sy++) {
    const y = GRID - 1 - sy;
    for (let x = 0; x < GRID; x++) {
      const i = idx(x, y);
      const col = sc.collision[i];
      const vis = sc.visual[i];
      const px = x * CELL;
      const py = sy * CELL;

      const terrain = sc.outdoor ? vis & 0x1f : (vis >> 2) & 0x3f;
      const hue = sc.outdoor ? 80 + (terrain * 7) % 60 : 0;
      const sat = sc.outdoor ? 35 + (terrain * 5) % 25 : 0;
      const lit = sc.outdoor ? 18 + (terrain * 3) % 12 : 12;
      ctx.fillStyle = sc.outdoor
        ? `hsl(${hue} ${sat}% ${lit}%)`
        : col & 0x2a
          ? "#120808"
          : "#1a0505";
      ctx.fillRect(px, py, CELL, CELL);

      if (col & 0x80) {
        ctx.fillStyle = "rgba(255, 99, 99, 0.35)";
        ctx.beginPath();
        ctx.arc(px + CELL / 2, py + CELL / 2, 5, 0, Math.PI * 2);
        ctx.fill();
      }

      const doorN = visualField(vis, 0) === 3;
      const doorE = visualField(vis, 1) === 3;
      const doorS = visualField(vis, 2) === 3;
      const doorW = visualField(vis, 3) === 3;

      ctx.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue("--wall");
      ctx.lineWidth = 3;

      if (wallN(col) || doorN) {
        ctx.strokeStyle = doorN ? "#d4a017" : "#eb3838";
        ctx.beginPath();
        ctx.moveTo(px, py);
        ctx.lineTo(px + CELL, py);
        ctx.stroke();
      }
      if (wallE(col)) {
        ctx.strokeStyle = doorE ? "#d4a017" : "#eb3838";
        ctx.beginPath();
        ctx.moveTo(px + CELL, py);
        ctx.lineTo(px + CELL, py + CELL);
        ctx.stroke();
      }
      if (wallS(col)) {
        ctx.strokeStyle = doorS ? "#d4a017" : "#eb3838";
        ctx.beginPath();
        ctx.moveTo(px, py + CELL);
        ctx.lineTo(px + CELL, py + CELL);
        ctx.stroke();
      }
      if (wallW(col)) {
        ctx.strokeStyle = doorW ? "#d4a017" : "#eb3838";
        ctx.beginPath();
        ctx.moveTo(px, py);
        ctx.lineTo(px, py + CELL);
        ctx.stroke();
      }
    }
  }

  const px = state.x * CELL + CELL / 2;
  const py = screenRow(state.y) * CELL + CELL / 2;
  const ang = [ -Math.PI / 2, 0, Math.PI / 2, Math.PI][state.facing];
  ctx.fillStyle = "#ffe566";
  ctx.beginPath();
  ctx.moveTo(px + Math.cos(ang) * 10, py + Math.sin(ang) * 10);
  ctx.lineTo(px + Math.cos(ang + 2.4) * 8, py + Math.sin(ang + 2.4) * 8);
  ctx.lineTo(px + Math.cos(ang - 2.4) * 8, py + Math.sin(ang - 2.4) * 8);
  ctx.closePath();
  ctx.fill();
  ctx.strokeStyle = "#000";
  ctx.lineWidth = 1;
  ctx.stroke();

  ctx.strokeStyle = "#731a1a";
  ctx.lineWidth = 2;
  ctx.strokeRect(0, 0, canvas.width, canvas.height);

  locName.textContent = sc.name;
  locScreen.textContent = `${state.screen} (${sc.outdoor ? "overland" : "interior"})`;
  locTile.textContent = tileLabel(state.x, state.y);
  locFacing.textContent = ["N", "E", "S", "W"][state.facing];
  locEvent.textContent = sc.collision[idx(state.x, state.y)] & 0x80 ? "yes" : "no";
}

function bindControls() {
  document.getElementById("btnEntry").addEventListener("click", jumpEntry);
  document.getElementById("btnNorth").addEventListener("click", () => tryMove(0, 1, 0));
  document.getElementById("btnSouth").addEventListener("click", () => tryMove(0, -1, 2));
  document.getElementById("btnWest").addEventListener("click", () => tryMove(-1, 0, 3));
  document.getElementById("btnEast").addEventListener("click", () => tryMove(1, 0, 1));

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
        tryMove(0, 1, 0);
        break;
      case "ArrowDown":
      case "s":
      case "S":
        ev.preventDefault();
        tryMove(0, -1, 2);
        break;
      case "ArrowLeft":
      case "a":
      case "A":
        ev.preventDefault();
        tryMove(-1, 0, 3);
        break;
      case "ArrowRight":
      case "d":
      case "D":
        ev.preventDefault();
        tryMove(1, 0, 1);
        break;
      case "q":
      case "Q":
        state.facing = (state.facing + 3) & 3;
        draw();
        break;
      case "e":
      case "E":
        state.facing = (state.facing + 1) & 3;
        draw();
        break;
      default:
        break;
    }
  });
}

async function init() {
  bindControls();
  try {
    const res = await fetch("maps.json");
    if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
    data = await res.json();
  } catch (err) {
    document.getElementById("loadError").hidden = false;
    document.getElementById("loadError").textContent =
      `Could not load maps.json (${err.message}). Run: python tools/export_map_walker.py`;
    statusEl.textContent = "No map data.";
    return;
  }

  for (const sc of data.screens) {
    const opt = document.createElement("option");
    opt.value = String(sc.id);
    opt.textContent = `${String(sc.id).padStart(2, "0")} — ${sc.name}`;
    screenSelect.appendChild(opt);
  }

  state.screen = 0;
  jumpEntry();
  statusEl.textContent = "Ready — WASD or arrow keys to walk.";
}

init();

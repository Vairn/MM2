# MM2 Wiki

Documentation for **Might and Magic II (Amiga)** reverse engineering. Two targets:

| Target | Use case |
|--------|----------|
| **[GitHub Wiki](https://github.com/Vairn/MM2/wiki)** | Hosted on GitHub — no build step |
| **VitePress** (`npm run dev`) | Local preview with MM2 font/theme |

Source of truth stays in [`EXTRACTED/docs/README.md`](../EXTRACTED/docs/README.md),
`EXTRACTED/docs/`, `tools/`, and `editor/README.md`.

---

## GitHub Wiki (recommended for hosting)

### One-time setup

1. On GitHub: **Settings → General → Features → Wikis** → enable **Wiki**.
2. Locally, have MM2 data files in the repo root (`monsters.dat`, `book.32`, `map.dat`, …) for the sprite gallery.

### Export & publish

```powershell
# Build wiki/gh-wiki/ (Home.md, _Sidebar.md, docs, images/)
python wiki/scripts/export-github-wiki.py

# Push to https://github.com/Vairn/MM2/wiki
python wiki/scripts/publish-github-wiki.py
```

Docs-only (no game assets needed):

```powershell
python wiki/scripts/export-github-wiki.py --skip-gallery
python wiki/scripts/publish-github-wiki.py --no-export
```

Dry-run push:

```powershell
python wiki/scripts/publish-github-wiki.py --dry-run
```

### GitHub Actions

**Actions → Sync GitHub Wiki → Run workflow**

- Default: docs-only (`skip_gallery=true`) — works without game binaries in CI.
- For a full gallery sync, run `export-github-wiki.py` locally (with assets) then `publish-github-wiki.py`.

Output is written to `wiki/gh-wiki/` (gitignored) and pushed to the separate **`MM2.wiki`** git repository that backs the GitHub Wiki UI.

---

## VitePress (local preview)

```bash
cd wiki
npm install
npm run dev
```

`npm run sync` copies markdown into `wiki/docs/` and regenerates the VitePress gallery under `wiki/public/gallery/`.

---

## Scripts

| Script | Purpose |
|--------|---------|
| `wiki/scripts/export-github-wiki.py` | Build GitHub Wiki tree → `wiki/gh-wiki/` |
| `wiki/scripts/publish-github-wiki.py` | Git push `gh-wiki/` → `MM2.wiki.git` |
| `wiki/scripts/export-gfx-gallery.py` | VitePress MM2 gallery only |
| `wiki/scripts/export-pc-gfx-gallery.py` | VitePress PC DOS `.4`/`.16` gallery |
| `wiki/scripts/export-mm1-gallery.py` | VitePress MM1 WALLPIX gallery (lagdotcom exports) |
| `wiki/scripts/export-wiki-diagrams.py` | PNG flowcharts → `gh-wiki/images/diagrams/` |
| `wiki/wiki_enrichments.py` | Per-page nav, TOC, diagrams, See also footers |
| `tools/mm2_gfx_export.py` | Core Amiga sprite decoder/exporter |
| `tools/decode_pc_gfx.py` | PC DOS `.4`/`.16` decoder |
| `tools/pc_gfx_export.py` | PC gallery sync for VitePress |
| `tools/export_map_walker.py` | Export `walker-bundle.js` for the HTML map walker |

---

## Map walker (interactive)

First-person **3D** explorer for all 60 `map.dat` screens — same ASM frustum pipeline as
`tools/view3d_indoor.py`, `tools/view3d_outdoor.py`, and the editor Map section (wall
sprites, sky/floor backdrops, cartography minimap).

**https://vairn.github.io/MM2/maze-walker/**

```powershell
# Amiga bundle + PC CGA/EGA PNG trees (pass GOG install for .4/.16)
python tools/export_map_walker.py --pc-gog "C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2"
cd wiki/maze-walker
python -m http.server 8080
# http://localhost:8080/
```

The bundle embeds all 60 map screens plus Amiga `.32`/`.anm` sprite frames as base64.
**PC wallsets** live under `pc/cga/` and `pc/ega/` (lazy-loaded `manifest.json` + PNGs).
Commit `walker-bundle.js` and `pc/**` then push — GitHub Actions deploys Pages.

Toolbar: **Graphics** (Amiga / PC CGA / PC EGA) and **Wallset** (Auto / Town / Cavern / Castle).
PC sprites use Amiga pen-0 masks where applicable; decode via `parse_wall_sheet` (packed u32).
Bump `PC_GFX_BUILD_ID` in `version.js` after re-exporting PC trees.

Controls match the Python/C++ viewers: **W/↑** step forward, **S/↓** back,
**A/←** turn left, **D/→** turn right.

### MM1 walker (DOS `MAZEDATA.DTA`, indoor only)

Same frustum engine, **55 screens**, map names from `MM.EXE` slug table @ `0x10C07`.
Uses MM2 `town.32` / `cave.32` as stand-in wall art.

```powershell
python tools/export_mm1_map_walker.py
cd wiki/mm1-maze-walker
python -m http.server 8081
# http://localhost:8081/
```

**One-time GitHub Pages setup** (required before the first deploy):

1. Open [MM2 → Settings → Pages](https://github.com/Vairn/MM2/settings/pages).
2. Under **Build and deployment**, set **Source** to **GitHub Actions** (not
   “Deploy from a branch”).
3. Re-run the failed workflow: **Actions → Deploy Map Walker → Re-run all jobs**.

Until step 2 is done, `GET /repos/Vairn/MM2/pages` returns **404** and
`actions/configure-pages` fails with “Get Pages site failed”. The default
`GITHUB_TOKEN` cannot create the site via the REST API — a repo admin must
enable Pages in Settings (or use a PAT with `administration:write` +
`pages:write` and `enablement: true` on `configure-pages`).

---

## GitHub Wiki layout

```
wiki/gh-wiki/
  Home.md              ← landing page
  _Sidebar.md          ← navigation (GitHub Wiki feature)
  _Footer.md
  Overview.md          ← from EXTRACTED/docs/
  …
  Gallery.md
  Monster-Sprites.md
  images/
    book-f00.png
    gallery/           ← decoded .32 / .anm sprites
```

Images use relative paths (`images/gallery/...`) so they render on GitHub Wiki.

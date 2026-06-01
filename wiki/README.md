# MM2 Wiki

Documentation for **Might and Magic II (Amiga)** reverse engineering. Two targets:

| Target | Use case |
|--------|----------|
| **[GitHub Wiki](https://github.com/Vairn/MM2/wiki)** | Hosted on GitHub — no build step |
| **VitePress** (`npm run dev`) | Local preview with MM2 font/theme |

Source of truth stays in `EXTRACTED/docs/`, `tools/`, and `editor/README.md`.

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
| `wiki/scripts/export-gfx-gallery.py` | VitePress gallery only |
| `wiki/scripts/export-book-logo.py` | `book.32` frame 0 logo |
| `tools/mm2_gfx_export.py` | Core sprite decoder/exporter |
| `tools/export_map_walker.py` | Export `maps.json` for the HTML map walker |

---

## Map walker (interactive)

Top-down **HTML5** maze explorer for all 60 `map.dat` screens (collision walls,
`attrib.dat` screen links). GitHub Wiki cannot run JavaScript — the app is hosted
on **GitHub Pages**:

**https://vairn.github.io/MM2/maze-walker/**

```powershell
python tools/export_map_walker.py
cd wiki/maze-walker
python -m http.server 8080
# http://localhost:8080/
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

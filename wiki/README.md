# MM2 Wiki

Static documentation site for the Might and Magic II (Amiga) reverse-engineering
notes in this repository. Built with [VitePress](https://vitepress.dev/).

Content is **not duplicated** — `npm run sync` copies markdown from
`EXTRACTED/docs/`, `tools/RE-TOOLS.md`, `editor/README.md`, and related paths
into `wiki/docs/` before each dev/build.

## Run locally

```bash
cd wiki
npm install
npm run dev
```

Open the URL printed in the terminal (default `http://localhost:5173`).

## Build static site

```bash
cd wiki
npm run build
npm run preview   # optional — serve dist/
```

Output lands in `wiki/.vitepress/dist/` (gitignored).

## Edit content

Change the source files under `EXTRACTED/docs/` (or the tool/editor READMEs),
then restart dev or re-run sync. Do **not** edit files under `wiki/docs/` —
they are regenerated.

Navigation and the home page live in:

- `wiki/index.md`
- `wiki/.vitepress/config.mts`

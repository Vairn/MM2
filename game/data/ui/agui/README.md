# Agui play HUD art pack

Native-resolution UI sprites for `--play-ui=agui`.

## Rules (anti-shithouse)

1. **Paint on the final grid.** Icons are drawn as **12×12**, faces as **28×28**. Concept sheets may supply **colours only** — never scale a soft 1024px image into the pack.
2. **Locked palette** — [`palette.json`](palette.json). No dither.
3. **Review at 1:1** — `python tools/ui_pack_preview.py`.

Disabled on purpose: `gen_agui_seed_art.py` (rectangles), `remaster_agui_art.py` (NEAREST crush).

## Workflow

```powershell
python tools/author_agui_pixel_art.py
python tools/author_agui_doll.py
python tools/pack_agui_ui.py
python tools/ui_pack_preview.py
```

## Item icons (paper doll)

`--play-ui=agui` character sheet uses a paper-doll + backpack grid. **You** generate
the 12×12 item sprites with an image AI — the repo does not invent them.

1. Prompt: [`ITEM_ICON_PROMPT.md`](ITEM_ICON_PROMPT.md)
2. Ingest: `python tools/ingest_agui_item_icons.py --from-dir <exports>`
   or `--sheet batch.png --start 0x01 --cols 8 --cell 96`
3. Drop results in [`items/`](items/) as `i01.png`…`iff.png`, then re-pack.

Doll chrome (`doll/body`, `doll/slot`) is authored on-grid by `author_agui_doll.py`.
Classic `--play-ui=classic` keeps the original Equipped 1–6 / Backpack A–F text list.

## Runtime

## Runtime

`--play-ui=agui` / `MM2_PLAY_UI=agui`. Atlas search paths: `ui/agui`, `data/ui/agui`, `game/data/ui/agui`.

## AGA follow-on

Planar pens 32–63 not packed yet — `EXTRACTED/docs/41-aga-port-plan.md` §10.

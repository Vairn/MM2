# Item icons (user-supplied)

12×12 indexed PNGs named `i01.png` … `iff.png` (hex id, lowercase). Skip `i00`.

Do not generate these in-repo. Prompt + ingest:

- [`../ITEM_ICON_PROMPT.md`](../ITEM_ICON_PROMPT.md)
- `python tools/ingest_agui_item_icons.py --from-dir <exports>`
- `python tools/pack_agui_ui.py`

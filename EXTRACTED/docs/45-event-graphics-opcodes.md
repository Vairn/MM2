# Event VM — Graphics-Related Opcodes

> **Corak ghost / Pegasus full illustration / castle overlays:** see
> [46-scripted-scene-graphics.md](46-scripted-scene-graphics.md) — viewport overlay
> (`0x316E` / mode `$17`). **`0x6FB8` is audio** (`play_sound_seq`) — [58-amiga-audio-in-exe.md](58-amiga-audio-in-exe.md).
> `0x64F8` is scripted text+wait, not the Paula title theme.

Which `event.dat` script opcodes (`0x00..0x32`) reach **sprites, `.32` sheets,
or `.anm` animation** (not plain 8×8 text). Companion docs:
[07-event-script-opcodes.md](07-event-script-opcodes.md),
[44-event-text-rendering.md](44-event-text-rendering.md),
[30-event-to-string-path.md](30-event-to-string-path.md),
[15-3d-view-and-game-screen.md](15-3d-view-and-game-screen.md).

ASM source: `EXTRACTED/mm2.capstone.annotated.asm` (dispatch @ `0x172CA`,
table @ `0x17494`).

---

## Short answer

| Question | Answer |
|----------|--------|
| **Only opcode that loads a non-text graphic asset from the event VM** | **`OP_0B`** — service signboard `.anm` over the 3D viewport |
| **Corak “spirit proclaims…” intro** | **Not** title-screen gfx; **not** `OP_0B`. Text lives in **event.dat loc 60** string bank; playback is an **overlay / castle-blob / scripted-scene** path (**text + optional music**, no Corak sprite traced) |
| **Guardian Pegasus outside (C2)** | **Text only** (`OP_01` + `OP_0B` direction sign). **No** pegasus `.32`/`.anm` in overland or map-transition code |
| **Title pegasus** | **Separate boot path** (`intro.32` + `introclips.32` @ `0x25FCE` / `0x26A1E`) — see [38-title-screen-and-intro-assets.md](38-title-screen-and-intro-assets.md), [39-title-screen-animation.md](39-title-screen-animation.md) |

---

## Opcode catalog (graphics relevance)

Classification: **Sprite** = `.anm`/planar sheet blit; **Font chrome** = control glyphs from font 8;
**Indirect** = handler delegates to another engine that draws gfx; **None** = text/state only.

| Op | Handler | Gfx? | Asset / mechanism | Placement | Example (loc · event) |
|----|---------|------|-------------------|-----------|------------------------|
| `01`–`05` | `0x15924`–`0x15A46` | Font chrome only | 8×8 font; `OP_05`/`06` use divider glyphs (`0x0E`–`0x15`, `0x7E`) | Rows 3–22 over viewport / panel | `01` loc 00 evt 20 — gate y/n text |
| `06` | `0x15AEE` | Font chrome | Popup B draws box frame via `-$7C62` before text | Cells (7,7)–(19,14) | loc 08 evt 01 — “Dead Zone” sign |
| `07` | `0x15CE6` | **Side effect** | In 3D mode (`-$79E2==0`), wait loop calls `key_read_3d` @ `0x1E9CE` → torch `-$7C20` blits from `-$7A1E` | Viewport wall torches | Any `OP_07` after a text block in dungeon/town |
| `08` | `0x15D26` | Same as `07` | Wraps `0x15CE6(1)` (scripted key replay) | — | — |
| **`0B`** | **`0x15DB0`** | **Sprite** | **`sign_sprite_load` @ `0x316E`** → **`0x9A30(id)`** loads **`.anm`**; mode **`$16`** prep @ `-$7BAE`; **`sign_sprite_place` @ `0x3266`** → mode **`$17`** overlay; table **`A4-$7538`** (24 slots); handle **`A4-$79FE`** | Over **3D viewport** (208×120 @ px 8,8); arg2 = placement index passed to `sign_sprite_place(pos, $40, $20)` | loc 00 evt 24 — `OP_0B` “Lock and Key LTD”; loc 11 evt 04 — Pegasus tile sign `"<- Castle / Pinehurst"` |
| `0C` | `0x15E12` | Indirect | `-$7FDA` map init → full **3D hood redraw** (walls from env `.32`); **no** event overlay sprite | New screen coords from args | loc 00 evt 20 — exit to overland `map_transition(0x0B, 0x37)` |
| `0D` | `0x15EC4` | None | `-$7E42(index)` → **`play_sound_seq`** @ `0x6FB8` (ids 0..9; [doc 58](58-amiga-audio-in-exe.md)) | — | e.g. `OP_0D 0x09` before map transition |
| `0E` | `0x160C2` | Indirect | Shop/temple/inn handlers (`0x1A132`, `0x1C54A`, …) — UI is mostly **text**; no VM blit in stub | Service windows | loc 00 — `exec_selector(0x01)` tavern |
| `0F`–`11` | — | None | Control flow | — | — |
| **`12`/`13`** | **`0x16300`/`0x16386`** | **Indirect** | **`-$7EDE` / `-$7F1A`** enter combat → **`0x316E`** layers up to **24 monster `.anm`** sprites (`A4-$7538`) | Viewport slots | loc 22 evt 09 — crypt guardian `encounter_setup` |
| `14` | `0x16398` | None | Clears tile event flag | — | loc 11 evt 04 — after Pegasus speech |
| `15`–`20` | — | None | Party bytes / effects | — | — |
| **`21`** | **`0x16A34`** | **Map gfx** | Writes visual/collision bytes to **`-$55BA` / `-$54BA`** → **permanent tile graphics** on next draw | Named `(y,x)` | loc 22 evt 04 — Corak's Cave barrier drop |
| `22`–`32` | — | None | Predicates, vars, treasure bytes, password | — | — |

### Opcodes `0x0D`–`0x32` with **no** traced sprite/`.32`/`.anm` path from the handler stub

`0D`, `0E` (stub only), `0F`, `10`, `11`, `14`, `15`–`20`, `22`–`32` — surveyed via handler entry in `mm2.capstone.annotated.asm` and cross-refs in docs 07/17/28. None call `-$7C20`, mode `$16`/`$17`, or `0x9A30` except where noted above.

---

## `OP_0B` — service signboard (only direct VM sprite op)

### Handler flow (`0x15DB0`)

1. Read `u8 str_idx`, `u8 pos` from script.
2. `0x15756(str_idx)` → **sign id** stored in `A4-$55C8` (maps string index → id using per-town tables @ `A4-$6C62`…`-$6C02` and env id `A4-$79E3`).
3. `sign_sprite_place(-1, 0, 0)` @ `0x3266` — clear prior `A4-$79FE`.
4. `sign_sprite_load(id)` @ `0x316E` — if can't-see (`A4-$79E1`), clear viewport preset #8; else load sheet via **`0x9A30`**: `subq #1` @ `$31E8` then `addq #1` @ `$9A4C` before the decimal loop — **net `id` → `"NN.anm"`** in scratch, gfx mode **`$16`** prep.
5. `sign_sprite_place(pos, $40, $20)` — mode **`$17`** draws board over viewport.
6. Sets exit flag bit 2 (`A4-$7950`).

**Note on “mode $17”:** doc 44 uses **$17** for the **graphics-library primitive mode** inside `-$7BAE` @ `0x23C8C`, **not** the `OP_0B` script arg2. Decoded scripts use **`mode=0x00`** on the second byte; observed **placement** values include `0x00`, `0x07`, `0x09`, `0x12`, `0x31`, etc. (placement index into `A4-$7538`, not a display mode).

### Cleanup (`0x171AC`)

When bit 2 set: `sign_sprite_place(-1,0,0)`; **`buf_copy_rect`** restores viewport `(8,8)–(215,127)` from back buffer.

### Worked examples

| Location | Event | Script fragment | String / role |
|----------|-------|-----------------|---------------|
| **00** Middlegate | 24 | `OP_0B` str[11], pos `0x00` | “Lock and Key LTD” shop sign |
| **03** Vulcania | 2 | `OP_0B` str[2] | “Bestway Blacksmith” |
| **11** C2 overland | **04** | `OP_0B` str[14], then `OP_01` str[5] | Direction sign + **“Guardian Pegasus…”** dialogue (text only) |
| **47** Woodhaven D1 | — | `OP_0B` str[7], pos `0x31` | “No ROBBERS!” dungeon class gate |

---

## Corak intro — trace

### What the player sees (FAQ)

Middlegate map `(7,4)` **[FIRST TIME]**: *“The spirit of Corak proclaims, ‘Fantastic adventure awaits you…’”* ([FAQ](Might%20and%20Magic%20FAQ.txt) §3-2-2). Related strings also cover Feldecarb Fountain `(15,15)`, Nordon `(10,2)`, etc.

### Where the text lives

| Source | Content |
|--------|---------|
| **event.dat loc 60** | String bank: `str[8]` = Corak proclamation; `str[9]` Nordon; `str[23]` Feldecarb Fountain; … ([loc_60](events/loc_60_quest_nordon_nordonna_corak.md)) |
| **loc 60 decode** | **0 bytes** in the standard script pool; tile triplets look like **placeholders** for overlay/queued use |
| **loc 00 Middlegate** | **No** trigger at `(7,4)`; fountain at `(15,15)` evt 17 uses **different** copy (`str[9]` clairvoyance fountain, not Feldecarb) |

### Runtime path (ASM — partial)

| Mechanism | Address | Role |
|-----------|---------|------|
| **Not** title screen | `0x25FCE` / `0x26A1E` | Pegasus art is pre-game only ([doc 38/39](38-title-screen-and-intro-assets.md)) |
| **Not** `OP_0B` | — | No Corak `.anm` signboard tied to intro |
| **`play_song_scripted`** | `0x64F8` | Table **`A4-$73C4`**: text lines + `-$7BE4` draw + wait; used @ **`0x78E6`** when **`A4-$79B2 == 2`** (script id **3** on stack) — **music + text**, no sprite load |
| **Queued dispatch** | `0x176B6` | Secondary event runner for overlay/castle records ([08-event-runtime.md](08-event-runtime.md)) |
| **Castle blobs** | loc **63** / **65** / **68** | Non-standard `event.dat` records; likely bytecode + triggers without `00 00 00` terminator |
| **`first_time_flag`** | `A4-$79E9` | Toggled @ `0x06EAC` area — candidate gate for one-shot intro UI |

**Conclusion:** Corak intro is **event-overlay text** (loc 60 strings) delivered through **castle/queued/scripted-scene** engine paths, **not** the title attract loop and **not** `OP_0B` sprite placement. No Corak portrait sprite has been traced in any intro handler.

---

## Guardian Pegasus (outside) — trace

### In-game (C2 overland, loc 11)

**Event 04** @ tile `(4,7)` ENTER:

```
apply_party(…) → OP_0B str[14] "<- Castle / Pinehurst" → OP_01 str[5] "Greetings! I'm your Guardian Pegasus…" → wait_key → OP_14
```

- **`OP_0B`**: wooden direction **signboard** `.anm` (same pipeline as inn/shop signs).
- **`OP_01`**: 8×8 text only.
- **No** `introclips.32` / pegasus body art.

### Other Pegasus mentions

| Place | Mechanism |
|-------|-----------|
| **B1** loc 06 | Password gate “What is my name?” — text blocks only |
| **C3** loc 14 | Linguist decodes “The Pegasus is called Meenu” — `OP_01`/`02` text |
| **Title screen** | `intro.32` + `introclips.32` phases 0–4 — **only** here |

### Overland / map transition

- Leaving Middlegate: **`OP_0C`** → `-$7FDA` @ `0x15E12` (standard map load + 3D draw).
- Overland sector view: **`overland_map_view` @ `0x223A`** — scrolls **map.dat** tiles via `-$7C20`; **no** pegasus sheet.
- **`outdoor_3d_master` / `A4-$79E2`**: switches outdoor 3D presentation; still env wall `.32` sets, not pegasus overlay.

**Conclusion:** The winged-horse **picture** is **title-screen-only**. Outside, “Pegasus” is **NPC dialogue + optional signboard**, same as any town service tile.

---

## Remake implementation notes (Phase 4+)

### Phase 4 — Event VM parity

1. **`OP_0B` pipeline** (blocking for shop tiles):
   - Implement `0x15756` string→sign-id tables per `A4-$79E3` env.
   - Load `NN.anm` via `0x9A30` naming convention; decode with existing `.anm` codec.
   - Mode `$16` prep + mode `$17` place @ `0x23C8C` — **placement math still OPEN** (doc 44 §8).
   - 24-entry `A4-$7538` slot table; cleanup viewport blit `(8,8)–(215,127)`.
2. **`OP_06` framed signs** — font chrome only (already in event text demos).
3. **`OP_07` wait** — optional torch flicker if emulating `key_read_3d` (cosmetic).
4. **`OP_12/13`** — route to combat renderer (`0x316E` shared with sign loader for sheet handle).

### Not event VM — separate milestones

| Feature | Engine path | Remake module |
|---------|-------------|---------------|
| Title pegasus | `0x25FCE`, `0x26A1E`, `0x27096` | `TitleScreen.cpp` ([doc 39](39-title-screen-animation.md)) — **done** |
| Corak intro text | loc 60 strings + `0x64F8` / castle blob / queued `@0x176B6` | New **scripted prologue** state: load loc 60 bank, run text ops + scripted audio id 3; trace castle blob before pixel parity |
| Feldecarb / Nordon quest | loc 60 strings; some tiles on loc 00 | Quest controller referencing loc 60 indices; align with FAQ tile coords |
| Overland Pegasus NPC | loc 11 evt 04 | Text + `OP_0B` sign asset only |

### Explicit non-goals (from this trace)

- Do **not** reuse title `introclips` coords for in-game Pegasus.
- Do **not** expect `OP_0C` or overland init to show a full-screen transition illustration.

---

## Related docs & tools

| Resource | Use |
|----------|-----|
| [44-event-text-rendering.md](44-event-text-rendering.md) | `OP_0B` §3.8, mode `$17` gap, cleanup blit |
| [28-town-services.md](28-town-services.md) | `OP_0B` + `OP_0E` shop tile coords |
| [06-gfx-loading.md](06-gfx-loading.md) | `.32`/`.anm` loader `-$7C2C` |
| `tools/decode_event.py` | Per-location disassembly |
| `game/build/event_demos/` | Text opcode screenshots (`OP_01`–`06`, prompts) |

---

## Open gaps

1. **`0x23C8C`** — `sign_sprite_place` pixel math (OP_0B arg2 → screen x/y).
2. **`0x9A30`** — full sign-id → `.anm` index table dump.
3. **Corak `(7,4)` first-time** — which record (loc 60 / castle 63/65 / embedded script) emits `str[8]`; trigger not in loc 00 triplet table.
4. Castle overlay slot tables beyond `0x316E` / mode `$17` (Corak ghost asset confirm).
5. **Feldecarb fountain** — FAQ/loc 60 `str[23]` vs loc 00 evt 17 clairvoyance text mismatch (retail vs FAQ or era-gated overlay).

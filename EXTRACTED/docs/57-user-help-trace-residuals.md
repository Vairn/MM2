# 57 — User help: playtest residuals for RE

**Audience:** players who can run retail Amiga MM2 (or a close remake) and report
what happens. **ASM is god** — if FAQ conflicts, ignore FAQ.

Companion gap inventory: [`56-event-system-remaining-gaps.md`](56-event-system-remaining-gaps.md).
Opcode / runtime refs: [`07-event-script-opcodes.md`](07-event-script-opcodes.md),
[`08-event-runtime.md`](08-event-runtime.md), [`43-exploration-input-and-ingame-options.md`](43-exploration-input-and-ingame-options.md).

**How to report:** map name + tile `(x,y)` facing if known, key pressed, exact
on-screen text, and where the party stands *after*. Screenshots / short notes
beat guesses.

---

## Top 5 things to do right now

1. **Party wipe / flee respawn** — Start a fight outdoors or in Middlegate Cavern,
   let the party die (or `R`un until wipe). Note **exact tile** you land on vs
   where the fight started. Compare to that map’s `attrib.dat` entry square
   (see §1). *Unblocks defeat coord restore.*
2. **Darkness “Can’t see”** — Enter **Corak’s Cave** (loc 22; many dark cells) with
   **no** Light / Lasting Light. Step onto dark tiles; note whether the viewport
   goes black / “Can’t see”, and whether door/sign text still appears. Cast Light
   (+1) or Lasting Light (+20) and re-check. *Unblocks `-$79E1` gate fidelity.*
3. **Rest ambush** — On a **dungeon/cavern event tile** (collision EVENT bit —
   e.g. Middlegate Cavern loc 17), press **`R`**, answer **Y**. Report: Rest OK /
   “Too dangerous!” / ambush fight / nothing. Repeat on a **plain corridor** tile
   (no event). *Unblocks Rest `-$55D6` gate.*
4. **Search after treasure** — Trigger an **OP_2A** chest (e.g. Middlegate Cavern
   treasure scripts, or any “you found…” after a quest reward that arms Search).
   Press **`S`**. Note Identify / rating UI, gold/gems/items lines, and what
   happens with a **full backpack**. *Unblocks Search Identify chrome.*
5. **Town portal tile** — Middlegate **Travel Moore** / portal family (see §9);
   pay and travel. Report destination map + landing tile. Cross-check FAQ portal
   table only after ASM/event.dat agree. *Unblocks default-range / portal leaf audit.*

---

## Residual help sheets

Combat **fight AI / spells / hide** past `0x1635E` is **not** listed — that is
engine depth, not a short playtest ask.

### 1. Defeat coord restore — `-$560C` / `0x1164A`

| | |
|--|--|
| **ASM leaf / A4** | `combat_defeat_retreat` **`0x11646`**; reads **`A4-$560C`**, unpacks lo nibble → `-$79F1` (`coord_b` / X), hi nibble → `-$79F0` (`coord_a` / Y), then funeral audio `-$7E96`. Same unpack pattern at `0x123A`, `0x1C6A`, `0x6F32`, `0x918E`. |
| **Engine** | On party wipe (and related retreat paths), party XY is restored from the **current screen’s attrib entry/safe square**, not from a combat-saved “last step”. |
| **ASM note (2026-07-10)** | **Not a missing combat writer.** `-$560C` = **`attrib.dat` byte `0x0E`** (`entry_coord`) inside the current-screen buffer at `A4-$561A` (loader **`0x923E`** bulk-copies 64 bytes). Mapping: buffer+`0x0E` → `-$560C` (see [`12-attrib-dat-format.md`](12-attrib-dat-format.md)). |
| **How to hit** | Any combat that ends in wipe: random step fight outdoors, arena ticket fight, or scripted `OP_12`/`OP_13`. Also compare **successful Run** vs **full wipe**. Known entry squares (attrib `0x0E`): Middlegate `(7,5)`, Atlantium `(15,15)`, Tundara `(8,11)`, Vulcania `(7,2)`, Sandsobar `(1,8)`, Middlegate Cavern `(15,8)`. |
| **Watch / dump** | After wipe: do you snap to that map’s entry square? Same map or town recall? Flee-without-wipe: coords unchanged? If remake dumps A4: `-$560C`, `-$79F0`, `-$79F1`, `-$79F2` (screen id). |
| **Priority** | **High** — wrong respawn breaks exploration after death. |
| **Remake (2026-07-10)** | **Wired.** `materializeScreenAttrib` copies attrib → `A4-$561A`; `finishLeave(false)` unpacks `-$560C` → coords. Verified Middlegate `(7,5)` etc. from `attrib.dat`. |

### 2. `-$79E1` darkness / can’t-see gate — `0x53C0`

| | |
|--|--|
| **ASM leaf / A4** | `session_interaction_gate` **`0x53A0`**: **`clr.b -$79E1` @ `0x53C0`**; if light `-$79AB==0`, set from `-$5600 >= $80` **or** `btst #5,-$55D6` (`0x53D6`/`0x53E4`). OP_04/05/06 **skip draw** when `-$79E1≠0`. |
| **Engine** | Per play-frame: suppress 3D / event text chrome when the cell is “dark” and the party has no light factor. |
| **ASM note** | Writers **are** in this leaf. Remake gap = call gate every frame with live tile bits + light counter (not “find a secret store”). |
| **How to hit** | **Corak’s Cave** (loc 22) — dense dark collision bits. Also Middlegate Cavern (few dark cells). Towns (e.g. Middlegate) have ~0 dark cells — poor test. Keys: walk; cast Cleric **Light** / **Lasting Light**; burn light by stepping dark squares (each step can consume one light factor — doc 15). |
| **Watch / dump** | Black viewport / “Can’t see” string @ `0x542C`? Do **OP_04/05/06** signs still print in the dark? With light up, do they return? Report `-$79AB` if dumping. |
| **Priority** | **High** for dungeon fidelity / can’t-see text gates. |
| **Remake (2026-07-10)** | **Wired.** `sessionInteractionGate` @ explore tick / move / screen enter maintains `-$79E1` from light + `-$5600` + bit5 of `-$55D6`. |

### 3. Rest exact tile gate — `0x19D7C` (`-$55D6 ≥ $80`)

| | |
|--|--|
| **ASM leaf / A4** | Rest key **`R`** → **`0x19E20`** (`-$7D2E`). Ambush helper **`0x19D64`**: at **`0x19D7C`** if `-$55D6 >= $80` → skip ambush path. Separate: **`btst #3,-$55D6` @ `0x19E32`** → `"Too dangerous!"`. |
| **Engine** | Rest on a tile whose runtime flags still show the **EVENT** high bit can suppress the random ambush branch; bit3 aborts Rest entirely. |
| **How to hit** | **`R`** then **Y** on: (a) cavern/dungeon **event** tiles (collision `0x80` — any triplet in loc 17/22/24…), (b) empty corridor, (c) outdoor wilderness, (d) tiles that feel “dangerous” (traps/special). Hirelings unpaid → different abort string (`Not enough gold…`). |
| **Watch / dump** | Ambush fight vs quiet Rest vs “Too dangerous!”; after Rest, does the tile event still fire when you step? Dump `-$55D6` before/after Rest if possible. |
| **Priority** | **High** — Rest is core exploration; wrong ambush rate is very noticeable. |
| **Remake (2026-07-10)** | **Wired.** Current-cell latch `0x1B1C` → `-$55D6`; Rest ambush uses `>= $80` exact; bit3 → `"Too dangerous!"` @ `0x19E32`. |

### 4. Rest SP recompute — `0x19C30` / `-$7F56`

| | |
|--|--|
| **ASM leaf / A4** | Inside Rest execution (~`0x19B28`…): at **`0x19C30`** spell-level path; **`jsr -$7F56` @ `0x19C6E`** → stub target **`0x4442`** (table walk on `A4-$7486`), then `mulu` with record `+$20` → write **`+$5A`** (SP-related). |
| **Engine** | After a successful Rest, caster SP (and related secondary fields) are recomputed from class/level/stat tables — not a simple “set SP = max”. |
| **How to hit** | Party with a **Sorcerer/Cleric** at known SP; drain SP (cast); **`R`** in a safe town inn-adjacent tile or quiet outdoor square; compare SP before/after. Repeat after **level-up** and after **Endurance** changes if you can. |
| **Watch / dump** | Exact SP numbers pre/post Rest; whether non-casters change; any “rested” condition bits. |
| **Priority** | **Medium** — wrong SP after Rest hurts casters; formula is local once `0x4442` table is dumped. |
| **Remake (2026-07-10)** | **Wired.** Table `A4-$7486` from data hunk; `(bonus+3)*+$20` → `+$5A` then copy to `+$58`. Roster `+$20`/`+$23` are the ASM fields (not `spell_level`/`level`). |
| **Drift bug found (2026-07-17)** | Stock `roster.dat` starters ship the *working* copies `+$20`/`+$23` stuck at their character-creation seed (`+$20=1`, `+$23=0` — see `0x272D6`) while the *canonical* fields `+$71`/`+$72` already hold the real level/spell-level. Rest multiplies by `+$20` directly (`0x19C9A`), and `sync_party_secondary_stats` @ `0x4476` mirrors **`+$20→+$71`** (working→canonical, not the reverse) — so an un-refreshed working copy first stalls Rest SP at the creation value, then can clobber a correct canonical level on the next sync. Fixed in the remake by `gameplay::syncRosterWorkingLevelFields()` (copies `+$71→+$20`, `+$72→+$23`), called from `GameSession::start` (party load) and before `recomputeRestSpellPoints()` in `executeRest`, and seeded in `CombatSession::enter` alongside the existing `+$23` seed. See [`06-roster-format.md`](06-roster-format.md) `$20`/`$23` fields. |

### 5. Search Identify / rating UI — `0x1B19C`

| | |
|--|--|
| **ASM leaf / A4** | Search **`S`** / auto-Search when `-$794C == $FE` → **`-$7D1C` → `0x1B19C`**. Strings include “The Party Has found:”, “Treasure!”, “Identify”, “(rating)”. |
| **Engine** | Presents pending found-item buffer (OP_2A / OP_19 overflow) and runs Identify/rating chrome before/while distributing loot. |
| **How to hit** | Any **OP_2A** tile (inventory: Tundara, A1, B1, D1, Middlegate Cavern, Ice Cavern, Sarakin’s Mine, Murray’s Cave, Dragon’s Dominion, …). Temple elemental-orb path that queues `$FE` then auto-Search. Fill all backpacks then overflow via OP_19. |
| **Watch / dump** | Full UI sequence; Identify key behavior; rating numbers; gold/gems/item assignment order; full-backpack refusal. |
| **Priority** | **Low–Medium** — state distribute already ported; this is presentation. |

### 6. OP_01–03 / OP_0B glyph chrome

| | |
|--|--|
| **ASM leaf / A4** | Text: **`0x15924` / `0x15942` / `0x159CE`**. Sign blit: **`OP_0B` @ `0x15DB0`**. |
| **Engine** | Script text windows and `.anm` shop/sign sprites — EXIT_FLAGS / GS already handled; remaining is glyph layout / centering / sheet pick. |
| **How to hit** | Walk **any town shop front** (Middlegate has many OP_0B). Atlantium Olympic / inn signs. Compare remake vs retail letter spacing, borders, sign art. |
| **Watch / dump** | Screenshots side-by-side; note arg2 / which `.anm` if known. |
| **Priority** | **Low** — chrome only. |

### 7. OP_26 / OP_27 prompt strings

| | |
|--|--|
| **ASM leaf / A4** | Member picker **`0x16BC0`** — GS writes (slot → cond / `-$5D42` / `-$5D3F`, dead reject) are exact; prompt **literals** not yet matched to ASM. |
| **Engine** | “Who …?” party-member selection inside scripts. |
| **How to hit** | **OP_26** common in towns + castles: Middlegate / Atlantium / Tundara skill trainers (“Who will learn this skill (1-8)”), dungeon locs 46–52. **OP_27** rarer: overland **B1** (loc 6), **Hillstone Dungeon L1** (loc 45). |
| **Watch / dump** | Exact prompt wording, ESC abort, dead-member rejection message. |
| **Priority** | **Low** — strings only. |

### 8. Default-range portals / selectors — `0x15EDC`

| | |
|--|--|
| **ASM leaf / A4** | `OP_0E` default → **`0x15EDC`** bins selector → category `0x3C`–`0x46` → `-$7DFA` re-enters event loader. Named portal travel often **`OP_0E 0x64`** (`-$7D9A`) — different leaf; still audit together. |
| **Engine** | Non-shop selectors: Olympic skills, Nordon/Feldecarb-style bins, misc travel, quest NPCs. |
| **How to hit** | **Town portals** (FAQ table is a *hint* only — confirm in-game): e.g. Middlegate ↔ Sandsobar legs in [`34-commerce-formulas.md`](34-commerce-formulas.md) §3. Middlegate **Travel Moore** sign / portal tiles; Atlantium Olympic “Hurl the spear…” (skill purchase, not arena). Loc 60 string bank (Nordon) via goblet quest tiles. Scripted portals: Murray’s Cave ↔ Dawn’s Mist Bog; Woodhaven L2 → Pinehurst; etc. (inventory sample strings). |
| **Watch / dump** | Selector feel (shop vs travel vs skill); gold cost; landing `(map,x,y)`; one-way Atlantium→Middlegate if you test that leg. |
| **Priority** | **Medium** — many are playable content; leaf-by-leaf audit. |

### 9. Loc ≥ `0x41` pool — location 67 Hall of Spells

| | |
|--|--|
| **ASM leaf / A4** | Outside VM `0x00`–`0x32` dispatcher. Loc **67** `mixed_pool` in `event_location_inventory.json`: opcodes **`OP_41`/`45`/`46`/`48`/`49`/`50`/`54`/`59`/`5A`/`75`** plus normal text/YN/map ops. Likely consumed via **queued** dispatch (`0x176B6`), not the normal tile scanner alone. |
| **Engine** | Hall of Spells / class-gated spell-purchase overlay data mixed with runnable script fragments. |
| **How to hit** | Enter each town’s **Hall of Spells** / mage-guild spell buy flows; note any odd transitions, class checks, or map warps that don’t match loc 0–4 guild scripts. Grep: `python tools/decode_event.py event.dat 67`. Cross-ref [`events/loc_67_hall_of_spells_pool.md`](events/loc_67_hall_of_spells_pool.md). |
| **Watch / dump** | Whether loc 67 bytes ever run in retail (vs loc 70 string bank only); any non-standard opcode visible as weird UI. |
| **Priority** | **Medium** if HoS misbehaves; else **Low** until a call site is proven. |

---

## Quick “how to find content” cheatsheet

| Goal | Command / place |
|------|-----------------|
| Opcode → locations | `EXTRACTED/event_location_inventory.json` `opcode_histogram` |
| One loc decode | `python tools/decode_event.py event.dat <id>` |
| Loc markdown | `EXTRACTED/docs/events/loc_*.md` |
| Entry / safe square | `attrib.dat` byte `0x0E` per screen ([doc 12](12-attrib-dat-format.md)) |
| Dark tiles | Collision page bit layout ([doc 15](15-3d-view-and-game-screen.md)); Corak’s Cave is a dense test |
| Rest / Search keys | [doc 43](43-exploration-input-and-ingame-options.md) §7.4–7.5 |

---

## Deferred (not a playtest ask)

| Item | Why |
|------|-----|
| Combat fight depth past `0x1635E` | Full AI / spells / targeting — multi-week engine, not a single scenario |
| Town OP_0E shop leaf polish | Out of scope for whole-game VM % (see doc 56 §1) |

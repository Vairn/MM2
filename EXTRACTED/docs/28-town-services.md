# Town Services (MM2 Amiga)

Reverse-engineered entry flow, ASM handlers, strings, and per-town tables for
the five surface towns (map screens **0–4**). Cross-references:
`[30-event-to-string-path.md](30-event-to-string-path.md)` (how `event.dat` vs
`str.dat` vs exe text connect), `[07-event-script-opcodes.md](07-event-script-opcodes.md)`
(`OP_0E`, `OP_0B`, `OP_24`), `[06-roster-format.md](06-roster-format.md)`,
`[11-str-decoded.txt](11-str-decoded.txt)`, `[14-game-state-struct.md](14-game-state-struct.md)`,
`[29-embedded-exe-strings.md](29-embedded-exe-strings.md)`.

**Data files:** `event.dat` is in the repo root; full decode:
`[EXTRACTED/event_decode.txt](../event_decode.txt)`,
`[EXTRACTED/event_location_inventory.json](../event_location_inventory.json)`.
`str.dat` may still be absent — UI text from
`[11-str-decoded.txt](11-str-decoded.txt)`. Regenerate:

```bash
python tools/decode_event.py event.dat > EXTRACTED/event_decode.txt
python tools/event_location_inventory.py event.dat EXTRACTED/event_location_inventory.json
python tools/mm2_codec.py str-decode str.dat EXTRACTED/docs/11-str-decoded.txt
```

---

## 1. Common entry flow

### 1.1 Tile trigger → script → service

1. Scanner `**0x1754A**` reads `(y,x)` triplets; event id selects a script
  segment in the location’s `event.dat` record.
2. Typical service tile chain:
  - `**OP_0B**` — service title window (string index + mode byte).
  - Optional y/n text (`OP_01`–`OP_03`, `OP_09`/`OP_0A`).
  - `**OP_0E**` — one selector byte → engine handler (below).
  - `**OP_0F**` — end script.
3. Current map index is in `**A4-$79F2**` (`screen_mode_id`, values **0–4** for
  the five towns). Shop/temple/training handlers index per-town tables with this
   byte (not the roster “home town” byte).

### 1.2 `OP_0E` dispatch (`0x160C2`)

Chained subtraction on the selector byte:


| Selector                                             | Name (decoder)         | Handler                          | Role                                      |
| ---------------------------------------------------- | ---------------------- | -------------------------------- | ----------------------------------------- |
| `0x01`                                               | `open_inn_lodging`     | `0x1A132`                        | Inn rest / roster dismiss                 |
| `0x02`                                               | `open_training`        | `-$7CD4`                         | Training (level-up from XP)               |
| `0x03`                                               | `open_tavern_food`     | `0x1D208`                        | Pub / food / drinks / rumors              |
| `0x04`                                               | `open_temple`          | `-$7D16`                         | Temple services + donations               |
| `0x05`                                               | `open_mages_guild`     | `-$7D10`                         | Mage guild spell shop                     |
| `0x06`                                               | `open_blacksmith_shop` | `0x1C54A`                        | Blacksmith (weapons/armor/specials)       |
| `0x07`                                               | `open_general_store`   | `-$7DB8`                         | General store (Middlegate m11, Vulcania k6 only) |
| `0x08`                                               | `open_arena_shop`      | `-$7DBE`                         | Arena / special shop                      |
| `0x0A`                                               | `goblet_quest`         | `-$7DAC`                         | Goblet / farthing side quest              |
| `0x0D`                                               | `enroll_mages_guild`   | `-$7DA0` (+ `$1A1F8` on success) | Guild membership                          |
| `0x64`, `0x7E`–`0x83`, `0xC9`–`0xCF`, `0xE2`, `0xFD` | specials               | various                          | Portals / quests                          |
| *default*                                            | (category shop)        | `0x15EDC` → `-$7DFA`             | Selector binned to category `0x3C`–`0x46` |


Default-path binning (from `0x15EDC`, range helper `-$7E9C`):


| Selector range | Category | Index adjust |
| -------------- | -------- | ------------ |
| `0x09`–`0x10`  | `0x3C`   | `-8`         |
| `0x11`–`0x37`  | `0x3D`   | `-0x10`      |
| `0x38`–`0x4B`  | `0x3E`   | `-0x37`      |
| `0x4C`–`0x54`  | `0x3F`   | `-0x4B`      |
| `0x56`–`0x5B`  | `0x40`   | `-0x55`      |
| `0x5C`–`0x5E`  | `0x41`   | `-0x5B`      |
| `0x65`–`0x69`  | `0x42`   | `-0x64`      |
| `0x6A`–`0x7C`  | `0x43`   | `-0x69`      |
| `0x97`–`0x98`  | `0x44`   | `-0x96`      |
| `0xE3`–`0xF3`  | `0x45`   | `-0xE2`      |
| `0xF4`–`0xFB`  | `0x46`   | `-0xF3`      |


During default dispatch the engine **temporarily overwrites** `A4-$79F2` with
the category byte, calls `-$7DFA`, then restores the real map index. Explicit
`0x06` blacksmith uses the **real** map index throughout.

Arena / Monster Bowl / guild spell counters use **named selectors** in the
default ranges (e.g. Middlegate `0x10`, Sandsobar `0x49`–`0x54`, Atlantium
Olympic Trial tiles `0x12`–`0x25`).

### 1.3 Payment / gates

- `**OP_24`** — `u16` gold check → `A4-$7951` (`cond_flag`). Observed amounts:
`10`, `25`, `50`, `200`, `500`, `1000`, `2000`, `5000` (training, travel,
Olympic skills, etc.).
- Training calendar mode `**1**` / `**2**` (stat vs HP) is resolved @ `**0x9B48**`
before `**0x9BCA**`. Stat path calls `**-$7EB4**` with mode `**1**` @ `**0x9BC0**`;
HP path enters `**0x9BCA**` when mode `**2**`.

### 1.4 Training / healing cost formulas (FAQ-sourced)

> **FAQ §3-6 (lines 1171-1175).** Closed-form expressions; exact multiply/index
> sequence in `0x9B48`/`0x9BCA`/`0x1D208` not yet ASM-confirmed.

```
Training  = current_level × town_index × 50  gp
Healing   = current_level × town_index × 10  gp
           (dead: × 100 gp total = × 10 gp × 10)
           (eradicated: × 1000 gp total = × 10 gp × 100)
```

Town index: Middlegate=1, Tundara=2, Sandsobar=2, Vulcania=4, Atlantium=5.

See `[34-commerce-formulas.md](34-commerce-formulas.md)` for town-index table,
day-bonus cycle, portals, bar food/drinks, and Fly landing coords.

**Port status (`game/src/events/EventTownServices.cpp`).** The `OP_0E` selectors
1–8 were de-fabricated (see `[07-event-script-opcodes.md](07-event-script-opcodes.md)`
§OP_0E "Port status"): each now shows the real `str.dat` entry text and defers the
interactive A–F menu (the engine handlers `0x1A132` / `-$7CD4` / `0x1D208` /
`-$7D16` / `-$7D10` / `0x1C54A` / `-$7DB8` / `-$7DBE` are unported). The training
(`level × index × 50`) and base living-healing (`level × index × 10`) costs are
implemented as helpers `eventVmTrainingCostPerChar` / `eventVmHealingCostPerChar`
and unit-tested, but charged per-character only inside the deferred menu. The dead
(×10) / eradicated (×100) healing multipliers are a **known gap** — roster `$26`
only groups `$80+` (Dead/Stone/Eradicated, see §5.3 / doc 06) and the
dead-vs-eradicated threshold is not yet ASM-confirmed.

### 1.4.1 Temple handler menu model (`0x1D208`, ASM-traced)

The temple open handler builds RNG caption pointer tables (`A4-$59EE` … `-$57F6`
via `-$7DE2`) then enters a key loop. The **key→leaf** dispatch is a fixed jump
table @ **`0x1D650`** (base `0x1D674`), `key − 'A'` in `0..6`:

| key | jump target | leaf handler | traced behavior |
| --- | ----------- | ------------ | ---------------- |
| A   | `0x1D58E`   | `0x1CA2E`    | pay `A4-$6742[map]`, top each member's food (`record+0x25`) to `0x28` |
| B   | `0x1D59C`   | `0x1CAC4`    | per-member restore loop (cleric/restore caption set `-$580E`) |
| C   | `0x1D5AE`   | `0x1CD2E`    | per-member restore loop (alt field) |
| D   | `0x1D5C0`   | `0x1CFCA`    | cleric-spell / restore leaf |
| E   | `0x1D5CE`   | `0x1D0B4`    | cleric-spell / restore leaf |
| F   | `0x1D5F8`   | (default)    | member-select digit fall-through |
| G   | `0x1D5DC`   | `0x1C9C0(0)` | "Gather Gold" — refresh gold display (deduct 0), select member `-$7E0C` |

Persistent command line strings (literal pool `0x1D68C`+): `"G-Gather Gold"`
(`0x1D699`), `"#-Other Char"` (`0x1D6A7`), `"Select (A-E)"` (`0x1D6B4`). The
six **displayed** option captions are RNG-selected per open (flavor text), so the
manual's A) Restore Cond / B) Restore Algn / C) Donations labels (doc §5) are the
*player-facing* names; the *internal* leaf each key triggers is the table above.

**Heal/restore leaf (`0x1D716`, used by the restore paths):** gold check `0x1D90C`
(per-char `record+0x66` ≥ cost) → on success `0x1DD48` (HP restore) + `clr.b $26`
(condition). HP restore (`0x1DD48`): if `record+$5E` (work max) < `record+$60`
(permanent max), set `$5E` and `$74` (current HP) to `$60`.

**Gold primitive (`0x1C9C0` == `0x1D90C`):** `scc (gold ≥ amount)` then deduct;
no partial spend. The whole temple/training/shop engine charges the **selected
character's own gold**, not a pooled party total.

### 1.4.2 Ported transaction layer (`game/src/events/`)

The faithful leaf mutations above are implemented byte-exact in
`TownServiceTransactions.{h,cpp}` and driven through a swappable UI backend
`ITownServiceUi` (`TownServiceMenu.{h,cpp}`) — interactive selection is pluggable
(CLAUDE.md), the costs/state are ASM-canonical. `eventExecTownSelector` runs the
real temple (`0x03`) / training (`0x04`) menu when `EventRuntime::bindTownServiceUi`
has been set; with no backend it shows the str.dat intro and defers (runtime
behavior unchanged). Per-town constants: `EXTRACTED/decomp/mm2_town_tables.{h,c}`
+ `tools/mm2_town_tables.py`. Unit tests: `game/tests/event_middlegate_test.cpp`
(`testTownServiceTransactions`) — heal deducts `level×index×10` + restores HP +
clears condition; training deducts `level×index×50` + raises the stat (cap +
8-bit overflow guards); donation deducts `A4-$6742[map]` + sets the `A4-$799E`
quest bit (0x1F all-five). See doc 07 §OP_0E "Town-service transaction layer".

### 1.5 Portal connections (FAQ-sourced)

See `[34-commerce-formulas.md](34-commerce-formulas.md)` § Portal connections.
Portal selector bytes (`0x64`, `0x7E`–`0x83`) are handled by the `OP_0E`
default-range path (§1.2 above, entry `0x64`, `0x7E`–`0x83`).

---

## 2. Per-town service matrix (event.dat)

Map index = location id: **0** Middlegate, **1** Atlantium, **2** Tundara,
**3** Vulcania, **4** Sandsobar.

**AUTHORITATIVE placement — `(y,x)/facing` of the `OP_0E` selector tile.** These
are the tiles the runtime tile-event scanner (`0x1754A`) actually triggers on,
decoded byte-exact from `event.dat` (`python tools/decode_event.py --decompile`,
service event = `service_sign(OP_0B) ; exec_selector(OP_0E sel)`). The remake's
`EventRuntime::scanAndRun` reads these same triplets, so port placement is
faithful by construction. (A character's `cond` byte is the facing mask: `W`
`0x10` / `S` `0x20` / `E` `0x40` / `N` `0x80`; `ANY` = `0xF0`.)

> The earlier version of this table was **wrong** (a stale/scrambled guess — it
> e.g. put the Middlegate Smith at `(7,7)`, which is actually the *Training* tile,
> and claimed Tundara had "no `OP_0E` hits"). The Middlegate **Smith `(4,4)/W`**
> is the known-good in-game reference; every other selector is corrected below.

| Town       | Inn `0x01` | Train `0x02` | Tavern `0x03` | Temple `0x04` | Guild `0x05` | Smith `0x06` | Store `0x07` | Notes                                                                              |
| ---------- | ---------- | ------------ | ------------- | ------------- | ------------ | ------------ | ------------ | ---------------------------------------------------------------------------------- |
| Middlegate | (3,7)/S    | (7,10)/E     | (6,4)/W       | (7,7)/N      | (14,7)/N     | (4,4)/W      | (10,12)/S    | arena `0x08` (2,13)/ANY; goblet `0x0A` (2,10)/W; enroll `0x0D` (12,1)/W; portal `0x11` (5,0)/W |
| Atlantium  | (13,9)/S   | (4,10)/E     | (10,12)/E     | (7,4)/W      | (4,5)/W      | (13,6)/S     | —            | arena `0x08` (9,7)/ANY; Olympic/skill tiles `0x12`–`0x25` on separate tiles      |
| Tundara    | (11,7)/W   | (6,11)/S     | (9,7)/W       | (13,11)/N    | (13,14)/S    | (10,11)/E    | —            | full `OP_0E` set present (the old "no hits" note was a cached-decode artifact)     |
| Vulcania   | (0,7)/S    | (3,4)/W      | (2,3)/W       | (8,11)/W     | (5,11)/S     | (8,15)/E     | (5,10)/S     | many arena selectors `0x3E`–`0x48`                                                 |
| Sandsobar  | (10,3)/E   | (7,3)/E      | (11,4)/N      | (11,6)/E     | (7,6)/E      | (15,7)/N     | —            | arena `0x08` (8,13)/ANY; arena `0x49`–`0x54`                                     |


### 2.1 `OP_0B` window titles (event strings)

> **Caveat:** the tiles in the table below come from the same stale cached decode
> as the old §2 matrix and are **door-label / sign tiles, not the `OP_0E` service
> tiles** — they do not line up with §2 (e.g. Middlegate's `OP_04` "S.J.
> Blacksmith" door label is at `(4,5)`, while the Smith *service* `OP_0E 0x06` is
> at `(4,4)`). `OP_0B`'s argument is a **sign-table key → `NN.anm`** (`0x15756`
> lookup), not a string index, so do not treat these as the service prompt text.
> Use §2 (above) + `tools/decode_event.py --decompile` for authoritative tiles.


| Town       | Building   | Tile    | Title string (from cached decode)   |
| ---------- | ---------- | ------- | ----------------------------------- |
| Middlegate | Inn sign   | (4,5)   | Middlegate Inn                      |
|            | Smith sign | (6,5)   | S.J. Blacksmith                     |
|            | Pub sign   | (6,7)   | Slaughtered Lamb                    |
|            | Service    | (3,7)   | Gateway Temple                      |
|            | Service    | (6,4)   | Turkov's Training                   |
| Atlantium  | Pub        | (13,6)  | Boar's Tongue Tavern                |
|            | Smith      | (7,4)   | Drewnhald Ironworks                 |
|            | Training   | (4,5)   | Beautify Atlantium                  |
|            | Temple     | (15,15) | Island Training *(title quirk)*     |
| Tundara    | Pub        | (13,11) | Lucky Dog Saloon                    |
|            | Training   | (9,7)   | Enhancement Center                  |
|            | Temple     | (11,7)  | White Dove Temple                   |
|            | Smith      | (6,11)  | Thundrax Weaponry                   |
| Vulcania   | Pub        | (3,4)   | Belinthra's Bar                     |
|            | Smith      | (8,11)  | Bestway Blacksmith                  |
|            | Training   | (2,3)   | Vulcanian Transport *(title quirk)* |
| Sandsobar  | Pub        | (7,3)   | Red Lantern Tavern                  |
|            | Smith      | (11,6)  | Big Al's Accessories                |
|            | Temple     | (10,3)  | Temple Benedictus                   |
|            | Training   | (11,4)  | Sheik Training Arena                |


Door labels (`OP_04`) match the same names where present.

---

## 3. `str.dat` UI text (shared shop strings)

From `[11-str-decoded.txt](11-str-decoded.txt)` — global strings used by engine
handlers (not per-location `event.dat` banks):

### 3.1 Pub (`0x1A132`)

- Menu: **A)** feeding frenzy **B)** drink **C)** specialties **D)** tip **E)** listen for rumors.
- Drink lines **A–F** (Orc Beer … Mystic Brew).
- Food lines **A–C** per town menu block (three columns of hors d’oeuvres / soup / steak variants).
- Pub NPC intro flavor (sequential block ~lines 89–108):
  - **Amber** — Middlegate *Slaughtered Lamb*
  - **Rowena** — Atlantium *Boar's Tongue*
  - **Belinthra** — Vulcania
  - **Gabrielle** — Sandsobar
  - **Lucindra** — (brawling bar; likely Sandsobar alternate / special)
- Rumor hint embedded in str pool: **"Donate at all / temples"** (~line 171) — pub
rumor **E)**, not hard-coded quest text.

### 3.2 Blacksmith (`0x1C54A`)

- NPC intros (~lines 255–273): **Svendegard**, **Morgan Drewnhald**, smelter smith,
friendly smith, **tough old** smith — map to towns by matching Drewnhald→Atlantium;
align others with shop names above when play-testing.
- Menu: **A)** Weapons **B)** Today's Specials **C)** Armor **D)** Misc **E)** Sell **F)** Identify.

### 3.3 Temple (`0x1D208`)

- Five priest intro variants (~lines 343–357).
- Menu: **A)** Restore Cond **B)** Restore Algn **C)** Donations **D–F)** cleric spell purchase (no sorcerer stock).
- Donation feedback: *"Your generosity is greatly appreciated"*, *"Today you are blessed!"*.

### 3.4 Mage guild

- Guild hall intros (~lines 328–342).
- `**Sorry, you must be a member of this guild to purchase spells.`** (~line 369).

### 3.5 Training / inn / misc

- Training payment messages via shared *"Not enough gold"* / stat prompts.
- Inn: `**Crispy Critters Inn*`* enrollment string @ `0x1A2B8` (post-enroll UI).

---

## 4. Blacksmith inventories (`OP_0E` `0x06` vs default range)

### 4.1 Explicit blacksmith (`0x1C54A`)

Uses `**A4-$79F2**` (map 0–4) directly. **Two layers:**

1. **Static item IDs** — builder `**0x1C00E**` walks `**town × 6 + slot**` into data-hunk
  tables (category jump @ `**0x1C09E**`):

  | Cat | Menu             | Item IDs (A4−) | Meta (A4−) |
  | --- | ---------------- | -------------- | ---------- |
  | 1   | Weapons          | `$68EE`        | `$68D0`    |
  | 2   | Today's Specials | `$683A`        | `$683A`    |
  | 3   | Armor            | `$68B2`        | `$6894`    |
  | 4   | Misc             | `$6876`        | `$6858`    |

   **6 slots per town per category** — full matrices in
   `EXTRACTED/shop_tables.json` → `static_by_town.blacksmith`.
2. **RNG str captions only** — open handler `**0x1C54A**` fills display pointer
  runtime tables via `**-$7DE2**`:

  | Table (A4−) | Layout          | Purpose                                          |
  | ----------- | --------------- | ------------------------------------------------ |
  | `$5ACE`     | 10 × pointer    | Weapon *caption* strings                         |
  | `$5AA6`     | 5 × 4 × pointer | Category headers (Weapons / Specials / …)        |
  | `$5A56`     | 6 × pointer     | Specials *wording* (item ids still from `$683A`) |


**Today's Specials item pick** (`0x1C146`, category 2): game date `**A4-$79B6**`
mod `**0x1E**` indexes static bytes `**A4-$681C` / `$6816**` — not a random item
pool. Discount/meta bytes copied in category 4 path.

Shop loop (`0x1C714`+): menu **A–F** → `0x1C25A` category id → `**0x1C00E**`
inventory builder → buy/sell/identify (`-$7DDC`, `-$7B78`).

#### 4.1.1 Buy leaf (`0x1BE44`) — **PORTED** (`game/src/events/`)

The blacksmith **buy** transaction is now implemented byte-exact and driven by
the swappable `ITownServiceUi` backend (`townSvcRunSmith`, `townSvcSmithBuy`):

| step | ASM | behaviour |
|------|-----|-----------|
| condition guard | `0x1BE4C` | reject if selected char `record+$26 != 0` (caption 8) |
| free slot | `0x1BE60` | first empty backpack id-run `record+$3A` (full → caption 2 @ `0x1BE82`) |
| price | `0x1BF16` | `base = items.dat gold (record+$12)`; `meta==0 → base`, else `base*2 + (meta-1)*1000` |
| gold | `0x1BDD6` (=`0x1C9C0`) | `scc(record+$66 ≥ price)` then subtract (no partial) |
| grant | `0x1BEBC` | write id→`+$3A`, charges→`+$40`, flags→`+$46` (SoA backpack; matches OP_19) |

Per-category buy fields (`0x1C00E` + the misc swap `0x1C1CC`): **Weapons/Armor**
put the magic `+` bonus in `flags`(`+$46`) and price; **Misc** puts the meta in
`charges`(`+$40`) with `price_meta=0` (base gold); **Specials** `flags` is the
**date-rolled** bonus (`0x1C146`) — deferred. Static item-id/meta matrices are in
`EXTRACTED/decomp/mm2_town_tables.{h,c}` (`mm2_smith_inventory`/`mm2_smith_price`/
`mm2_smith_buy_fields`) + `tools/mm2_town_tables.py`; tests in
`event_middlegate_test.cpp` (`testSmithTransactions`).

**Still deferred (engine):** Sell (`0x1BC26`) and Identify (`0x1B6E0`) leaves
(presentation-heavy), the Today's-Specials date-roll bonus (`0x1C146`,
`A4-$79B6`→`$681C`/`$6816`), and the Merchant-skill half-price discount
(`-$7F32` @ `0x1BFB4`; FAQ §3-10 "cut in half for merchant prices").

### 4.2 Default-range shops (`0x15EDC`)

Selectors `**0x09`–`0xFB**` map to category `**0x3C`–`0x46**`, **not** the
blacksmith handler. Same `**-$7DFA**` shell as other category shops:

- Menu driver `**-$7F68**`: modes `**0x31**` = deduct gold, `**0x32**` = deduct gems,
`**0x33**` = HP/food-style deduction (`0x7CB0`/`0x7D3E`).
- Used for **general store**, **special shop**, **arena prizes**, **Vulcania**
counter tiles, etc.

`**0x06` vs default:** only `**0x06**` enters `0x1C54A` (full smith UI + 6
specials). Default bins share the generic shop shell with different underlying
item pools keyed by **adjusted selector index** + category byte.

#### 4.2.1 Store/Special shop port status — **ENGINE-GATED (deferred)**

General store (`0x07`→`-$7DB8`) and special shop (`0x08`→`-$7DBE`) both route
through the shared category-shop thunk `-$7DFA`, whose runtime target is the
**mega-handler `0x9D76`** (also serving arena/Monster Bowl ticket flows and
class-quest rewards; thunk stubs are bound at init `0x24928`). Unlike the
blacksmith, this is **not** a thin items.dat-gold buy:

- Buy loop `**0xA62C**`: Y/N shell (`-$7D0A` @ `0xA68C`) → character pick
  (`-$7F98` mode `0x31` @ `0xA6F6`) → affordability **hard gate**
  `**record+$66 ≥ 100 gp**` (`cmpi.l #$64,$66(a0)` @ `**0xA75E**`) → purchase
  effect via the `**0xA3AE**` jump table (keyed on roster `+$50` nibbles).
- Item pools are **not** the blacksmith `A4-$68xx` matrices; the general-store
  display reads per-slot bytes `A4-$7136`/`A4-$713C` (`0xA812`+) and menu-line
  pointers `A4-$719E` (`0xA672`). `tools/dump_shop_tables.py` does not yet dump
  these (unlike blacksmith `A4-$68xx`; extract via `0x7FFE − disp` in
  `ghidra/mm2_data_00.bin`). Character pick in the buy loop uses `-$7F98` mode
  `0x31` (`0xA6F6`), not the training key reader `-$7F68`→`0x042AA`.
- For `0x07`/`0x08` the real town index stays in `A4-$79F2`, so Middlegate (0)
  uses the simple `0xA644` menu while non-zero towns use the full `0xA11C` sheet
  UI; default-range selectors temporarily set `A4-$79F2` to the category byte.

Reproducing this faithfully requires porting the whole `0x9D76` subgraph, so the
remake keeps the OP_0B sign-title + defer fallback (no fabricated buy). The
`OP_0E` dispatch + default-range binning themselves are already byte-exact
(`eventVmIsTownServiceSelector`).

---

## 5. Temple

### 5.1 Handler (`0x1D208`)

Builds per-town **str pointer** tables at `**A4-$59EE**`, `**-$599E**`, `**-$5986**`,
**-$594E`**, **`-$58AE`**, **`-$580E`**, **`-$57F6`** via **`-$7DE2` RNG** at open
(caption text only).

**Cleric spell stock only** — temples do **not** sell sorcerer spells. The sorcerer
price helper `**0x1D97A`** (reads `**A4-$66E2**`) is called from the **mage guild**
path only (`0x1E62E`, `**cmpi #4`** @ `0x1E64A`). Temple purchase loop calls
`**0x1DAC6**` only (`0x1DEFC`, `**cmpi #3**` @ `0x1DF1A`).


| Table (A4−) | Layout    | Purpose                                                                                       |
| ----------- | --------- | --------------------------------------------------------------------------------------------- |
| `$66F6`     | 5×4 bytes | Cleric spell id bytes — **3 sold** @ `0x1DF1A` (menu keys **`D`–`F`**, handler **`0x1DAC6`**) |
| `$670A`     | 5×4 bytes | Internal menu key bytes (slot 3 usually `0x80` pad)                                           |
| `$6738`     | 6× BE u16 | Restore Cond/Algn menu costs (keys **A/B** @ `0x1CB48`) — **not** cleric spell D/E/F prices   |


Town index **`A4-$79F2`** (0–4). Fourth byte per town is padding; only slots **0–2**
are built into the purchase UI.

**Cleric spell gold:** `decode_gold_encoding(A4-$66F6[town×4+slot])` @ **`0x1DAC6`**
→ stored in **`A4-$56BE[slot+3]`**; debited @ **`0x1D872`**. Same bit encoding as
guild (`$66CE`): low 5 bits = base; bit5 ×10, bit6 ×100, bit7 ×1000.

**Cleric id decode:** `flat_index = raw_byte + CLERIC_FLAT_ADJUST[town][slot]` (see
`tools/dump_shop_tables.py`). Output: `shop_tables.json` →
`temple_spells.cleric_spells_by_town`.

### 5.1.1 Cleric spells sold (3 per temple)


| Town       | D (slot 0)          | E (slot 1)                   | F (slot 2)            |
| ---------- | ------------------- | ---------------------------- | --------------------- |
| Middlegate | 1-1 Apparition (10) | 1-2 Awaken (10)              | 1-6 Power Cure (1000) |
| Atlantium  | 8-3 Mass Distortion (20000) | 9-3 Resurrection (50000) | 9-4 Uncurse Item (100000) |
| Tundara    | 3-1 Cold Ray (400)  | 3-5 Lasting Light (100)      | 4-4 Restore Alignment (500) |
| Vulcania   | 4-6 Holy Bonus (2000) | 5-5 Remove Condition (3000) | 7-2 Fiery Flail (10000) |
| Sandsobar  | 2-2 Heroism (250)   | 2-5 Protection From Elements (300) | 2-7 Weaken (200) |


(`level-number` = manual cleric list; `flat_index` = `spells.dat` record 48..95.)

Full acquisition index: [31-spell-sources.md](31-spell-sources.md).

Donation cost table `**A4-$6742`** (BE **u16** per town, index `**A4-$79F2`**
0–4). Debited at `**0x1CA3A**` → `**0x1C9C0**` (subtract `**roster.$66**`
gold). **Not** loaded from data-hunk `**0x25F72`** — that u16 cluster has no
read/copy to `**-$6742**` in ASM.

### 5.2 Donation quest (farthing / dime / flick)

**Tracking byte:** `**A4-$799E`** (global quest bitfield).

On successful donation (`0x1D796`, after `**0x1D90C**` gold check passes):

```text
bit_mask = A4-$66B1[ A4-$79F2 ]          ; u8 per town 0..4
A4-$799E |= bit_mask
if A4-$799E == 0x1F:                       ; all 5 temple bits
    trigger reward sequence (sets A4-$794C, A4-$3F1C, clears A4-$799E)
    increment stat A4-$5770
RNG: engine query -$7BB4(100,1); if result > 0x5A → "Today you are blessed!" buff path
```

`**A4-$66B1**`: 5-byte table in the data hunk (same bytes as master copy @
`**0x2637C**` first five). **Donation GP** and **quest bit** are separate from
**cleric spell purchase** (spell ids @ `**A4-$66F6`**, costs @ `**A4-$6738**`
/ menu `**0x1D872**`).

**Donation gold (`A4-$6742`, BE u16, town 0→4):**


| Town       | GP  |
| ---------- | --- |
| Middlegate | 20  |
| Atlantium  | 250 |
| Tundara    | 40  |
| Vulcania   | 120 |
| Sandsobar  | 40  |


**Quest bit masks (`A4-$66B1`, OR into `-$799E`):**


| Town       | Bit |
| ---------- | --- |
| Middlegate | 1   |
| Atlantium  | 2   |
| Tundara    | 4   |
| Vulcania   | 8   |
| Sandsobar  | 16  |


Temple menu shows donation amount from `**A4-$6742`** @ `**0x1D556**` (display
only; debit uses same table @ `**0x1CA3A**`).

**Quest item:** `**OP_0E` `0x0A`** (`goblet_quest`, `-$7DAC`) at Middlegate
**(2,1)**. Cached scripts show gate text **"Fool, you have no farthing to flick!"**
on related tiles — Olympic/farthing item gate tied to `**OP_28`** / inventory
(not fully traced here without live `event.dat`).

**Pub rumor:** str **"Donate at all temples"** — option **E)** in tavern; random
rumor pool (below), not a dedicated quest opcode.

### 5.3 Hireling vs party member (temple / services)


| Mechanism             | Field                           | Rule                                                                                                                                           |
| --------------------- | ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| **Hireling flag**     | roster `**$0B` bit 7** (`0x80`) | Set when hiring (`0x216E6`); **clear** for normal party members. Murray’s Cave uses `**OP_15`** `op=0x01 val=0x80` → *"No hirelings allowed!"* |
| **Home town**         | roster `**$0B` & `0x7F*`*       | Values 1–5 (Middlegate…Tundara) for inn storage / guild membership                                                                             |
| **Dead / gone**       | roster `**$26` ≥ `0x81`**       | `**OP_26**` party pick skips (`0x16C42`)                                                                                                       |
| **Class / attr gate** | `**OP_2D`**                     | Checks `**$0C`/`$0E`/`$0F**` with arg bitmask (class/race/sex), not hireling bit                                                               |


Temple member selection uses the same party iterator as other services (`-$7F20`
slot lookup); hirelings with `**$0B.7**` still appear unless a script explicitly
filters via `**OP_2D**` / `**OP_15**`. Guild **spell** purchase checks **guild
membership** via roster `**$0B` low 7 bits** vs current town (see §6).

---

## 6. Mage guild


| Step            | Selector | Handler                                  | Effect                                             |
| --------------- | -------- | ---------------------------------------- | -------------------------------------------------- |
| Open spell shop | `0x05`   | `-$7D10` → `**0x1E3E6`**                 | Four sorcerer spells per town; membership required |
| Enroll          | `0x0D`   | `-$7DA0` → `**0x1A1B2**` / `**0x1A1F8**` | y/n; on success sets roster guild-home byte        |


### 6.1 Prior RE errors (corrected)

An earlier pass mis-attributed the **alternate** stock builders at `**0x1EACC` /
`0x1EB2E`** (tables `**A4-$6692` / `$669E**`, **3** menu slots, keys `**9` / `:` /
`;`**) to retail `**OP_0E` `0x05**`. That produced **the same three spellbook
indices everywhere** (decoded as Implosion / Light / Silence) with only per-town
gold varying — **wrong**.


| Wrong (old)                                   | Correct (ASM + hunk)                                                  |
| --------------------------------------------- | --------------------------------------------------------------------- |
| Spell ids @ `**A4-$669E`** (3× BE u16) global | `**A4-$66E2**` — **5×4** bytes, sorcerer flat **0..47**, **per town** |
| Keys `**9` / `:` / `;`**                      | Keys `**A` / `B` / `C` / `D**` (`sub #$41` @ `**0x1E864**`)           |
| **3** spells                                  | **4** spells (`cmpi #4` @ `**0x1E64A`**)                              |
| Same spells all towns                         | **Different four spells per town** (school progression by town tier)  |


Machine-readable stock: `shop_tables.json` →
`static_by_town.mage_guild_spells` (regenerate:
`python tools/dump_shop_tables.py`).

### 6.2 Enrollment (`OP_0E` `0x0D`)

After `**-$7FBC`** y/n (`0xFFFF` mode @ `**0x1A1A6**`), success path `**0x1A1B2**`
loops active party slots:

- Index: `**A4-$796A[slot]**` × `**$82**` → roster base `**A4-$2A33**`
- Write: `**roster.$0B ← A4-$79F2 + 1**` (1=Middlegate … 5=Sandsobar) @ `**0x1A1CE**`
- Then `**0x1A1F8**` (guild UI cleanup; restores map index)

Purchase gate @ `**0x1E410**`: roster `**$79**` (class flags) AND `**A4-$66A9[town]**`
must be non-zero — else *"Sorry, you must be a member of this guild…"* (`str.dat`
~369).

### 6.3 Spell shop (`OP_0E` `0x05`)

**Flow:** `**0x160C2*`* → `**-$7D10**` → `**0x1E3E6**` → `**0x1E8B0**` (RNG guild
name pointers from `**A4-$5732`…`$5714**`, `**-$7DE2**`) → stock build
`**0x1E618`–`0x1E650**` calling `**0x1D97A**` per slot.


| Data           | Layout         | Use                                                        |
| -------------- | -------------- | ---------------------------------------------------------- |
| `**A4-$66E2**` | 5×4 **u8**     | Sorcerer `**spells.dat` flat index** (0..47) per town×slot |
| `**A4-$6698`** | 5×4 **BE u16** | Legacy/alternate UI only (`0x1EB2E`); **not** retail guild menu @ `0x1D97A` |
| `**A4-$6686`** | 4 **BE u16**   | Legacy slot addends for alternate UI path                                |
| `**A4-$66CE`** | 5×4 **u8**     | **Retail** cost-encoding bytes for `**0x1D97A**` → `**A4-$56BE**`       |


**Gold (retail):** `decode_gold_encoding(66CE[town×4+slot])` — low 5 bits = base; bit5 ×10,
bit6 ×100, bit7 ×1000. **Grant:** `**0x1D872`** sets spellbook bit from the `**66E2**` byte (not
cleric `**66F6**`). **No cleric spells** at guild.

`**str.dat` hall intros** (`11-str-decoded.txt` ~328–342), one per town tone:


| Town       | Intro flavour       |
| ---------- | ------------------- |
| Middlegate | Archmage            |
| Atlantium  | Cabalist            |
| Tundara    | Mystics by the fire |
| Vulcania   | Magicians           |
| Sandsobar  | Sorcerers / phials  |


### 6.4 Per-town sorcerer stock (verified hunk)


| Town           | A                                | B                           | C                        | D                           |
| -------------- | -------------------------------- | --------------------------- | ------------------------ | --------------------------- |
| **Middlegate** | S1/1 Awaken (10)                 | S1/3 Energy Blast (1000)    | S1/7 Sleep (50)          | S2/3 Identify Monster (100) |
| **Atlantium**  | S8/2 Mega Volts (50000)          | S8/3 Meteor Shower (50000)  | S9/1 Implosion (100000)  | S9/2 Inferno (100000)       |
| **Tundara**    | S4/2 Feeble Mind (600)           | S4/3 Fire Ball (2000)       | S5/1 Disrupt (3000)      | S5/3 Sand Storm (3000)      |
| **Vulcania**   | S6/1 Disintegration (5000)       | S6/3 Fantastic Freeze (5000) | S6/5 Super Shock (5000) | S7/2 Duplication (25000)  |
| **Sandsobar**  | S2/7 Protection from Magic (400) | S3/1 Acid Stream (200)      | S3/4 Lightning Bolt (1000) | S4/1 Cold Beam (500)       |


Spell acquisition index: [31-spell-sources.md](31-spell-sources.md) (regenerate
guild tables: `python tools/build_spell_sources_doc.py`).

### 6.5 Alternate tables (not retail `0x05`)

Other engine paths still reference `**A4-$6692` / `$6698` / `$669E`** with **slot-only**
indexing and/or `**slot×3+9`** keys (`**0x1EACC**`, `**0x1EB2E**`, `**0x1EA66**`, …).
Do **not** use these for per-town guild shop lists.

### 6.6 Atlantium / Vulcania selectors `0x1D`–`0x25`

These are `**OP_0E` default-path** selectors (category `**0x3D`**, index = selector −
`**0x10**` → **13..21**), used on **Olympic Trial / skill** tiles in Atlantium — **not**
the mage-guild spell shop (`**0x05`**). They route through `**0x15EDC` → `-$7DFA**`
with a temporary category byte in `**A4-$79F2**`, separate from `**0x1E3E6**`.

---

## 7. Pub rumors

Handler `**0x1A132**` → rumor branch uses:

- `**A4-$119A**` (`-$EE66`): bytecode stream (opcodes + string indices), walked
by `**0x97FC`–`0x9932**`.
- `**A4-$1110**`: rumor cursor; `**A4-$71D6**`: secondary cursor; `**A4-$71DB**`:
last opcode; compared against `**A4-$1116**` (day/era gate).
- Random `**-$7BB4(1, n)**` for which rumor variant fires.

**Town binding:** stream is **not** indexed by map directly in the walker; town-
specific rumors are **authored in the `$119A` script** for each pub event
(different event segments loc 0–4). Pub **NPC name** strings (Amber, Rowena, …)
are fixed in `**str.dat`**; **E)** pulls from the shared rumor table with
RNG + day gates.

Embedded quest hints in rumor str pool (examples from `11-str-decoded.txt`):
*Donate at all temples*, *Meal B, then C1 2,10*, *Hirelings at 15,10*, etc.

---

## 8. Deals of the day / Today's Specials

- **Blacksmith menu B)** → `**0x1C25A(1..6)`** → reads pre-rolled `**A4-$5A56**`
entries (6 per town, built at shop open from `**-$7DE2**` RNG).
- **General/special shops** (`0x07`/`0x08`/default range): inventory from
category-specific tables inside `**-$7DFA`**; refreshed on each `**OP_0E**`
entry (no separate save flag found in ASM skim).
- **Sandsobar** drunk guild recruit (ev 26): `**OP_24` `200`** gp — separate from
smith specials.

Data hunk extract: `EXTRACTED/ghidra/mm2_data_00.bin` (see
`python tools/extract_segments.py mm2 -o EXTRACTED/ghidra`). **Per-town item id
lists** for smith specials / pub food are still **RNG + str pointers** at
shop-open — not static bytes in the data hunk (structure above is ASM-confirmed).

---

## 9. Training — stats + HP (`0x9BCA`, `-$7D16`, `0x1C898`)

Entry: `**OP_0E` `0x04`** → `**-$7D16**` (`0x8050` menu). Training **mode**
(stat vs HP) comes from the calendar table `**A4-$55BA`**, indexed by month/day
@ `**0x9B48**` → `**-$2(a5)**`: `**1**` = stat path, `**2**` = HP path @
`**0x9BCA**`. Outdoor/special gate @ `**0x9B80**`.

**Shop menu (`-$7D16`):** `**-$7F68`** keys `**0x31**` / `**0x32**` only —
`**0x31**` → stat gold path (`**0x7F68**`), `**0x32**` → redistribute current HP
(`**0x7FF8**`, sums party `**$5c**`). This is separate from temple/inn `**-$7EB4**`
menus that also use `**0x31`–`0x33**`.

### 9.1 Map index vs training “town index”

`**A4-$79F2**` (map screen **0–4**) is **not** the multiplier used for training
cost or HP-per-level. That byte follows **location order** in the game data:


| `A4-$79F2` | Town       |
| ---------- | ---------- |
| 0          | Middlegate |
| 1          | Atlantium  |
| 2          | Tundara    |
| 3          | Vulcania   |
| 4          | Sandsobar  |


**Training toughness** (weakest → strongest), from
`[Might and Magic FAQ.txt](Might%20and%20Magic%20FAQ.txt)` §3-6: Middlegate →
Sandsobar → Tundara → Vulcania → Atlantium. Uses a separate **training town
index** (FAQ: **1, 2, 2, 4, 5** — no index **3**):


| Training index | Town(s)            | Notes                             |
| -------------- | ------------------ | --------------------------------- |
| 1              | Middlegate         | Cheapest; least HP per level      |
| 2              | Sandsobar, Tundara | Same multiplier in FAQ            |
| 4              | Vulcania           |                                   |
| 5              | Atlantium          | ~2× Middlegate HP per level (FAQ) |


**Map index → training index:** `**[1, 5, 2, 4, 2]`** (map 0..4 as above).

**Training gold (FAQ):** `current_level × training_town_index × 50` gp.

**Stat training (`0x1C898`):** `**A4-$6720` / `A4-$671A`** indexed by **map**
`**A4-$79F2`**, not training index. Rolled byte += addend; dual compare @
`**0x1C8A8`/`0x1C8BA**`; stat id `**0`–`6**` → roster `**$6B`–`$72**` @
`**0x1C7F0**`.


| Town (map index) | Stat + | Cap |
| ---------------- | ------ | --- |
| Middlegate (0)   | 5      | 100 |
| Atlantium (1)    | 20     | 100 |
| Tundara (2)      | 10     | 100 |
| Vulcania (3)     | 10     | 100 |
| Sandsobar (4)    | 3      | 50  |


### 9.2 HP path (`0x9BCA`, calendar mode 2)

`**0x9BCA**` does **not** read `**A4-$6714`** (temple donation ×100 @
`**0x1DC26**` / `**0x1DC84**`).

```text
base = roster[slot].$6B
if party_count > 1: base += one other roster[slot].$6B
RNG (0x6D, 0xA) -> divu #10 -> add to base
if base >= byte_at(A4-$5608): hp_gain_count += 1   ; cmp.b @ 0x9C52
second RNG @ 0x9C54 -> -$7D22 / -$7D28 -> roster.$25 / $5c
```

Per-level HP also depends on class, endurance, and **training town index when
leveling** (FAQ §2-5, §4-2: ~6 HP/level lost by deferring Atlantium).

### 9.3 Tier template (`0x26578`)

7 × `**(u8 cost, u8 gain, u16 pad)`** — retail **100 gp**; gains **50…116**.
First five gains match difficulty **M, S, T, V, A** (not map order). Runtime copy
`**A4-$56E`**. Not `**A4-$6720**`.


| Tier (M→S→T→V→A + extras) | Cost | Gain               |
| ------------------------- | ---- | ------------------ |
| 0–4 towns                 | 100  | 50, 55, 60, 74, 86 |
| 5–6 extended              | 100  | 100, 116           |


**Tiles (FAQ §8):** Middlegate (9,7), Sandsobar (2,7), Tundara (11,7),
Vulcania (5,3), Atlantium (9,4). `**OP_24`** gates in `**event.dat**` (typical
**200** gp).

---

## 10. Code / tooling map


| Asset                     | Location                                                           |
| ------------------------- | ------------------------------------------------------------------ |
| Event opcodes + selectors | `tools/decode_event.py` (`OPCODES`, `SELECTOR_NAMES`)              |
| C event ops mirror        | `editor/src/core/EventOps.h`                                       |
| Roster layout             | `editor/src/core/RosterFile`, `EXTRACTED/docs/06-roster-format.md` |
| Game-state offsets        | `tools/mm2_gamestate.py`, `EXTRACTED/decomp/mm2_gamestate.h`       |
| str codec                 | `tools/mm2_codec.py`                                               |
| Items                     | `editor/src/core/ItemsFile`, `tools/decode_items.py`               |
| Spells                    | `editor/src/core/SpellsFile`, `tools/decode_spells.py`             |


| Shop table dump | `tools/dump_shop_tables.py` → `EXTRACTED/shop_tables.json` |

---

## 11. Completeness checklist


| Topic                            | Status                                                                    |
| -------------------------------- | ------------------------------------------------------------------------- |
| `OP_0E` selector → handler map   | **Complete** (ASM)                                                        |
| Per-town service **tiles**       | **Complete** for loc 0,1,3,4 from cached decode                           |
| Tundara `**OP_0E`** wiring       | **Missing** in cached decode — titles only; needs `event.dat`             |
| `**str.dat`** shop UI strings    | **Complete** (11-str-decoded.txt)                                         |
| Blacksmith **item ids per town** | **Complete** — static 5×6 matrices @ `$68EE`…`$6876`                      |
| Blacksmith **buy transaction**   | **PORTED** — `townSvcSmithBuy` (`0x1BE44`/`0x1BF16`/`0x1BDD6`), tested (§4.1.1) |
| Blacksmith **sell/identify**     | **Deferred** — `0x1BC26` / `0x1B6E0` (presentation-heavy)                 |
| Store/Special **buy**            | **Engine-gated** — mega-handler `0x9D76`, buy loop `0xA62C` (§4.2.1)       |
| Tavern **food/drink/rumor**      | **Engine/RNG/VM-gated** — `0x1A132`/`0x18EC0`/`0x18F78`/`0x97FC` (§13.3.1)  |
| Temple **donation gold**         | **Complete** — `**A4-$6742`** BE u16 ×5 (not `**0x25F72**`)               |
| Temple **donation bitfield**     | **Complete** — `**A4-$66B1`** → `**-$799E**`, `**0x1F**` reward           |
| Temple **spell id bytes**        | **Complete** — `$66F6` cleric ×3/town only (no sorcerer @ temple)         |
| Farthing **quest item + flick**  | **Partial** — gate strings + `0x0A` selector; Gold Goblet = item **224**  |
| Guild **membership byte**        | **Complete** (roster `$0B`, enroll `$1A1CE`)                              |
| Guild **spell tables**           | **Complete** — `$66E2` ids 5×4, `$6698` costs 5×4, `$6686` addends ×4     |
| Pub **rumor mechanism**          | **Complete** (stream `$119A` + RNG)                                       |
| Pub **food costs + menu text**   | **Complete** — `$6760` BE u16 + str block per town                        |
| Pub **meal item ids**            | **N/A** — roster `**$78`** encoding, not `items.dat`                      |
| Rumor **index → town**           | **Partial** (NPC names fixed; rumor ids per loc in event segments)        |
| Training **cost / town index**   | **Complete** (FAQ + ASM stat path); map ≠ training index                  |
| Training **HP path @ `0x9BCA`**  | **Partial** — might-sum + RNG; no `6714`; town index in FAQ only          |
| Training **stat tables**         | **Complete** — `**A4-$6720` / `A4-$671A`** @ `0x1C898` (map index)        |
| Today's **specials item sets**   | **Complete** — static `$683A` + date index `$681C`/`$6816`                |
| **Event → text routing**         | **Complete** — `[30-event-to-string-path.md](30-event-to-string-path.md)` |


**Overall static shop table completeness (town 0–4): ~92%.**
RNG at shop open affects **str.dat captions** only (smith/temple/guild names);
numeric stock (item ids, spell indices, food GP) is in the data hunk.

---

## 13. Extracted numeric tables (data hunk masters)

Regenerate: `python tools/dump_shop_tables.py --json EXTRACTED/shop_tables.json`

### 13.1 Confirmed static (five towns)


| Table                | Location                                        | Values (town 0→4)          |
| -------------------- | ----------------------------------------------- | -------------------------- |
| Temple donation GP   | `**A4-$6742`**                                  | 20, 250, 40, 120, 40       |
| Temple quest bit     | `**A4-$66B1**` (= master `**0x2637C**` first 5) | 1, 2, 4, 8, 16             |
| Training stat +      | `**A4-$6720**` (by **map** 0→4)                 | 5, 20, 10, 10, 3           |
| Training stat cap    | `**A4-$671A`** (by **map** 0→4)                 | 100, 100, 100, 100, 50     |
| Training town index  | FAQ §3-6 (not map order)                        | 1, 5, 2, 4, 2              |
| Temple donation ×100 | `**A4-$6714`** (by **map** 0→4)                 | 1, 5, 2, 3, 2 → GP 100…500 |


### 13.2 Training + Healing closed-form formulas (FAQ §3-6)

**Training cost:** `level × training_town_index × 50`. Indices:
**1** Middlegate, **2** Sandsobar & Tundara, **4** Vulcania, **5** Atlantium.

**Healing cost:** `level × training_town_index × 10`  (same town index as training).

- ×10 multiplier when character is **dead**: `level × index × 100`
- ×100 multiplier when **eradicated**: `level × index × 1,000`
- Go to Middlegate (index 1) for cheapest healing. [FAQ §3-6]

**Portal table:** see `[34-commerce-formulas.md §3](34-commerce-formulas.md)` — five portals linking all towns, costs 10–100 gp per leg.

**Day-based shop bonus cycle:** see `[34-commerce-formulas.md §2](34-commerce-formulas.md)` — ~30-day cycle; best bonus on days 30/60/90/120/150/180 (+5/+6/+7/+8/+9/+12). Calendar day at `A4-$79DE`; era at `A4-$79B6`.

**HP path @ `0x9BCA`** (calendar mode `**2**`): might-sum + `**divu #10**` RNG addend;
`**cmp.b**` vs `**A4-$5608**` @ `**0x9C52**`. Does **not** read `**A4-$6714`**.

**Stat training @ `0x1C898`:** add `**A4-$6720[map]`**; cap `**A4-$671A[map]**`
(`**A4-$79F2**`, not training index).

**Tier template @ `0x26578`:** 7 × `**(u8 cost, u8 gain, u16 pad)`** — retail
**100 gp** / gains **50…116**; first five gains match difficulty order
**M, S, T, V, A**.

`**A4-$6714`:** five BE `**u16`** after stat-cap bytes — used @ `**0x1DC26**` as
**temple donation multiplier × 100**, not HP training.

**Not training stat addends:** data hunk `**0x26800`** (24 bytes) — unrelated to
`**A4-$6720**`.

### 13.3 Pub food / drinks

- **Not inn:** food/drinks = `**OP_0E` `0x01`**, handler `**0x1A132**`. Inn `**0x02**`
→ `**-$7CD4**` (rest/dismiss only).
- **Fixed per town:** gold costs in data hunk `**A4-$6760`**, indexed
`**town×6 + menu×2**`, **BE u16** @ `**0x1CEA4`**. Menu labels A/B/C map to
fixed `**str.dat**` text blocks (see `food_menu_text` in `shop_tables.json`).


| Town       | A (gp) | B (gp) | C (gp) |
| ---------- | ------ | ------ | ------ |
| Middlegate | 10     | 50     | 100    |
| Atlantium  | 1000   | 2000   | 3000   |
| Tundara    | 200    | 100    | 1000   |
| Vulcania   | 5000   | 500    | 1000   |
| Sandsobar  | 20     | 50     | 250    |


- **Meal mechanics:** tier table `**A4-$6B08`**, cost addends `**A4-$6B1A**`
(these are **not** item ids — prior misread mapped to Staff/Blowpipe). Handler
`**0x18EC0`** computes gold; purchase stores encoding on roster `**$78**`.
- **Drinks:** six global entries — base index `**A4-$6AF0`**, addend `**A4-$6AED**`
resolved via `**0x18F78**` / `**-$7BB4**`. Same drink names all towns (`str.dat`
lines 209–214).

#### 13.3.1 Tavern port status — **ENGINE/RNG/VM-GATED (deferred)**

Finer ASM tracing (for the C++ port) shows the pub is **not** a deterministic
cost/effect leaf and is deferred with these addresses:

- The `OP_0E 0x01` handler `**0x1A132**` is only the NPC intro + guild-enroll y/n.
  The food/drink submenus enter via **selectors `0xC9`/`0xCA`** → `**0x1980A**`
  and route through the runtime vtable thunk `**-$7D40**` (no static target).
- Cost leaves `**0x18EC0**` (food) / `**0x18F78**` (drinks) are **RNG-gated**
  (`-$7BB4`): they produce a tier/addend **encoding byte** (food `A4-$6B08`/
  `A4-$6B1A`; drink `A4-$6AF0`/`A4-$6AED`) written to roster `**+$78**` (+ a `+$7C`
  mode bit) by `**0x019030**` — the feeding-frenzy path does **not** deduct gp.
- The gp food table `**A4-$6760**` (10/50/100…) is consumed by `**0x1CEA4**` +
  `**0x1C9C0**` from a **temple-style food UI** (`**0x1CAC4**`); whether the pub
  reuses it is **unconfirmed** (this doc attributes it to the pub; the only static
  caller found is in the temple region). Not ported to avoid fabrication.
- Drink stat bonuses (doc 34 §5: Orc Beer +5 Might, etc.) are applied by **unported
  VM opcode handlers** via success counters `A4-$79A6`–`$79AB`; the sick/success
  roll is `**0x19D64**` (`RNG(50)==2`) / stat-reset `**0x19B28**`.
- Rumors `**0x97FC**` walk per-location event bytecode `A4-$119A` (cursors
  `A4-$1110`/`$71D6`/`$71DB`, day gate `A4-$1116`) — **display-only**, no static
  hook from the pub menu. The food/drink key loop `**0x19962**` only accepts **A–D**
  (purchase A–C + tip D); **E) rumors** is authored in per-location event bytecode,
  not this key handler.
- Drinks minigame (patron Y/N, gold pool, sick roll) is a separate leaf `**0x19E20**`
  (`0x19AD6`/`0x19D64`/`0x19B28`), not the `0x1980A` A–D loop.

The remake keeps the faithful str.dat NPC intro + y/n and defers (no invented
cost/effect).

### 13.4 Blacksmith / guild / temple spells


| Service                           | Static in data hunk                                | Runtime RNG                                          |
| --------------------------------- | -------------------------------------------------- | ---------------------------------------------------- |
| Smith weapons/armor/misc/specials | **Yes** — `$68EE`…`$6876`, 6×/town/cat             | `$5ACE` / `$5AA6` / `$5A56` str captions @ `0x1C54A` |
| Smith Today's Specials            | **Yes** — `$683A` ids + `$681C`/`$6816` date index | Specials caption ptrs `$5A56`                        |
| Temple cleric (×3)                | **Yes** — `$66F6` + `CLERIC_FLAT_ADJUST` decode    | `$59EE`…`$57F6` name ptrs @ `0x1D208`                |
| Guild sorcerer (×4)               | **Yes** — `$66E2` per town; `$6698`/`$6686` costs  | `$576E` / `$56F6` name ptrs @ `0x1E8B0`              |


Full dumps: `python tools/dump_shop_tables.py` → `EXTRACTED/shop_tables.json`
(`static_by_town` + `random_pool`).

### 13.5 Global `OP_24` gold amounts (data @ `0x271DA`)

Sparse BE u16 cluster includes **10, 50, 100, 1000, 2000** (travel, training gates,
skills). Per-tile amounts still live in `**event.dat`** (`OP_24` argc).

---

## 14. Quest items & consumables (not pub meals)

Pub meals are **not** `items.dat` SKUs (they encode on roster `**$78`**). This table
lists **inventory quest items**, **consumables**, and **travel food** — do not confuse
with tavern food menus (§13 / `static_by_town.pub`).


| Ref              | Category             | Event / usage                                                                                                                                                                                                                     |
| ---------------- | -------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **224**          | **dungeon_treasure** | **Gold Goblet** — pick up **Middlegate Cavern** loc **17** tile **(7,0)** (`OP_19` adds item `0xE0`); **not food** (no use power in `items.dat`). Return to Middlegate **(2,10)** for Nordon `**OP_0E` `0x0A`** (farthing flick). |
| **175**          | consumable           | Magic Meal — Create Food; treasure / Sandsobar smith misc                                                                                                                                                                         |
| **176**          | consumable           | Antidote Ale — Cure Poison drink                                                                                                                                                                                                  |
| (str)            | pub_meal             | Devils Food Brownie — Atlantium pub meal C only (roster `$78`)                                                                                                                                                                    |
| roster `**$25`** | party_food           | Travel food supply; `**OP_15**` oasis +40                                                                                                                                                                                         |
| `**OP_28**`      | inventory            | Consume item in scripts (e.g. `0x10` + count)                                                                                                                                                                                     |


Machine-readable: `quest_items` in `EXTRACTED/shop_tables.json`.

---

## 12. Related Olympic / Atlantium note

**Olympic Trial** (Atlantium **(4,8)**, event str *"Hurl the spear…"*) is **not**
training `**0x04`**. It uses `**OP_02**` + `**OP_24` 500** + `**OP_15`/`OP_20`**
on roster `**$6D**` (personality base) and `**party_effect**` to grant **Athlete**
skill — separate from `**-$7D16`** HP training.
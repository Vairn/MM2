# Event scripts → on-screen text (MM2 Amiga)

How **`event.dat`** dialogue reaches the player, and how **`OP_0E`** town services
pull text from **`str.dat`** or the **executable** instead. ASM references:
[`mm2.capstone.asm`](../mm2.capstone.asm) (handlers below), [`07-event-script-opcodes.md`](07-event-script-opcodes.md),
[`29-embedded-exe-strings.md`](29-embedded-exe-strings.md), [`28-town-services.md`](28-town-services.md).

---

## Short answer (plain language)

**Mixed model.** Tile scripts in **`event.dat`** only pass **small numeric indices**
(string index, `OP_0E` selector byte, gold amount, etc.). They **do not** open
`str.dat` directly.

- **`OP_01`–`OP_06`** and **`OP_0B`** resolve text from the **current location’s
  `event.dat` string bank** (copied into workspace `A4-$47C8`, indexed via
  `A4-$86AC` / `A4-$5D3C`).
- **`OP_0E`** passes one **selector byte** into a **C-like service handler**
  (pub, inn, temple, training, guild, smith, …). That handler loads UI strings
  from **`str.dat`** (`A4-$703e` base, drawn with `-$7BE4`) and/or **embedded
  ASCII in the code hunk** (`PEA` PC-relative addresses; see doc 29).
- **Numeric shop data** (donation gold, spell bytes, rolled stock) comes from
  **data-hunk master tables** and/or **RNG** at shop-open, not from `event.dat`.

**Pub vs inn:** Food, drinks, and specialties are **`OP_0E` `0x01` → `0x1A132`**
(tavern). The inn is **`0x02` → `-$7CD4`** (rest, registry, hirelings) and uses
mostly **exe-embedded** prompts (`0x19D00`…, doc 29).

---

## 1. Interpreter entry (`0x172CA`)

| Step | Address | Role |
|------|---------|------|
| Tile scan | `0x1754A` | `(y,x)` triplets → event id → script PC in location record |
| Dispatch loop | `0x1750C` | Fetch opcode byte |
| Opcode table | `0x17494` | Jump table for `0x00`…`0x32` |
| Invalid | `0x1748C` | Sets abort `A4-$8616` |

`OP_0E` stub: `0x1734E` → `jsr 0x160C2` → return to `0x1750C`.

---

## 2. Event-local text (`OP_01`–`OP_06`, `OP_0B`)

### 2.1 Index → string bytes

| Step | Address | What |
|------|---------|------|
| Read `u8` index | `0x155BE` | Next script byte |
| Resolve string | `0x15884` | Walk `0xFF`-terminated strings in `A4-$47C8`; save cursor in `A4-$5D44` |
| Copy for draw | `0x158C8` | Copy into `A4-$5D3C`; `@` (`0x40`) → newline `0x0A` |

Loader (`0x92F2` / scanner `0x1754A`) sets **`A4-$86AC`** to the location’s
string base inside the decoded event work buffer. **No `str.dat` read** on this path.

### 2.2 Per-opcode display

| Op | Handler | Text source | Draw path |
|----|---------|-------------|-----------|
| `01` | `0x15924` | event.dat | `-$7EC0` |
| `02` | `0x15942` | event.dat | `-$7F62` / `-$7BFC` / `-$7C62` (multiline) |
| `03` | `0x159CE` | event.dat (via `02`) | prep + `02` |
| `04` | `0x159F4` | event.dat | door label `-$7BE4` |
| `05` | `0x15A46` | event.dat | large popup `-$7C74` / `-$7BE4` |
| `06` | `0x15AEE` | event.dat | framed sign + border glyphs |
| `0B` | `0x15DB0` | event.dat (`0x15756` resolves index) | service title window |

**`OP_09` / `OP_0A`:** y/n prompts; question text still comes from prior
**event** opcodes, not `str.dat`.

---

## 3. `OP_0E` selector dispatch (`0x160C2`)

Reads one **`u8` selector** (`0x160C0` / `0x155BE`), then chained subtract /
range bins (see [`07-event-script-opcodes.md`](07-event-script-opcodes.md)).

### 3.1 Named selectors (town services)

| Sel | Decoder name | Call | Notes |
|-----|--------------|------|-------|
| `0x01` | `open_tavern_food` | `0x1A132` | Pub menu; **`A4-$79F2`** town index |
| `0x02` | `open_inn_lodging` | `-$7CD4` | Inn; exe strings + gold |
| `0x03` | `open_temple` | `0x1D208` | Temple; `str.dat` + spell/donation tables |
| `0x04` | `open_training` | `-$7D16` | Training; HP @ `0x9BCA` |
| `0x05` | `open_mages_guild` | `-$7D10` | Guild spells; membership check |
| `0x06` | `open_blacksmith_shop` | `0x1C54A` | Smith; RNG stock tables |
| `0x07`–`0x08` | store / special | `-$7DB8` / `-$7DBE` | Generic shop shell |
| `0x0A` | `goblet_quest` | `-$7DAC` | Side quest |
| `0x0D` | `enroll_mages_guild` | `-$7DA0` | Sets roster `$0B` home town |

Default path (`0x15EDC` → `-$7DFA`): selector binned to category `0x3C`–`0x46`;
**temporarily overwrites** `A4-$79F2` with category byte for the call.

### 3.2 What `OP_0E` does *not* do

- Does **not** read `event.dat` strings for shop menus.
- Does **not** embed item names in the script; handlers use **`str.dat`** and/or
  **rolled pointers** built at shop-open.

---

## 4. Service handlers → text sources

Global UI base: **`A4-$703e`** = loaded **`str.dat`** (see `0x7DF6`, `0x807A`,
`0xE63C`: `move.l -$703e(a4), -(a7)` before `-$7BE4` text draw).

| Handler | Selector | Primary strings | Secondary |
|---------|----------|-----------------|-----------|
| Pub `0x1A132` | `0x01` | **`str.dat`** menus (food A–C, drinks A–F, rumors) | Pub NPC intros in str; rumor bytecode `A4-$119A` uses **event-style indices** into rumor stream |
| Inn `-$7CD4` | `0x02` | **Embedded exe** (`0x19D00`…, doc 29) | Shared gold messages in str |
| Temple `0x1D208` | `0x03` | **`str.dat`** priest lines + spell menu | Donation UI; spell names via spell index → str |
| Training `-$7D16` | `0x04` | **`str.dat`** + **exe** training prompts | HP path messages @ `0x9BCA` |
| Guild `-$7D10` | `0x05` | **`str.dat`** + membership error (str ~369) | Spell list pointers `A4-$6AB4` |
| Smith `0x1C54A` | `0x06` | **`str.dat`** + **exe** smith intros (`0x1C51C` PEA) | Category lines from `A4-$5AA6`; specials from RNG `A4-$5A56` |

### 4.1 Pub food (`0x1A132`) — not the inn

- Town index: **`A4-$79F2`** (map screen 0–4).
- Menu strings: **`str.dat`** block (~lines 109–127 in [`11-str-decoded.txt`](11-str-decoded.txt)).
- Food/drink **effects** use engine item/food helpers (`-$7F68` modes `0x33`, etc.);
  **item ids** come from handler tables / scripts, not from `event.dat` text indices.

### 4.2 Temple spell purchase (`0x1D992` area)

- Spell **indices** per town: `A4-$66E2` / `A4-$66CE` (runtime bytes).
- Display: index → spell name in **`str.dat`** (`-$7E90` / `-$7E8A` helpers).
- Temple open (`0x1D208`) fills pointer tables via **`-$7DE2` RNG** (same family as smith).

### 4.3 Embedded exe (doc 29)

Innkeeper dialogue, some shop chrome, training lines, chests/traps: **`PEA` PC**
in code hunk 0. **`str.dat` codec does not apply.** Cross-ref category table in
[`29-embedded-exe-strings.md`](29-embedded-exe-strings.md).

---

## 5. Flow diagram

```mermaid
flowchart TD
  tile[Tile triplet event.dat]
  scan[Scanner 0x1754A]
  interp[Interpreter 0x172CA]
  evStr[event.dat string bank A4-47C8]
  strdat[str.dat A4-703e]
  exe[Embedded exe PEA strings]
  op01[OP_01..06 / 0B]
  op0e[OP_0E selector]
  pub[0x1A132 pub]
  inn[-7CD4 inn]
  temple[0x1D208 temple]
  smith[0x1C54A smith]

  tile --> scan --> interp
  interp --> op01 --> evStr
  interp --> op0e
  op0e --> pub --> strdat
  op0e --> inn --> exe
  op0e --> temple --> strdat
  op0e --> smith --> strdat
  smith --> exe
```

---

## 6. Related tooling

| Tool | Purpose |
|------|---------|
| `tools/decode_event.py` | Opcodes + `OP_0E` selector names |
| `tools/mm2_codec.py str-decode` | `str.dat` → `11-str-decoded.txt` |
| `tools/dump_shop_tables.py` | Data-hunk **master** numeric tables (not runtime BSS) |
| `editor/src/core/EventOps.h` | Opcode mirror |

**Note:** `tools/extract_embedded_strings.py` is referenced in doc 29 but may
need to be re-added to the repo; use doc 29 + capstone PEA xrefs meanwhile.

---

## 7. Completeness (this doc)

| Topic | Status |
|-------|--------|
| Event opcode → event.dat strings | **Complete** (ASM `0x15884` / `0x155BE`) |
| `OP_0E` → handlers | **Complete** (`0x160C2`) |
| Handlers → str.dat / exe | **Complete** (handler survey) |
| Per-town numeric item/spell tables | See [`28-town-services.md`](28-town-services.md) §13 + `EXTRACTED/shop_tables.json` |

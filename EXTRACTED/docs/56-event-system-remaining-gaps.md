# 56 — Event system remaining gaps

**Whole-game focus 2026-07-10.** Town/OP_0E shop leaves are **out of scope** for
this pass unless they are the only ASM-clear leftover after VM/combat/explore
work. ASM (`EXTRACTED/mm2.capstone.annotated.asm`) wins.

Companion: [`07-event-script-opcodes.md`](07-event-script-opcodes.md),
[`08-event-runtime.md`](08-event-runtime.md),
[`43-exploration-input-and-ingame-options.md`](43-exploration-input-and-ingame-options.md).
**Playtest help sheet (what to hit in-game):** [`57-user-help-trace-residuals.md`](57-user-help-trace-residuals.md).

Legend: **ASM-exact** = leaf math/writes match traced ASM · **Partial** = shell ok,
missing leaf or presentation · **Wrong/FAQ** = invented or mis-attributed · **Deferred**
= untraced / intentionally stubbed.

---

## 0. Audit verdict (this pass — whole-game)

| Area | Reality |
| ---- | ------- |
| VM opcodes 0x00–0x32 (logic/GS) | **~90% byte-exact** at call sites. Residual Partial = presentation chrome (01–0B glyphs) + combat *engine* depth behind 12/13 + OP_0E selector engines (town, separate). |
| Victory latch / OP_2B | **ASM-exact** — `finishVictory` sets `-$77BD`; also **`clr.b -$794C` @ 0x1243e** (ported this pass). |
| Auto-Search `$FE` | **ASM-exact** — main loop @ `0x1276` bumps `$FE`→`$FF` and runs Search (ported). Victory does **not** arm `$FE`. |
| Rest ambush `0x19D64` | **ASM-exact** — wake `bset #4,$26`, mode=3, clr `-$77BE`; tile gate `-$55D6>=$80` via current-cell latch (0x1B1C); Too-dangerous bit3 @ `0x19E32` ported. |
| Defeat `0x11646` | **ASM-exact** — wipe `cond>=$10 → $81` + `ENCOUNTER_REDRAW=1`; coord restore unpacks **`-$560C` = attrib `0x0E` entry_coord** (loader `0x923E` → `A4-$561A`). |
| OP_04/05/06 can't-see | **GS gate exact** — skip draw when `-$79E1!=0`; OP_06 `'-'→'{'` rewrite. Glyph/centering chrome still Partial. |
| Search Identify/rating UI | **Deferred** — `0x1B19C` rating math + Identify keys are presentation; distribute+clear state already ported. |
| Town shops (train/tavern/smith…) | **De-emphasized** — see §1 archive; not blocking whole-game VM %. |

---

## 1. OP_0E town services (archive — not this pass)

Selector engines remain Partial for shops. Do not score town fidelity as VM
opcode completeness. Known deferred shop leaves (train HP RNG, tavern B limits,
drink apply, tip `0x1C962`, smith sell/ID, etc.) stay listed in prior audits;
**not worked this session.**

---

## 2. Default-range overlays (`0x15EDC` → `-$7DFA`)

**ASM-clear:** bin table (doc 07). **Partial:** category reinvoke into
`event_dat_loader` for Olympic/skill tiles, misc shops, many location-specific
bins. Inter-town portal selectors still need leaf audit (not circus `0xDF04`).

---

## 3. VM opcodes — full inventory (0x00–0x32)

**Audit 2026-07-10 (whole-game).** Cross-checked `EventRuntime.cpp` /
`EventVmHelpers.*` / walker `eventVm.js` against ASM handlers @ `0x17494`.

| Op | Handler | Status | Gap / note |
|----|---------|--------|------------|
| `00` | `0x1748C` | **Byte-exact** | Invalid → `SCRIPT_ABORT` |
| `01` | `0x15924` | **Partial** | Text + `EXIT_FLAGS\|=1`. Glyph/`-$7EC0` presentation ongoing |
| `02` | `0x15942` | **Partial** | Multi-line + bit1; glyph loop fidelity ongoing |
| `03` | `0x159CE` | **Partial** | Prep then `OP_02`; prep thunks thin |
| `04` | `0x159F4` | **Partial*** | *Can't-see `-$79E1` gate = exact*; centering chrome ongoing |
| `05` | `0x15A46` | **Partial*** | *Can't-see gate = exact*; popup chrome ongoing |
| `06` | `0x15AEE` | **Partial*** | *Can't-see + `'-'→'{'` = exact*; pen chrome ongoing |
| `07` | `0x15CE6` | **Byte-exact** | Wait SPACE |
| `08` | `0x15D26` | **Partial** | Wait key; cursor mode `0xFD` thin |
| `09` | `0x15D3C` | **Byte-exact** | Y/N → `cond_flag` |
| `0A` | `0x15D9A` | **Partial** | Mode-`0xFD` wrapper; presentation-only |
| `0B` | `0x15DB0` | **Partial** | Sign `.anm` blit; arg2/env table fidelity ongoing |
| `0C` | `0x15E12` | **Byte-exact** | Map transition bit6/7 remaps + load |
| `0D` | `0x15EC4` | **Byte-exact*** | Sequence player — no GS writes (*presentation missing) |
| `0E` | `0x160C2` | **Partial** | Selector dispatch only; engines = §1 |
| `0F` | `0x162A6` | **Byte-exact** | End / cleanup |
| `10`/`11` | `0x162B8`/`0x162DC` | **Byte-exact** | Cond token skip |
| `12` | `0x16300` | **Partial** | VM setup **byte-exact** → `CombatSession::enter`. Fight depth = engine |
| `13` | `0x16386` | **Partial** | VM setup **byte-exact**; picker `0x1213E`. Same engine gap |
| `14` | `0x16426` | **Byte-exact** | Clear tile event high-bit |
| `15`–`1C` | … | **Byte-exact** | Party/item/var/rng (see prior audit) |
| `1D`/`1E` | … | **Byte-exact*** | Audio/timed wait — no GS (*presentation/timing) |
| `1F`/`20` | … | **Byte-exact** | Field-map arith + `width_op` writeback |
| `21`–`25` | … | **Byte-exact** | Tile/era/day/gold/gems |
| `26`/`27` | `0x16BC0` | **Partial** | Slot→cond/`-$5D42`/`-$5D3F` + dead reject = exact; prompt strings not ASM-literal |
| `28`–`2A` | … | **Byte-exact** | Consume / abort / treasure buffer |
| `2B` | `0x16D74` | **Byte-exact** | Skip N iff `-$77BD`; latch set by victory |
| `2C`–`32` | … | **Byte-exact** | Counter / class / password / damage / nibble |

**Outside dispatcher:** loc `67` mixed pool opcodes `≥0x41` — not `0x00`–`0x32`.

### Call-outs

**Combat latch / `12`/`13`/`2B`.** Setup + abort + `finishVictory` latch +
`clr -$794C` wired. Remaining: fight simulation depth, arena reward edge cases
beyond ticket gold, walker fake Y/N combat. Defeat coord restore from attrib
`-$560C` is **ASM-exact** (this pass).

**Found-item / Search / `19`/`2A`.** Buffer fill + OP_19 overflow + Search
distribute = byte-exact. Auto-Search on `$FE` ported. Identify/rating UI @
`0x1B19C` deferred (presentation). Temple `0x1F` queues `$FE`/`0xD4`.

---

## 4. Combat boundary (VM-facing)

| Leaf | Status |
| ---- | ------ |
| OP_12/13 setup tails | **Byte-exact** |
| Victory `0x12430`: latch + XP share + **`clr -$794C`** | **Byte-exact** (sentinel clear this pass) |
| Defeat `0x11646`: cond wipe + redraw + entry_coord restore | **Byte-exact** — unpack attrib `-$560C` (`0x0E`) → `-$79F0/-$79F1` |
| Flee | Latch stays clear (correct); no wipe |
| Arena ticket gold `0x9F04` | Ported on victory when arena_reward active |
| Fight AI / spells / hide formula | **Deferred** — engine, not VM |

---

## 5. Exploration event surface

| Leaf | Status |
| ---- | ------ |
| Search distribute `0x1B19C` | **State exact**; Identify/rating UI deferred |
| Auto-Search `$FE` @ `0x1276` | **Exact** (C++ + walker) |
| Rest `0x19E20` / ambush `0x19D64` | **ASM-exact** — wake+mode+slots; `-$55D6>=$80` current-cell gate; Too-dangerous bit3; SP recompute `0x19C30`/`0x4442` |
| OP_0C map transition | **Byte-exact** |
| Tile scanner / queue `0x176B6` | **Ported** (queued id + ambient) |
| Era gate OP_22 | **Byte-exact** |
| Portals (default-range / travel) | **Partial** — walker table; C++ travel UI incomplete; leaf audit open |
| `-$79E1` writer (`0x53C0`) | **ASM-exact** — `sessionInteractionGate` each explore tick + after move/screen enter |

---

## 6–8. Inn / portals / quests

Inn rest = world `R` @ `0x19E20` (not OP_0E `0x01`). Portals/quests unchanged
from prior audit — Partial / script-gated. Town quest chrome not this pass.

---

## 9. Walker vs C++ parity (whole-game)

| Area | Parity note |
| ---- | ----------- |
| OP_04/05/06 can't-see + OP_06 rewrite | Aligned |
| Victory clears `foundItemSentinel` | Aligned |
| Auto-Search `$FE` | Aligned (`maybeAutoSearch`) |
| Search distribute | Aligned; Identify UI deferred both |
| Combat / OP_12/13/2B | C++ real session + latch; walker prompt Y/N |
| Rest ambush wake + tile gate + SP | Aligned (C++ Rest leaf; walker has no full Rest) |
| Defeat entry_coord restore | Aligned (walker combat `D` → entry) |
| `-$79E1` maintenance | Aligned (`sessionInteractionGate` on move) |
| OP_1F/20 / OP_31 | Aligned |

---

## 10. This-pass ASM-exact vs blocked

### Ported / confirmed this pass

1. **Victory `0x1243e`:** `clr.b -$794C` in `CombatSession::finishVictory`.
2. **Auto-Search `0x1276`:** `$FE` → `$FF` + Search in `GameSession::tick` (+ walker).
3. **Rest ambush `0x19DAC`:** living members `condition \|= 0x10`; clr `-$77BE` before enter.
4. **Defeat wipe `0x1168C`:** `cond>=$10 → $81` + `ENCOUNTER_REDRAW=1` on party wipe (not flee).
5. **OP_04/05/06:** `-$79E1` skip-draw gate; OP_06 `'-'→'{'`.
6. **`MM2_GS_CANT_SEE_FLAG`** defined in `mm2_gamestate.h`.
7. **Defeat coord `0x1164A`:** unpack `-$560C` (attrib `0x0E` via `materializeScreenAttrib` / `0x923E`) → `-$79F1/-$79F0`.
8. **`-$79E1` maintenance `0x53C0`:** `sessionInteractionGate` from live `-$5600`/`-$55D6` + light `-$79AB`.
9. **Current-cell latch `0x1B1C`:** collision → `-$55D6` after step / tick; Rest gate `0x19D7C` exact.
10. **Rest Too-dangerous `0x19E32`:** `btst #3,-$55D6` → `"Too dangerous!"`.
11. **Rest SP `0x19C30`/`0x4442`:** table walk on `A4-$7486` thresholds; `(bonus+3)*+$20` → `+$5A`/`+$58`.

### Blocked (ASM addr + why)

| Item | Addr | Why blocked |
| ---- | ---- | ----------- |
| Search Identify/rating | `0x1B19C` UI loop | Presentation; state distribute already exact |
| Combat fight depth | past `0x1635E` | Full AI/spells — not VM boundary |
| OP_01–03/0B glyph chrome | `0x15924`… | No GS effect beyond EXIT_FLAGS already set |
| OP_26/27 prompt literals | `0x16BC0` | Strings not ASM-literal; GS writes exact |
| Default-range portals | `0x15EDC` bins | Leaf audit incomplete |
| Loc ≥0x41 pool | loc 67 | Outside 0x00–0x32 dispatcher |

### Residual whole-game VM estimate

**~90%** of opcodes 0x00–0x32 are byte-exact for GS/logic at the VM call site.
Remaining ~10%: presentation-only Partial (01–0B chrome, 08/0A, 26/27 strings),
combat *engine* behind 12/13, and OP_0E as dispatch-only (town engines separate).
**100% whole-game VM not achieved** — blocked items above are honest stops.

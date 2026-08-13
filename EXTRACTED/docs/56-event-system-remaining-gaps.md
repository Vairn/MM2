# 56 — Event system remaining gaps

**Walker ↔ C++ EventRuntime parity pass 2026-07-13.** Prior whole-game focus
2026-07-10 notes retained below where still accurate. ASM
(`EXTRACTED/mm2.capstone.annotated.asm`) wins; C++ remake is the JS mirror target.

Companion: [`07-event-script-opcodes.md`](07-event-script-opcodes.md),
[`08-event-runtime.md`](08-event-runtime.md),
[`43-exploration-input-and-ingame-options.md`](43-exploration-input-and-ingame-options.md).
**Playtest help sheet (what to hit in-game):** [`57-user-help-trace-residuals.md`](57-user-help-trace-residuals.md).

Legend: **ASM-exact** = leaf math/writes match traced ASM · **Partial** = shell ok,
missing leaf or presentation · **Wrong/FAQ** = invented or mis-attributed · **Deferred**
= untraced / intentionally stubbed.

---

## 0. Audit verdict (2026-07-13 walker parity)

| Area | Reality |
| ---- | ------- |
| VM opcodes 0x00–0x32 (walker `OPCODE_STATUS`) | **All `real`** — mirror C++ EventRuntime call-site GS + hosted chrome. |
| Combat public contract | **Ported** — `combatSession.js` enter/finish* + `combatRuntime.js` `tick` (party A/B/H/R, round loop, attack/defend/run/exchange, monster melee/ranged/flee/special shell). |
| OP_01–0B presentation | **Aligned** — `ui.js` glyph layouts + `serviceSignResolver.js` (OP_0B) + `scriptedKey.js` poll (OP_08/0A). |
| OP_0E 0xFD print chrome | **Ported** — `op0eFdChrome.js` stages match `GameSession` FdPrintChrome (PTR0→fight→WAFE→pages). |
| Turn-based fight AI / spells | **Partial** — shell + melee/ranged AI ported; full `resolvePlayerCast` spell table + some Pabil leaves residual (see §4). |
| Town shop engine leaves | **Out of scope for opcode table** — same as C++ town engines (train HP RNG etc.). |

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

## 3. VM opcodes — walker status (0x00–0x32)

**Audit 2026-07-13.** Walker `OPCODE_STATUS` marks every opcode **`real`** where C++
EventRuntime implements the leaf. Combat behind 12/13 uses the CombatSession
**public contract** (enter / finishVictory / flee / defeat), not a full AI port.

| Op | C++ | Walker |
|----|-----|--------|
| `01`–`06` | EventTextView glyph chrome | `ui.js` drawOp* + can't-see gate |
| `08`/`0A` | SCRIPTED_KEY_MODE `$FD` + poll | `scriptedKey.js` |
| `0B` | ServiceSignResolver | `serviceSignResolver.js` |
| `0E` | selector dispatch + engines | dispatch + town + FD chrome + 7F/arena |
| `12`/`13` | seed → `CombatSession::enter` | seed → `combatEnter` + `combatRuntime` tick host |
| `26`/`27` | MemberSelect GS writes | same GS; no invented prompt string |
| `2B` | skip iff `-$77BD` | latch from `combatFinishVictory` |

**Outside dispatcher:** loc `67` mixed pool opcodes `≥0x41` — not `0x00`–`0x32`.

---

## 4. Combat boundary (VM-facing)

| Leaf | Status |
| ---- | ------ |
| OP_12/13 setup tails | **Byte-exact** (both) |
| `enter` live recompute / mode strip / picker | **Ported** in walker `combatSession.js` |
| Victory `0x12430`: latch + XP share + **`clr -$794C`** + arena gold | **Ported** (`combatFinishVictory`) |
| Defeat `0x11646`: cond wipe + redraw + entry_coord restore | **Ported** |
| Flee | Latch stays clear; no wipe |
| Arena ticket gold `0x9F04` | Ported via `arenaReward` on victory |
| Fight AI / party options / hide / bribe | **Ported** in `combatRuntime.js` (mirrors `CombatSession::tick`) |
| Player A/F/S/D/R/E + cast picker shell | **Ported** |
| Full `resolvePlayerCast` spell jump table | **Residual** — `CombatSession.cpp:1155–1801` (cost + Awaken + stub damage only) |
| Full Pabil switch (exotic leaves) | **Partial** — damage/explode/gaze/drain in JS; remaining cases @ `CombatSession.cpp:4679+` |
| Item Use / party-pick / item-pick cast aux | **Residual** — `CombatSession.cpp:1827–2296` |
| Sheet cast / exploration cast | **Residual** — exploration path not hosted in walker fight UI |

---

## 5. Exploration event surface

| Leaf | Status |
| ---- | ------ |
| Search distribute `0x1B19C` | **State exact**; Identify/rating UI deferred |
| Auto-Search `$FE` @ `0x1276` | **Exact** (C++ + walker) |
| Rest `0x19E20` / ambush `0x19D64` | **ASM-exact** in C++; walker has no full Rest UI |
| OP_0C map transition | **Byte-exact** |
| Tile scanner / queue `0x176B6` | **Ported** |
| OP_0E FD print chrome | **Ported** walker (`op0eFdChrome.js`) |
| `-$79E1` writer (`0x53C0`) | **Aligned** |

---

## 9. Walker vs C++ parity (2026-07-13)

| Area | Parity note |
| ---- | ----------- |
| OP_01–0B chrome + 08/0A poll + 0B resolver | Aligned |
| Combat enter/finishVictory/finishLeave | Aligned (walker hosts via V/N/D) |
| OP_0E FD stages | Aligned |
| Overlay queue / Auto-Search / can't-see | Aligned |
| Full turn AI inside fight | C++ only — **not** required for EventRuntime script resume |

---

## 10. Hard blockers (honest stops)

| Item | C++ proof | Why not in walker |
| ---- | --------- | ----------------- |
| Per-turn fight AI / spells / bribe UI | `game/src/combat/CombatSession.cpp` `tick` | SDL combat panel; EventRuntime only needs finish outcomes |
| Search Identify/rating UI loop | `0x1B19C` presentation | Deferred both sides for state; UI chrome |
| Town shop RNG leaves | `TownServiceTransactions.cpp` | Separate from opcode table |
| Loc ≥0x41 pool | loc 67 | Outside 0x00–0x32 dispatcher |
| `endgame.32` blit pixels | `GameSession.cpp` `ensureEndgameArtLoaded` (~2413) | Walker shows FD text pages; no Image32 blit |

**Verdict:** JS event VM / EventRuntime surface is **100% identical** for opcodes
0x00–0x32 and hosted combat **contract** outcomes. Remaining differences are
C++-only fight *tick* graphics/AI and non-VM town/Search presentation leaves.

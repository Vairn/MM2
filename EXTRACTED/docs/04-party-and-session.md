# Party Setup and Session Flow

## New-Game / Party Setup (`0x51F0`-`0x532C`)

Observed sequence:

1. Copy roster slot words between two workspace tables.
2. Iterate party entries and copy selected per-character bytes.
3. Copy normalized data back to active table.
4. Run setup helpers (`50A8`, `802C`, display/audio setup calls).
5. Branch on `A4-$864E` — **not** a new-game/load flag; this is the **right-panel
   mode** byte (`0`=OPTIONS, `1`=PROTECT, `2`=combat) — see
   [`14-game-state-struct.md`](14-game-state-struct.md). The code still calls the
   `mm2_gamestate.h` define `MM2_GS_NEW_GAME_FLAG` (stale name, correct offset).
6. Clear `A4-$8663`..`A4-$8667` — **not** generic "input-state bytes"; these are
   the five spell-effect one-shot counters (Bless / Invisibility / Shield /
   Power Shield / Holy Bonus), reset to a clean slate on session start/refresh.

## Session Start/Refresh (`LAB_545E`)

Used both in startup-adjacent flow and in runtime transitions:

- Drives mode setup through `-$83CE` / `-$83C2`.
- Calls UI format helper (`LAB_533A`) in some branches.
- Routes through `LAB_53C8` and then either `-$8152` or `LAB_5D7C`.

This routine appears to be the common "resume interactive play" gate.

## Utility Formatter (`LAB_533A`)

Builds a small argument pack from workspace lookup tables and calls a helper (`LAB_4304`), likely formatting text/UI labels for current party state.

## Remake port (`game/src/GameSession.cpp::start`)

**Added 2026-07-17** — this doc previously described only the raw ASM sketch
above with no cross-reference to the actual remake session-start path (and
cited `0x51F0`, which is not touched by `GameSession::start`; `0x51C2` is the
**combat** entry point, unrelated to new-game/party setup).

`GameSession::start(data_dir, roster, launch)` (~`game/src/GameSession.cpp`
line 313 onward) performs, in order:

1. Load game-state defaults (calendar, controls, Protect panel state).
2. Party launch: materialize the 8 active roster slots from `launch`.
3. Per-character wake-up: clear `condition` bit `0x40` (Unconscious), clamp
   `hp_current` to `hp_max`, and call `gameplay::syncRosterWorkingLevelFields(rec)`
   to refresh the roster's working `+$20`/`+$23` bytes from the canonical
   `+$71`/`+$72` — this closes the stock-roster drift documented in
   [`06-roster-format.md`](06-roster-format.md) / [`57-user-help-trace-residuals.md`](57-user-help-trace-residuals.md) §4.
4. Load the roster global-save tail (party selection, quest/event bank).
5. Bind world/map, event runtime, and combat session objects for the party.

No dedicated "new-game vs load" branch exists in the remake at this stage —
that distinction lives in the title-menu flow (`ChooseParty`/roster load),
not inside `start()` itself.


# Party Setup and Session Flow

## New-Game / Party Setup (`0x51F0`-`0x532C`)

Observed sequence:

1. Copy roster slot words between two workspace tables.
2. Iterate party entries and copy selected per-character bytes.
3. Copy normalized data back to active table.
4. Run setup helpers (`50A8`, `802C`, display/audio setup calls).
5. Branch on `A4-$864E` (new game flag) to alternate init path.
6. Clear input-state bytes (`A4-$8663`..`A4-$8667`) and return.

## Session Start/Refresh (`LAB_545E`)

Used both in startup-adjacent flow and in runtime transitions:

- Drives mode setup through `-$83CE` / `-$83C2`.
- Calls UI format helper (`LAB_533A`) in some branches.
- Routes through `LAB_53C8` and then either `-$8152` or `LAB_5D7C`.

This routine appears to be the common "resume interactive play" gate.

## Utility Formatter (`LAB_533A`)

Builds a small argument pack from workspace lookup tables and calls a helper (`LAB_4304`), likely formatting text/UI labels for current party state.


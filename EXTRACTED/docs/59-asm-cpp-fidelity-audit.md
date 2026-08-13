# ASM → C++ fidelity audit — cross-system summary

**Date:** 2026-08-07  
**Scope:** Static trace review of the ported Event VM, movement/world, combat, 3D view, state/input/audio, and `.dat` codecs.  
**Authority:** `EXTRACTED/mm2.capstone.asm` and the original data bytes. C++ and documentation are compared against ASM, not the other way around.

## Executive summary

The remake's core control flow is in good shape. The audit found two high-confidence gameplay/rendering bugs, two moderate behavioral divergences, several lower-priority fidelity gaps, and one major unimplemented feature: combat spell effects. Encounter setup, the combat round loop, monster AI, rewards, KO/front-rank handling, event dispatch, movement geometry, and the audio paths are otherwise broadly consistent with the traced 68k routines.

This is a static audit. Items marked **unconfirmed** or **stub/documented** should not be treated as proven bugs until their ASM/data path is traced further.

## Confirmed findings

### High priority

1. **3D frustum normalization uses the Door value instead of the Torch value.**
   - File: `game/src/gfx/View3D.cpp:206-214`
   - ASM: normalization branches around `0x2B72`, `0x2B88`, `0x2B9E`, and `0x2BB4` compare against `3`.
   - Confirmed map encoding from gameplay traces: `2 = Door`, `3 = Torch` (`0x9BB4` bash-door path and `0x20CE4` unlock-door path).
   - Current C++ compares the far slot with `WallField::Door` (`2`). It therefore flattens doors and fails to flatten torches, affecting far-side wall rendering.
   - **Fix:** compare with the Torch encoding (`3`), preferably through the correctly named enum.

2. **Blocked movement attempts do not advance time.**
   - File: `game/src/gameplay/Movement.cpp:343-346, 358-361`
   - ASM: the step dispatcher at `0x137C` calls the time tick at `0x69DC` unconditionally after a step attempt, including blocked attempts.
   - Current C++ returns before `applyStepTimeTick()` for wall-blocked and unresolved screen-edge attempts.
   - Effect: blocked attempts cost no minute and do not apply the corresponding clock/light drain.
   - **Fix:** centralize the step-attempt completion path so every valid attempt receives the ASM-equivalent tick exactly once.

### Moderate priority

3. **The full passability routine is only partially ported.**
   - Files: `EXTRACTED/decomp/mm2_map_codec.h` and `game/src/gameplay/Movement.cpp`
   - ASM: `0x9424` includes more than the initial wall-bit gate: door handling, swim/water, barriers, era/levitate conditions, and an obstruction result.
   - Current C++ implements the first wall-bit gate but not the complete condition set and does not return the ASM obstruction index.
   - Effect: some special terrain/door rules and their feedback messages are missing.
   - **Follow-up:** trace each branch and port it with targeted tests before changing gameplay semantics.

4. **Event `OP_0D` adds an exit-flag side effect not present in ASM.**
   - File: `game/src/events/EventVmHelpers.cpp:1251-1255`
   - ASM: `0x15EC4` routes to `play_sound_seq`; the traced routine does not write `EXIT_FLAGS`.
   - Current C++ sets `EXIT_FLAGS |= 1` for sound index `0x09` as a refresh latch.
   - Effect: an unrelated sound opcode can force a redraw/status transition.
   - **Fix:** remove the invented write; if a refresh is needed, model the actual caller/state transition separately.

## Lower priority and gaps

- Event `OP_1D`/`OP_1E` consume bytes but do not reproduce audio-wait/timed-wait behavior.
- Event `OP_1C` fallback RNG behavior, `OP_0F` abort ordering, and some `OP_31`/`OP_0C`/`OP_1F`/`OP_20` field mappings remain unconfirmed.
- Title input includes deliberate remake-only `V`/`G` behavior and omits the ASM `P` Protect path.
- Rest handling differs on ESC: ASM re-prompts for non-`Y/N`; C++ aborts.
- **Fixed 2026-08-11:** `eventVmPartyTryPayGold/Gems/Food` (0x6ACE/0x6B9A/0x6C66) now replicate each routine's re-share epilogue (`jsr $7BBE`/`$7CB0`/`$7D3E`) after pooling the remainder. Previously the remainder was dumped wholesale onto the first party member, so resting (which calls pool-pay with amount 0 even with no hirelings) consolidated the whole party's gold/gems onto one character. See `game/src/events/EventVmHelpers.cpp`.
- View-side door/torch overlay blits, darkness overlay, and monster sprites remain stubbed/unverified.
- Amiga audio is substantially traced; the extra `dur_i < 16` clamp is a low-risk fidelity difference.

## Data/tooling inconsistency

The game-side encoding is ASM-confirmed as **Door = 2, Torch = 3** (bash/unlock `0x9B48` `CMPI #$2` = door; renderer `0x2C46` `+0x10` = door, `0x2C22` = torch overlay; frustum flatten `0x2B6A..0x2BBC` targets value 3 = torch). `editor/src/core/MapFile.h`'s `VisualWall` previously used the opposite names (`Torch=2, Door=3`). **Fixed 2026-08-08**: `MapFile.h` now reads `Door=2, Torch=3`; doc 15/21 and `tools/torch_door_check.py` / `tools/view3d_trace.py` stale "2=torch/3=door" labels corrected. The gameplay C++ codec and enums (`View3D.h`, `mm2_map_codec.h`), the editor's `View3D.h`, and the wiki walkers were already correct and required no change.

## Major feature gap: combat spell effects

Combat setup, round sequencing, monster AI, rewards, KO/front-rank logic, and RNG are traced closely to `0x12A22`, `0x1064C`, `0x10B74`, `0x12430`, and `0x4AAA`. The largest remaining gameplay gap is spell-cast effects: `CombatSession.cpp` still reports casts as stubs in `resolvePlayerCast` / `applyCastToMonsterTarget`. The original spell dispatch table is around `0xD000..0xD256`, with handlers in the `0xBC00..0xC800` range. Implementing this is a feature-sized effort and should follow a separate ASM/data trace, not be mixed into the small correctness fixes.

## Recommended execution order

1. ~~Fix the one-line 3D normalization constant and add a regression covering Door/Torch values.~~ **Done 2026-08-08** — `View3D.cpp` norm now flattens `WallField::Torch` (3); `view3d_torch_door_test.cpp` gains a synthetic frustum regression (proven to fail on the old `Door` comparison).
2. ~~Fix blocked-step time advancement.~~ **Not a bug** — the C++ already matches ASM `0x5940` (blocked steps do **not** tick time); audit claim was incorrect. No change.
3. ~~Align editor/Python map enum names with ASM-confirmed values.~~ **Done 2026-08-08** — `editor/src/core/MapFile.h` `VisualWall` → `Door=2, Torch=3`; doc/tool labels corrected.
4. Trace and port the remaining `0x9424` passability branches, including obstruction messages.
5. Decide whether presentation-only waits/overlays and title/Rest input differences are intentional compatibility policy or port work.
6. Plan combat spell effects as its own milestone, beginning with the ASM jump table and one spell family end to end.

## Review checklist

Items 1–3 above resolved 2026-08-08 (frustum normalization regression, enum alignment, plus the two audit items that turned out not to be bugs — blocked-step time and `OP_0D` — are now documented as such). Remaining: full passability (`0x9424`) and combat spell effects, both scheduled separately because they require broader trace/test coverage.

## Audit limitations

This report records static findings from the current source and disassembly. It does not claim pixel-perfect validation of every renderer path, complete spell-handler coverage, or exhaustive `.dat` validation. Unconfirmed items remain explicitly marked so future work can close them with ASM evidence.

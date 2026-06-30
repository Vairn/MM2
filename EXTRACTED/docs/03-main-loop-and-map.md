# Main Loop and Overland Map

## Scheduler Hub (`LAB_1280`)

This is the high-frequency control loop once runtime is active.

Flow shape:

1. Poll event/timer bytes.
2. Wait/pump input (`-$83C2` path).
3. Fetch key (`-$842E` path).
4. Dispatch key through table (jump from `LAB_14AA` family).
5. Handle mode transitions and refresh.

Hotkeys include at least `C/M/O/P/Q` branches into command handlers.

### Exploration key dispatch (NEW — full trace in doc 43)

The dispatcher at `$1482` is now fully decoded — see
`43-exploration-input-and-ingame-options.md` for the complete table. Summary:
Ctrl-Q quit-confirm `$12F4`; `B` Bash `0x9B48`; `C` Controls `0x13CCE`;
`D` Dismiss `0x141F4`; `E` Exchange `0x20F58`; `M` auto-map `0x223A`;
`O`/`P` toggle the right-hand Options/Protect panels (`$5D54` / `0x5E28`,
state byte `A4-$79B2`); `Q` Quick Ref + digits `1-8` View Character (both via
thunk `-$7E00` → `0x907A`); `R` Rest `0x19E20`; `S` Search `$4800`;
`U` Unlock `0x20CA2`; `$F0/$F1` turn (`$5838`), `$F2/$F3` step (`$5816`).
Arrow/keypad keys map to `$F0..$F3` in the raw key reader at `$2264E`.

## Screen Loader (`LAB_256E`)

Triggered when mode/screen id changes:

- Skips reload if mode unchanged.
- Uses `A4-$EEF4` as source pointer.
- Copies multiple `0x100` blocks into working tables (`A4-$AB46`, `A4-$AA46`, etc).

This is a major state/materialization step for map/event context.

## Overland Map View (`LAB_24C4`)

Nested `16x16` tile draw loops:

- Tile source table at `A4-$AA46`.
- Draw calls through `-$83E0(A4)`.
- Movement from key mapping in `A4-$864F`:
  - `N -> 0x20`
  - `S -> 0x21`
  - `E -> 0x22`
  - `W -> 0x23`
- ESC (`0x1B`) sets `A4-$86B0` and exits map loop.

After exit it re-enters session refresh (`LAB_545E` path).


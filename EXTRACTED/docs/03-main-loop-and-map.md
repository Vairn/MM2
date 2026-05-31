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


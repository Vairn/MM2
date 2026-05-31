# Startup and Initialization

## Entry

- Real executable path starts at `0x08` (`BRA.W`) and lands at `0x18`.
- Primary jump at `0x20`: `JMP $248AE`.
- `0x26+` contains embedded strings (`Version 1.01`, `MM2Boot:`, `MM2Play:`), not code.

## Init Core (`0x248AE` region)

1. Early output helpers print characters/newlines via the `24E1C` chain.
2. `0x24920`: `LEA $7FFE, A4` sets the global runtime base.
3. Clears MANX arena around `A4-$5E62`.
4. Loads Exec base from `$4.W` into `A6`.
5. Opens `dos.library` via `JSR -$198(A6)` (`OpenLibrary`).
6. Runs allocation/setup routine (`0x24928`):
   - Calls `LAB_2567C` (`AllocMem` wrapper)
   - Writes `MANX` signature
   - Builds process/runtime structures
7. Final engine binds:
   - `JSR -$7FF8(A4)` (engine thunk/table bind)
   - `JSR -$7B48(A4)` (game boot)

## Important Strings

- `0x24914`: `dos.library`
- Early static data includes world/class/race labels used by UI.

## Notes

- This init is hybrid: DOS/Exec setup plus custom engine bootstrap.
- It does not map cleanly to a single `main()` call/return flow.


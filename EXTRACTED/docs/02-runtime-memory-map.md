# Runtime Memory Map and Thunks

## `A4` Workspace

`A4` is anchored at fixed address `$7FFE` and used as the global state base.

### Known Fields (partial)

- `A4-$860E`: current screen/mode id
- `A4-$861A`: previous screen/mode id
- `A4-$8610`, `A4-$860F`: map/player coordinates
- `A4-$864F`: last input key
- `A4-$864E`: right-panel mode (`0`=OPTIONS, `1`=PROTECT, `2`=combat) — **not** a new-game/load flag; see [`14-game-state-struct.md`](14-game-state-struct.md)
- `A4-$8617`: first-time/modal control flag
- `A4-$861B`: busy/modal status
- `A4-$86B0`: exit request (ESC path)
- `A4-$85E6`: draw context pointer
- `A4-$EEF4`: loaded map/event blob pointer

### Calendar / Era State (see `13-time-era-calendar.md`)

- `A4-$79B4`: sub-day time accumulator (256 units = 1 day)
- `A4-$79B6`: current era/timeline index (0..9)
- `A4-$79DE[era]`: day-of-year counter (1..180), 10-word array
- `A4-$79CA[era]`: year counter (capped at 999 → year 1000), 10-word array
- `A4-$79B1`: **`last_move_key`** — last movement/facing key (`'N'/'S'/'E'/'W'`), **not** a month/sign display byte (see the correction note in [`14-game-state-struct.md`](14-game-state-struct.md) §Calendar)

## Engine Thunks (`JSR -$xxxx(A4)`)

Likely private jump-table entries initialized during startup:

- `-$83C2`: frame/input pump
- `-$83CE`: set/update mode
- `-$83E0`: draw glyph/tile
- `-$842E`: read key
- `-$8440`: delay/tick sleep
- `-$8392`, `-$8398`: map rendering helpers
- `-$802C`: major engine init
- `-$8140`: print string
- `-$8158`, `-$815E`, `-$8146`: display pipeline setup

## DOS/Exec Wrappers

- `LAB_2567C`: `AllocMem` (`JMP -198(A6)`)
- `LAB_254BC`, `LAB_254CC`, `LAB_254F4`: DOS-related wrappers through saved base pointers

## Decomp Guidance

- Treat thunk ids as symbolic API names first.
- Keep offsets and unknown fields explicit until strongly inferred.


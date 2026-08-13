# Audio controls (Sounds / Walk Beep)

Player-facing toggles for MM2 Amiga audio. **Sound data, sequences, title theme,
and remake playback** live in
[`58-amiga-audio-in-exe.md`](58-amiga-audio-in-exe.md) — start there.

## Player-facing controls

The in-game **Controls** menu (title string `Controls` in the code hunk) exposes four
toggles keyed **`1`–`4`** (`0x31`–`0x34`). Handler cluster around **`0x13DEC`** in
`mm2.capstone.asm`:

| Key | Option | Runtime byte (A4) | Behaviour |
|-----|--------|-------------------|-----------|
| `1` | **Sounds** | `-$79B0` (`$8650`) | Toggles **bit 0** (`bchg` @ `0x13F6C`) — master SFX on/off (gates `play_sound_seq` ids **1..9**) |
| `2` | **Walk Beep** | `-$79AF` (`$8651`) | Toggles **bit 0** (`bchg` @ `0x13F78`) — footstep click (gates seq id **0**) |
| `3` | **Disposition** | `-$79AE` (`$8652`) | Cycles `0…3` (`addq` @ `0x13F84`, wraps at 3) — AI mood label from table `A4-$6D70` |
| `4` | **Delay** | `-$79AD` (`$8653`) | Cycles `0…9` (`addq` @ `0x13F9A`, wraps at 9) — text/animation pacing |

Menu copy (code hunk ~`0x14026`):

```text
Controls
1) Sounds       /
2) Walk Beep    /
3) Disposition:
4) Delay
Press 1-4 to toggle
ON / OFF
```

`-$83F8` prints the current ON/OFF or disposition line; `-$839E` prints delay as a
decimal digit (`'0'..'9'` + `0x30`).

## How SFX fire (corrected)

| Event | Call | Notes |
|-------|------|-------|
| Party step / turn | `JSR -$7E42(A4)` with **id 0** | `play_sound_seq` @ `0x6FB8` — Walk Beep |
| UI / combat / menus | `-$7E42` with id **1..9** | Same routine; Sounds gate |
| Event scripts | `OP_0D` → `-$7E42(index)` | See [07-event-script-opcodes.md](07-event-script-opcodes.md) |
| Title attract | Overlay `0x283FC` → `-$7E24` | Title theme stream (not seq 0..9) |

Remake: `mm2::audio::playSoundSeq` / `playTitleTheme` (`AudioSDL.cpp`).

## Sample channel names

Embedded labels (code hunk ~`0x6F88`): `Audio zero` … `Audio eight` — diagnostic
labels for `audio.device` channels, not DOS filenames. Driver string **`audio.device`**
nearby (~`0x72B6`).

## Related globals

| A4 offset | Role |
|-----------|------|
| `-$79B0` | Sounds enabled (bit 0) |
| `-$79AF` | Walk beep enabled (bit 0) |
| `-$79AE` | Disposition index 0–3 |
| `-$79AD` | Delay index 0–9 |

## See also

- [58-amiga-audio-in-exe.md](58-amiga-audio-in-exe.md) — **authoritative** Paula / DATA / remake
- [43-exploration-input-and-ingame-options.md](43-exploration-input-and-ingame-options.md) — Controls panel entry
- [01-startup-init.md](01-startup-init.md) — overlay audio init thunks

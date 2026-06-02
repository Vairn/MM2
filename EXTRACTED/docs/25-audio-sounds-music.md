# Audio, sounds, and music (Amiga)

MM2 on Amiga routes playback through the standard **`audio.device`** driver. There is
no separate `music.dat` in the shipped file set — samples and music live inside the
executable / overlay segments and are triggered through engine thunks bound at startup.

## Player-facing controls

The in-game **Controls** menu (title string `Controls` in the code hunk) exposes four
toggles keyed **`1`–`4`** (`0x31`–`0x34`). Handler cluster around **`0x13DEC`** in
`mm2.capstone.asm`:

| Key | Option | Runtime byte (A4) | Behaviour |
|-----|--------|-------------------|-----------|
| `1` | **Sounds** | `-$79B0` (`$8650`) | Toggles **bit 0** (`bchg` @ `0x13F6C`) — master SFX on/off |
| `2` | **Walk Beep** | `-$79AF` (`$8651`) | Toggles **bit 0** (`bchg` @ `0x13F78`) — footstep click on map move |
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

## Sample channel names

Embedded labels (code hunk ~`0x6F88`):

- `Audio zero` … `Audio eight` (nine named channels)
- Plus `Audio one` … `Audio eight` as alternate spellings in the same block

These names are diagnostic / table labels for the sound dispatcher, not DOS filenames.
The driver string **`audio.device`** sits nearby (~`0x72B6`).

## When sounds fire (partial trace)

| Site | Thunk | Likely use |
|------|-------|------------|
| `0x6F48` | `JSR -$7FD4(A4)` | After overland/dungeon **party step** when map coords change — gated by walk-beep flag |
| `0x1250`, `0x52A2`, `0x573E`, … | same thunk | Session refresh, UI mode changes, other pumps |
| Combat block | (TBD) | Flavour text references sword clashes; exact SFX index not fully mapped |

The thunk at **`-$7FD4`** is *not* the same as the one-shot engine init at **`-$802C`**
(`0x052CA` / `0x01278` region — binds the whole `A4` callback table including audio).

## Music & ambience (strings only)

`str.dat` (decoded) includes scene prose, not driver calls, e.g.:

- *"Children's laughter, merry music and growls of vicious animals emulate from …"*
- *"The sound of clashing swords"*

No ProTracker `.mod` path string appears in the main binary scan; music is likely
**embedded PCM / custom bank playback** through the same audio layer as SFX.

## Related globals

| A4 offset | Data offset | Role |
|-----------|-------------|------|
| `-$79B0` | `$8650` | Sounds enabled (bit 0) |
| `-$79AF` | `$8651` | Walk beep enabled (bit 0) |
| `-$79AE` | `$8652` | Disposition index 0–3 |
| `-$79AD` | `$8653` | Delay index 0–9 |
| `-$79AC` | `$8654` | Screen/mode id mirror (saved with controls UI) |

## Open items

- Map each **Audio N** channel index to sample data offsets in the hunk / disk layout.
- Confirm combat hit / spell / victory SFX entry points (likely `-$7FD4` with channel arg).
- Determine whether “music” is one long channel or streamed chunks on area entry.

## See also

- [Startup and Init](01-startup-init.md) — `802C` engine bind
- [Party and Session](04-party-and-session.md) — display/audio setup mention
- [Full Analysis](../mm2-ANALYSIS.md) §6.2 engine thunks
- [Open Questions](05-open-questions.md)

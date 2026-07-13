# Amiga audio — data lives in the executable

> ASM-traced + remake-verified. Supersedes `25-mm2-music-format.md`,
> `26-audio-callpaths-title-death-shared.md`, and `27-mm2-known-songs-catalog.md`
> (those mistook blits / save-parse for music). Controls menu toggles still in
> [`25-audio-sounds-music.md`](25-audio-sounds-music.md).

## Summary

MM2 Amiga sound is **Paula square-ish wavetable synthesis** via `audio.device`.
All tone tables and the **10 short song/SFX sequences** are embedded in the **DATA hunk**
(near A4). Waveforms are **generated at runtime** into an allocated 0x400-byte buffer.
`master.32` is a dead filename-table string — **ignore it** (PC used `master.4`/`master.16` for gfx).

**`0x6FB8` is `play_sound_seq` (audio).** Older RE notes that called it a “canned
on-screen sequence”, “world/3D refresh”, or “castle bytecode runner” were wrong —
same address, wrong semantics. See §Wrong docs below.

## Pipeline

1. `audio_device_init` @ `0x7070` — AllocMem `0x2A8`, open `Audio zero` + `audio.device`, 4 IO channels
2. `wave_synth_init` @ `0x766E` — AllocMem `0x400` → `A4-$EDA`; fill + octave downsample (`0x75E2`/`0x760A`)
3. `play_sound_seq` @ `0x6FB8` via thunk **`JSR -$7E42(A4)`** — arg = sound id `0..9`
4. `play_tone_env` @ `0x77AA` — note + duration → period/wave → BeginIO write

Gating inside `play_sound_seq`:

| Id | Gate |
|----|------|
| `0` | Walk Beep flag `A4-$79AF` bit0 |
| `1..9` | Sounds flag `A4-$79B0` bit0 |

## Embedded tables (DATA hunk, A4 base `DATA_LOAD+0x7FFE`)

| A4 | DATA off | Role |
|----|----------|------|
| `-$7326` | `0xCD8` | 12 Paula periods (C..B): 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226 |
| `-$734A` | `0xCB4` | Octave wave offsets into synth buffer |
| `-$7338` | `0xCC6` | Octave wave lengths (256..1) |
| `-$7252` | `0xDAC` | Duration/volume index table |
| `-$7232` | `0xDCC` | 10 pointers → sequences at `0xD22..` (**SFX**, not scene bytecode) |
| `-$72FC` | `0xD02` | Ptrs to `Audio one`..`eight` name strings in CODE |

## Sequence format

At DATA `0xD22+`: pairs `[note+0x10][dur_index]` terminated by `0xFF`.

| ID | WAV export | Notes (midi-ish) | Callers / use |
|----|------------|------------------|---------------|
| 0 | `00_walk_beep.wav` | `30/6` | Step/turn: `-$7E42(0)` — Walk Beep |
| 1 | `01_ui_chirp.wav` | `12/4` | UI / confirm chirp |
| 2 | `02_phrase_short.wav` | `44/3…` | Combat **"oh noes"** — `0x51C2` entry (`-$7EDE`) @ `0x51D8`→`-$7E42(2)` |
| 3 | `03_victory.wav` | `41/2…` | Combat victory (`0x125FC` / remake `CombatSession`) |
| 4 | `04_phrase_b.wav` | `39/2…` | Phrase B |
| 5 | `05_flourish.wav` | `46/3…` | Flourish |
| 6 | `06_combat_a.wav` | `44/2…` | Combat sting A |
| 7 | `07_menu.wav` | `49/10…` | Menu / controls (`0x141BE`) |
| 8 | `08_combat_b.wav` | `42/3…` | Combat sting B |
| 9 | `09_alert.wav` | `55/7, 56/5` | Hit / alert; also `OP_0D` index `0x09` (sets exit bit0 in port) |

**Event VM:** `OP_0D` @ `0x15EC4` → `-$7E42` → this table (ids 0..9). Not graphics.

## Startup (overlay)

Overlay CODE1 @ `0x25C74` / `0x25C78`:

```text
jsr -$7E3C(A4)   ; audio_device_init
jsr -$7E2A(A4)   ; wave_synth_init
```

## Title / “music”

**Title theme lives in the DATA hunk** and is played by the **overlay**, not `play_sound_seq`.

| Item | Value |
|------|-------|
| Player | Overlay `0x283FC`, loop `0x2841C` |
| Stream | `A4-$6285` → DATA **`0x1D79`** |
| Duration table | `A4-$61BC` → DATA **`0x1E42`** (same shape as SFX table) |
| Length | `cmpi #$C5` at loop top; last pair at `$C4/$C5` → **99 notes** (`… 3a 01`, then `ff ff`) |
| Output | `JSR -$7E24(A4)` = `play_tone_env` |

Format matches SFX: `[note+0x10][dur_index]…`. Export: `EXTRACTED/audio/title_theme.wav`.

The 10 `play_sound_seq` ids remain short jingles/SFX (victory, walk beep, etc.).

Prior docs mistook bitplane blits (`-$7B9C` → `0x22EAA`, “song” `0x12D`) for music.

## Timing

All note length comes from **`play_tone_env` @ `0x77AA`**:

```text
; each duration unit:
pea.l   #1
jsr     -$7B42(A4)   ; → 0x25470 → dos.library Delay (LVO -198)
subq.w  #1, duration
; volume envelope step, then loop while duration > 0
```

- `Delay(1)` = **one AmigaDOS tick = 1/50 s** (dos.library; not display rate).
- Duration word = lookup from title table `A4-$61BC` / SFX `A4-$7252`:
  `[64,32,16,8,4,2,1,0, 64,48,24,12,6,3,1,0]`
- Title stream: **99 notes / 1024 ticks ≈ 20.5 s** at Delay@50Hz (final pair `3a 01` = 32 ticks; most others index 3 → 8 ticks).
- No extra WaitTOF / VBlank wait on this path; title loop helper `0x281B2` is **input abort**, not tempo.
- Audio IO is `BeginIO` (`-$7B30`); it does not add the note length.

Exporter: `--tempo-scale 1` = Delay@50Hz. Default **`1.2` (`60/50`)** — face-value 50Hz sounded rushed, `2.0` a bit slow.

## Remake (`game/` → `mm2::audio`)

| Piece | Path |
|-------|------|
| API | `game/include/mm2/platform/Audio.h` |
| Desktop | `game/src/platform/sdl/AudioSDL.cpp` — SDL mixer, 22050 Hz mono S16 |
| Amiga / tests | `game/src/platform/AudioStub.cpp` — no-op (Paula TBD) |
| Assets | `EXTRACTED/audio/*.wav` (copied beside binary or found via `data_dir`) |

```text
init(data_dir)          load 00..09 + title_theme.wav
playSoundSeq(id, sounds, walk)   // mirrors 0x6FB8 gates
playTitleTheme(loop)    // overlay title stream
stopTitleTheme / stopAll
```

Call sites in the remake:

| Seq | C++ |
|-----|-----|
| 0 | `Movement.cpp` (step) |
| 2 | `CombatSession` encounter entry ("oh noes") |
| 3 | `CombatSession` victory |
| 6 / 8 / 9 | combat KO / compact / hit paths |
| `OP_0D` | `EventVmHelpers::eventVmExecEngineCall` → `playSoundSeq` |

## Extract / listen

```powershell
python tools\export_mm2_amiga_sfx.py
# raw ASM pitch:     python tools\export_mm2_amiga_sfx.py --octave-shift 0
# Delay@50Hz tempo:  python tools\export_mm2_amiga_sfx.py --tempo-scale 1
# half-speed:        python tools\export_mm2_amiga_sfx.py --tempo-scale 2
```

Writes to `EXTRACTED/audio/` (see that folder’s `README.md` for note dumps).
Also exports medleys: `medley_jingles.wav`, `medley_title_and_jingles.wav`, etc.

## Thunk map (A4 = DATA_LOAD + `$7FFE`)

| Thunk | Target |
|-------|--------|
| `-$7E42` | `play_sound_seq` `0x6FB8` |
| `-$7E3C` | `audio_device_init` `0x7070` |
| `-$7E30` | stop channels `0x7562` |
| `-$7E2A` | `wave_synth_init` `0x766E` |
| `-$7E24` | `play_tone_env` `0x77AA` |

## Wrong docs / symbols to retire

| Claim | Truth |
|-------|--------|
| `0x6FB8` = on-screen / 3D refresh / castle bytecode | **`play_sound_seq`** (audio) |
| `A4-$7232` = scene bytecode ptrs | **10 SFX sequence pointers** |
| `0x77AA` = frame/scene dispatch | **`play_tone_env`** |
| `0x823C` = `audio_init` | global save parse |
| `A4-$79B2` = Sounds | panel mode; Sounds is `-$79B0` |
| `-$7B9C` / `0x12D` = title song | graphics blit coords |
| `A4-$48C` = Paula | `graphics.library` |
| `master.32` music blob | dead string; ignore |
| Bootstrap `play_song_scripted` as music | often text+wait; not the title theme |

## See also

- [25-audio-sounds-music.md](25-audio-sounds-music.md) — Controls menu Sounds / Walk Beep
- [07-event-script-opcodes.md](07-event-script-opcodes.md) — `OP_0D`
- [43-exploration-input-and-ingame-options.md](43-exploration-input-and-ingame-options.md) — step calls `-$7E42(0)`
- [game/README.md](../../game/README.md) — remake audio phase

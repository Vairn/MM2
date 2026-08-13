#!/usr/bin/env python3
"""
Extract MM2 Amiga embedded SFX + title music from the DATA hunk and render WAVs.

Title music: overlay @ 0x283FC reads A4-$6285 (DATA 0x1D79), durations A4-$61BC
(DATA 0x1E42), plays via play_tone_env (-$7E24). See docs/58-amiga-audio-in-exe.md
"""
from __future__ import annotations

import argparse
import math
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATA = ROOT / "EXTRACTED/hunks/mm2_data_00.bin"
DEFAULT_OUT = ROOT / "EXTRACTED/audio"

OFF_PERIODS = 0xCD8
OFF_DUR_SFX = 0xDAC
OFF_SEQ = 0xD22
OFF_SEQ_END = 0xDAC

# Title theme (overlay player)
OFF_TITLE = 0x1D79  # A4-$6285
OFF_TITLE_DUR = 0x1E42  # A4-$61BC
# Overlay @ 0x2841C: while index < 0xC5, read note then dur (no re-check between).
# Last pair starts at 0xC4 → byte 0xC5 is dur (`3a 01`), then `ff ff` before dur table.
TITLE_INDEX_LIMIT = 0xC5

PAL_CLOCK = 3_546_895.0
# play_tone_env @ 0x77AA: each duration unit does dos.library Delay(1).
# DOS ticks are 50Hz on paper; listening default uses 60/50 so note lengths
# match an NTSC-VBlank feel (50Hz alone sounded rushed; 2× too draggy).
FRAME_HZ = 50.0
SAMPLE_RATE = 22050
WAVE_LEN = [256, 128, 64, 32, 16, 8, 4, 2, 1]

# ASM uses wavelength mipmaps per octave; exports were ~1 octave sharp vs real hardware
# feel — drop one octave for listening (period*len*2).
OCTAVE_SHIFT = 1
# 1.0 = Delay(1)@50Hz. Default 1.2 ≈ NTSC 60 vs PAL 50.
TEMPO_SCALE = 60.0 / 50.0

# Filenames keep historical stems; id 2 is combat encounter "oh noes"
# (ASM 0x51D8 → -$7E42), not a generic phrase.
NAMES = [
    "00_walk_beep",
    "01_ui_chirp",
    "02_phrase_short",  # combat "oh noes" (0x51C2 entry)
    "03_victory",
    "04_phrase_b",
    "05_flourish",
    "06_combat_a",
    "07_menu",
    "08_combat_b",
    "09_alert",
]

# Short roles written into EXTRACTED/audio/README.md (filename unchanged).
ROLES = [
    "walk beep",
    "UI chirp",
    'combat "oh noes" (encounter entry 0x51C2)',
    "combat victory",
    "phrase B",
    "flourish",
    "combat sting A / party KO",
    "menu",
    "combat sting B (monster compact)",
    "hit / alert",
]


def u16s(buf: bytes, off: int, n: int) -> list[int]:
    return [struct.unpack_from(">H", buf, off + i * 2)[0] for i in range(n)]


def parse_sequences(data: bytes) -> list[list[tuple[int, int]]]:
    seqs: list[list[tuple[int, int]]] = []
    i = OFF_SEQ
    while i < OFF_SEQ_END and len(seqs) < 10:
        notes: list[tuple[int, int]] = []
        while i + 1 < len(data) and data[i] != 0xFF:
            notes.append((data[i] - 0x10, data[i + 1]))
            i += 2
        if i < len(data) and data[i] == 0xFF:
            i += 1
        seqs.append(notes)
    return seqs


def parse_title(data: bytes) -> list[tuple[int, int]]:
    notes: list[tuple[int, int]] = []
    i = 0
    while i < TITLE_INDEX_LIMIT:
        note_b = data[OFF_TITLE + i]
        i += 1
        dur_b = data[OFF_TITLE + i]
        i += 1
        if note_b == 0xFF:
            break
        notes.append((note_b - 0x10, dur_b))
    return notes


def note_hz(note: int, periods: list[int]) -> float:
    if note < 0:
        return 0.0
    octave, semi = divmod(note, 12)
    if semi >= len(periods):
        return 0.0
    period = periods[semi]
    if period <= 0:
        return 0.0
    wlen = WAVE_LEN[min(octave, len(WAVE_LEN) - 1)]
    if wlen <= 0:
        return 0.0
    hz = PAL_CLOCK / (period * wlen)
    return hz / (2 ** OCTAVE_SHIFT)


def dur_frames(dur_index: int, dur_table: list[int], tempo_scale: float = 1.0) -> int:
    if 0 <= dur_index < len(dur_table):
        ticks = max(dur_table[dur_index], 1)
    else:
        ticks = 4
    return max(1, int(round(ticks * tempo_scale)))


def render_tone(hz: float, frames: int, vol_start: float = 0.35) -> list[float]:
    n = max(1, int(SAMPLE_RATE * frames / FRAME_HZ))
    out = [0.0] * n
    if hz <= 0:
        return out
    for i in range(n):
        t = i / SAMPLE_RATE
        env = vol_start * (1.0 - 0.85 * (i / n))
        phase = math.sin(2 * math.pi * hz * t)
        out[i] = math.tanh(phase * 4.0) * env
    return out


def render_sequence(
    notes: list[tuple[int, int]],
    periods: list[int],
    dur_table: list[int],
    tempo_scale: float = 1.0,
) -> list[float]:
    samples: list[float] = []
    for note, dur_i in notes:
        samples.extend(
            render_tone(
                note_hz(note, periods),
                dur_frames(dur_i, dur_table, tempo_scale),
            )
        )
    samples.extend([0.0] * int(SAMPLE_RATE * 0.2))
    return samples


def write_wav(path: Path, samples: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for s in samples:
            v = int(max(-1.0, min(1.0, s)) * 32767)
            frames += struct.pack("<h", v)
        w.writeframes(frames)


def medley(
    seqs: list[list[tuple[int, int]]],
    periods: list[int],
    dur_table: list[int],
    ids: list[int],
    tempo_scale: float = 1.0,
) -> list[float]:
    out: list[float] = []
    pause = [0.0] * int(SAMPLE_RATE * 0.35)
    for i in ids:
        out.extend(render_sequence(seqs[i], periods, dur_table, tempo_scale))
        out.extend(pause)
    return out


def main() -> None:
    global OCTAVE_SHIFT

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data", type=Path, default=DEFAULT_DATA)
    ap.add_argument("-o", "--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument(
        "--octave-shift",
        type=int,
        default=OCTAVE_SHIFT,
        help="Octaves to drop (default 1; use 0 for raw ASM pitch)",
    )
    ap.add_argument(
        "--tempo-scale",
        type=float,
        default=TEMPO_SCALE,
        help="Stretch note lengths (1.0 = ASM Delay(1)@50Hz; 2.0 = half speed)",
    )
    args = ap.parse_args()
    OCTAVE_SHIFT = args.octave_shift
    tempo = args.tempo_scale

    data = args.data.read_bytes()
    periods = u16s(data, OFF_PERIODS, 12)
    dur_sfx = u16s(data, OFF_DUR_SFX, 16)
    dur_title = u16s(data, OFF_TITLE_DUR, 16)
    seqs = parse_sequences(data)
    title = parse_title(data)

    title_ticks = sum(dur_frames(d, dur_title, 1.0) for _, d in title)
    print(f"octave_shift={OCTAVE_SHIFT} tempo_scale={tempo}")
    print("periods:", periods)
    print(
        f"title notes: {len(title)} @ DATA {OFF_TITLE:#x} "
        f"({title_ticks} Delay ticks, {title_ticks / FRAME_HZ:.2f}s @50Hz)"
    )

    meta: list[str] = [
        "# MM2 Amiga audio exports",
        "",
        f"octave_shift: {OCTAVE_SHIFT} (1 = listening pitch; 0 = raw ASM)",
        f"tempo_scale: {tempo} (1.0 = dos Delay(1) per duration tick @ 50Hz)",
        f"periods_C_to_B: {periods}",
        "",
        "## Timing (ASM)",
        "play_tone_env @ 0x77AA: loop while duration>0; each iter pea #1 / "
        "JSR -$7B42 = dos.library Delay(1).",
        "Title dur table A4-$61BC; SFX A4-$7252. Same shape: "
        "[64,32,16,8,4,2,1,0,64,48,24,12,6,3,1,0].",
        "",
        "## Title theme",
        f"DATA {OFF_TITLE:#x} (A4-$6285), dur table {OFF_TITLE_DUR:#x} (A4-$61BC)",
        f"overlay player 0x283FC / loop 0x2841C, {len(title)} notes, "
        f"{title_ticks} ticks ({title_ticks / FRAME_HZ:.2f}s)",
        f"notes: {title}",
        "",
    ]

    # Title (and a 2x loop version)
    title_samples = render_sequence(title, periods, dur_title, tempo)
    write_wav(args.out / "title_theme.wav", title_samples)
    write_wav(
        args.out / "title_theme_loop2.wav",
        title_samples[:- int(SAMPLE_RATE * 0.2)]
        + title_samples,
    )
    print(f"  title_theme.wav ({len(title)} notes)")

    for i, notes in enumerate(seqs):
        name = NAMES[i] if i < len(NAMES) else f"{i:02d}"
        role = ROLES[i] if i < len(ROLES) else ""
        meta.append(f"## id {i} — {name}" + (f" — {role}" if role else ""))
        meta.append(f"notes: {notes}")
        meta.append("")
        write_wav(
            args.out / f"{name}.wav",
            render_sequence(notes, periods, dur_sfx, tempo),
        )
        print(f"  {name}.wav")

    write_wav(
        args.out / "medley_jingles.wav",
        medley(seqs, periods, dur_sfx, [2, 3, 4, 5, 6, 7, 8], tempo),
    )
    write_wav(
        args.out / "medley_best.wav",
        medley(seqs, periods, dur_sfx, [5, 3, 4, 7], tempo),
    )
    # Title + best jingles
    combo = render_sequence(title, periods, dur_title, tempo)
    combo += [0.0] * int(SAMPLE_RATE * 0.5)
    combo += medley(seqs, periods, dur_sfx, [5, 3, 7], tempo)
    write_wav(args.out / "medley_title_and_jingles.wav", combo)

    (args.out / "README.md").write_text("\n".join(meta) + "\n", encoding="utf-8")
    print(f"Wrote -> {args.out}")


if __name__ == "__main__":
    main()

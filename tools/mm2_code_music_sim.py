#!/usr/bin/env python3
"""Simulate MM2 in-code note/tone sequences to WAV.

This does not require master.32. It models sequences that are explicit in code
(e.g. victory note loops, title chirp, death tone flow with user-provided stats).
"""

from __future__ import annotations

import argparse
import struct
import wave
from pathlib import Path
from typing import Iterable, List


PAL_CLOCK = 709379.0

# From EXTRACTED/tmp_mm2_audio_tables.txt defaults (when no master.32 is loaded).
DEFAULT_PITCH_B = [0x0000, 0x0064, 0x00C8, 0x012C, 0x0190, 0x01F4, 0x0258, 0x02BC, 0x0320, 0x0384]


def period_to_hz(period: int) -> float:
    if period <= 0:
        return 0.0
    return PAL_CLOCK / float(period)


def note_index_to_hz(note_index: int) -> float:
    """Approx note map for code-only simulation.

    We do not have the real runtime note_map/pitch_a from master.32 in this workspace.
    Use a stable semitone mapping so relative patterns are audible.
    """
    # A4 = 440, map index 0 to A2-ish.
    midi = 45 + max(0, note_index)
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))


def tone_word_to_hz(tone_word: int) -> float:
    """Map play_tone period-ish word to frequency."""
    if tone_word <= 0:
        return 0.0
    # Use Paula period mapping first; clamp to sane audible band.
    hz = period_to_hz(tone_word)
    if hz < 20.0:
        hz *= 8.0
    while hz > 4000.0:
        hz *= 0.5
    return hz


def render_square(hz: float, duration_s: float, sample_rate: int, volume: float) -> List[float]:
    n = int(duration_s * sample_rate)
    if n <= 0:
        return []
    if hz <= 0:
        return [0.0] * n
    out: List[float] = []
    phase = 0.0
    inc = hz / sample_rate
    for _ in range(n):
        out.append(volume if phase < 0.5 else -volume)
        phase += inc
        if phase >= 1.0:
            phase -= 1.0
    return out


def concat(parts: Iterable[List[float]]) -> List[float]:
    out: List[float] = []
    for p in parts:
        out.extend(p)
    return out


def write_wav(path: Path, samples: List[float], sample_rate: int) -> None:
    pcm = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s))
        pcm.extend(struct.pack("<h", int(v * 32767.0)))
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        w.writeframes(pcm)


def sim_victory(note_ms: int, gap_ms: int, sample_rate: int) -> List[float]:
    # asm @ 0x12430: note index 5 repeated 0x17 times, twice.
    hz = note_index_to_hz(5)
    on = note_ms / 1000.0
    off = gap_ms / 1000.0
    one_note = render_square(hz, on, sample_rate, 0.25)
    one_gap = render_square(0.0, off, sample_rate, 0.0)
    first_parts: List[List[float]] = []
    second_parts: List[List[float]] = []
    for _ in range(0x17):
        first_parts.extend([one_note, one_gap])
    for _ in range(0x17):
        second_parts.extend([one_note, one_gap])
    first = concat(first_parts)
    second = concat(second_parts)
    return concat([first, render_square(0.0, 0.18, sample_rate, 0.0), second])


def sim_title_chirp(sample_rate: int) -> List[float]:
    # visible callsite: title UI chirp note index 0x11.
    hz = note_index_to_hz(0x11)
    return concat(
        [
            render_square(hz, 0.10, sample_rate, 0.25),
            render_square(0.0, 0.05, sample_rate, 0.0),
            render_square(hz * 0.9, 0.08, sample_rate, 0.25),
        ]
    )


def sim_death(gold: int, age: int, level: int, sample_rate: int) -> List[float]:
    # asm @ 0x7DCC path chooses 0x31/0x32/0x33 and feeds play_tone with fields
    # ($66, $5C, $25). We synthesize one pass of those three tones.
    seq = []
    for tone_word in (max(1, gold), max(1, age), max(1, level)):
        hz = tone_word_to_hz(tone_word)
        seq.append(render_square(hz, 0.20, sample_rate, 0.22))
        seq.append(render_square(0.0, 0.06, sample_rate, 0.0))
    return concat(seq)


def main() -> None:
    ap = argparse.ArgumentParser(description="MM2 code-only music/sfx simulator")
    ap.add_argument("--event", choices=["victory", "death", "title"], required=True, help="sequence to simulate")
    ap.add_argument("--wav", type=Path, required=True, help="output wav path")
    ap.add_argument("--sample-rate", type=int, default=22050)
    ap.add_argument("--note-ms", type=int, default=55, help="victory note duration ms")
    ap.add_argument("--gap-ms", type=int, default=18, help="victory inter-note gap ms")
    ap.add_argument("--gold", type=int, default=320, help="death sim: tone word for gold")
    ap.add_argument("--age", type=int, default=120, help="death sim: tone word for age")
    ap.add_argument("--level", type=int, default=24, help="death sim: tone word for level")
    args = ap.parse_args()

    if args.event == "victory":
        samples = sim_victory(args.note_ms, args.gap_ms, args.sample_rate)
        note = "source: asm 0x12430 repeated play_note index 5"
    elif args.event == "title":
        samples = sim_title_chirp(args.sample_rate)
        note = "source: asm title callsite play_note index 0x11 (chirp)"
    else:
        samples = sim_death(args.gold, args.age, args.level, args.sample_rate)
        note = "source: asm 0x7DCC death handler play_tone inputs (gold/age/level)"

    args.wav.parent.mkdir(parents=True, exist_ok=True)
    write_wav(args.wav, samples, args.sample_rate)
    print(f"wrote {args.wav} samples={len(samples)}")
    print(note)
    print("note: full title melody depends on runtime song data (master stream not present here)")


if __name__ == "__main__":
    main()

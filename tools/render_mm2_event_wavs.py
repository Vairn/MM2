#!/usr/bin/env python3
"""Render simple audible WAVs from extracted MM2 event audio call paths."""

from __future__ import annotations

import json
import math
import struct
import wave
from pathlib import Path
from typing import Iterable, List, Tuple


ROOT = Path(__file__).resolve().parents[1]
EVENT_DIR = ROOT / "EXTRACTED" / "audio-events"
TABLES_JSON = ROOT / "EXTRACTED" / "tmp_mm2_audio_tables.json"
OUT_DIR = ROOT / "EXTRACTED" / "audio-events"
SAMPLE_RATE = 22050


def load_tables() -> dict:
    return json.loads(TABLES_JSON.read_text(encoding="utf-8"))


def freq_from_note_index(note_idx: int) -> float:
    # Keep this deterministic: map MM2 note-ish indices onto a musical range.
    midi = 36 + note_idx
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))


def sine_segment(freq: float, sec: float, amp: float = 0.32) -> List[float]:
    n = max(1, int(SAMPLE_RATE * sec))
    out = []
    # Short attack/release to avoid clicks.
    attack = max(1, int(0.01 * n))
    release = max(1, int(0.04 * n))
    for i in range(n):
        env = 1.0
        if i < attack:
            env = i / attack
        elif i > n - release:
            env = max(0.0, (n - i) / release)
        s = amp * env * math.sin((2.0 * math.pi * freq * i) / SAMPLE_RATE)
        out.append(s)
    return out


def noise_segment(sec: float, amp: float = 0.12) -> List[float]:
    n = max(1, int(SAMPLE_RATE * sec))
    out = []
    seed = 0x1234ABCD
    for _ in range(n):
        # Tiny deterministic LCG.
        seed = (1103515245 * seed + 12345) & 0x7FFFFFFF
        val = ((seed / 0x7FFFFFFF) * 2.0 - 1.0) * amp
        out.append(val)
    return out


def silence(sec: float) -> List[float]:
    return [0.0] * max(1, int(SAMPLE_RATE * sec))


def clamp16(x: float) -> int:
    y = max(-1.0, min(1.0, x))
    return int(y * 32767)


def write_wav(path: Path, samples: Iterable[float]) -> None:
    data = [clamp16(s) for s in samples]
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(struct.pack("<" + ("h" * len(data)), *data))


def arg_int(args: List[object], idx: int, default: int = 0) -> int:
    if idx >= len(args):
        return default
    v = args[idx]
    if isinstance(v, int):
        return v
    return default


def render_title(calls: List[dict], note_lut: List[int]) -> List[float]:
    out: List[float] = []
    for c in calls:
        slot = c.get("slot")
        args = c.get("literal_stack_args", [])
        if slot == "-$7BA8(A4)":
            repeats = c.get("runtime_loop_repeats_estimate") or 1
            tempo = arg_int(args, 2, 232)
            step_dur = max(0.055, min(0.22, tempo / 1400.0))
            for i in range(int(repeats)):
                note_idx = note_lut[i % len(note_lut)]
                f = freq_from_note_index(note_idx)
                out.extend(sine_segment(f, step_dur, amp=0.28))
        elif slot == "-$7C62(A4)":
            note_idx = arg_int(args, 0, 17)
            out.extend(sine_segment(freq_from_note_index(note_idx), 0.11, amp=0.32))
        elif slot == "-$7B9C(A4)":
            # Song bootstrap marker: brief two-tone cue from first args.
            base = arg_int(args, 0, 48) & 0x3F
            out.extend(sine_segment(freq_from_note_index(base), 0.12, amp=0.24))
            out.extend(sine_segment(freq_from_note_index(base + 7), 0.12, amp=0.22))
        elif slot == "-$7BFC(A4)":
            n = arg_int(args, 0, 20)
            out.extend(sine_segment(freq_from_note_index(n), 0.09, amp=0.2))
        elif slot == "-$7C6E(A4)":
            out.extend(noise_segment(0.16, amp=0.09))
        else:
            # Keep timing shape for non-note calls.
            out.extend(silence(0.02))
    return out


def render_generic(calls: List[dict], note_lut: List[int]) -> List[float]:
    out: List[float] = []
    step_counter = 0
    for c in calls:
        slot = c.get("slot")
        args = c.get("literal_stack_args", [])
        repeats = int(c.get("runtime_loop_repeats_estimate") or 1)
        repeats = max(1, repeats)
        for _ in range(repeats):
            if slot == "-$7C62(A4)":
                n = arg_int(args, 0, note_lut[step_counter % len(note_lut)])
                out.extend(sine_segment(freq_from_note_index(n), 0.08, amp=0.3))
            elif slot in ("-$7B9C(A4)", "-$7BA8(A4)"):
                n = note_lut[step_counter % len(note_lut)]
                out.extend(sine_segment(freq_from_note_index(n), 0.075, amp=0.23))
            elif slot == "-$7BFC(A4)":
                n = arg_int(args, 0, 12)
                out.extend(sine_segment(freq_from_note_index(n), 0.065, amp=0.2))
            elif slot == "-$7BE4(A4)":
                out.extend(noise_segment(0.045, amp=0.08))
            else:
                out.extend(silence(0.018))
            step_counter += 1
    return out


def render_event(event_name: str, note_lut: List[int]) -> Tuple[Path, int]:
    src = EVENT_DIR / f"{event_name}.json"
    payload = json.loads(src.read_text(encoding="utf-8"))
    calls: List[dict] = payload.get("calls", [])
    if event_name == "title":
        samples = render_title(calls, note_lut)
    else:
        samples = render_generic(calls, note_lut)

    out_wav = OUT_DIR / f"{event_name}.wav"
    write_wav(out_wav, samples)
    return out_wav, len(samples)


def main() -> None:
    t = load_tables()
    note_lut = t["tables"]["note_lut_762e_u8_64"]
    events = ["title", "combat_enter", "combat_victory", "party_death_or_defeat"]
    for ev in events:
        path, sample_count = render_event(ev, note_lut)
        secs = sample_count / SAMPLE_RATE
        print(f"Wrote {path} ({secs:.2f}s)")


if __name__ == "__main__":
    main()


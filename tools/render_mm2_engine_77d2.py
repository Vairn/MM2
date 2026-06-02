#!/usr/bin/env python3
"""Render MM2 audio candidate from asm routines 0x77D2 -> 0x76D4 tables."""

from __future__ import annotations

import argparse
import math
import struct
import wave
from pathlib import Path
from typing import List


ROOT = Path(__file__).resolve().parents[1]
DATA_HUNK = ROOT / "EXTRACTED" / "hunks" / "mm2_data_00.bin"
OUT_DEFAULT = ROOT / "EXTRACTED" / "audio-events" / "title_engine_77d2.wav"
SAMPLE_RATE = 22050
PAL_CLOCK = 709379.0
A4 = 0x7FFE


def a4_off(neg_disp: int) -> int:
    return A4 - neg_disp


def be_u16(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 2], "big")


def load_tables(buf: bytes) -> tuple[List[int], List[int], List[int]]:
    # 0x76D4 references:
    # -$7326: 12-entry note period-like table
    # -$734A: octave/offset table
    # -$7338: envelope/shape table (used as timbre weight here)
    t_7326 = [be_u16(buf, a4_off(0x7326) + i * 2) for i in range(12)]
    t_734a = [be_u16(buf, a4_off(0x734A) + i * 2) for i in range(16)]
    t_7338 = [be_u16(buf, a4_off(0x7338) + i * 2) for i in range(16)]
    return t_7326, t_734a, t_7338


def period_to_hz(period: int) -> float:
    if period <= 0:
        return 0.0
    hz = PAL_CLOCK / float(period)
    while hz > 5000.0:
        hz *= 0.5
    while 0 < hz < 30.0:
        hz *= 2.0
    return hz


def render_square(hz: float, sec: float, amp: float) -> List[float]:
    n = max(1, int(sec * SAMPLE_RATE))
    if hz <= 0.0:
        return [0.0] * n
    out: List[float] = []
    phase = 0.0
    inc = hz / SAMPLE_RATE
    atk = max(1, int(n * 0.02))
    rel = max(1, int(n * 0.06))
    for i in range(n):
        env = 1.0
        if i < atk:
            env = i / atk
        elif i > n - rel:
            env = max(0.0, (n - i) / rel)
        s = (amp if phase < 0.5 else -amp) * env
        out.append(s)
        phase += inc
        if phase >= 1.0:
            phase -= 1.0
    return out


def clamp16(x: float) -> int:
    return int(max(-1.0, min(1.0, x)) * 32767)


def write_wav(path: Path, samples: List[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    pcm = struct.pack("<" + ("h" * len(samples)), *[clamp16(s) for s in samples])
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(pcm)


def tone_from_76d4(x: int, _chan: int, vol: int, t7326: List[int], t734a: List[int], t7338: List[int]) -> tuple[float, float]:
    # Approximate 0x76D4 math path for x < 0x64.
    if x < 0:
        x = 0
    if x >= 0x64:
        x = 0x63
    q = x // 12
    r = x % 12
    period = t7326[r]
    # t734a/t7338 affect sample offset and shape; use them to modulate timbre.
    shape = t7338[min(q, len(t7338) - 1)]
    off = t734a[min(q, len(t734a) - 1)]
    hz = period_to_hz(max(1, period))
    # subtle detune from offset table to keep octave behavior audible
    hz *= 1.0 + (off / 8192.0)
    amp = max(0.05, min(0.35, (vol / 32.0) * (0.6 + (shape & 0xFF) / 255.0 * 0.5)))
    return hz, amp


def render_77d2(seed_note: int, ticks: int, t7326: List[int], t734a: List[int], t7338: List[int]) -> List[float]:
    # 0x77D2 local init:
    v = 0x20
    dv = 6
    n = seed_note - 0x0C
    out: List[float] = []
    step_sec = 0.08
    for _ in range(max(1, ticks)):
        if n >= 0:
            hz, amp = tone_from_76d4(n, 0, v, t7326, t734a, t7338)
            out.extend(render_square(hz, step_sec, amp))
            hz, amp = tone_from_76d4(n, 1, v, t7326, t734a, t7338)
            out.extend(render_square(hz * 1.01, step_sec, amp * 0.9))
        hz, amp = tone_from_76d4(seed_note, 2, v, t7326, t734a, t7338)
        out.extend(render_square(hz * 0.5, step_sec, amp * 0.8))
        hz, amp = tone_from_76d4(seed_note, 3, v, t7326, t734a, t7338)
        out.extend(render_square(hz * 0.25, step_sec, amp * 0.7))

        v -= dv
        if dv != 0:
            dv = max(0, dv - 2)
        else:
            if v > 0x0A:
                v -= 1
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description="Render asm 0x77D2 engine candidate")
    ap.add_argument("--note", type=int, default=0x47, help="seed note arg (title path uses 0x47)")
    ap.add_argument("--ticks", type=int, default=18, help="loop ticks")
    ap.add_argument("--out", type=Path, default=OUT_DEFAULT, help="output wav")
    args = ap.parse_args()

    data = DATA_HUNK.read_bytes()
    t7326, t734a, t7338 = load_tables(data)
    samples = render_77d2(args.note, args.ticks, t7326, t734a, t7338)
    write_wav(args.out, samples)
    print(f"Wrote {args.out} ({len(samples)/SAMPLE_RATE:.2f}s)")
    print(f"source: asm 0x77D2/0x76D4 with tables -$7326/-$734A/-$7338 note=0x{args.note:02X} ticks={args.ticks}")


if __name__ == "__main__":
    main()


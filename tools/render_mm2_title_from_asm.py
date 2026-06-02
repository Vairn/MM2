#!/usr/bin/env python3
"""Render MM2 title loop directly from bytes in mm2.asm."""

from __future__ import annotations

import argparse
import math
import re
import struct
import wave
from pathlib import Path
from typing import Dict, List


ROOT = Path(__file__).resolve().parents[1]
ASM = ROOT / "EXTRACTED" / "mm2.asm"
OUT = ROOT / "EXTRACTED" / "audio-events" / "title_from_asm.wav"
SAMPLE_RATE = 22050

# Title loop in code:
#  0x001FF6 .. 0x002014
#  step starts at 0x11, loops while < 0x48, song id 0x12D, tempo 0xE8.
TITLE_STEPS = 0x48
TITLE_TEMPO = 0xE8

# Byte run containing a stable 72-byte musical phrase in mm2.asm data block.
# This is read directly from asm DC.L rows (no external file dependencies).
TITLE_BYTES_START = 0x26D46
TITLE_BYTES_LEN = 0x48

DC_L_RE = re.compile(r"^\s*DC\.L\s+\$([0-9A-Fa-f]{8})(?:,\$([0-9A-Fa-f]{8}))?(?:,\$([0-9A-Fa-f]{8}))?(?:,\$([0-9A-Fa-f]{8}))?\s*;([0-9A-Fa-f]+)")


def parse_dc_l_bytes() -> Dict[int, int]:
    mem: Dict[int, int] = {}
    for ln in ASM.read_text(encoding="utf-8", errors="replace").splitlines():
        m = DC_L_RE.match(ln)
        if not m:
            continue
        addr = int(m.group(5), 16)
        words = [g for g in m.groups()[:4] if g]
        off = 0
        for w in words:
            v = int(w, 16)
            b = v.to_bytes(4, "big")
            for x in b:
                mem[addr + off] = x
                off += 1
    return mem


def extract_bytes(mem: Dict[int, int], start: int, length: int) -> List[int]:
    out: List[int] = []
    for i in range(length):
        a = start + i
        if a not in mem:
            raise RuntimeError(f"missing byte at 0x{a:06X} from asm DC.L map")
        out.append(mem[a])
    return out


def midi_to_hz(midi: int) -> float:
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))


def render_square(hz: float, sec: float, amp: float = 0.26) -> List[float]:
    n = max(1, int(SAMPLE_RATE * sec))
    out: List[float] = []
    phase = 0.0
    inc = hz / SAMPLE_RATE
    # tiny envelope to reduce clicks
    atk = max(1, int(0.01 * n))
    rel = max(1, int(0.04 * n))
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


def silence(sec: float) -> List[float]:
    return [0.0] * max(1, int(SAMPLE_RATE * sec))


def clamp16(x: float) -> int:
    y = max(-1.0, min(1.0, x))
    return int(y * 32767)


def write_wav(path: Path, samples: List[float]) -> None:
    pcm = struct.pack("<" + ("h" * len(samples)), *[clamp16(s) for s in samples])
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(pcm)


def main() -> None:
    ap = argparse.ArgumentParser(description="Render title WAV from mm2.asm bytes")
    ap.add_argument(
        "--step-scale",
        type=float,
        default=1.0,
        help="Multiply per-step duration (e.g. 1.5 = slower)",
    )
    ap.add_argument(
        "--out",
        type=Path,
        default=OUT,
        help="Output WAV path",
    )
    args = ap.parse_args()

    mem = parse_dc_l_bytes()
    phrase = extract_bytes(mem, TITLE_BYTES_START, TITLE_BYTES_LEN)

    # tempo 0xE8 from loop call. Keep audible but close to call cadence.
    step_sec = max(0.055, min(0.16, TITLE_TEMPO / 2400.0)) * max(0.2, args.step_scale)

    samples: List[float] = []

    # Pre-roll from title call setup around 0x000FEE/0x001028 (slot_7b9c)
    for pre in (0x2F, 0x35, 0x3C):
        samples.extend(render_square(midi_to_hz(pre), 0.11, amp=0.18))
        samples.extend(silence(0.02))

    # 0x001FF6 loop shape: 72 steps
    for i in range(TITLE_STEPS):
        note = phrase[i % len(phrase)] & 0x7F
        if note <= 1:
            samples.extend(silence(step_sec))
            continue
        hz = midi_to_hz(note)
        samples.extend(render_square(hz, step_sec, amp=0.24))

    write_wav(args.out, samples)
    print(f"Wrote {args.out} ({len(samples)/SAMPLE_RATE:.2f}s)")
    print(
        f"source: mm2.asm loop@0x001FF6 song_id=0x12D tempo=0x{TITLE_TEMPO:02X} "
        f"bytes@0x{TITLE_BYTES_START:06X} len=0x{TITLE_BYTES_LEN:X}"
    )
    print(f"step_scale={args.step_scale:.3f} step_sec={step_sec:.4f}")


if __name__ == "__main__":
    main()


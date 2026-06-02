#!/usr/bin/env python3
"""
Decode and simulate MM2 master.32 music blob (0x1860 bytes, BE u16 tables).

ASM: audio_init @ 0x823C, song bank @ blob+0x780 -> runtime A4-$4F4C.
See EXTRACTED/docs/25-mm2-music-format.md
"""

from __future__ import annotations

import argparse
import struct
import wave
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, List, Optional, Tuple

MASTER_SIZE = 0x1860
SONG_COUNT = 60
STEPS_PER_SONG = 16
SONG_BANK_OFF = 0x780
SONG_STRIDE = 32

# Amiga PAL clock / tone period (approx) for square-wave export
PAL_CLOCK = 709379.0


@dataclass
class MasterMusic:
    pitch_a: List[int] = field(default_factory=lambda: [0] * 10)
    pitch_b: List[int] = field(default_factory=lambda: [0] * 10)
    note_map: List[int] = field(default_factory=lambda: [0] * 8)
    voice_cap: int = 0
    field_79b6: int = 0
    field_79b4: int = 0
    transpose: List[int] = field(default_factory=lambda: [0] * 3)
    songs: List[List[int]] = field(
        default_factory=lambda: [[0] * STEPS_PER_SONG for _ in range(SONG_COUNT)]
    )
    table_7995: bytes = b"\x00" * 10
    table_798b: bytes = b"\x00" * 24
    table_79a4: bytes = b"\x00" * 4
    sounds_flag: int = 0
    walk_flags: bytes = b"\x00" * 14
    raw: bytes = b""


def u16be(buf: bytes, off: int) -> int:
    return struct.unpack_from(">H", buf, off)[0]


def decode_master(blob: bytes) -> MasterMusic:
    if len(blob) < MASTER_SIZE:
        raise ValueError(f"expected {MASTER_SIZE} bytes, got {len(blob)}")
    m = MasterMusic(raw=blob[:MASTER_SIZE])
    off = 0
    for i in range(10):
        m.pitch_a[i] = u16be(blob, off)
        off += 2
    for i in range(10):
        m.pitch_b[i] = u16be(blob, off)
        off += 2
    for i in range(8):
        m.note_map[i] = u16be(blob, off)
        off += 2
    m.voice_cap = u16be(blob, off)
    off += 2
    m.field_79b6 = u16be(blob, off)
    off += 2
    m.field_79b4 = u16be(blob, off)
    off += 2
    for i in range(3):
        m.transpose[i] = u16be(blob, off)
        off += 2
    base = SONG_BANK_OFF
    for s in range(SONG_COUNT):
        for st in range(STEPS_PER_SONG):
            m.songs[s][st] = u16be(blob, base + s * SONG_STRIDE + st * 2)
    m.table_7995 = blob[off : off + 10]
    off += 10
    m.table_798b = blob[off : off + 24]
    off += 24
    m.table_79a4 = blob[off : off + 4]
    off += 4
    m.sounds_flag = blob[off]
    off += 1
    m.walk_flags = blob[off : off + 15]
    return m


def period_to_hz(period: int) -> float:
    if period <= 0:
        return 0.0
    return PAL_CLOCK / float(period)


def note_index_period(m: MasterMusic, note_index: int) -> int:
    if note_index < 0 or note_index >= len(m.note_map):
        return 0
    idx = m.note_map[note_index]
    if idx >= len(m.pitch_a):
        return 0
    return m.pitch_a[idx]


def describe_song(m: MasterMusic, song_id: int) -> str:
    if song_id < 0 or song_id >= SONG_COUNT:
        return f"song {song_id}: out of 0..{SONG_COUNT - 1} bank range"
    lines = [f"Song {song_id} (bank @ 0x780, stride 0x20):"]
    for i, w in enumerate(m.songs[song_id]):
        lines.append(f"  step {i:2d}: 0x{w:04X} ({w})")
    return "\n".join(lines)


def render_square(
    hz: float,
    duration_s: float,
    sample_rate: int = 22050,
    volume: float = 0.25,
) -> List[float]:
    if hz <= 0 or duration_s <= 0:
        return [0.0] * int(duration_s * sample_rate)
    n = int(duration_s * sample_rate)
    out: List[float] = []
    phase = 0.0
    inc = hz / sample_rate
    for _ in range(n):
        out.append(volume if phase < 0.5 else -volume)
        phase += inc
        if phase >= 1.0:
            phase -= 1.0
    return out


def mix_tracks(tracks: Iterable[List[float]]) -> List[float]:
    lengths = [len(t) for t in tracks]
    if not lengths:
        return []
    n = max(lengths)
    mix = [0.0] * n
    for t in tracks:
        for i, v in enumerate(t):
            mix[i] += v
    peak = max((abs(x) for x in mix), default=1.0)
    if peak > 1.0:
        mix = [x / peak for x in mix]
    return mix


def song_to_audio(
    m: MasterMusic,
    song_id: int,
    step_ms: int = 120,
    sample_rate: int = 22050,
) -> List[float]:
    """Naive square-wave render: each step word's low byte >> 2 indexes note_map."""
    tracks: List[List[float]] = []
    dur = step_ms / 1000.0
    for w in m.songs[song_id]:
        if w == 0:
            tracks.append(render_square(0, dur, sample_rate))
            continue
        note = (w >> 2) & 0xFF
        if note >= len(m.note_map):
            tracks.append(render_square(0, dur, sample_rate))
            continue
        period = note_index_period(m, note)
        hz = period_to_hz(period)
        tracks.append(render_square(hz, dur, sample_rate))
    return mix_tracks(tracks)


def write_wav(path: Path, samples: List[float], sample_rate: int = 22050) -> None:
    pcm = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s))
        pcm.extend(struct.pack("<h", int(v * 32767)))
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        w.writeframes(pcm)


def find_blob_candidates(data: bytes) -> List[Tuple[int, str]]:
    hits: List[Tuple[int, str]] = []
    for off in range(0, len(data) - MASTER_SIZE, 16):
        chunk = data[off : off + MASTER_SIZE]
        try:
            m = decode_master(chunk)
        except ValueError:
            continue
        nz = sum(1 for s in m.songs for w in s if w != 0)
        pa = [p for p in m.pitch_a if p]
        pa_ok = len(pa) >= 4 and all(40 < p < 5000 for p in pa)
        song_words = [w for s in m.songs for w in s if w]
        small = sum(1 for w in song_words if w < 0x0200)
        if nz >= 120 and pa_ok and small >= nz * 0.6:
            hits.append((off, f"songs_nz={nz} pitch_a={m.pitch_a[:4]}"))
    return hits[:20]


def main() -> None:
    ap = argparse.ArgumentParser(description="MM2 master.32 music decode / simulate")
    ap.add_argument(
        "--master",
        type=Path,
        help="master blob file (exactly 0x1860 bytes)",
    )
    ap.add_argument(
        "--scan",
        type=Path,
        help="scan EXTRACTED/mm2 or code hunk for embedded blob",
    )
    ap.add_argument("--song", type=int, default=0, help="song index 0..59")
    ap.add_argument("--dump", action="store_true", help="print tables + song")
    ap.add_argument("--wav", type=Path, help="export naive square-wave WAV")
    ap.add_argument("--title-id", type=int, default=0x12D, help="report only (script id)")
    args = ap.parse_args()

    blob: Optional[bytes] = None
    if args.master:
        blob = args.master.read_bytes()
        if len(blob) != MASTER_SIZE:
            print(f"warning: {args.master} size {len(blob)}, expected {MASTER_SIZE}")
    elif args.scan:
        data = args.scan.read_bytes()
        hits = find_blob_candidates(data)
        if not hits:
            print(f"no candidate {MASTER_SIZE}-byte master blob in {args.scan}")
            return
        off, why = hits[0]
        print(f"using offset 0x{off:X} ({why})")
        blob = data[off : off + MASTER_SIZE]
    else:
        ap.error("provide --master or --scan")

    m = decode_master(blob)
    print(f"Title script id (not bank index): 0x{args.title_id:03X}")
    print(f"pitch_a: {m.pitch_a}")
    print(f"pitch_b: {m.pitch_b}")
    print(f"note_map: {m.note_map}")
    print(f"voice_cap: {m.voice_cap} sounds_flag: {m.sounds_flag}")

    if args.dump:
        print(describe_song(m, args.song))

    if args.wav:
        samples = song_to_audio(m, args.song)
        write_wav(args.wav, samples)
        print(f"wrote {args.wav} ({len(samples)} samples)")


if __name__ == "__main__":
    main()

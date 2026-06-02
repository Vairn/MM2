#!/usr/bin/env python3
"""Build a first working MM2 MOD prototype from intermediate audio events.

This emits a minimal 4-channel ProTracker-compatible MOD with:
- deterministic sample headers + sample payloads
- deterministic pattern order/data
- pattern 0: proven title/death event mapping
- pattern 1: likely/approximate event mapping (poll/type31/walk beep)
"""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "EXTRACTED" / "mm2_mod_audio_intermediate.json"
DEFAULT_OUTPUT = ROOT / "EXTRACTED" / "mm2_prototype.mod"

# ProTracker periods (finetune 0), one octave block.
BASE_PERIODS = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453]
NOTE_NAMES = ["C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"]


@dataclass(frozen=True)
class SampleDef:
    index: int
    name: str
    volume: int
    data: bytes
    loop_start_words: int
    loop_len_words: int


@dataclass(frozen=True)
class Cell:
    sample: int = 0
    period: int = 0
    effect: int = 0
    effect_param: int = 0


def clamp_u8(v: int) -> int:
    return max(0, min(255, v))


def pack_text(text: str, n: int) -> bytes:
    raw = text.encode("ascii", errors="replace")[:n]
    return raw + (b"\x00" * (n - len(raw)))


def ensure_even_bytes(data: bytes) -> bytes:
    if len(data) % 2:
        return data + b"\x00"
    return data


def signed_byte(v: float) -> int:
    return int(max(-128, min(127, round(v))))


def bytes_from_i8(values: Iterable[int]) -> bytes:
    out = bytearray()
    for v in values:
        out.append(v & 0xFF)
    return bytes(out)


def make_wave_sample(kind: str, length: int, amp: int) -> bytes:
    """Return signed 8-bit sample payload for a tiny looping instrument."""
    vals: List[int] = []
    for i in range(length):
        t = i / float(length)
        if kind == "square":
            x = amp if i < (length // 2) else -amp
        elif kind == "triangle":
            x = (4.0 * amp * abs(t - 0.5)) - amp
        elif kind == "saw":
            x = (2.0 * amp * t) - amp
        elif kind == "sine":
            x = amp * math.sin(2.0 * math.pi * t)
        else:
            raise ValueError(f"unknown wave kind: {kind}")
        vals.append(signed_byte(x))
    return ensure_even_bytes(bytes_from_i8(vals))


def make_sample_bank() -> List[SampleDef]:
    # Starter bank requested by task: walk-beep + tone classes.
    return [
        SampleDef(
            index=1,
            name="walk_beep",
            volume=44,
            data=make_wave_sample("square", length=32, amp=52),
            loop_start_words=0,
            loop_len_words=16,
        ),
        SampleDef(
            index=2,
            name="title_pulse",
            volume=40,
            data=make_wave_sample("triangle", length=64, amp=46),
            loop_start_words=0,
            loop_len_words=32,
        ),
        SampleDef(
            index=3,
            name="death_t31",
            volume=46,
            data=make_wave_sample("sine", length=64, amp=50),
            loop_start_words=0,
            loop_len_words=32,
        ),
        SampleDef(
            index=4,
            name="death_t32",
            volume=48,
            data=make_wave_sample("saw", length=64, amp=46),
            loop_start_words=0,
            loop_len_words=32,
        ),
        SampleDef(
            index=5,
            name="death_t33",
            volume=48,
            data=make_wave_sample("triangle", length=64, amp=58),
            loop_start_words=0,
            loop_len_words=32,
        ),
    ]


def parse_events(path: Path) -> Dict[str, dict]:
    blob = json.loads(path.read_text(encoding="utf-8"))
    return {e["event_id"]: e for e in blob.get("events", [])}


def period_for_oct_note(octave: int, semitone: int) -> int:
    # Start from C-1 period table entry and scale by octaves.
    base = BASE_PERIODS[semitone % 12]
    shift = octave - 1
    if shift > 0:
        return max(113, int(round(base / (2**shift))))
    return min(1712, int(round(base * (2 ** (-shift)))))


def period_from_note_index(note_index: int) -> int:
    """Approximate mapping for MM2 short-note immediates -> MOD pitch.

    This is intentionally simple for the prototype:
    - wrap index over 12 semitones
    - map to octave 2..4 from index bands
    """
    semitone = note_index % 12
    octave = 2 + (note_index // 24)
    octave = max(2, min(4, octave))
    return period_for_oct_note(octave, semitone)


def empty_pattern(rows: int = 64) -> List[List[Cell]]:
    return [[Cell() for _ in range(4)] for _ in range(rows)]


def put_note(pattern: List[List[Cell]], row: int, ch: int, sample: int, period: int) -> None:
    if 0 <= row < len(pattern) and 0 <= ch < 4:
        pattern[row][ch] = Cell(sample=sample, period=period, effect=0, effect_param=0)


def put_effect(pattern: List[List[Cell]], row: int, ch: int, effect: int, param: int) -> None:
    if 0 <= row < len(pattern) and 0 <= ch < 4:
        cur = pattern[row][ch]
        pattern[row][ch] = Cell(
            sample=cur.sample,
            period=cur.period,
            effect=effect & 0x0F,
            effect_param=param & 0xFF,
        )


def build_patterns(events: Dict[str, dict]) -> Tuple[List[List[List[Cell]]], List[int], Dict[str, object]]:
    p0 = empty_pattern()  # proven-first block
    p1 = empty_pattern()  # likely/approximate block

    # Pattern 0: proven title/death anchors.
    if "title.song.start_blocking" in events:
        put_note(p0, row=0, ch=0, sample=2, period=period_for_oct_note(3, 0))   # C-3
        put_effect(p0, row=0, ch=0, effect=0xF, param=0x06)  # speed = 6
    if "title.loop.song_step" in events:
        # 72 loop iterations in asm; show first 16 pulses as deterministic stub.
        for i in range(16):
            semitone = i % 12
            put_note(p0, row=4 + i * 2, ch=1, sample=2, period=period_for_oct_note(2, semitone))
    if "death.dispatch.pick_type_31_33" in events:
        put_note(p0, row=24, ch=2, sample=3, period=period_for_oct_note(2, 2))   # D-2
    if "death.tone.type32_from_age_word" in events:
        put_note(p0, row=28, ch=2, sample=4, period=period_for_oct_note(2, 5))   # F-2
    if "death.tone.type33_from_level_byte" in events:
        put_note(p0, row=32, ch=3, sample=5, period=period_for_oct_note(2, 9))   # A-2

    # Pattern 1: likely/approximate content separated on purpose.
    if "title.song.poll_mode0" in events:
        put_note(p1, row=0, ch=0, sample=2, period=period_for_oct_note(3, 7))    # G-3
    if "title.song.poll_mode1" in events:
        put_note(p1, row=4, ch=0, sample=2, period=period_for_oct_note(3, 9))    # A-3
    if "death.dispatch.type31_call_7BD8" in events:
        put_note(p1, row=12, ch=2, sample=3, period=period_for_oct_note(2, 0))   # C-2

    # Walk beep family callsites all use immediate note index 0x2D (45) in current export.
    walk_ids = sorted(k for k in events if k.startswith("walk.beep.note_0x2D."))
    walk_period = period_from_note_index(45)
    for i, _event_id in enumerate(walk_ids):
        put_note(p1, row=20 + i * 4, ch=1 + (i % 2), sample=1, period=walk_period)

    patterns = [p0, p1]
    pattern_order = [0, 1]
    mapping = {
        "pattern_0": {
            "confidence_focus": "proven",
            "notes": [
                "title.song.start_blocking -> row0/ch0 sample2",
                "title.loop.song_step -> rows4..34/ch1 sample2",
                "death.dispatch.pick_type_31_33 -> row24/ch2 sample3",
                "death.tone.type32_from_age_word -> row28/ch2 sample4",
                "death.tone.type33_from_level_byte -> row32/ch3 sample5",
            ],
        },
        "pattern_1": {
            "confidence_focus": "likely",
            "notes": [
                "title.song.poll_mode0 + poll_mode1 preview on ch0",
                "death.dispatch.type31_call_7BD8 preview on ch2",
                f"{len(walk_ids)} walk.beep note_0x2D callsites staged with sample1",
            ],
        },
    }
    return patterns, pattern_order, mapping


def encode_cell(cell: Cell) -> bytes:
    period = cell.period & 0x0FFF
    sample = cell.sample & 0x1F
    b0 = ((sample >> 4) << 4) | ((period >> 8) & 0x0F)
    b1 = period & 0xFF
    b2 = ((sample & 0x0F) << 4) | (cell.effect & 0x0F)
    b3 = cell.effect_param & 0xFF
    return bytes((b0, b1, b2, b3))


def write_mod(
    out_path: Path,
    title: str,
    samples: List[SampleDef],
    patterns: List[List[List[Cell]]],
    order: List[int],
) -> None:
    out = bytearray()
    out.extend(pack_text(title, 20))

    # 31 sample headers total.
    sample_map = {s.index: s for s in samples}
    for idx in range(1, 32):
        s = sample_map.get(idx)
        if s is None:
            out.extend(pack_text("", 22))
            out.extend((0).to_bytes(2, "big"))  # length words
            out.append(0)  # finetune
            out.append(0)  # volume
            out.extend((0).to_bytes(2, "big"))  # loop start words
            out.extend((1).to_bytes(2, "big"))  # loop length words (minimum safe)
            continue

        data = ensure_even_bytes(s.data)
        out.extend(pack_text(s.name, 22))
        out.extend((len(data) // 2).to_bytes(2, "big"))
        out.append(0)  # finetune
        out.append(clamp_u8(s.volume))
        out.extend((s.loop_start_words & 0xFFFF).to_bytes(2, "big"))
        out.extend((max(1, s.loop_len_words) & 0xFFFF).to_bytes(2, "big"))

    song_len = max(1, min(128, len(order)))
    out.append(song_len)
    out.append(127)  # restart position (standard)

    order_bytes = bytearray(128)
    for i, p in enumerate(order[:128]):
        order_bytes[i] = p & 0x7F
    out.extend(order_bytes)
    out.extend(b"M.K.")  # 4-channel signature

    max_pat = max(order) if order else 0
    for pat_idx in range(max_pat + 1):
        pat = patterns[pat_idx]
        if len(pat) != 64:
            raise ValueError(f"pattern {pat_idx} row count must be 64")
        for row in pat:
            if len(row) != 4:
                raise ValueError(f"pattern {pat_idx} must have 4 channels")
            for cell in row:
                out.extend(encode_cell(cell))

    for idx in range(1, 32):
        s = sample_map.get(idx)
        if s is None:
            continue
        out.extend(ensure_even_bytes(s.data))

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(out))


def main() -> None:
    parser = argparse.ArgumentParser(description="Build first working MM2 MOD prototype")
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT, help="Intermediate JSON input")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Output MOD file path")
    args = parser.parse_args()

    events = parse_events(args.input)
    samples = make_sample_bank()
    patterns, order, mapping = build_patterns(events)
    write_mod(
        out_path=args.output,
        title="MM2 MOD Prototype",
        samples=samples,
        patterns=patterns,
        order=order,
    )

    mapping_path = args.output.with_suffix(".pattern_map.json")
    mapping_blob = {
        "input": str(args.input),
        "output_mod": str(args.output),
        "pattern_order": order,
        "mapping": mapping,
        "sample_bank": [
            {
                "index": s.index,
                "name": s.name,
                "bytes": len(s.data),
                "loop_start_words": s.loop_start_words,
                "loop_len_words": s.loop_len_words,
            }
            for s in samples
        ],
    }
    mapping_path.write_text(json.dumps(mapping_blob, indent=2), encoding="utf-8")

    print(f"Wrote {args.output}")
    print(f"Wrote {mapping_path}")


if __name__ == "__main__":
    main()


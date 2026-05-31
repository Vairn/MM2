#!/usr/bin/env python3
"""
Decode MM2 ANM/32 image chunks (custom 'TV' variant).

This parser is intentionally conservative:
- It preserves unknown fields.
- It decodes frame plane payload using the nibble stream codec.
- It writes metadata JSON and raw per-plane frame blobs.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Iterable


def be16(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 2], "big", signed=False)


def rassize(width: int, height: int) -> int:
    return height * ((((width) + 15) >> 3) & 0xFFFE)


@dataclass
class FrameDesc:
    a: int
    b: int
    width: int
    height: int


@dataclass
class ImageFrameInfo:
    width: int
    height: int
    flags: int


@dataclass
class ParsedAnm:
    path: str
    file_size: int
    file_magic: str
    prelude_frames: list[FrameDesc]
    prelude_offset: int
    seq_header_a: int
    seq_header_b: int
    seq_header_c: int
    sequences: list[list[int]]
    image_chunk_offset: int
    image_frames: int
    image_depth: int
    image_width: int
    image_height: int
    image_frame_info: list[ImageFrameInfo]
    palette_words_be: list[int]
    decode_start_offset: int
    decoded_frame_consumed: list[int]
    decode_end_offset: int


def parse_prelude_frames(data: bytes) -> list[FrameDesc]:
    # 11 slots of 4 bytes (offsets 0x04..0x2F), 0xFF-filled when unused.
    out: list[FrameDesc] = []
    for i in range(11):
        off = 4 + i * 4
        entry = data[off : off + 4]
        if entry == b"\xff\xff\xff\xff":
            continue
        out.append(
            FrameDesc(
                a=entry[0],
                b=entry[1],
                width=entry[2],
                height=entry[3],
            )
        )
    return out


def parse_sequences(data: bytes, off: int, expected_count: int) -> tuple[list[list[int]], int]:
    seqs: list[list[int]] = []
    cur = off

    while cur < len(data) and len(seqs) < expected_count:
        if data[cur] != 0xFF:
            # Unknown preamble/noise; skip until explicit sequence marker.
            cur += 1
            continue
        cur += 1
        seq: list[int] = []
        while cur < len(data) and data[cur] != 0xFF:
            seq.append(data[cur])
            cur += 1
        seqs.append(seq)
    return seqs, cur


def find_image_chunk(data: bytes, search_from: int) -> int:
    # Based on old tool behavior:
    # FF [00 frames:u16 depth:u16 width:u16 height:u16 ...]
    # i.e. the FF byte is a sentinel and header words start at i+1.
    for i in range(search_from, max(search_from, len(data) - 10)):
        if data[i] != 0xFF:
            continue
        if data[i + 1] != 0x00:
            continue
        hdr = i + 1
        frames = be16(data, hdr + 0)
        depth = be16(data, hdr + 2)
        width = be16(data, hdr + 4)
        height = be16(data, hdr + 6)
        plausible = (
            0 < frames < 256
            and 0 < depth < 64
            and 0 < width <= 1024
            and 0 < height <= 1024
        )
        if plausible:
            return hdr
    raise ValueError("Could not find image chunk marker (FF 00 ...)")


def emit_nibble(out: bytearray, state: dict, nibble: int, limit: int) -> None:
    if len(out) >= limit:
        return
    pending = state.get("pending")
    if pending is None:
        state["pending"] = nibble & 0xF
        return
    out.append(((pending & 0xF) << 4) | (nibble & 0xF))
    state["pending"] = None


def decode_plane_stream(data: bytes, off: int, out_bytes: int) -> tuple[bytes, int]:
    out = bytearray()
    state: dict[str, int | None] = {"pending": None}
    cur = off

    while len(out) < out_bytes:
        if cur >= len(data):
            raise ValueError("Unexpected EOF in nibble stream decode")
        p = data[cur]
        cur += 1
        cmd = p & 0xF0

        if cmd in (0x00, 0xF0):
            nib = (p >> 4) & 0xF
            times = (p & 0x0F) + 1
            for _ in range(times):
                emit_nibble(out, state, nib, out_bytes)
                if len(out) >= out_bytes:
                    break
        else:
            emit_nibble(out, state, (p >> 4) & 0xF, out_bytes)
            emit_nibble(out, state, p & 0xF, out_bytes)

    return bytes(out), cur


def parse_anm(path: Path, out_dir: Path | None) -> ParsedAnm:
    data = path.read_bytes()
    if len(data) < 64:
        raise ValueError("File too small")

    magic = data[:4].hex()
    prelude = parse_prelude_frames(data)

    seq_header_a = data[0x30]
    seq_header_b = data[0x31]
    seq_header_c = data[0x32]
    seq_count = seq_header_b & 0x7F
    seqs, seq_end = parse_sequences(data, 0x33, seq_count)

    img_off = find_image_chunk(data, max(0x33, seq_end - 1))
    frames = be16(data, img_off + 0)
    depth = be16(data, img_off + 2)

    frame_info_off = img_off + 4
    frame_info: list[ImageFrameInfo] = []
    for i in range(frames):
        o = frame_info_off + i * 6
        frame_info.append(
            ImageFrameInfo(
                width=be16(data, o),
                height=be16(data, o + 2),
                flags=be16(data, o + 4),
            )
        )

    img_w = frame_info[0].width if frame_info else 0
    img_h = frame_info[0].height if frame_info else 0

    pal_off = frame_info_off + frames * 6
    pal_words = [be16(data, pal_off + i * 2) for i in range(32)]
    decode_off = pal_off + 64

    consumed: list[int] = []
    cur = decode_off

    if out_dir is not None:
        file_out = out_dir / path.stem
        file_out.mkdir(parents=True, exist_ok=True)

    for idx, fi in enumerate(frame_info):
        rs = rassize(fi.width, fi.height)
        frame_bytes = 5 * rs
        decoded, next_off = decode_plane_stream(data, cur, frame_bytes)
        consumed.append(next_off - cur)

        if out_dir is not None:
            frame_out = file_out / f"frame_{idx:03d}"
            frame_out.mkdir(parents=True, exist_ok=True)
            for p in range(5):
                start = p * rs
                end = start + rs
                (frame_out / f"plane{p}.bin").write_bytes(decoded[start:end])

        cur = next_off

    parsed = ParsedAnm(
        path=str(path),
        file_size=len(data),
        file_magic=magic,
        prelude_frames=prelude,
        prelude_offset=0x04,
        seq_header_a=seq_header_a,
        seq_header_b=seq_header_b,
        seq_header_c=seq_header_c,
        sequences=seqs,
        image_chunk_offset=img_off,
        image_frames=frames,
        image_depth=depth,
        image_width=img_w,
        image_height=img_h,
        image_frame_info=frame_info,
        palette_words_be=pal_words,
        decode_start_offset=decode_off,
        decoded_frame_consumed=consumed,
        decode_end_offset=cur,
    )

    if out_dir is not None:
        (out_dir / path.stem / "meta.json").write_text(
            json.dumps(asdict(parsed), indent=2),
            encoding="utf-8",
        )

    return parsed


def resolve_inputs(items: Iterable[str]) -> list[Path]:
    out: list[Path] = []
    for item in items:
        p = Path(item)
        if p.is_dir():
            out.extend(sorted(x for x in p.glob("*.anm") if x.is_file()))
        elif p.is_file():
            out.append(p)
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description="Decode MM2 ANM files.")
    ap.add_argument("inputs", nargs="+", help="ANM files or directories")
    ap.add_argument(
        "--out",
        default="EXTRACTED/anm_decoded",
        help="Output directory for metadata and plane blobs",
    )
    ap.add_argument(
        "--no-write",
        action="store_true",
        help="Parse and print only; do not write output files",
    )
    args = ap.parse_args()

    files = resolve_inputs(args.inputs)
    if not files:
        raise SystemExit("No input files found.")

    out_dir = None if args.no_write else Path(args.out)
    if out_dir is not None:
        out_dir.mkdir(parents=True, exist_ok=True)

    for p in files:
        parsed = parse_anm(p, out_dir)
        print(
            f"{p.name}: img_off=0x{parsed.image_chunk_offset:x} "
            f"frames={parsed.image_frames} depth={parsed.image_depth} "
            f"size={parsed.image_width}x{parsed.image_height} "
            f"decode_end=0x{parsed.decode_end_offset:x}"
        )


if __name__ == "__main__":
    main()


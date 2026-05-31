#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass, asdict
from pathlib import Path


PLANES = 5


def be16(b: bytes, off: int) -> int:
    return int.from_bytes(b[off : off + 2], "big", signed=False)


def rassize(width: int, height: int) -> int:
    return height * ((((width) + 15) >> 3) & 0xFFFE)


def find_image_chunk(data: bytes, search_from: int = 0x33) -> tuple[int, int]:
    """Return (ff_offset, hdr_offset). hdr_offset points to u16 frame_count."""
    for i in range(search_from, len(data) - 10):
        if data[i] != 0xFF or data[i + 1] != 0x00:
            continue
        hdr = i + 1
        frames = be16(data, hdr + 0)
        depth = be16(data, hdr + 2)
        width = be16(data, hdr + 4)
        height = be16(data, hdr + 6)
        if 0 < frames < 256 and 0 < depth < 64 and 0 < width <= 1024 and 0 < height <= 1024:
            return i, hdr
    raise ValueError("Image chunk marker not found")


def emit_nibble(state: dict[str, int | None], out: bytearray, nib: int, limit: int) -> None:
    if len(out) >= limit:
        return
    pending = state["pending"]
    if pending is None:
        state["pending"] = nib & 0xF
        return
    out.append(((pending & 0xF) << 4) | (nib & 0xF))
    state["pending"] = None


def decode_nibble_stream(data: bytes, off: int, out_len: int) -> tuple[bytes, int]:
    out = bytearray()
    state: dict[str, int | None] = {"pending": None}
    cur = off

    while len(out) < out_len:
        if cur >= len(data):
            raise ValueError("Unexpected EOF in nibble stream")
        p = data[cur]
        cur += 1
        cmd = p & 0xF0
        if cmd in (0x00, 0xF0):
            nib = (p >> 4) & 0xF
            times = (p & 0xF) + 1
            for _ in range(times):
                emit_nibble(state, out, nib, out_len)
                if len(out) >= out_len:
                    break
        else:
            emit_nibble(state, out, (p >> 4) & 0xF, out_len)
            emit_nibble(state, out, p & 0xF, out_len)
    return bytes(out), cur


def encode_nibble_stream(raw: bytes) -> bytes:
    nibs: list[int] = []
    for b in raw:
        nibs.append((b >> 4) & 0xF)
        nibs.append(b & 0xF)

    out = bytearray()
    i = 0
    n = len(nibs)
    while i < n:
        v = nibs[i]
        if v in (0x0, 0xF):
            run = 1
            while i + run < n and run < 16 and nibs[i + run] == v:
                run += 1
            out.append((v << 4) | (run - 1))
            i += run
        else:
            if i + 1 >= n:
                # pad odd nibble count (shouldn't happen with byte input)
                out.append((v << 4) | v)
                i += 1
            else:
                out.append((v << 4) | nibs[i + 1])
                i += 2
    return bytes(out)


def sha1_hex(data: bytes) -> str:
    return hashlib.sha1(data).hexdigest()


@dataclass
class FileReport:
    file: str
    original_size: int
    rebuilt_size: int
    original_sha1: str
    rebuilt_sha1: str
    byte_identical: bool
    first_diff_offset: int
    frames: int
    depth: int
    image_width: int
    image_height: int


def process_one(path: Path, extract_root: Path, reenc_root: Path) -> FileReport:
    data = path.read_bytes()
    ff_off, hdr = find_image_chunk(data)
    frame_count = be16(data, hdr + 0)
    depth = be16(data, hdr + 2)

    frame_info_off = hdr + 4
    frame_infos: list[tuple[int, int, int]] = []
    for i in range(frame_count):
        o = frame_info_off + i * 6
        frame_infos.append((be16(data, o + 0), be16(data, o + 2), be16(data, o + 4)))

    img_w, img_h, _ = frame_infos[0]
    palette_off = frame_info_off + frame_count * 6
    payload_off = palette_off + 64

    out_dir = extract_root / path.stem
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "palette.raw").write_bytes(data[palette_off:payload_off])

    decoded_frames: list[bytes] = []
    cur = payload_off
    for i, (w, h, flags) in enumerate(frame_infos):
        need = PLANES * rassize(w, h)
        raw, cur = decode_nibble_stream(data, cur, need)
        decoded_frames.append(raw)
        fi = out_dir / f"frame_{i:03d}"
        fi.mkdir(parents=True, exist_ok=True)
        rs = rassize(w, h)
        for p in range(PLANES):
            start = p * rs
            end = start + rs
            (fi / f"plane{p}.bin").write_bytes(raw[start:end])

    meta = {
        "path": str(path),
        "ff_offset": ff_off,
        "hdr_offset": hdr,
        "frame_count": frame_count,
        "depth": depth,
        "frame_info": [{"width": w, "height": h, "flags": f} for (w, h, f) in frame_infos],
        "payload_offset": payload_off,
    }
    (out_dir / "meta.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")

    # Rebuild: preserve exact prefix/header/palette bytes, only re-encode payload
    rebuilt = bytearray()
    rebuilt.extend(data[:payload_off])
    for raw in decoded_frames:
        rebuilt.extend(encode_nibble_stream(raw))

    rebuilt_bytes = bytes(rebuilt)
    out_file = reenc_root / path.name
    out_file.parent.mkdir(parents=True, exist_ok=True)
    out_file.write_bytes(rebuilt_bytes)

    same = rebuilt_bytes == data
    first_diff = -1
    if not same:
        lim = min(len(data), len(rebuilt_bytes))
        for i in range(lim):
            if data[i] != rebuilt_bytes[i]:
                first_diff = i
                break
        if first_diff < 0 and len(data) != len(rebuilt_bytes):
            first_diff = lim

    return FileReport(
        file=path.name,
        original_size=len(data),
        rebuilt_size=len(rebuilt_bytes),
        original_sha1=sha1_hex(data),
        rebuilt_sha1=sha1_hex(rebuilt_bytes),
        byte_identical=same,
        first_diff_offset=first_diff,
        frames=frame_count,
        depth=depth,
        image_width=img_w,
        image_height=img_h,
    )


def main() -> None:
    ap = argparse.ArgumentParser(description="Extract MM2 ANM gfx and roundtrip-encode.")
    ap.add_argument("inputs", nargs="+", help="ANM files and/or directories")
    ap.add_argument("--out-root", default="GFX_EXTRACT_RT", help="New output root")
    args = ap.parse_args()

    files: list[Path] = []
    for s in args.inputs:
        p = Path(s)
        if p.is_dir():
            files.extend(sorted(x for x in p.glob("*.anm") if x.is_file()))
        elif p.is_file():
            files.append(p)
    if not files:
        raise SystemExit("No inputs found.")

    root = Path(args.out_root)
    extract_root = root / "extracted_planes"
    reenc_root = root / "reencoded"
    report_root = root / "reports"
    report_root.mkdir(parents=True, exist_ok=True)

    reports: list[FileReport] = []
    for p in files:
        reports.append(process_one(p, extract_root, reenc_root))
        print(f"processed {p.name}")

    report_json = {
        "total_files": len(reports),
        "identical_files": sum(1 for r in reports if r.byte_identical),
        "non_identical_files": sum(1 for r in reports if not r.byte_identical),
        "files": [asdict(r) for r in reports],
    }
    (report_root / "roundtrip_report.json").write_text(
        json.dumps(report_json, indent=2),
        encoding="utf-8",
    )

    lines = [
        f"total: {report_json['total_files']}",
        f"byte-identical: {report_json['identical_files']}",
        f"not-identical: {report_json['non_identical_files']}",
        "",
        "non-identical details:",
    ]
    for r in reports:
        if not r.byte_identical:
            lines.append(
                f"- {r.file}: orig={r.original_size} rebuilt={r.rebuilt_size} "
                f"first_diff=0x{r.first_diff_offset:x}"
            )
    (report_root / "roundtrip_summary.txt").write_text("\n".join(lines), encoding="utf-8")

    print("\n" + "\n".join(lines[:3]))
    print(f"report: {report_root / 'roundtrip_report.json'}")


if __name__ == "__main__":
    main()


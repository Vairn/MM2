#!/usr/bin/env python3
"""Per-track sector classification for MM2 C64 D64 images.

Maps each track/sector to empty | iec_code | code | text | text_mixed | data,
aggregates per disk, and flags loader/IEC sectors.

Usage:
  python tools/c64_track_map.py EXTRACTED/c64 --out EXTRACTED/c64/analysis
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

from c64_d64_layout import (
    IEC_PATTERNS,
    ROOT,
    SPT,
    classify_payload,
    read_sector,
    sector_offset,
    sector_payload,
)

LOADER_ORG = 0x0400  # T18S1 disassembled here (confirmed IEC bit-bang)


def analyze_disk(path: Path) -> dict:
    data = path.read_bytes()
    sectors: list[dict] = []
    track_summary: dict[int, Counter] = {t: Counter() for t in range(1, 36)}

    for track in range(1, 36):
        for sector in range(SPT[track - 1]):
            raw = read_sector(data, track, sector)
            payload = raw[2:]
            klass = classify_payload(payload)
            track_summary[track][klass] += 1
            entry: dict = {
                "track": track,
                "sector": sector,
                "offset": sector_offset(track, sector),
                "link": [raw[0], raw[1]],
                "class": klass,
            }
            for name, pat in IEC_PATTERNS.items():
                if pat in payload:
                    entry.setdefault("iec_patterns", []).append(name)
            if klass in ("text", "text_mixed"):
                # sample longest ASCII run
                best = ""
                cur = []
                for b in payload:
                    if 32 <= b < 127:
                        cur.append(chr(b))
                    else:
                        if len(cur) > len(best):
                            best = "".join(cur)
                        cur = []
                if len(cur) > len(best):
                    best = "".join(cur)
                if len(best) >= 8:
                    entry["ascii_sample"] = best[:80]
            sectors.append(entry)

    tracks = []
    for track in range(1, 36):
        counts = dict(track_summary[track])
        dominant = max(counts, key=counts.get) if counts else "empty"
        tracks.append({"track": track, "sectors": SPT[track - 1], "counts": counts, "dominant": dominant})

    # Cross-disk loader fingerprint: T18S1 payload hash marker
    t18s1 = sector_payload(data, 18, 1)
    return {
        "source": str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
        "size": len(data),
        "tracks": tracks,
        "sectors": sectors,
        "t18s1_md5": __import__("hashlib").md5(t18s1).hexdigest(),
        "loader_note": "T18S1 IEC routine; org $0400 for static disasm",
    }


def main() -> None:
    ap = argparse.ArgumentParser(description="C64 MM2 track/sector map")
    ap.add_argument("input", type=Path, help="D64 file or directory")
    ap.add_argument("--out", type=Path, default=ROOT / "EXTRACTED" / "c64" / "analysis")
    args = ap.parse_args()

    paths = sorted(args.input.glob("*.D64")) if args.input.is_dir() else [args.input]
    args.out.mkdir(parents=True, exist_ok=True)

    report = [analyze_disk(p) for p in paths]
    out_json = args.out / "track_map.json"
    out_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Wrote {out_json} ({len(report)} disks)")

    # Human-readable summary
    summary_lines = ["# C64 track map summary\n"]
    for disk in report:
        name = Path(disk["source"]).name
        summary_lines.append(f"## {name}\n")
        summary_lines.append(f"- T18S1 MD5: `{disk['t18s1_md5']}`\n")
        for tr in disk["tracks"]:
            c = tr["counts"]
            if tr["dominant"] == "empty" and sum(c.values()) == c.get("empty", 0):
                continue
            summary_lines.append(
                f"- T{tr['track']:02d}: {tr['dominant']} "
                f"({', '.join(f'{k}={v}' for k, v in sorted(c.items()))})\n"
            )
        summary_lines.append("\n")

    summary_path = args.out / "track_map_summary.md"
    summary_path.write_text("".join(summary_lines), encoding="utf-8")
    print(f"Wrote {summary_path}")


if __name__ == "__main__":
    main()

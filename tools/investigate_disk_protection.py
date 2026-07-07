#!/usr/bin/env python3
"""Investigate Apple II WOZ and C64 D64 images for crack vs copy-protection evidence."""
from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from c64_d64_layout import read_sector, sector_offset  # noqa: E402

CRACK_PATTERNS = [
    b"CRACKED", b"CRACK", b"CRACKER", b"TRAINER", b"FREE VERSION", b"FREEWARE",
    b"TRSI", b"FAIRLIGHT", b"SKID ROW", b"SKIDROW", b"REBELS", b"CLASS",
    b"DOMINATORS", b"RAZOR", b"HYBRID", b"SCENE", b"PRESENTS", b"WAREZ",
    b"INTRO", b"NFO", b"TRSI", b"FLT", b"FLT/", b"FLT\\",
    b"CRACKED BY", b"CRACKEDBY", b"BY ", b"TRAINED", b"UNLIMITED",
    b"NO COPY", b"NO PROTECTION", b"REMOVED", b"PATCHED",
    b"NEW WORLD", b"MIGHT AND MAGIC", b"MAGIC 2",
]

APPLE_CRACK_PATTERNS = CRACK_PATTERNS + [
    b"WOZ-A-DAY", b"APPLESauce", b"4AM", b"SAN INC", b"PASSPORT",
]


def ascii_strings(data: bytes, min_len: int = 4) -> list[tuple[int, str]]:
    out: list[tuple[int, str]] = []
    cur: list[int] = []
    start = 0
    for i, b in enumerate(data):
        if 0x20 <= b < 0x7F:
            if not cur:
                start = i
            cur.append(b)
        else:
            if len(cur) >= min_len:
                out.append((start, bytes(cur).decode("ascii", errors="replace")))
            cur = []
    if len(cur) >= min_len:
        out.append((start, bytes(cur).decode("ascii", errors="replace")))
    return out


def petscii_strings(data: bytes, min_len: int = 4) -> list[tuple[int, str]]:
    out: list[tuple[int, str]] = []
    cur: list[int] = []
    start = 0
    for i, b in enumerate(data):
        if (0x41 <= b <= 0x5A) or (0x30 <= b <= 0x39) or b in (0x20, 0x2E, 0x2D, 0x2B, 0x2F):
            if not cur:
                start = i
            cur.append(b)
        else:
            if len(cur) >= min_len:
                out.append((start, bytes(cur).decode("ascii", errors="replace")))
            cur = []
    if len(cur) >= min_len:
        out.append((start, bytes(cur).decode("ascii", errors="replace")))
    return out


def find_pattern_hits(data: bytes, patterns: list[bytes]) -> list[dict]:
    hits = []
    for pat in patterns:
        off = 0
        while True:
            i = data.find(pat, off)
            if i < 0:
                break
            ctx = data[max(0, i - 16) : i + len(pat) + 32]
            hits.append({
                "pattern": pat.decode("ascii", errors="replace"),
                "offset": i,
                "context": ctx.decode("ascii", errors="replace"),
            })
            off = i + 1
    return hits


def parse_woz_meta(woz_path: Path) -> dict:
    data = woz_path.read_bytes()
    result = {"path": str(woz_path), "size": len(data), "magic": data[:4].hex()}
    if data[:4] != b"WOZ2":
        result["error"] = "not WOZ2"
        return result

    # WOZ2 chunk table
    info_count = struct.unpack_from("<I", data, 4)[0]
    chunks = []
    pos = 8
    for _ in range(info_count):
        if pos + 12 > len(data):
            break
        cid, offset, size = struct.unpack_from("<4sII", data, pos)
        chunks.append({"id": cid.decode("ascii", errors="replace"), "offset": offset, "size": size})
        pos += 12

    result["chunks"] = chunks
    for ch in chunks:
        if ch["id"] == "META":
            raw = data[ch["offset"] : ch["offset"] + ch["size"]]
            try:
                meta = json.loads(raw.decode("utf-8"))
                result["meta"] = meta
            except Exception as exc:  # noqa: BLE001
                result["meta_raw"] = raw[:500].decode("utf-8", errors="replace")
                result["meta_error"] = str(exc)
        elif ch["id"] == "INFO":
            raw = data[ch["offset"] : ch["offset"] + ch["size"]]
            result["info"] = raw.decode("ascii", errors="replace").strip("\x00")

    return result


def analyze_c64_bam(data: bytes) -> dict:
    bam = read_sector(data, 18, 0)
    name = petscii_strings(bam[0x90:0xA0])[0][1] if petscii_strings(bam[0x90:0xA0]) else ""
    dos_type = chr(bam[0xA2]) if 0x20 <= bam[0xA2] < 0x7F else hex(bam[0xA2])
    dir_track = bam[0xA0]
    dir_sector = bam[0xA1]
    link_t = bam[0x00]
    link_s = bam[0x01]
    return {
        "disk_name": name,
        "dos_type": dos_type,
        "dir_track": dir_track,
        "dir_sector": dir_sector,
        "bam_link": f"T{link_t}S{link_s}",
        "bam_header_hex": bam[:16].hex(),
    }


def parse_c64_directory(data: bytes) -> dict:
    """Attempt standard CBM directory parse on T18."""
    entries = []
    bogus = 0
    for sec in range(1, 8):
        try:
            sector = read_sector(data, 18, sec)
        except Exception:
            break
        for slot in range(8):
            off = slot * 32
            entry = sector[off : off + 32]
            if entry[0] == 0:
                continue
            ftype = entry[2] & 0x07
            closed = bool(entry[2] & 0x80)
            start_t = entry[3]
            start_s = entry[4]
            name_raw = entry[5:21]
            name = petscii_strings(name_raw, 1)
            name_s = name[0][1] if name else name_raw.hex()
            # Heuristic: valid PETSCII filename
            valid = all(
                (0x41 <= b <= 0x5A) or (0x30 <= b <= 0x39) or b in (0x20, 0x2E, 0x2D, 0x2B) or b == 0xA0
                for b in name_raw if b != 0xA0
            ) and start_t <= 35
            if not valid:
                bogus += 1
            entries.append({
                "sector": sec,
                "slot": slot,
                "type": ftype,
                "closed": closed,
                "start": f"T{start_t}S{start_s}",
                "name": name_s,
                "valid_heuristic": valid,
                "raw_type": hex(entry[2]),
            })
    return {"entries": entries, "bogus_count": bogus, "total": len(entries)}


def trace_sector_chain(data: bytes, track: int, sector: int, max_sectors: int = 200) -> dict:
    chain = []
    visited = set()
    total = 0
    t, s = track, sector
    for _ in range(max_sectors):
        key = (t, s)
        if key in visited or t < 1 or t > 35:
            break
        visited.add(key)
        try:
            sec = read_sector(data, t, s)
        except Exception:
            break
        link_t, link_s = sec[0], sec[1]
        payload = sec[2:]
        chain.append({"track": t, "sector": s, "link": f"T{link_t}S{link_s}", "payload_len": len(payload)})
        total += len(payload)
        if link_t == 0:
            break
        t, s = link_t, link_s
    return {"head": f"T{track}S{sector}", "sectors": len(chain), "bytes": total, "chain": chain[:20]}


def find_prg_headers(data: bytes) -> list[dict]:
    hits = []
    for off in range(0, len(data) - 2):
        if data[off] == 0x01 and data[off + 1] == 0x08:
            if off + 3 < len(data):
                end_lo, end_hi = data[off + 2], data[off + 3]
                load_addr = 0x0801
                end_addr = end_lo | (end_hi << 8)
                if 0x0801 <= end_addr <= 0xC000 and end_addr > load_addr:
                    hits.append({
                        "offset": off,
                        "load": hex(load_addr),
                        "end": hex(end_addr),
                        "size_est": end_addr - load_addr + 1,
                    })
    return hits[:50]


def analyze_apple_dsk(dsk_path: Path) -> dict:
    data = dsk_path.read_bytes()
    result = {"path": str(dsk_path), "size": len(data)}
    # T17S0 VTOC check (DOS 3.3)
    t17s0_off = 17 * 16 * 256  # track 17 sector 0 in DOS order? Actually need proper mapping
    # DOS 3.3: track 17 sector 0 is at offset 17*4096? Standard: 4096 bytes per track in DOS order
    t17s0_off = 17 * 4096
    vtoc = data[t17s0_off : t17s0_off + 256]
    result["t17s0_hex"] = vtoc[:32].hex()
    result["t17s0_is_vtoc"] = vtoc[3] == 0x03 and vtoc[6] == 0x11  # typical VTOC markers

    # Search for ProDOS blocks ($AA $AA at block start) and DOS file headers
    prodos_blocks = []
    for off in range(0, len(data) - 4, 256):
        if data[off] == 0xAA and data[off + 1] == 0xAA:
            prodos_blocks.append(off)
    result["prodos_aa_blocks"] = len(prodos_blocks)

    # Search for $AA $55 DOS 3.3 sector address field pattern in sector headers
    crack_hits = find_pattern_hits(data, APPLE_CRACK_PATTERNS)
    result["pattern_hits"] = crack_hits[:30]
    result["interesting_strings"] = [
        s for s in ascii_strings(data, 8)
        if any(k in s[1].upper() for k in ("CRACK", "TRAIN", "FREE", "WOZ", "MAGIC", "WORLD", "NEW"))
    ][:40]

    try:
        from nibbler.dsk import read_catalog, read_vtoc
        try:
            vtoc_info = read_vtoc(data)
            files = read_catalog(data)
            result["catalog"] = {"ok": True, "file_count": len(files), "files": files[:20]}
        except Exception as exc:  # noqa: BLE001
            result["catalog"] = {"ok": False, "error": str(exc)}
    except ImportError:
        result["catalog"] = {"ok": False, "error": "nibbler not installed"}

    return result


def main() -> int:
    out_dir = ROOT / "EXTRACTED" / "disk_investigation"
    out_dir.mkdir(parents=True, exist_ok=True)
    report: dict = {"apple": {}, "c64": {}}

    apple_dir = ROOT / "EXTRACTED" / "apple" / "Might and Magic Book Two (woz-a-day collection)"
    if apple_dir.is_dir():
        woz_files = sorted(apple_dir.glob("*.woz"))
        report["apple"]["woz_meta"] = [parse_woz_meta(p) for p in woz_files]
        report["apple"]["woz_pattern_hits"] = []
        for p in woz_files:
            raw = p.read_bytes()
            hits = find_pattern_hits(raw, APPLE_CRACK_PATTERNS)
            if hits:
                report["apple"]["woz_pattern_hits"].append({"file": p.name, "hits": hits[:10]})

    apple2_disks = ROOT / "EXTRACTED" / "apple2" / "disks"
    if apple2_disks.is_dir():
        report["apple"]["dsk_analysis"] = [
            analyze_apple_dsk(p) for p in sorted(apple2_disks.glob("disk_*.dsk"))
        ]

    c64_dir = ROOT / "EXTRACTED" / "c64"
    for d64 in sorted(c64_dir.glob("MM2-*.D64")):
        data = d64.read_bytes()
        name = d64.name
        info = {
            "bam": analyze_c64_bam(data),
            "directory": parse_c64_directory(data),
            "pattern_hits": find_pattern_hits(data, CRACK_PATTERNS)[:20],
            "interesting_strings": [
                s for s in petscii_strings(data, 6)
                if any(k in s[1].upper() for k in ("CRACK", "TRAIN", "FREE", "MAGIC", "WORLD", "NEW", "COMP"))
            ][:30],
            "prg_headers": find_prg_headers(data)[:15],
        }
        # Trace chains from T18
        info["t18s0_chain"] = trace_sector_chain(data, 18, 0, 10)
        # Try common data chain heads on disks 2-6
        chains = []
        for t in range(1, 36):
            for s in range(0, 21):
                try:
                    sec = read_sector(data, t, s)
                except Exception:
                    continue
                lt, ls = sec[0], sec[1]
                if 1 <= lt <= 35 and ls < 21 and (lt, ls) != (t, s):
                    c = trace_sector_chain(data, t, s, 5)
                    if c["sectors"] >= 3:
                        chains.append(c)
        # dedupe by head
        seen = set()
        unique = []
        for c in sorted(chains, key=lambda x: -x["bytes"]):
            if c["head"] not in seen:
                seen.add(c["head"])
                unique.append(c)
        info["long_chains_top10"] = unique[:10]
        report["c64"][name] = info

    out_path = out_dir / "protection_investigation.json"
    out_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Wrote {out_path}")

    # Summary to stdout
    print("\n=== APPLE WOZ META ===")
    for m in report["apple"].get("woz_meta", []):
        meta = m.get("meta", {})
        info = m.get("info", "")
        print(f"  {Path(m['path']).name}: info={info!r}")
        if meta:
            for k in ("title", "subtitle", "publisher", "developer", "language", "requires_machine", "requires_ram", "notes"):
                if k in meta:
                    print(f"    {k}: {meta[k]}")

    print("\n=== APPLE CATALOG ===")
    for d in report["apple"].get("dsk_analysis", []):
        cat = d.get("catalog", {})
        print(f"  {Path(d['path']).name}: catalog_ok={cat.get('ok')} files={cat.get('file_count', 0)} error={cat.get('error', '')[:60]}")

    print("\n=== C64 BAM / DIRECTORY ===")
    for name, info in report["c64"].items():
        bam = info["bam"]
        dirent = info["directory"]
        print(f"  {name}: name={bam['disk_name']!r} dir=T{bam['dir_track']}S{bam['dir_sector']} "
              f"bam_link={bam['bam_link']} dir_entries={dirent['total']} bogus={dirent['bogus_count']}")
        if info["pattern_hits"]:
            print(f"    pattern hits: {[h['pattern'] for h in info['pattern_hits'][:5]]}")
        if info["long_chains_top10"]:
            print(f"    top chain: {info['long_chains_top10'][0]['head']} ({info['long_chains_top10'][0]['bytes']} bytes)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Extract printable / embedded strings from the MM2 Amiga executable.

Scans hunk segments (code + data), optional str.dat codec blobs, and PEA
(PC-relative) references in mm2.capstone.asm.  Cross-references str.dat and
event.dat when present.

Usage:
  python tools/extract_embedded_strings.py mm2
  python tools/extract_embedded_strings.py mm2 -o EXTRACTED/embedded_strings.json
  python tools/extract_embedded_strings.py mm2 --doc EXTRACTED/docs/29-embedded-exe-strings.md
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "EXTRACTED" / "ghidra" / "mm2_segments.json"
DEFAULT_ASM = ROOT / "EXTRACTED" / "mm2.capstone.asm"
DEFAULT_STR_DECODED = ROOT / "EXTRACTED" / "docs" / "11-str-decoded.txt"
CODE_HUNK0_FILE_OFF = 0x20  # first CODE hunk data starts 32 bytes into mm2

# Town-service handler anchors (Capstone load addresses, code hunk 0).
HANDLER_ADDRS = {
    "open_tavern_food": 0x1A132,
    "open_inn_lodging": 0x19EC0,  # -$7CD4 thunk region ~0x19Exx strings
    "open_temple": 0x1D208,
    "open_training": 0x1F800,  # approximate string block
    "open_mages_guild": 0x1E870,
    "open_blacksmith_shop": 0x1C54A,
}

TOWN_SERVICE_RANGES = [
    ("town_inn", 0x19D00, 0x1A200, "Inn / rest / registry / dismiss hirelings"),
    ("town_pub_quests", 0x18800, 0x18C00, "Pub-adjacent quest NPCs (Lord Hoardall etc.)"),
    ("town_chests_traps", 0x1A280, 0x1A800, "Chest labels + trap messages"),
    ("town_blacksmith", 0x1C400, 0x1D000, "Blacksmith shop UI strings"),
    ("town_temple", 0x1D100, 0x1E400, "Temple UI + donation/heal strings"),
    ("town_guild", 0x1E800, 0x1EA00, "Mage guild UI"),
    ("town_training", 0x1F800, 0x20600, "Training enrollment strings"),
]

PREFIX_RE = re.compile(r"^[^A-Za-z<{(]*")


@dataclass
class Segment:
    kind: str
    load_addr: int
    file_offset: int
    size: int
    name: str


@dataclass
class ExtractedString:
    load_addr: int
    file_offset: int
    segment: str
    kind: str
    text: str
    text_clean: str
    length: int
    category: str
    source: str = "binary_scan"
    pea_refs: list[int] = field(default_factory=list)
    in_str_dat: bool = False
    in_event_dat: bool = False
    str_dat_match: str | None = None


def load_manifest(path: Path) -> list[Segment]:
    data = json.loads(path.read_text(encoding="utf-8"))
    out: list[Segment] = []
    for seg in data["segments"]:
        if not seg.get("name"):
            continue
        out.append(
            Segment(
                kind=seg["kind"],
                load_addr=seg["load_addr"],
                file_offset=seg["file_offset"],
                size=seg["size"],
                name=seg["name"],
            )
        )
    return out


def sign_ext16(word: int) -> int:
    return word - 0x10000 if word >= 0x8000 else word


def clean_embedded_text(raw: str) -> str:
    # MM2 UI lines are often prefixed with a 4-byte tag ending in "]Nu".
    if "]Nu" in raw:
        raw = raw.split("]Nu", 1)[1]
    return PREFIX_RE.sub("", raw).strip()


def is_probably_text(s: str) -> bool:
    if len(s) < 4:
        return False
    letters = sum(c.isalpha() for c in s)
    if letters < max(3, len(s) // 4):
        return False
    # Skip obvious mis-decoded instructions (many 4-char opcode fragments).
    if len(s) <= 5 and s.isupper() and " " not in s:
        return False
    return True


def scan_ascii(data: bytes, base_load: int, file_base: int, segment: Segment,
               min_len: int = 4) -> list[ExtractedString]:
    out: list[ExtractedString] = []
    i = 0
    while i < len(data):
        b = data[i]
        if 32 <= b < 127:
            start = i
            while i < len(data) and 32 <= data[i] < 127:
                i += 1
            raw = data[start:i].decode("ascii")
            if len(raw) >= min_len and is_probably_text(raw):
                load = base_load + start
                out.append(
                    ExtractedString(
                        load_addr=load,
                        file_offset=file_base + start,
                        segment=segment.name,
                        kind=segment.kind,
                        text=raw,
                        text_clean=clean_embedded_text(raw),
                        length=len(raw),
                        category=categorize(load, raw),
                    )
                )
        else:
            i += 1
    return out


def scan_str_codec(data: bytes, base_load: int, file_base: int,
                   segment: Segment, min_len: int = 6) -> list[ExtractedString]:
    """Find runs that decode to printable ASCII via str.dat codec."""
    out: list[ExtractedString] = []
    decoded = bytes((0x0A if b == 0x01 else (b + 0x1C) & 0xFF) for b in data)
    i = 0
    while i < len(decoded):
        b = decoded[i]
        if 32 <= b < 127 or b == 0x0A:
            start = i
            while i < len(decoded) and (32 <= decoded[i] < 127 or decoded[i] == 0x0A):
                i += 1
            chunk = decoded[start:i].replace(b"\x0A", b" ").decode("ascii")
            if len(chunk.strip()) >= min_len and is_probably_text(chunk.strip()):
                out.append(
                    ExtractedString(
                        load_addr=base_load + start,
                        file_offset=file_base + start,
                        segment=segment.name,
                        kind=segment.kind,
                        text=chunk.strip(),
                        text_clean=chunk.strip(),
                        length=len(chunk.strip()),
                        category="str_codec_blob",
                        source="str_codec_scan",
                    )
                )
        else:
            i += 1
    return out


def categorize(load_addr: int, text: str) -> str:
    lo = text.lower()
    for cat, start, end, _ in TOWN_SERVICE_RANGES:
        if start <= load_addr < end:
            return cat

    if load_addr < 0x400:
        if any(k in lo for k in ("version", "mm2boot", "mm2play")):
            return "boot_meta"
        if lo in ("middlegate", "atlantium", "tundara", "vulcania", "sansobar"):
            return "town_names"
        return "early_tables"

    if load_addr < 0x1000:
        if any(k in lo for k in ("knight", "paladin", "human", "elf", "female", "male")):
            return "party_tables"

    if load_addr >= 0x2593C:
        if ".library" in lo or lo.endswith(".font"):
            return "amiga_libraries"
        if lo.endswith(".dat") or lo.endswith(".32") or lo.endswith(".anm"):
            return "data_filenames"
        if "ram" in lo or "couldn't" in lo or "ctrl-amiga" in lo:
            return "boot_errors"
        if "setup" in lo or "characters" in lo:
            return "ui_menus"
        return "code_overlay"

    if any(k in lo for k in ("copy", "manual", "paragraph", "page")):
        return "copy_protection"

    if any(k in lo for k in ("attack", "damage", "monster", "spell", "hit points", "dead")):
        return "combat"

    if re.search(r"\([a-f]\)", lo) or "select (" in lo or lo.startswith("a)"):
        return "ui_menus"

    if "not enough gold" in lo or "not enough" in lo:
        return "shop_common"

    if "globe" in lo or "xor" in lo:
        return "copy_protection"

    return "misc_code"


def parse_pea_refs(asm_path: Path) -> dict[int, list[int]]:
    """Map string load address -> list of PEA instruction addresses."""
    if not asm_path.is_file():
        return {}
    text = asm_path.read_text(encoding="utf-8", errors="replace")
    refs: dict[int, list[int]] = {}
    rx_pc = re.compile(r"^([0-9a-f]{6})\s+487a([0-9a-f]{4})\s+pea", re.I | re.M)
    rx_abs = re.compile(r"^([0-9a-f]{6})\s+4879([0-9a-f]{8})\s+pea", re.I | re.M)

    for m in rx_pc.finditer(text):
        insn = int(m.group(1), 16)
        disp = sign_ext16(int(m.group(2), 16))
        target = (insn + 2 + disp) & 0xFFFFFFFF
        refs.setdefault(target, []).append(insn)

    for m in rx_abs.finditer(text):
        insn = int(m.group(1), 16)
        target = int(m.group(2), 16)
        refs.setdefault(target, []).append(insn)

    return refs


def load_str_dat_blocks(str_path: Path | None, decoded_path: Path | None) -> list[str]:
    blocks: list[str] = []
    if decoded_path and decoded_path.is_file():
        raw = decoded_path.read_text(encoding="utf-8", errors="replace")
        for para in re.split(r"\n\s*\n", raw):
            t = " ".join(para.split())
            if len(t) >= 4:
                blocks.append(t)
        return blocks

    if str_path and str_path.is_file():
        data = str_path.read_bytes()
        dec = bytes((0x0A if b == 0x01 else (b + 0x1C) & 0xFF) for b in data)
        text = dec.decode("latin-1", errors="replace")
        for para in re.split(r"\n\s*\n", text):
            t = " ".join(para.split())
            if len(t) >= 4:
                blocks.append(t)
    return blocks


def load_event_strings(event_path: Path) -> list[str]:
    if not event_path.is_file():
        return []
    sys.path.insert(0, str(ROOT / "tools"))
    import decode_event  # noqa: WPS433

    data = event_path.read_bytes()
    header = decode_event.read_header(data)
    out: list[str] = []
    for loc_id, (off, length) in enumerate(header):
        if length == 0:
            continue
        blob = data[off : off + length]
        rec = decode_event.decode_location(blob, loc_id)
        for s in rec.get("strings", []):
            norm = " ".join(s.replace("@", " ").split())
            if len(norm) >= 4:
                out.append(norm)
    return out


def normalize(s: str) -> str:
    return " ".join(s.lower().split())


def cross_reference(strings: list[ExtractedString], str_blocks: list[str],
                    event_strings: list[str]) -> None:
    str_norm = [normalize(b) for b in str_blocks]
    evt_norm = [normalize(e) for e in event_strings]

    for item in strings:
        probe = normalize(item.text_clean)
        if len(probe) < 12:
            continue
        for block, bn in zip(str_blocks, str_norm):
            if probe in bn or (len(bn) >= 20 and bn in probe):
                item.in_str_dat = True
                item.str_dat_match = block[:120]
                break
        if item.in_str_dat:
            continue
        for en in evt_norm:
            if probe in en or (len(en) >= 20 and en in probe):
                item.in_event_dat = True
                break


def attach_pea_refs(strings: list[ExtractedString], pea_map: dict[int, list[int]]) -> None:
    by_addr = {s.load_addr: s for s in strings}
    for addr, insns in pea_map.items():
        # Exact hit or string starts within a few bytes (N]Nu tag).
        for delta in range(0, 8):
            hit = by_addr.get(addr + delta)
            if hit:
                hit.pea_refs.extend(insns)
                if hit.source == "binary_scan":
                    hit.source = "binary_scan+pea"
                break


def dedupe(strings: list[ExtractedString]) -> list[ExtractedString]:
    seen: set[tuple[int, str]] = set()
    out: list[ExtractedString] = []
    for s in sorted(strings, key=lambda x: (x.load_addr, x.text)):
        key = (s.load_addr, s.text)
        if key in seen:
            continue
        seen.add(key)
        out.append(s)
    return out


def filter_quality(strings: list[ExtractedString], min_clean: int = 6) -> list[ExtractedString]:
    out: list[ExtractedString] = []
    for s in strings:
        if len(s.text_clean) >= min_clean:
            out.append(s)
        elif s.category in ("boot_meta", "town_names", "data_filenames", "amiga_libraries"):
            out.append(s)
        elif s.pea_refs:
            out.append(s)
    return out


def town_service_lines(strings: list[ExtractedString]) -> dict[str, list[dict]]:
    grouped: dict[str, list[dict]] = {cat: [] for cat, *_ in TOWN_SERVICE_RANGES}
    for s in strings:
        if s.category in grouped and len(s.text_clean) >= 8:
            grouped[s.category].append(asdict(s))
    return grouped


def write_json(path: Path, strings: list[ExtractedString], meta: dict) -> None:
    payload = {
        "meta": meta,
        "handler_anchors": HANDLER_ADDRS,
        "town_service_ranges": [
            {"category": c, "start": a, "end": b, "note": n}
            for c, a, b, n in TOWN_SERVICE_RANGES
        ],
        "strings": [asdict(s) for s in strings],
        "town_services": town_service_lines(strings),
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def write_txt(path: Path, strings: list[ExtractedString], meta: dict) -> None:
    lines: list[str] = []
    lines.append("MM2 embedded executable strings")
    lines.append("=" * 60)
    for k, v in meta.items():
        lines.append(f"{k}: {v}")
    lines.append("")

    cur_cat = None
    for s in strings:
        if s.category != cur_cat:
            cur_cat = s.category
            lines.append("")
            lines.append(f"## {cur_cat}")
            lines.append("-" * 40)
        flags = []
        if s.in_str_dat:
            flags.append("str.dat")
        if s.in_event_dat:
            flags.append("event.dat")
        if s.pea_refs:
            flags.append(f"PEA@{','.join(f'0x{a:05X}' for a in s.pea_refs[:3])}")
        flag_s = f" [{', '.join(flags)}]" if flags else " [exe-only]"
        lines.append(f"0x{s.load_addr:05X}  {s.text_clean[:100]}{flag_s}")

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_doc(path: Path, strings: list[ExtractedString], meta: dict,
              str_blocks: list[str], event_strings: list[str]) -> None:
    cats: dict[str, int] = {}
    for s in strings:
        cats[s.category] = cats.get(s.category, 0) + 1

    exe_only = [s for s in strings if not s.in_str_dat and not s.in_event_dat
                and len(s.text_clean) >= 8]
    dup_str = [s for s in strings if s.in_str_dat]
    dup_evt = [s for s in strings if s.in_event_dat]

    lines: list[str] = [
        "# Embedded executable strings (MM2 Amiga)",
        "",
        "Generated by `tools/extract_embedded_strings.py`.",
        "",
        "## Binary survey",
        "",
        f"- **Source executable:** `{meta.get('source')}` ({meta.get('size')} bytes)",
        f"- **Segment manifest:** `EXTRACTED/ghidra/mm2_segments.json`",
        f"- **Code hunk 0:** load `0x00000`, size `0x2593C` ({153916} bytes) — main game code + embedded ASCII tables",
        f"- **Data hunk 0:** load `0x2593C`, size `0x21AC` — mostly numeric tables; few ASCII strings",
        f"- **Code hunk 1 (overlay):** load `0x27B1C`, size `0x3CB8` — boot errors, Amiga library names, `.dat` filenames",
        f"- **Capstone listing:** `EXTRACTED/mm2.capstone.asm` (PEA PC-relative refs parsed for xrefs)",
        "",
        "## Encoding",
        "",
        "- **Embedded exe strings:** raw ASCII, NUL or inline `N]Nu` / `XON]Nu` tag prefixes before UI lines.",
        "- **`str.dat`:** `(byte + 0x1C) & 0xFF` with `0x01` → newline — **not** used for exe-embedded text.",
        "- **`event.dat`:** per-location `0xFF`-terminated string banks; door titles & scripted dialogue.",
        "",
        "## Extraction summary",
        "",
        f"| Metric | Count |",
        f"|--------|------:|",
        f"| Raw ASCII runs (quality-filtered) | {meta.get('total_filtered')} |",
        f"| PEA (PC) xrefs attached | {meta.get('pea_refs')} |",
        f"| Also in str.dat | {len(dup_str)} |",
        f"| Also in event.dat | {len(dup_evt)} |",
        f"| Exe-only (≥8 clean chars) | {len(exe_only)} |",
        f"| str.dat blocks loaded | {len(str_blocks)} |",
        f"| event.dat strings loaded | {len(event_strings)} |",
        "",
        "### By category",
        "",
        "| Category | Count |",
        "|----------|------:|",
    ]
    for cat, n in sorted(cats.items(), key=lambda x: (-x[1], x[0])):
        lines.append(f"| `{cat}` | {n} |")

    lines.extend([
        "",
        "## Town service string blocks (exe)",
        "",
        "Handlers (`OP_0E` selectors) live in code hunk 0; most **scene dialogue** for",
        "pub/shop/temple is in **`str.dat`**, while **inn/rest/chest/trap/shop chrome**",
        "and many **menu labels** are embedded next to handlers:",
        "",
    ])

    for cat, start, end, note in TOWN_SERVICE_RANGES:
        hits = [s for s in strings if s.category == cat and len(s.text_clean) >= 8]
        lines.append(f"### `{cat}` — `0x{start:05X}`..`0x{end:05X}`")
        lines.append(f"{note}. **{len(hits)}** strings.")
        lines.append("")
        for s in hits[:40]:
            src = "str.dat" if s.in_str_dat else ("event.dat" if s.in_event_dat else "exe-only")
            lines.append(f"- `0x{s.load_addr:05X}` [{src}] {s.text_clean[:90]}")
        if len(hits) > 40:
            lines.append(f"- … +{len(hits) - 40} more (see JSON)")
        lines.append("")

    lines.extend([
        "## Handler anchor addresses",
        "",
        "| Service | Load address |",
        "|---------|-------------:|",
    ])
    for name, addr in HANDLER_ADDRS.items():
        lines.append(f"| `{name}` | `0x{addr:05X}` |")

    lines.extend([
        "",
        "## str.dat vs exe split (town services)",
        "",
        "| Content | Primary location |",
        "|---------|------------------|",
        "| Pub NPC intros (Amber, Rowena, Belinthra…), food/drink menus, rumors | **str.dat** |",
        "| Blacksmith NPC intros (Svendegard…), buy/sell prompts | **str.dat** + exe menu labels |",
        "| Temple priest intros, donation feedback | **str.dat** |",
        "| Mage guild intros, membership gate | **str.dat** + exe `Mage Guild` label |",
        "| Innkeeper scenes, rest prompts, registry y/n | **exe** `0x19D00`..`0x1A200` |",
        "| Chest/trap messages | **exe** `0x1A280`..`0x1A800` |",
        "| Building sign titles (Slaughtered Lamb, etc.) | **event.dat** (`OP_0B`) |",
        "",
        "## Tooling",
        "",
        "```bash",
        "python tools/extract_segments.py mm2          # ghidra/*.bin + manifest",
        "python tools/extract_embedded_strings.py mm2",
        "python tools/mm2_codec.py str-decode str.dat EXTRACTED/docs/11-str-decoded.txt",
        "python tools/decode_event.py event.dat",
        "```",
        "",
    ])
    path.write_text("\n".join(lines), encoding="utf-8")


def extract(executable: Path, manifest: Path, asm: Path,
            str_path: Path | None, str_decoded: Path | None,
            event_path: Path | None, min_len: int = 4) -> tuple[list[ExtractedString], dict]:
    mm2 = executable.read_bytes()
    segments = load_manifest(manifest)

    all_strings: list[ExtractedString] = []
    for seg in segments:
        chunk = mm2[seg.file_offset : seg.file_offset + seg.size]
        all_strings.extend(scan_ascii(chunk, seg.load_addr, seg.file_offset, seg, min_len))
        # str.dat uses (byte+0x1C) codec; spot-check showed no encoded blobs in mm2.

    pea_map = parse_pea_refs(asm)
    all_strings = dedupe(all_strings)
    attach_pea_refs(all_strings, pea_map)
    all_strings = filter_quality(all_strings)

    str_blocks = load_str_dat_blocks(str_path, str_decoded)
    event_strings = load_event_strings(event_path) if event_path else []
    cross_reference(all_strings, str_blocks, event_strings)

    pea_attached = sum(1 for s in all_strings if s.pea_refs)
    meta = {
        "source": str(executable.resolve()),
        "size": len(mm2),
        "total_raw": len(all_strings),
        "total_filtered": len(all_strings),
        "pea_refs": pea_attached,
        "str_dat_blocks": len(str_blocks),
        "event_dat_strings": len(event_strings),
    }
    return all_strings, meta


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Extract embedded strings from MM2 executable")
    ap.add_argument("executable", type=Path, nargs="?", default=ROOT / "mm2")
    ap.add_argument("-o", "--output", type=Path, default=ROOT / "EXTRACTED" / "embedded_strings.json")
    ap.add_argument("--txt", type=Path, default=ROOT / "EXTRACTED" / "embedded_strings.txt")
    ap.add_argument("--doc", type=Path, default=ROOT / "EXTRACTED" / "docs" / "29-embedded-exe-strings.md")
    ap.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    ap.add_argument("--asm", type=Path, default=DEFAULT_ASM)
    ap.add_argument("--str-dat", type=Path, default=ROOT / "str.dat")
    ap.add_argument("--str-decoded", type=Path, default=DEFAULT_STR_DECODED)
    ap.add_argument("--event-dat", type=Path, default=ROOT / "event.dat")
    ap.add_argument("--min-len", type=int, default=4)
    args = ap.parse_args(argv)

    if not args.executable.is_file():
        print(f"Missing executable: {args.executable}", file=sys.stderr)
        return 1
    if not args.manifest.is_file():
        print(f"Missing manifest (run extract_segments.py first): {args.manifest}", file=sys.stderr)
        return 1

    strings, meta = extract(
        args.executable,
        args.manifest,
        args.asm,
        args.str_dat if args.str_dat.is_file() else None,
        args.str_decoded if args.str_decoded.is_file() else None,
        args.event_dat if args.event_dat.is_file() else None,
        args.min_len,
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_json(args.output, strings, meta)
    if args.txt:
        write_txt(args.txt, strings, meta)
    if args.doc:
        args.doc.parent.mkdir(parents=True, exist_ok=True)
        str_blocks = load_str_dat_blocks(
            args.str_dat if args.str_dat.is_file() else None,
            args.str_decoded if args.str_decoded.is_file() else None,
        )
        event_strings = load_event_strings(args.event_dat) if args.event_dat.is_file() else []
        write_doc(args.doc, strings, meta, str_blocks, event_strings)

    print(f"Wrote {len(strings)} strings -> {args.output}")
    if args.txt:
        print(f"Wrote text dump -> {args.txt}")
    if args.doc:
        print(f"Wrote doc -> {args.doc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

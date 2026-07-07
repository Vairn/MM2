#!/usr/bin/env python3
"""Decode + classify every MM2 Genesis LZ resource table and render candidates.

Reads the table list from ``EXTRACTED/genesis/analysis/lz_scan_30000.json``,
decodes each via the ASM-exact codec (``genesis_lz_decompress``), classifies it
(palette / tileset / data), pairs tilesets with nearby CRAM palettes, and
renders coloured 4bpp tile sheets into ``EXTRACTED/genesis/gfx/catalog/``.

Outputs:
  - EXTRACTED/genesis/analysis/tile_catalog.json  (per-table classification)
  - EXTRACTED/genesis/gfx/catalog/<off>.png       (per-table coloured render)
  - EXTRACTED/genesis/gfx/catalog/_contact.png    (index contact sheet)

Nothing here guesses codec parameters; the codec is the confirmed Okumura LZSS.
"""

from __future__ import annotations

import importlib.util
import json
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw

ROM = Path("EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen")
SCAN = Path("EXTRACTED/genesis/analysis/lz_scan_30000.json")
OUT_DIR = Path("EXTRACTED/genesis/gfx/catalog")
CATALOG = Path("EXTRACTED/genesis/analysis/tile_catalog.json")
BOOT_PAL_OFF = 0xB12  # boot CRAM palette table (< scan range)

_spec = importlib.util.spec_from_file_location(
    "genesis_lz_decompress", Path(__file__).with_name("genesis_lz_decompress.py")
)
_lz = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_lz)

# Bits that must be zero in a valid 9-bit BGR555 CRAM word (0000 BBB0 GGG0 RRR0).
CRAM_ZERO_MASK = 0xF111


def cram_to_rgb(word: int) -> tuple[int, int, int]:
    r = (word >> 1) & 0x7
    g = (word >> 5) & 0x7
    b = (word >> 9) & 0x7
    scale = lambda v: (v * 255) // 7
    return scale(r), scale(g), scale(b)


def palette_validity(blob: bytes) -> float:
    """Fraction of 16-bit words that are structurally valid CRAM colours."""
    n = len(blob) // 2
    if n == 0:
        return 0.0
    ok = 0
    for i in range(n):
        w = struct.unpack_from(">H", blob, i * 2)[0]
        if (w & CRAM_ZERO_MASK) == 0:
            ok += 1
    return ok / n


def decode_palettes(blob: bytes) -> list[list[tuple[int, int, int]]]:
    words = [struct.unpack_from(">H", blob, i)[0] for i in range(0, len(blob) & ~1, 2)]
    colors = [cram_to_rgb(w) for w in words]
    pals = [colors[i : i + 16] for i in range(0, len(colors), 16)]
    if pals and len(pals[-1]) < 16:
        pals[-1] += [(0, 0, 0)] * (16 - len(pals[-1]))
    return pals


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = [0] * 256
    for b in data:
        counts[b] += 1
    n = len(data)
    h = 0.0
    for c in counts:
        if c:
            p = c / n
            h -= p * math.log2(p)
    return h


def render_tiles(blob: bytes, palette, cols: int = 16, bg=(40, 40, 56)) -> Image.Image:
    n_tiles = len(blob) // 32
    if n_tiles == 0:
        return Image.new("RGB", (8, 8), bg)
    rows = (n_tiles + cols - 1) // cols
    img = Image.new("RGB", (cols * 8, rows * 8), bg)
    px = img.load()
    pal = list(palette) + [(0, 0, 0)] * (16 - len(palette))
    for t in range(n_tiles):
        base = t * 32
        tx = (t % cols) * 8
        ty = (t // cols) * 8
        for row in range(8):
            for bcol in range(4):
                byte = blob[base + row * 4 + bcol]
                px[tx + bcol * 2, ty + row] = pal[byte >> 4]
                px[tx + bcol * 2 + 1, ty + row] = pal[byte & 0x0F]
    return img


def printable_fraction(blob: bytes) -> float:
    if not blob:
        return 0.0
    return sum(1 for b in blob if 32 <= b < 127) / len(blob)


def hibyte_zero_fraction(blob: bytes) -> float:
    """Fraction of 16-bit words whose high byte is zero (tilemap index signal)."""
    n = len(blob) // 2
    if n == 0:
        return 0.0
    return sum(1 for i in range(0, n * 2, 2) if blob[i] == 0) / n


def classify(off: int, blob: bytes, pal_val: float) -> str:
    """Content class from structure. Honest, ASM/data-shape-driven heuristics.

    - palette: >=97% valid CRAM words, small blob.
    - text: length-prefixed ASCII strings (confirmed for the 0x80000+ pool).
    - chr: dense 4bpp pixel banks (low high-byte-zero fraction).
    - tilemap: 16-bit tile-index arrays (high byte ~always zero).
    """
    size = len(blob)
    if pal_val >= 0.97 and size <= 512:
        return "palette"
    high = sum(1 for b in blob if b >= 128) / max(1, size)
    # The confirmed ASCII string pool only lives at >= 0x80000 (spell/item/town
    # names, dialog). Below that is the graphics pool: CHR pixel banks vs. 16-bit
    # tilemap index arrays. Keeping the split region-aware avoids misreading a
    # low-palette-index font as "text".
    # Text pool is confirmed at 0x804D2..0x910E8; the trailing group at >=0x93000
    # (framed by the two palette tables) is graphics again.
    if 0x80000 <= off < 0x93000:
        if printable_fraction(blob) >= 0.85 and high < 0.02:
            return "text"
        return "data"
    if hibyte_zero_fraction(blob) > 0.55:
        return "tilemap"
    return "chr"


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    rom = ROM.read_bytes()
    scan = json.loads(SCAN.read_text())
    tables = scan["tables"]

    # Boot palette (reference).
    _, _, boot_blob = _lz.read_resource(rom, BOOT_PAL_OFF)
    boot_pals = decode_palettes(boot_blob)

    # Pass 1: decode + classify, collect palette tables.
    entries = []
    palette_tables = []  # (offset, [palettes])
    for t in tables:
        off = t["offset"]
        try:
            _, out_size, blob = _lz.read_resource(rom, off)
        except Exception as exc:  # noqa: BLE001
            entries.append({**t, "error": str(exc)})
            continue
        pal_val = palette_validity(blob)
        cls = classify(off, blob, pal_val)
        e = {
            "offset": off,
            "offset_hex": t["offset_hex"],
            "compressed_len": t["compressed_len"],
            "out_size": len(blob),
            "tiles": len(blob) // 32,
            "size_mod32": len(blob) % 32,
            "dec_entropy": round(entropy(blob), 3),
            "palette_validity": round(pal_val, 3),
            "printable_frac": round(printable_fraction(blob), 3),
            "hibyte0_frac": round(hibyte_zero_fraction(blob), 3),
            "class": cls,
        }
        if cls == "chr" and len(blob) // 32 == 256:
            e["note"] = "256 tiles — likely font/charset"
        if cls == "palette":
            pals = decode_palettes(blob)
            palette_tables.append((off, pals))
            e["palettes"] = len(pals)
        entries.append(e)

    # Assemble a working palette set: boot palettes + all detected palette tables.
    all_pals = list(boot_pals)
    for off, pals in palette_tables:
        all_pals.extend(pals)

    def nearest_palette(off: int):
        """Nearest palette table by ROM offset; fall back to boot pal 0."""
        best = None
        best_d = 1 << 30
        for poff, pals in palette_tables:
            d = abs(poff - off)
            if d < best_d:
                best_d = d
                best = (poff, pals)
        if best is None:
            return BOOT_PAL_OFF, boot_pals
        return best

    # Pass 2: render tilesets. Use the nearest palette table's sub-palettes plus
    # boot palettes, tile them side by side so the user can eyeball which fits.
    rendered = []
    for e in entries:
        if e.get("class") != "chr":
            continue
        off = e["offset"]
        _, _, blob = _lz.read_resource(rom, off)
        poff, pals = nearest_palette(off)
        # Candidate palettes: nearest table's palettes (up to 4) + boot pal 0.
        cand = [(f"{poff:X}.{i}", p) for i, p in enumerate(pals[:4])]
        cand.append(("boot.0", boot_pals[0] if boot_pals else [(0, 0, 0)] * 16))
        # Stack renders vertically with a thin separator.
        renders = [(lbl, render_tiles(blob, pal, cols=32)) for lbl, pal in cand]
        w = max(r.width for _, r in renders)
        gap = 3
        total_h = sum(r.height for _, r in renders) + gap * (len(renders) - 1)
        combo = Image.new("RGB", (w, total_h), (12, 12, 16))
        y = 0
        for _, r in renders:
            combo.paste(r, (0, y))
            y += r.height + gap
        scale = 3
        combo = combo.resize((combo.width * scale, combo.height * scale), Image.NEAREST)
        path = OUT_DIR / f"{off:06X}.png"
        combo.save(path)
        e["paired_palette"] = f"0x{poff:X}"
        e["png"] = str(path).replace("\\", "/")
        rendered.append(e)

    # Render palette swatches.
    for off, pals in palette_tables:
        sw = Image.new("RGB", (16 * 18, len(pals) * 18), (0, 0, 0))
        d = ImageDraw.Draw(sw)
        for pi, pal in enumerate(pals):
            for ci, col in enumerate(pal):
                d.rectangle([ci * 18, pi * 18, ci * 18 + 17, pi * 18 + 17], fill=col)
        sw.save(OUT_DIR / f"pal_{off:06X}.png")

    # Contact sheet: one thumbnail row per tileset (first palette candidate).
    thumbs = []
    for e in rendered:
        off = e["offset"]
        _, _, blob = _lz.read_resource(rom, off)
        poff, pals = nearest_palette(off)
        pal = pals[0] if pals else (boot_pals[0] if boot_pals else [(0, 0, 0)] * 16)
        thumbs.append((off, render_tiles(blob, pal, cols=32)))

    if thumbs:
        pad = 22
        cw = max(t.width for _, t in thumbs)
        total = sum(t.height + pad for _, t in thumbs)
        sheet = Image.new("RGB", (cw + 8, total + 8), (16, 16, 20))
        d = ImageDraw.Draw(sheet)
        y = 4
        for off, t in thumbs:
            d.text((4, y), f"0x{off:X}  {t.width//8}x{t.height//8} tiles", fill=(200, 200, 120))
            y += 12
            sheet.paste(t, (4, y))
            y += t.height + pad - 12
        sheet.save(OUT_DIR / "_contact.png")

    summary = {
        "rom": str(ROM).replace("\\", "/"),
        "total_tables": len(entries),
        "counts": {
            k: sum(1 for e in entries if e.get("class") == k)
            for k in ("palette", "chr", "tilemap", "text", "data")
        },
        "palette_tables": [f"0x{o:X}" for o, _ in palette_tables],
        "tables": entries,
    }
    CATALOG.write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary["counts"], indent=2))
    print("palette tables:", summary["palette_tables"])
    print("rendered tilesets:", len(rendered))
    print("catalog:", CATALOG)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

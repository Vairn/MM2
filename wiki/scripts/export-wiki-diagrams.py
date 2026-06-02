#!/usr/bin/env python3
"""Generate the few PNG diagrams Mermaid can't do well (byte strips, size chart).

Flowcharts / relationship graphs live as inline Mermaid in wiki_enrichments.py —
GitHub Wiki renders them natively, so we don't hand-draw those anymore.
"""
from __future__ import annotations

from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    Image = None  # type: ignore[misc, assignment]

OUT_DIR = Path(__file__).resolve().parents[1] / "gh-wiki" / "images" / "diagrams"

# MM2 wiki palette
BG = (20, 6, 6)
CELL = (60, 18, 18)
CELL_ALT = (78, 24, 24)
PAD_CELL = (40, 14, 14)
BORDER = (150, 60, 60)
TEXT = (245, 235, 228)
MUTED = (185, 150, 142)
ACCENT = (224, 96, 84)
HILITE = (255, 196, 110)
GRID = (44, 18, 18)


def _font(size: int, bold: bool = False, mono: bool = False):
    paths = []
    if mono:
        paths = ["C:/Windows/Fonts/consolab.ttf" if bold else "C:/Windows/Fonts/consola.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"]
    else:
        paths = ["C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold
                 else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"]
    for p in paths:
        if p and Path(p).is_file():
            try:
                return ImageFont.truetype(p, size)
            except OSError:
                pass
    return ImageFont.load_default()


def _text_w(draw, text, font) -> int:
    return int(draw.textlength(text, font=font))


def _centered(draw, cx, y, text, font, fill):
    draw.text((cx - _text_w(draw, text, font) / 2, y), text, fill=fill, font=font)


def _title(draw, w, title, subtitle=""):
    draw.text((28, 20), title, fill=TEXT, font=_font(22, bold=True))
    if subtitle:
        draw.text((30, 52), subtitle, fill=MUTED, font=_font(13))


def diagram_record_strip(
    name: str,
    title: str,
    subtitle: str,
    fields: list[tuple[int, str, bool]],
    *,
    unit: int = 30,
) -> None:
    """Proportional byte-strip with numbered cells + a legend table below.

    fields: list of (size_in_bytes, label, is_padding).
    """
    total_bytes = sum(f[0] for f in fields)
    strip_w = total_bytes * unit
    left = 30
    top = 96
    cell_h = 66

    legend_rows = len(fields)
    h = top + cell_h + 40 + legend_rows * 22 + 30
    w = max(strip_w + left * 2, 560)

    img = Image.new("RGB", (w, h), BG)
    draw = ImageDraw.Draw(img)
    _title(draw, w, title, subtitle)

    f_idx = _font(15, bold=True)
    f_off = _font(11, mono=True)
    f_lbl = _font(12, bold=True)

    # byte ruler above strip
    for b in range(total_bytes + 1):
        x = left + b * unit
        draw.line([(x, top - 6), (x, top)], fill=GRID, width=1)

    x = left
    off = 0
    for i, (size, label, is_pad) in enumerate(fields):
        cw = size * unit
        fill = PAD_CELL if is_pad else (CELL if i % 2 == 0 else CELL_ALT)
        draw.rectangle((x, top, x + cw, top + cell_h), fill=fill, outline=BORDER, width=2)
        cx = x + cw / 2
        # Only label cells wide enough to avoid overlap; the legend carries the rest.
        if cw >= 16:
            _centered(draw, cx, top + 10, str(i + 1), f_idx, HILITE if not is_pad else MUTED)
        if cw >= 40:
            _centered(draw, cx, top + 34, f"0x{off:02X}", f_off, MUTED)
            _centered(draw, cx, top + 48, f"{size}B", f_off, ACCENT)
        x += cw
        off += size

    # legend
    ly = top + cell_h + 30
    draw.text((left, ly - 24), "Fields", fill=HILITE, font=_font(14, bold=True))
    off = 0
    for i, (size, label, is_pad) in enumerate(fields):
        c = MUTED if is_pad else TEXT
        draw.text((left, ly), f"{i + 1}.", fill=HILITE if not is_pad else MUTED, font=f_lbl)
        draw.text((left + 26, ly), f"0x{off:02X}", fill=ACCENT, font=f_off)
        draw.text((left + 78, ly), f"{size}B", fill=MUTED, font=f_off)
        draw.text((left + 122, ly), label, fill=c, font=_font(12))
        ly += 22
        off += size

    img.save(OUT_DIR / name)


def diagram_dat_inventory() -> None:
    """Flat proportional file-size bars (no pills)."""
    files = [
        ("items.dat", 5120, "256 \u00d7 20 B \u2014 weapons, armor, use spells"),
        ("monsters.dat", 6656, "256 \u00d7 26 B \u2014 stats, attacks, sprite id"),
        ("roster.dat", 8320, "64 \u00d7 130 B \u2014 party characters"),
        ("map.dat", 30720, "60 \u00d7 512 B \u2014 visual + collision pages"),
        ("event.dat", 95687, "71 locations \u2014 tile scripts + string VM"),
        ("attrib.dat", 3840, "per-screen env, neighbours, roof bits"),
        ("spells.dat", 256, "96 \u00d7 2 B \u2014 cast flags, SP/gem cost"),
        ("str.dat", 7808, "UI / message strings"),
    ]
    w, h = 760, 420
    img = Image.new("RGB", (w, h), BG)
    draw = ImageDraw.Draw(img)
    _title(draw, w, ".dat File Inventory",
           "All multibyte fields little-endian on disk unless noted")

    f_name = _font(14, bold=True, mono=True)
    f_meta = _font(11)
    f_size = _font(11, mono=True)
    bar_x = 170
    bar_max = 470
    max_size = max(s for _, s, _ in files)
    y = 86
    row_h = 40
    for name, nbytes, desc in files:
        bw = max(6, int(bar_max * (nbytes / max_size)))
        draw.text((28, y + 4), name, fill=HILITE, font=f_name)
        draw.rectangle((bar_x, y, bar_x + bw, y + 18), fill=ACCENT, outline=BORDER, width=1)
        size_label = f"{nbytes:,} B" if nbytes < 1024 else f"{nbytes / 1024:.1f} KiB"
        draw.text((bar_x + bw + 8, y + 2), size_label, fill=TEXT, font=f_size)
        draw.text((28, y + 21), desc, fill=MUTED, font=f_meta)
        y += row_h

    img.save(OUT_DIR / "dat-inventory.png")


def export_all_diagrams(out: Path | None = None) -> Path:
    global OUT_DIR
    if out is not None:
        OUT_DIR = out
    if Image is None:
        print("  skip diagrams: Pillow not installed")
        return OUT_DIR
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    diagram_dat_inventory()
    diagram_record_strip(
        "items-record.png",
        "items.dat record \u2014 20 bytes",
        "256 records \u00b7 see items.dat Format for field semantics",
        [
            (12, "name \u2014 ASCII, space-padded", False),
            (1, "pad (editor writes 0)", True),
            (1, "forbidden-class bitmask", False),
            (1, "equip bonus (type<<4 | amount)", False),
            (1, "use power \u2014 flat spell index", False),
            (1, "damage / AC value", False),
            (1, "pad / unused", True),
            (2, "gold (uint16, little-endian)", False),
        ],
    )
    diagram_record_strip(
        "roster-record.png",
        "roster.dat record \u2014 130 bytes (key fields)",
        "64 characters \u00b7 see roster.dat Format",
        [
            (11, "name", False),
            (1, "town / party flag", False),
            (1, "sex", False),
            (1, "alignment", False),
            (1, "race", False),
            (1, "class", False),
            (6, "core stats", False),
            (18, "\u2026 status / endurance \u2026", True),
            (18, "equipped (6\u00d73)", False),
            (18, "backpack (6\u00d73)", False),
            (12, "spellbook", False),
            (10, "SP / HP / gems", False),
            (4, "XP (dword)", False),
            (4, "gold (dword)", False),
            (24, "base stats / level / tail", True),
        ],
        unit=7,
    )
    n = len(list(OUT_DIR.glob("*.png")))
    print(f"  diagrams -> {OUT_DIR} ({n} PNGs: byte strips + size chart)")
    return OUT_DIR


def main() -> int:
    export_all_diagrams()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

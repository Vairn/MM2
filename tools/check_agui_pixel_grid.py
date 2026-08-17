#!/usr/bin/env python3
"""Quantize + validate AI-generated Agui item art before it reaches ingest/pack.

Image models emit a fixed canvas (Gemini: 1024x1024) and cannot be made to
produce exact 8x pixel blocks, so asking them for "96x96, solid 8x8 blocks" is
a losing game. The workable contract is: the model supplies the *artwork* as a
12x12 grid of large flat blocks at whatever size it likes, and this module
recovers the exact 12x12 logical image from it.

    quantize_to_logical()  1024x1024 (or anything) -> exact 12x12, palette-snapped
    check_image()          validate a file; strict when it is already 12x/96x/192x,
                           tolerant + purity-scored when it is an arbitrary size

`purity` is the fraction of each logical cell that agrees with that cell's
winning colour, averaged over filled cells. 1.00 means perfectly flat blocks;
below ~0.55 the model drew detail or texture that will not survive the
reduction to 12x12, which is the signal to regenerate.

Usage
  python tools/check_agui_pixel_grid.py exports/                     # report
  python tools/check_agui_pixel_grid.py exports/ -v                  # per-file detail
  python tools/check_agui_pixel_grid.py exports/raw --quantize-dir exports/
  python tools/check_agui_pixel_grid.py exports/ --json report.json --bad-list bad.txt
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence

try:
    from PIL import Image, ImageChops
except ImportError:  # pragma: no cover
    print("pip install pillow", file=sys.stderr)
    raise

ROOT = Path(__file__).resolve().parents[1]
AGUI = ROOT / "game" / "data" / "ui" / "agui"
PALETTE_PATH = AGUI / "palette.json"

LOGICAL = 12            # icons are 12x12 logical pixels
ALPHA_CUTOFF = 16       # below this alpha a pixel counts as transparent
INNER = 0.62            # sample only the middle of each cell, dodging soft edges

DEFAULT_PALETTE_TOL = 32     # max distance to the nearest pen
DEFAULT_BLOCK_TOL = 10       # max RGB spread inside an exact block
DEFAULT_PURITY = 0.55        # min cell agreement for arbitrary-size sources
DEFAULT_BG_TOL = 30          # corner-key background match tolerance


# ---------------------------------------------------------------------------
# palette
# ---------------------------------------------------------------------------


def load_palette(path: Path = PALETTE_PATH) -> list[tuple[int, int, int]]:
    if not path.is_file():
        raise SystemExit(f"palette not found: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    return [tuple(int(c) for c in rgb) for rgb in data["colors"]]


def nearest_pen(rgb: tuple[int, int, int],
                palette: Sequence[tuple[int, int, int]]) -> tuple[int, float]:
    best_i, best_d = 0, float("inf")
    for i, p in enumerate(palette):
        d = (rgb[0] - p[0]) ** 2 + (rgb[1] - p[1]) ** 2 + (rgb[2] - p[2]) ** 2
        if d < best_d:
            best_i, best_d = i, d
    return best_i, best_d ** 0.5


# ---------------------------------------------------------------------------
# source conditioning
# ---------------------------------------------------------------------------


def strip_background(img: Image.Image, tolerance: int = DEFAULT_BG_TOL) -> tuple[Image.Image, bool]:
    """Key out a flat opaque background, which models emit despite instructions.

    Only fires when all four corners agree, so real art that reaches the edge
    is left alone. Returns (image, did_strip).
    """
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    corners = [px[0, 0], px[w - 1, 0], px[0, h - 1], px[w - 1, h - 1]]
    if any(c[3] < ALPHA_CUTOFF for c in corners):
        return img, False           # already has transparency where it matters
    ref = corners[0]
    for c in corners[1:]:
        if max(abs(c[i] - ref[i]) for i in range(3)) > tolerance:
            return img, False       # corners disagree: not a flat backdrop

    # Per-channel distance from the key colour, entirely inside PIL — the
    # equivalent Python pixel loop costs ~1s on a 1024x1024 model output.
    solid = Image.new("RGB", (w, h), (ref[0], ref[1], ref[2]))
    delta = ImageChops.difference(img.convert("RGB"), solid).split()
    spread = ImageChops.lighter(ImageChops.lighter(delta[0], delta[1]), delta[2])
    keep = spread.point(lambda v: 0 if v <= tolerance else 255)

    out = img.copy()
    out.putalpha(ImageChops.multiply(img.getchannel("A"), keep))
    return out, True


def content_bbox(img: Image.Image) -> tuple[int, int, int, int] | None:
    """Opaque bounding box of the artwork, or None if the image is empty."""
    return img.getbbox()


# ---------------------------------------------------------------------------
# cell sampling
# ---------------------------------------------------------------------------


def _downsample_alpha_correct(img: Image.Image, n: int) -> Image.Image:
    """Area-average `img` down to n x n without letting transparent pixels bleed.

    PIL's resize averages the RGB of fully transparent pixels along with the
    visible ones, which drags every edge toward black. Premultiplying by alpha
    first, resizing, then dividing back out is the correct reduction — and it
    is what makes a 1024x1024 model output collapse to clean 12x12 colours.
    """
    r, g, b, a = img.split()
    pr = ImageChops.multiply(r, a)
    pg = ImageChops.multiply(g, a)
    pb = ImageChops.multiply(b, a)
    small = [ch.resize((n, n), Image.BOX) for ch in (pr, pg, pb, a)]
    return Image.merge("RGBA", small)


def _unpremultiply(px: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    r, g, b, a = px
    if a == 0:
        return (0, 0, 0, 0)
    scale = 255.0 / a
    return (min(255, int(r * scale + 0.5)),
            min(255, int(g * scale + 0.5)),
            min(255, int(b * scale + 0.5)), a)


@dataclass
class QuantStats:
    purity: float = 0.0             # 1.0 = flat blocks, low = mushy/detailed
    filled: int = 0                 # filled logical cells out of 144
    pens: int = 0
    bg_stripped: bool = False
    cropped: bool = False
    source_size: tuple[int, int] = (0, 0)
    exact_grid: bool = False
    mean_pen_dist: float = 0.0      # how far source colours sat from the palette
    content_cells: int = 10

    def as_dict(self) -> dict:
        return {
            "purity": round(self.purity, 3), "filled": self.filled, "pens": self.pens,
            "bg_stripped": self.bg_stripped, "cropped": self.cropped,
            "source_size": list(self.source_size), "exact_grid": self.exact_grid,
            "mean_pen_dist": round(self.mean_pen_dist, 1),
            "content_cells": self.content_cells,
        }


def quantize_to_logical(
    img: Image.Image,
    palette: Sequence[tuple[int, int, int]],
    *,
    strip_bg: bool = True,
    autocrop: bool = True,
    content_cells: int = 10,
    alpha_cut: int = 128,
) -> tuple[Image.Image, QuantStats]:
    """Reduce any source image to an exact 12x12 palette-snapped RGBA icon.

    Steps: key out a flat background, crop to the artwork, pad to square so the
    aspect survives, area-average down to `content_cells` (10 by default, the
    spec's "~10x10 of content with a 1 logical-pixel margin"), then snap every
    cell to its nearest palette pen and centre it in the 12x12 canvas.

    Deliberately does NOT try to recover the model's own block grid. Real icon
    art does not fill its bounding box uniformly, so grid detection locks onto
    the wrong division and smears edges; a straight area reduction is stable
    and never worse than a misaligned fit.
    """
    src = img.convert("RGBA")
    stats = QuantStats(source_size=src.size, content_cells=content_cells)
    stats.exact_grid = src.width == src.height and src.width % LOGICAL == 0

    if strip_bg:
        src, stats.bg_stripped = strip_background(src)

    if autocrop:
        box = src.getbbox()
        if box:
            stats.cropped = box != (0, 0, src.width, src.height)
            src = src.crop(box)

    if src.width == 0 or src.height == 0:
        return Image.new("RGBA", (LOGICAL, LOGICAL), (0, 0, 0, 0)), stats

    # pad to square so a tall blade is not squashed into a wide one
    side = max(src.width, src.height)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(src, ((side - src.width) // 2, (side - src.height) // 2))

    n = max(1, min(LOGICAL, content_cells))
    small = _downsample_alpha_correct(square, n)
    sp = small.load()

    out = Image.new("RGBA", (LOGICAL, LOGICAL), (0, 0, 0, 0))
    op = out.load()
    ox = (LOGICAL - n) // 2
    oy = (LOGICAL - n) // 2

    dist_sum = 0.0
    pens_used: set[int] = set()
    for y in range(n):
        for x in range(n):
            r, g, b, a = _unpremultiply(sp[x, y])
            if a < alpha_cut:
                continue
            pen, dist = nearest_pen((r, g, b), palette)
            if pen == 0:                       # pen 0 is the transparent pen
                continue
            pr, pg, pb = palette[pen]
            op[x + ox, y + oy] = (pr, pg, pb, 255)
            pens_used.add(pen)
            stats.filled += 1
            dist_sum += dist

    stats.pens = len(pens_used)
    stats.mean_pen_dist = dist_sum / stats.filled if stats.filled else 0.0
    # purity: 1.0 when every averaged colour landed on a pen exactly, falling
    # off as the source turns out to have been textured/anti-aliased mush.
    stats.purity = max(0.0, 1.0 - stats.mean_pen_dist / 110.0)
    return out, stats


def quantize_file(
    path: Path,
    palette: Sequence[tuple[int, int, int]],
    dest: Path,
    *,
    emit: int = 96,
    **kw,
) -> QuantStats:
    """Quantize `path` and write it to `dest` at `emit` pixels square."""
    native, stats = quantize_to_logical(Image.open(path), palette, **kw)
    dest.parent.mkdir(parents=True, exist_ok=True)
    scale = max(1, emit // LOGICAL)
    native.resize((LOGICAL * scale, LOGICAL * scale), Image.NEAREST).save(dest)
    return stats


# ---------------------------------------------------------------------------
# validation
# ---------------------------------------------------------------------------


@dataclass
class Report:
    path: Path
    ok: bool = True
    scale: int = 0
    size: tuple[int, int] = (0, 0)
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    soft_blocks: int = 0
    off_palette_blocks: int = 0
    worst_palette_dist: float = 0.0
    distinct_pens: int = 0
    filled_blocks: int = 0
    margin_violations: int = 0
    purity: float = 1.0
    tolerant: bool = False          # source was not an exact multiple of 12

    def as_dict(self) -> dict:
        return {
            "path": str(self.path), "ok": self.ok, "size": list(self.size),
            "scale": self.scale, "tolerant": self.tolerant,
            "purity": round(self.purity, 3),
            "soft_blocks": self.soft_blocks,
            "off_palette_blocks": self.off_palette_blocks,
            "worst_palette_dist": round(self.worst_palette_dist, 1),
            "distinct_pens": self.distinct_pens, "filled_blocks": self.filled_blocks,
            "margin_violations": self.margin_violations,
            "errors": self.errors, "warnings": self.warnings,
        }

    def line(self) -> str:
        mark = "PASS" if self.ok else "FAIL"
        if self.tolerant:
            detail = (f"{self.size[0]}x{self.size[1]} purity={self.purity:.2f} "
                      f"pens={self.distinct_pens} fill={self.filled_blocks}")
        else:
            detail = (f"{self.size[0]}x{self.size[1]} scale={self.scale or '?'} "
                      f"soft={self.soft_blocks} off={self.off_palette_blocks} "
                      f"pens={self.distinct_pens} fill={self.filled_blocks}")
        msg = "; ".join(self.errors or self.warnings)
        return f"[{mark}] {self.path.name:<14} {detail}" + (f"  {msg}" if msg else "")


def check_image(
    path: Path,
    palette: Sequence[tuple[int, int, int]],
    *,
    block_tol: int = DEFAULT_BLOCK_TOL,
    palette_tol: int = DEFAULT_PALETTE_TOL,
    max_soft: int = 0,
    max_off: int = 0,
    max_pens: int = 16,
    min_purity: float = DEFAULT_PURITY,
    require_margin: bool = True,
    fix_dir: Path | None = None,
) -> Report:
    """Validate one image.

    Exact 12x / 96x / 192x sources are held to the strict block contract.
    Anything else (a raw 1024x1024 model output) is scored on how cleanly it
    reduces to 12x12 instead of being failed for its dimensions.
    """
    rep = Report(path=path)
    try:
        img = Image.open(path).convert("RGBA")
    except Exception as exc:  # noqa: BLE001
        rep.ok = False
        rep.errors.append(f"unreadable: {exc}")
        return rep

    w, h = img.size
    rep.size = (w, h)
    exact = w == h and w % LOGICAL == 0 and w >= LOGICAL

    if not exact:
        return _check_tolerant(img, rep, palette, min_purity=min_purity,
                               max_pens=max_pens, require_margin=require_margin,
                               fix_dir=fix_dir)

    rep.scale = w // LOGICAL
    scale = rep.scale
    px = img.load()
    grid: list[list[tuple[int, int, int] | None]] = [[None] * LOGICAL for _ in range(LOGICAL)]
    pens: Counter[int] = Counter()

    for gy in range(LOGICAL):
        for gx in range(LOGICAL):
            samples = [px[x, y]
                       for y in range(gy * scale, (gy + 1) * scale)
                       for x in range(gx * scale, (gx + 1) * scale)]
            opaque = [s for s in samples if s[3] >= ALPHA_CUTOFF]
            if opaque and len(opaque) != len(samples):
                rep.soft_blocks += 1
            if not opaque:
                continue
            rs = [s[0] for s in opaque]
            gs = [s[1] for s in opaque]
            bs = [s[2] for s in opaque]
            if max(max(rs) - min(rs), max(gs) - min(gs), max(bs) - min(bs)) > block_tol:
                rep.soft_blocks += 1
            avg = (sum(rs) // len(opaque), sum(gs) // len(opaque), sum(bs) // len(opaque))
            grid[gy][gx] = avg
            rep.filled_blocks += 1
            pen, dist = nearest_pen(avg, palette)
            rep.worst_palette_dist = max(rep.worst_palette_dist, dist)
            if dist > palette_tol:
                rep.off_palette_blocks += 1
            pens[pen] += 1

    rep.distinct_pens = len(pens)
    if require_margin:
        for i in range(LOGICAL):
            for gy, gx in ((0, i), (LOGICAL - 1, i), (i, 0), (i, LOGICAL - 1)):
                if grid[gy][gx] is not None:
                    rep.margin_violations += 1

    if rep.filled_blocks == 0:
        rep.ok = False
        rep.errors.append("image is fully transparent")
    elif rep.filled_blocks < 6:
        rep.ok = False
        rep.errors.append(f"only {rep.filled_blocks} filled blocks — content missing")
    if rep.soft_blocks > max_soft:
        rep.ok = False
        rep.errors.append(f"{rep.soft_blocks} soft/anti-aliased blocks "
                          f"(not a clean {LOGICAL}x{LOGICAL} grid)")
    if rep.off_palette_blocks > max_off:
        rep.ok = False
        rep.errors.append(f"{rep.off_palette_blocks} off-palette blocks "
                          f"(worst dist {rep.worst_palette_dist:.0f})")
    if rep.distinct_pens > max_pens:
        rep.warnings.append(f"{rep.distinct_pens} distinct pens — busy for 12x12")
    if rep.margin_violations:
        rep.warnings.append(f"{rep.margin_violations} border blocks used (no 1px margin)")

    if fix_dir is not None:
        _write_snapped(grid, palette, fix_dir / path.name, scale=max(scale, 8))
    return rep


def _check_tolerant(
    img: Image.Image,
    rep: Report,
    palette: Sequence[tuple[int, int, int]],
    *,
    min_purity: float,
    max_pens: int,
    require_margin: bool,
    fix_dir: Path | None,
) -> Report:
    rep.tolerant = True
    native, stats = quantize_to_logical(img, palette)
    rep.purity = stats.purity
    rep.filled_blocks = stats.filled
    rep.distinct_pens = stats.pens
    rep.worst_palette_dist = stats.mean_pen_dist

    px = native.load()
    if require_margin:
        for i in range(LOGICAL):
            for gy, gx in ((0, i), (LOGICAL - 1, i), (i, 0), (i, LOGICAL - 1)):
                if px[gx, gy][3] >= ALPHA_CUTOFF:
                    rep.margin_violations += 1

    if stats.filled == 0:
        rep.ok = False
        rep.errors.append("nothing survives reduction to 12x12 (blank or keyed out)")
    elif stats.filled < 6:
        rep.ok = False
        rep.errors.append(f"only {stats.filled} filled cells — content missing")
    elif stats.filled > 132:
        rep.ok = False
        rep.errors.append(f"{stats.filled}/144 cells filled — background not "
                          "transparent, or art fills the whole canvas")
    if stats.purity < min_purity:
        rep.ok = False
        rep.errors.append(f"purity {stats.purity:.2f} < {min_purity:.2f} — too much "
                          "detail/texture to survive 12x12")
    if stats.mean_pen_dist > DEFAULT_PALETTE_TOL:
        rep.warnings.append(f"mean palette distance {stats.mean_pen_dist:.0f} — "
                            "colours drifted off the locked palette")
    if stats.pens > max_pens:
        rep.warnings.append(f"{stats.pens} distinct pens — busy for 12x12")
    if rep.margin_violations:
        rep.warnings.append(f"{rep.margin_violations} border cells used (no 1px margin)")
    if stats.bg_stripped:
        rep.warnings.append("opaque background keyed out")

    if fix_dir is not None:
        fix_dir.mkdir(parents=True, exist_ok=True)
        native.resize((96, 96), Image.NEAREST).save(fix_dir / path_with_png(rep.path))
    return rep


def path_with_png(p: Path) -> str:
    return p.stem + ".png"


def _write_snapped(
    grid: list[list[tuple[int, int, int] | None]],
    palette: Sequence[tuple[int, int, int]],
    dest: Path,
    *,
    scale: int,
) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    native = Image.new("RGBA", (LOGICAL, LOGICAL), (0, 0, 0, 0))
    out = native.load()
    for gy in range(LOGICAL):
        for gx in range(LOGICAL):
            cell = grid[gy][gx]
            if cell is None:
                continue
            pen, _ = nearest_pen(cell, palette)
            if pen == 0:
                continue
            r, g, b = palette[pen]
            out[gx, gy] = (r, g, b, 255)
    native.resize((LOGICAL * scale, LOGICAL * scale), Image.NEAREST).save(dest)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def iter_images(target: Path) -> list[Path]:
    if target.is_file():
        return [target]
    if not target.is_dir():
        raise SystemExit(f"no such path: {target}")
    return sorted(p for p in target.iterdir()
                  if p.suffix.lower() in {".png", ".gif", ".bmp", ".webp", ".jpg", ".jpeg"})


def main(argv: Sequence[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", type=Path, help="image file or folder")
    ap.add_argument("--palette", type=Path, default=PALETTE_PATH)
    ap.add_argument("--quantize-dir", type=Path,
                    help="write exact 12x12-derived PNGs here (ready for ingest)")
    ap.add_argument("--emit", type=int, default=96, choices=[12, 96, 192],
                    help="pixel size for --quantize-dir output (default 96)")
    ap.add_argument("--no-strip-bg", action="store_true", help="keep opaque backgrounds")
    ap.add_argument("--no-autocrop", action="store_true", help="do not re-centre the subject")
    ap.add_argument("--block-tol", type=int, default=DEFAULT_BLOCK_TOL)
    ap.add_argument("--palette-tol", type=int, default=DEFAULT_PALETTE_TOL)
    ap.add_argument("--max-soft", type=int, default=0)
    ap.add_argument("--max-off", type=int, default=0)
    ap.add_argument("--max-pens", type=int, default=16)
    ap.add_argument("--min-purity", type=float, default=DEFAULT_PURITY)
    ap.add_argument("--no-margin-check", action="store_true")
    ap.add_argument("--fix-dir", type=Path, help="write palette-snapped copies here")
    ap.add_argument("--json", type=Path)
    ap.add_argument("--bad-list", type=Path)
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("-q", "--quiet", action="store_true", help="only print failures")
    args = ap.parse_args(argv)

    palette = load_palette(args.palette)
    files = iter_images(args.target)
    if not files:
        print(f"no images under {args.target}", file=sys.stderr)
        return 2

    if args.quantize_dir:
        n = 0
        for path in files:
            stats = quantize_file(path, palette, args.quantize_dir / (path.stem + ".png"),
                                  emit=args.emit,
                                  strip_bg=not args.no_strip_bg,
                                  autocrop=not args.no_autocrop)
            n += 1
            if not args.quiet:
                print(f"  {path.name:<16} {stats.source_size[0]}x{stats.source_size[1]}"
                      f" -> {args.emit}px  purity={stats.purity:.2f}"
                      f" fill={stats.filled} pens={stats.pens}"
                      + ("  bg-keyed" if stats.bg_stripped else "")
                      + ("  cropped" if stats.cropped else ""))
        print(f"\nquantized {n} file(s) -> {args.quantize_dir}")
        files = iter_images(args.quantize_dir)

    reports: list[Report] = []
    for path in files:
        rep = check_image(path, palette,
                          block_tol=args.block_tol, palette_tol=args.palette_tol,
                          max_soft=args.max_soft, max_off=args.max_off,
                          max_pens=args.max_pens, min_purity=args.min_purity,
                          require_margin=not args.no_margin_check,
                          fix_dir=args.fix_dir)
        reports.append(rep)
        if not args.quiet or not rep.ok:
            print(rep.line())
            if args.verbose and rep.warnings and rep.ok:
                for w in rep.warnings:
                    print(f"         warn: {w}")

    bad = [r for r in reports if not r.ok]
    warned = [r for r in reports if r.ok and r.warnings]
    print(f"\n{len(reports) - len(bad)}/{len(reports)} passed"
          f"{f', {len(warned)} with warnings' if warned else ''}")

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps([r.as_dict() for r in reports], indent=2),
                             encoding="utf-8")
        print(f"report -> {args.json}")
    if args.bad_list:
        args.bad_list.parent.mkdir(parents=True, exist_ok=True)
        args.bad_list.write_text("\n".join(r.path.name for r in bad) + ("\n" if bad else ""),
                                 encoding="utf-8")
        print(f"failures -> {args.bad_list}")
    if args.fix_dir:
        print(f"snapped copies -> {args.fix_dir}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())

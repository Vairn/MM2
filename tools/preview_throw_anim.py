#!/usr/bin/env python3
"""throw.32 preview — compositing tuned against WinUAE ScreenToGif refs only.

Usage:
  python tools/preview_throw_anim.py
  python tools/preview_throw_anim.py --ref-dir "path/to/174.png..205.png"
  python tools/tune_throw_gui.py   # interactive tuner vs refs
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import NamedTuple

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("pip install pillow", file=sys.stderr)
    raise

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from render_view_refs import load_frame  # noqa: E402

W, H = 320, 200
BLIT_Y = 16  # orange screen rows 59-86 in refs
TABLEAU_W = 304
TABLEAU_X = 8
ORANGE_ROW = 44
ORANGE_H = 28
ORANGE_Y = BLIT_Y + ORANGE_ROW
SPRITE_H = 72
ORANGE_RGB = (255, 170, 0)

DEFAULT_REF_DIR = Path(
    r"C:\Users\Adam Templeton\AppData\Local\Temp\ScreenToGif\Recording\2026-06-03 21-51-07"
)

class BlitSpec(NamedTuple):
    hand_fi: int
    hand_x: int
    die_fi: int | None = None
    die_x: int | None = None


# Tuned by pixel search vs user ScreenToGif captures (2026-06-03).
REF_BLIT: dict[int, BlitSpec] = {
    # tune_throw_gui.py / tune_throw_bruteforce.py --apply
    174: BlitSpec(0, 7),
    177: BlitSpec(1, 41),
    180: BlitSpec(2, 59),
    183: BlitSpec(2, 59, 3, 144),
    186: BlitSpec(0, 59, 4, 190),
    189: BlitSpec(2, 59, 5, 215),
    192: BlitSpec(2, 59, 6, 242),
    194: BlitSpec(2, 59, 7, 257),
    197: BlitSpec(2, 59, 8, 258),
    199: BlitSpec(2, 59, 9, 236),
    202: BlitSpec(2, 59, 10, 224),
    204: BlitSpec(2, 59, 6, 193),
    205: BlitSpec(2, 59, 6, 193),
}

# 15-step reroll loop (tune_throw_bruteforce.py --apply syncs from REF_BLIT roll keys).
ANIM_SEQUENCE: list[BlitSpec] = [
    # tune_throw_bruteforce.py --apply
    BlitSpec(1, 41),
    BlitSpec(2, 59),
    BlitSpec(2, 59, 3, 144),
    BlitSpec(0, 59, 4, 190),
    BlitSpec(2, 59, 5, 215),
    BlitSpec(2, 59, 6, 242),
    BlitSpec(2, 59, 7, 257),
    BlitSpec(2, 59, 8, 258),
    BlitSpec(2, 59, 9, 236),
    BlitSpec(2, 59, 10, 224),
    BlitSpec(1, 41),
    BlitSpec(2, 59),
    BlitSpec(2, 59, 3, 144),
    BlitSpec(0, 59, 4, 190),
    BlitSpec(2, 59, 5, 215),
]


def load_throw(data_dir: Path) -> list[Image.Image]:
    path = data_dir / "throw.32"
    if not path.is_file():
        raise SystemExit(f"missing {path}")
    frames = [load_frame("throw.32", i, data_dir) for i in range(11)]
    print("throw.32:")
    for i, im in enumerate(frames):
        print(f"  {i}: {im.size[0]}x{im.size[1]}")
    return frames


def canvas() -> Image.Image:
    return Image.new("RGBA", (W, H), (0, 0, 0, 255))


def blit(dst: Image.Image, src: Image.Image, x: int, y: int) -> None:
    if src.mode != "RGBA":
        src = src.convert("RGBA")
    dst.paste(src, (x, y), src)


def paint_full_orange(dst: Image.Image, frames: list[Image.Image]) -> None:
    """Continuous table bar (refs never show a broken orange strip)."""
    band = frames[0].crop((0, ORANGE_ROW, frames[0].width, ORANGE_ROW + ORANGE_H))
    bar = band.resize((TABLEAU_W, ORANGE_H), Image.Resampling.NEAREST)
    blit(dst, bar, TABLEAU_X, ORANGE_Y)


def compose_raw(
    frames: list[Image.Image],
    hand_fi: int,
    hand_x: int,
    die_fi: int | None = None,
    die_x: int | None = None,
) -> Image.Image:
    """Pen 0 = transparent; orange bar under hand (Amiga-style). No RGB masking."""
    dst = canvas()
    paint_full_orange(dst, frames)
    blit(dst, frames[hand_fi], hand_x, BLIT_Y)
    if die_fi is not None and die_x is not None:
        blit(dst, frames[die_fi], die_x, BLIT_Y)
    return dst


def compose_spec(frames: list[Image.Image], spec: BlitSpec) -> Image.Image:
    return compose_raw(frames, spec.hand_fi, spec.hand_x, spec.die_fi, spec.die_x)


def crop_tableau(im: Image.Image) -> Image.Image:
    return im.crop((0, BLIT_Y, W, min(H, BLIT_Y + SPRITE_H + 4)))


def label(im: Image.Image, text: str) -> Image.Image:
    out = im.copy()
    d = ImageDraw.Draw(out)
    try:
        font = ImageFont.truetype("arial.ttf", 10)
    except OSError:
        font = ImageFont.load_default()
    d.rectangle([0, 0, W - 1, 11], fill=(0, 0, 0, 200))
    d.text((4, 1), text, fill=(255, 80, 80))
    return out


def spec_label(spec: BlitSpec) -> str:
    if spec.die_fi is None:
        return f"h{spec.hand_fi}@{spec.hand_x}"
    return f"hand@{spec.hand_x}+d{spec.die_fi}@{spec.die_x}"


def save_strip(images: list[Image.Image], path: Path, cols: int) -> None:
    cols = min(cols, len(images))
    rows = (len(images) + cols - 1) // cols
    cw, ch = images[0].size
    out = Image.new("RGB", (cols * cw, rows * ch), (32, 32, 32))
    for i, im in enumerate(images):
        r, c = divmod(i, cols)
        out.paste(im.convert("RGB"), (c * cw, r * ch))
    path.parent.mkdir(parents=True, exist_ok=True)
    out.save(path)
    print(f"wrote {path}")


def build_refs_validation(frames: list[Image.Image], ref_dir: Path, out: Path) -> None:
    from compare_throw_refs import crop_game_viewport

    rows = []
    for ref_num in sorted(REF_BLIT):
        rp = ref_dir / f"{ref_num}.png"
        if not rp.is_file():
            continue
        spec = REF_BLIT[ref_num]
        prev = compose_spec(frames, spec)
        ref = crop_tableau(crop_game_viewport(Image.open(rp)))
        tab = crop_tableau(prev)
        row_h = max(ref.height, tab.height)
        row = Image.new("RGB", (W * 2, row_h), (0, 0, 0))
        row.paste(tab.convert("RGB"), (0, 0))
        row.paste(ref.convert("RGB"), (W, 0))
        d = ImageDraw.Draw(row)
        d.text((2, 2), spec_label(spec), fill=(255, 80, 80))
        d.text((W + 2, 2), f"ref {ref_num}", fill=(80, 255, 128))
        rows.append(row)

    h = sum(r.height for r in rows)
    sheet = Image.new("RGB", (W * 2, h), (32, 32, 32))
    y = 0
    for r in rows:
        sheet.paste(r, (0, y))
        y += r.height
    sheet.save(out / "refs_validation.png")
    print(f"wrote {out / 'refs_validation.png'}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data-dir", type=Path, default=ROOT)
    ap.add_argument("--out", type=Path, default=ROOT / "game" / "build" / "throw_preview")
    ap.add_argument("--ref-dir", type=Path, default=None)
    ap.add_argument("--open", action="store_true")
    ap.add_argument("--export-steps", action="store_true")
    args = ap.parse_args()

    frames = load_throw(args.data_dir)
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    raw = [
        label(compose_spec(frames, REF_BLIT[n]), f"ref {n}")
        for n in sorted(REF_BLIT)
    ]
    save_strip(raw, out / "00_ref_sequence.png", 4)

    steps = [
        label(compose_spec(frames, ANIM_SEQUENCE[i]), f"step{i+1:02d} {spec_label(ANIM_SEQUENCE[i])}")
        for i in range(len(ANIM_SEQUENCE))
    ]
    save_strip(steps, out / "anim_steps.png", 5)
    if args.export_steps:
        for i, spec in enumerate(ANIM_SEQUENCE, 1):
            compose_spec(frames, spec).save(out / f"step_{i:02d}.png")

    ref_dir = args.ref_dir or DEFAULT_REF_DIR
    if ref_dir.is_dir():
        build_refs_validation(frames, ref_dir, out)

    (out / "README.txt").write_text(
        "Image-tuned compositing (ScreenToGif 2026-06-03).\n"
        "Full orange bar + hand frame at fixed x; die frames blit on top for roll phase.\n"
        "Pen 0 only transparent; orange under hand. Tune: tune_throw_bruteforce.py\n",
        encoding="utf-8",
    )
    print(f"Output: {out}")

    if args.open and sys.platform == "win32":
        import os

        os.startfile(out)  # type: ignore[attr-defined]


if __name__ == "__main__":
    main()

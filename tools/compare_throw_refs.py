#!/usr/bin/env python3
"""Score preview_throw_anim vs WinUAE refs."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from preview_throw_anim import (  # noqa: E402
    DEFAULT_REF_DIR,
    REF_BLIT,
    H,
    W,
    compose_spec,
    crop_tableau,
    load_throw,
    spec_label,
)


def crop_game_viewport(im: Image.Image) -> Image.Image:
    rgb = np.array(im.convert("RGB"))
    r, g, b = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    border = (r > 180) & (g < 80) & (b < 80)
    ys, xs = np.where(~border)
    if ys.size == 0:
        return im.resize((W, H), Image.Resampling.NEAREST)
    inner = im.crop((int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1))
    return inner.resize((W, H), Image.Resampling.NEAREST)


def is_orange(rgb: np.ndarray) -> np.ndarray:
    r, g, b = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    return (r > 200) & (g > 120) & (g < 200) & (b < 80)


def content_mask(rgb: np.ndarray) -> np.ndarray:
    return (rgb.sum(axis=2) > 40) & ~is_orange(rgb)


def mae(a: Image.Image, b: Image.Image, mask: np.ndarray) -> float:
    pa = np.array(a.convert("RGB"), dtype=np.int16)
    pb = np.array(b.convert("RGB"), dtype=np.int16)
    m = mask & ((pa.sum(axis=2) > 40) | (pb.sum(axis=2) > 40))
    if not m.any():
        return 9999.0
    return float(np.abs(pa - pb).mean(axis=2)[m].mean())


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref-dir", type=Path, default=DEFAULT_REF_DIR)
    ap.add_argument("--data-dir", type=Path, default=ROOT)
    args = ap.parse_args()

    frames = load_throw(args.data_dir)
    print("MAE (hand/die pixels, lower=better):")
    for ref_num in sorted(REF_BLIT):
        rp = args.ref_dir / f"{ref_num}.png"
        if not rp.is_file():
            continue
        ref = crop_tableau(crop_game_viewport(Image.open(rp)))
        prev = crop_tableau(compose_spec(frames, REF_BLIT[ref_num]))
        mask = content_mask(np.array(ref.convert("RGB")))
        err = mae(prev, ref, mask)
        print(f"  {ref_num}: {spec_label(REF_BLIT[ref_num])}  mae={err:.1f}")


if __name__ == "__main__":
    main()

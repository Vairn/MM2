#!/usr/bin/env python3
"""Fast interactive throw.32 tuner vs WinUAE refs.

Pen 0 only = transparent (see render_view_refs.load_frame). Paint orange bar
under the hand so pen-0 shading shows orange, not black holes.

Usage:
  python tools/tune_throw_bruteforce.py              # all refs, ~seconds each
  python tools/tune_throw_bruteforce.py --ref 192    # one ref + compare PNG
  python tools/tune_throw_bruteforce.py --apply      # write preview_throw_anim.py
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from compare_throw_refs import crop_game_viewport, crop_tableau, content_mask
from preview_throw_anim import (
    BLIT_Y,
    DEFAULT_REF_DIR,
    ORANGE_H,
    ORANGE_ROW,
    ORANGE_Y,
    REF_BLIT,
    TABLEAU_W,
    TABLEAU_X,
    W,
    H,
    blit,
    canvas,
    load_throw,
)

REFS = sorted(REF_BLIT.keys())


@dataclass
class TuneResult:
    ref: int
    mae: float
    orange: bool
    hand_fi: int
    hand_x: int
    die_fi: int | None = None
    die_x: int | None = None


def paint_orange(dst: Image.Image, frames: list[Image.Image]) -> None:
    band = frames[0].crop((0, ORANGE_ROW, frames[0].width, ORANGE_ROW + ORANGE_H))
    bar = band.resize((TABLEAU_W, ORANGE_H), Image.Resampling.NEAREST)
    blit(dst, bar, TABLEAU_X, ORANGE_Y)


def compose_raw(
    frames: list[Image.Image],
    orange: bool,
    hand_fi: int,
    hand_x: int,
    die_fi: int | None = None,
    die_x: int | None = None,
) -> Image.Image:
    dst = canvas()
    if orange:
        paint_orange(dst, frames)
    blit(dst, frames[hand_fi], hand_x, BLIT_Y)
    if die_fi is not None and die_x is not None:
        blit(dst, frames[die_fi], die_x, BLIT_Y)
    return dst


def mae(prev: Image.Image, ref: Image.Image) -> float:
    pa = np.array(crop_tableau(prev), dtype=np.int16)
    pr = np.array(ref, dtype=np.int16)
    mask = content_mask(pr)
    if not mask.any():
        return 9999.0
    return float(np.abs(pa - pr).mean(axis=2)[mask].mean())


def search_hand_x(
    frames: list[Image.Image],
    ref_tab: Image.Image,
    orange: bool,
    hand_fi: int,
    x_min: int,
    x_max: int,
    step: int,
) -> tuple[float, int]:
    best = (9999.0, x_min)
    hw = frames[hand_fi].width
    for hx in range(max(0, x_min), min(W - hw, x_max) + 1, step):
        e = mae(compose_raw(frames, orange, hand_fi, hx), ref_tab)
        if e < best[0]:
            best = (e, hx)
    return best


def search_die_x(
    frames: list[Image.Image],
    ref_tab: Image.Image,
    orange: bool,
    hand_fi: int,
    hand_x: int,
    die_fi: int,
    x_min: int,
    x_max: int,
    step: int,
) -> tuple[float, int]:
    best = (9999.0, x_min)
    dw = frames[die_fi].width
    for dx in range(max(0, x_min), min(W - dw, x_max) + 1, step):
        e = mae(
            compose_raw(frames, orange, hand_fi, hand_x, die_fi, dx), ref_tab
        )
        if e < best[0]:
            best = (e, dx)
    return best


def tune_one(
    frames: list[Image.Image],
    ref_num: int,
    ref_dir: Path,
    out: Path,
    *,
    verbose: bool = True,
    interactive: bool = False,
) -> TuneResult:
    t0 = time.perf_counter()
    ref_tab = crop_tableau(crop_game_viewport(Image.open(ref_dir / f"{ref_num}.png")))
    roll = ref_num >= 183

    # 1) Orange underlay? (2 tries, probe hand@58)
    e0, _ = search_hand_x(frames, ref_tab, False, 2, 40, 80, 8)
    e1, _ = search_hand_x(frames, ref_tab, True, 2, 40, 80, 8)
    orange = e1 < e0
    if verbose:
        print(f"  orange_under={orange}  (no={e0:.1f} yes={e1:.1f})")
    if interactive:
        input("  [Enter] hand frame search...")

    # 2) Hand frame + coarse X
    hand_candidates = [0, 1, 2, 3, 4, 5] if not roll else [2, 3, 4, 5]
    best_hand = (9999.0, 2, 58)
    for hf in hand_candidates:
        e, hx = search_hand_x(frames, ref_tab, orange, hf, 0, W - 1, 8)
        if verbose:
            print(f"  hand f{hf}: best coarse mae={e:.1f} @x{hx}")
        if e < best_hand[0]:
            best_hand = (e, hf, hx)

    hf, hx = best_hand[1], best_hand[2]
    if interactive:
        input(f"  [Enter] fine hand f{hf} + die search...")

    # 3) Fine hand X (roll: lock before die search)
    e, hx = search_hand_x(
        frames, ref_tab, orange, hf, max(0, hx - 12), min(W - frames[hf].width, hx + 12), 1
    )
    best_hand = (e, hf, hx)
    hx = best_hand[2]

    die_fi = die_x = None
    if roll:
        die_guess = {183: 3, 186: 4, 189: 5, 192: 6, 194: 7, 197: 8, 199: 9, 202: 10}.get(
            ref_num, 6
        )
        best_die = (9999.0, die_guess, 140)
        for df in range(max(3, die_guess - 1), min(11, die_guess + 2)):
            e, dx = search_die_x(frames, ref_tab, orange, hf, hx, df, 120, 280, 4)
            if verbose:
                print(f"  die f{df}: coarse mae={e:.1f} @x{dx}")
            if e < best_die[0]:
                best_die = (e, df, dx)
        die_fi, die_x = best_die[1], best_die[2]
        e, die_x = search_die_x(
            frames,
            ref_tab,
            orange,
            hf,
            hx,
            die_fi,
            max(0, die_x - 20),
            min(W - frames[die_fi].width, die_x + 20),
            1,
        )
        if e < best_hand[0]:
            best_hand = (e, hf, hx)

    res = TuneResult(ref_num, best_hand[0], orange, hf, hx, die_fi, die_x)
    dt = time.perf_counter() - t0
    if verbose:
        extra = f" d{die_fi}@{die_x}" if die_fi is not None else ""
        print(f"  => mae={res.mae:.1f} orange={orange} h{hf}@{hx}{extra}  ({dt:.1f}s)")

    # Side-by-side PNG immediately
    prev = compose_raw(frames, orange, hf, hx, die_fi, die_x)
    tab = crop_tableau(prev)
    row = Image.new("RGB", (W * 2, max(ref_tab.height, tab.height)), (0, 0, 0))
    row.paste(tab, (0, 0))
    row.paste(ref_tab, (W, 0))
    d = ImageDraw.Draw(row)
    d.text((2, 2), str(asdict(res)), fill=(255, 100, 100))
    d.text((W + 2, 2), f"ref {ref_num}", fill=(100, 255, 100))
    out.mkdir(parents=True, exist_ok=True)
    row.save(out / f"tune_{ref_num}.png")
    print(f"  wrote {out / f'tune_{ref_num}.png'}")
    return res


def apply_to_preview(results: list[TuneResult], path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    start = text.find("REF_BLIT: dict[int, BlitSpec] = {")
    end = text.find("\n\n# 15-step", start)
    if start < 0 or end < 0:
        raise SystemExit("could not find REF_BLIT block in preview_throw_anim.py")

    lines = ["REF_BLIT: dict[int, BlitSpec] = {"]
    lines.append("    # tune_throw_gui.py / tune_throw_bruteforce.py --apply")
    for r in results:
        if r.die_fi is None:
            lines.append(f"    {r.ref}: BlitSpec({r.hand_fi}, {r.hand_x}),")
        else:
            lines.append(
                f"    {r.ref}: BlitSpec({r.hand_fi}, {r.hand_x}, {r.die_fi}, {r.die_x}),"
            )
    lines.append("}")

    seq_start = text.find("ANIM_SEQUENCE: list[BlitSpec] = [", end)
    seq_end = text.find("\n\n\ndef load_throw", seq_start)
    if seq_start >= 0 and seq_end >= 0:
        roll = [r for r in results if r.die_fi is not None]
        if len(roll) >= 8:
            seq_lines = ["ANIM_SEQUENCE: list[BlitSpec] = ["]
            seq_lines.append("    # tune_throw_bruteforce.py --apply")
            seq_lines.append(f"    BlitSpec({results[1].hand_fi}, {results[1].hand_x}),")
            seq_lines.append(f"    BlitSpec({results[2].hand_fi}, {results[2].hand_x}),")
            for r in roll[:8]:
                seq_lines.append(
                    f"    BlitSpec({r.hand_fi}, {r.hand_x}, {r.die_fi}, {r.die_x}),"
                )
            seq_lines.append(f"    BlitSpec({results[1].hand_fi}, {results[1].hand_x}),")
            seq_lines.append(f"    BlitSpec({results[2].hand_fi}, {results[2].hand_x}),")
            for r in roll[:3]:
                seq_lines.append(
                    f"    BlitSpec({r.hand_fi}, {r.hand_x}, {r.die_fi}, {r.die_x}),"
                )
            seq_lines.append("]")
            new = text[:start] + "\n".join(lines) + text[end:seq_start] + "\n".join(seq_lines) + text[seq_end:]
        else:
            new = text[:start] + "\n".join(lines) + text[end:]
    else:
        new = text[:start] + "\n".join(lines) + text[end:]

    # Also patch compose to use orange + raw frames (remove static_hand_image)
    if "static_hand_image" in new:
        new = new.replace(
            "def compose_spec(frames: list[Image.Image], spec: BlitSpec) -> Image.Image:\n"
            "    dst = canvas()\n"
            "    paint_full_orange(dst, frames)\n"
            "    if spec.die_fi is not None and spec.die_x is not None:\n"
            "        blit(dst, static_hand_image(frames), STATIC_HAND_BLIT_X, BLIT_Y)\n"
            "        blit(dst, frames[spec.die_fi], spec.die_x, BLIT_Y)\n"
            "    else:\n"
            "        blit(dst, frames[spec.hand_fi], spec.hand_x, BLIT_Y)\n"
            "    return dst\n",
            "def compose_spec(frames: list[Image.Image], spec: BlitSpec) -> Image.Image:\n"
            "    return compose_raw(\n"
            "        frames, True, spec.hand_fi, spec.hand_x, spec.die_fi, spec.die_x\n"
            "    )\n",
        )
    path.write_text(new, encoding="utf-8")
    print(f"updated {path}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ref-dir", type=Path, default=DEFAULT_REF_DIR)
    ap.add_argument("--data-dir", type=Path, default=ROOT)
    ap.add_argument("--out", type=Path, default=ROOT / "game" / "build" / "throw_preview")
    ap.add_argument("--ref", type=int, action="append", help="tune only these refs")
    ap.add_argument("--apply", action="store_true", help="patch preview_throw_anim.py")
    ap.add_argument(
        "-i",
        "--interactive",
        action="store_true",
        help="pause between stages (one ref, ~10s total)",
    )
    ap.add_argument(
        "--open",
        action="store_true",
        help="open tune_<ref>.png after each ref (Windows)",
    )
    args = ap.parse_args()

    frames = load_throw(args.data_dir)
    out = args.out
    targets = args.ref if args.ref else REFS

    print("Decode: pen index 0 only transparent (not all black pixels).\n")
    results: list[TuneResult] = []
    for ref_num in targets:
        print(f"ref {ref_num}:")
        r = tune_one(
            frames,
            ref_num,
            args.ref_dir,
            out,
            interactive=args.interactive and len(targets) == 1,
        )
        results.append(r)
        if args.open:
            import os

            p = out / f"tune_{ref_num}.png"
            if p.is_file():
                os.startfile(p)

    json_path = out / "tune_results.json"
    json_path.write_text(
        json.dumps([asdict(r) for r in results], indent=2), encoding="utf-8"
    )
    print(f"\nwrote {json_path}")

    if args.apply:
        apply_to_preview(results, ROOT / "tools" / "preview_throw_anim.py")


if __name__ == "__main__":
    main()

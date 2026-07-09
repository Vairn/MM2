#!/usr/bin/env python3
"""Probe blit simulator (#3) and place-table row decode (#2) for Genesis monsters."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mm2_gfx_export import composite_anm_frame, load_anm_asset
from rip_genesis_monsters import (
    PLACE_BASE,
    SAT_MOSAIC,
    compose_sat,
    compose_simulated,
    combat_portrait_row,
    load_pic,
    parse_11a,
    parse_meta,
    parse_place_table,
    simulate_blit7788,
    staging_diff_count,
)

ROM = ROOT / "EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen"
OUT = ROOT / "EXTRACTED/genesis/gfx/catalog/monsters/_probe"


def ncc(a: Image.Image, b: Image.Image) -> float:
    g = b.resize(a.size, Image.NEAREST)
    A = np.array(a.convert("RGBA"), dtype=np.float32)
    G = np.array(g.convert("RGBA"), dtype=np.float32)
    m = (A[:, :, 3] > 0) | (G[:, :, 3] > 0)
    if m.sum() < 20:
        return -1.0
    al = (0.299 * A[:, :, 0] + 0.587 * A[:, :, 1] + 0.114 * A[:, :, 2])[m]
    gl = (0.299 * G[:, :, 0] + 0.587 * G[:, :, 1] + 0.114 * G[:, :, 2])[m]
    al -= al.mean()
    gl -= gl.mean()
    d = np.linalg.norm(al) * np.linalg.norm(gl)
    if d < 1e-6:
        return -1.0
    return float(np.dot(al, gl) / d)


def fit(im: Image.Image, box: tuple[int, int]) -> Image.Image:
    s = min(box[0] / im.size[0], box[1] / im.size[1], 4)
    return im.resize(
        (max(1, int(im.size[0] * s)), max(1, int(im.size[1] * s))), Image.NEAREST
    )


def main() -> int:
    rom = ROM.read_bytes()
    ents = parse_11a(rom)
    meta = parse_meta(rom)
    place = parse_place_table(rom)
    OUT.mkdir(parents=True, exist_ok=True)

    try:
        font = ImageFont.truetype("arial.ttf", 12)
        font_sm = ImageFont.truetype("arial.ttf", 10)
        font_lg = ImageFont.truetype("arial.ttf", 14)
    except OSError:
        font = font_sm = font_lg = ImageFont.load_default()

    cols = [
        "Amiga f0",
        "Direct row0",
        "Sim row0",
        "Sim+mask row0",
        "Sim 1st place row",
        "Sim masked",
    ]
    cell = 120
    hdr = 52
    W = 40 + len(cols) * (cell + 6)
    H = hdr + 5 * (cell + 36)
    sheet = Image.new("RGB", (W, H), (18, 18, 22))
    d = ImageDraw.Draw(sheet)
    d.text(
        (8, 6),
        "Blit sim @0x7788 + place table @0x91738 (hi=layout row, lo=duration)",
        fill=(235, 235, 235),
        font=font_lg,
    )
    d.text(
        (8, 26),
        "Combat still uses row 0 (@7756→7788). Mask = skip SAT slots with empty staging.",
        fill=(140, 140, 150),
        font=font_sm,
    )
    for c, lab in enumerate(cols):
        d.text((40 + c * (cell + 6), hdr - 14), lab, fill=(160, 190, 255), font=font_sm)

    report = []
    for ri, eid in enumerate(range(1, 6)):
        asset = load_anm_asset(ROOT / f"{eid:02d}.anm")
        amiga = composite_anm_frame(asset, 0)
        pic = load_pic(rom, eid, ents, meta)
        if not pic:
            continue
        prow = combat_portrait_row(pic, place.get(pic["w3"]))
        anim = place.get(pic["w3"], {"rows": [], "durations": [], "words": []})
        place_row = anim["rows"][0] if anim["rows"] else prow

        direct = compose_sat(pic, prow, SAT_MOSAIC)
        sim0 = compose_simulated(pic, prow, mask_empty=False)
        sim0m = compose_simulated(pic, prow, mask_empty=True)
        sim_p = compose_simulated(pic, place_row, mask_empty=False)
        sim_pm = compose_simulated(pic, place_row, mask_empty=True)

        st = simulate_blit7788(pic, prow)
        # verify sim matches direct when re-read from staging
        diff_tiles = staging_diff_count(st, simulate_blit7788(pic, prow))

        scores = {
            "direct": ncc(amiga, direct),
            "sim0": ncc(amiga, sim0),
            "sim0m": ncc(amiga, sim0m),
            "sim_place": ncc(amiga, sim_p),
            "sim_placem": ncc(amiga, sim_pm),
        }
        report.append(
            {
                "id": eid,
                "w3": pic["w3"],
                "place_rows": anim["rows"],
                "place_dur": anim["durations"],
                "combat_row": prow,
                "first_place_row": place_row,
                "scores": scores,
            }
        )
        print(
            f"#{eid} w3={pic['w3']} place_rows={anim['rows']} dur={anim['durations']} "
            f"combat_r={prow} scores={{{', '.join(f'{k}:{v:.3f}' for k,v in scores.items())}}}"
        )

        y = hdr + ri * (cell + 36)
        d.text((4, y + 50), f"#{eid}", fill=(255, 210, 100), font=font)
        d.text((4, y + cell + 2), f"rows {anim['rows']}", fill=(120, 120, 130), font=font_sm)
        for c, im in enumerate([amiga, direct, sim0, sim0m, sim_p, sim_pm]):
            x = 40 + c * (cell + 6)
            d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(6, 6, 10), outline=(50, 50, 60))
            fi = fit(im, (cell - 4, cell - 4))
            ox = x + (cell - fi.size[0]) // 2
            oy = y + (cell - fi.size[1]) // 2
            sheet.paste(fi, (ox, oy), fi if fi.mode == "RGBA" else None)

    path = OUT / "cmp_blit_sim_place_1_5.png"
    sheet.save(path)
    sheet.resize((W * 2, H * 2), Image.NEAREST).save(OUT / "cmp_blit_sim_place_1_5_x2.png")
    print("wrote", path)

    # Staging vs direct pixel diff (should be 0 if pipeline equivalent)
    for eid in range(1, 6):
        pic = load_pic(rom, eid, ents, meta)
        r = combat_portrait_row(pic, place.get(pic["w3"]))
        d0 = compose_sat(pic, r, SAT_MOSAIC)
        s0 = compose_simulated(pic, r)
        # pixel diff
        if d0.size == s0.size:
            A = np.array(d0)
            B = np.array(s0)
            diff = np.abs(A.astype(int) - B.astype(int)).sum()
            print(f"  #{eid} direct vs sim pixel diff sum={diff}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

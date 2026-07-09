#!/usr/bin/env python3
"""Probe composition heuristics for Genesis $11A picture layouts."""

from __future__ import annotations

import importlib.util
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("lz", ROOT / "tools/genesis_lz_decompress.py")
lz = importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(lz)

ROM = (
    ROOT / "EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen"
).read_bytes()
OUT = ROOT / "EXTRACTED/genesis/gfx/catalog/monsters/_probe"
OUT.mkdir(parents=True, exist_ok=True)


def parse_11a(rom: bytes) -> dict[int, dict]:
    a = 0x33D4A
    ents: dict[int, dict] = {}
    while True:
        eid = struct.unpack_from(">H", rom, a)[0]
        if eid == 0:
            break
        sz1 = struct.unpack_from(">I", rom, a + 2)[0]
        sec2 = a + sz1 + 0xA
        sz2 = struct.unpack_from(">I", rom, sec2)[0]
        ents[eid] = {"base": a, "sec2": sec2, "pal": sec2 + sz2 + 8}
        a = sec2 + sz2 + 0x28
    return ents


def parse_meta(rom: bytes) -> dict[int, dict]:
    a = 0x913B4
    out: dict[int, dict] = {}
    while True:
        eid = struct.unpack_from(">H", rom, a)[0]
        if eid == 0xFFFF:
            break
        w2, w3, w4 = struct.unpack_from(">HHH", rom, a + 2)
        extra = [struct.unpack_from(">H", rom, a + 8 + i)[0] for i in range(0, w4 * 2, 2)]
        out[eid] = {"w2": w2, "w3": w3, "w4": w4, "extra": extra}
        a += w4 * 2 + 8
    return out


def cram(w: int) -> tuple[int, int, int]:
    r = (w >> 1) & 7
    g = (w >> 5) & 7
    b = (w >> 9) & 7
    s = lambda v: v * 255 // 7
    return s(r), s(g), s(b)


def blit_cm(
    words: list[int],
    tw: int,
    th: int,
    chr_data: bytes,
    pal: list[tuple[int, int, int]],
    ntiles: int,
) -> Image.Image:
    img = Image.new("RGBA", (tw * 8, th * 8), (0, 0, 0, 0))
    px = img.load()
    for i, w in enumerate(words[: tw * th]):
        idx = w & 0x7FF
        if not idx:
            continue
        ti = idx - 1
        if ti >= ntiles:
            continue
        flip_h = bool(w & 0x800)
        flip_v = bool(w & 0x1000)
        tile = chr_data[ti * 32 : (ti + 1) * 32]
        x, y = i // th, i % th
        for yy in range(8):
            for xx in range(8):
                bi = yy * 4 + xx // 2
                n = (tile[bi] >> 4) if (xx & 1) == 0 else (tile[bi] & 0xF)
                if not n:
                    continue
                dx = 7 - xx if flip_h else xx
                dy = 7 - yy if flip_v else yy
                px[x * 8 + dx, y * 8 + dy] = (*pal[n], 255)
    return img


def blit_rm(
    words: list[int],
    tw: int,
    th: int,
    chr_data: bytes,
    pal: list[tuple[int, int, int]],
    ntiles: int,
) -> Image.Image:
    img = Image.new("RGBA", (tw * 8, th * 8), (0, 0, 0, 0))
    px = img.load()
    for i, w in enumerate(words[: tw * th]):
        idx = w & 0x7FF
        if not idx:
            continue
        ti = idx - 1
        if ti >= ntiles:
            continue
        flip_h = bool(w & 0x800)
        flip_v = bool(w & 0x1000)
        tile = chr_data[ti * 32 : (ti + 1) * 32]
        x, y = i % tw, i // tw
        for yy in range(8):
            for xx in range(8):
                bi = yy * 4 + xx // 2
                n = (tile[bi] >> 4) if (xx & 1) == 0 else (tile[bi] & 0xF)
                if not n:
                    continue
                dx = 7 - xx if flip_h else xx
                dy = 7 - yy if flip_v else yy
                px[x * 8 + dx, y * 8 + dy] = (*pal[n], 255)
    return img


def edge_score(img: Image.Image) -> tuple[float, int]:
    px = img.load()
    w, h = img.size
    score = 0
    n = 0
    for x in range(8, w, 8):
        for y in range(h):
            a = px[x - 1, y]
            b = px[x, y]
            if a[3] == 0 and b[3] == 0:
                continue
            n += 1
            if a[3] == 0 or b[3] == 0:
                score += 3
                continue
            score += abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])
    for y in range(8, h, 8):
        for x in range(w):
            a = px[x, y - 1]
            b = px[x, y]
            if a[3] == 0 and b[3] == 0:
                continue
            n += 1
            if a[3] == 0 or b[3] == 0:
                score += 3
                continue
            score += abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])
    return score / max(1, n), n


def load(eid: int, ents: dict, meta: dict) -> dict:
    e = ents[eid]
    m = meta[eid]
    chr_data = lz.decompress_lz(ROM[e["base"] + 2 :])
    layout = lz.decompress_lz(ROM[e["sec2"] :])
    pal = [cram(struct.unpack_from(">H", ROM, e["pal"] + i)[0]) for i in range(0, 32, 2)]
    words = [struct.unpack_from(">H", layout, i)[0] for i in range(0, len(layout) & ~1, 2)]
    return {
        "w2": m["w2"],
        "chr": chr_data,
        "pal": pal,
        "ntiles": len(chr_data) // 32,
        "words": words,
        "frames": len(words) // 121,
    }


def densest(pic: dict) -> tuple[int, int]:
    best = 0
    best_nz = 0
    for fr in range(pic["frames"]):
        row = pic["words"][fr * 121 : (fr + 1) * 121]
        nz = sum(1 for w in row if w & 0x7FF)
        if nz > best_nz:
            best_nz = nz
            best = fr
    return best, best_nz


def pack_uniform(sw: int, sh: int, cols: int) -> list[tuple[int, int, int, int]]:
    cell = sw * sh
    n = 121 // cell
    return [((i % cols) * sw, (i // cols) * sh, sw, sh) for i in range(n)]


def gen_templates() -> list[tuple[str, list[tuple[int, int, int, int]]]]:
    tpls: list[tuple[str, list[tuple[int, int, int, int]]]] = []
    tpls.append(
        (
            "ui72",
            [
                (0, 0, 4, 4),
                (4, 0, 4, 4),
                (8, 0, 4, 3),
                (0, 4, 4, 4),
                (4, 4, 4, 4),
                (8, 4, 4, 3),
                (0, 8, 3, 4),
                (4, 8, 3, 4),
                (8, 8, 3, 3),
            ],
        )
    )
    for sw, sh in [
        (4, 4),
        (3, 3),
        (2, 2),
        (4, 3),
        (3, 4),
        (2, 3),
        (3, 2),
        (1, 1),
        (4, 2),
        (2, 4),
        (5, 5),
        (7, 7),
    ]:
        for cols in range(1, 9):
            sp = pack_uniform(sw, sh, cols)
            used = sum(w * h for _, _, w, h in sp)
            if used >= 80:
                tpls.append((f"{sw}x{sh}_c{cols}", sp))
    return tpls


def compose_sat(
    row: list[int],
    sprites: list[tuple[int, int, int, int]],
    pic: dict,
) -> Image.Image:
    max_x = max(x + w for x, y, w, h in sprites) * 8
    max_y = max(y + h for x, y, w, h in sprites) * 8
    canvas = Image.new("RGBA", (max_x, max_y), (0, 0, 0, 0))
    off = 0
    for x, y, w, h in sprites:
        cell = blit_cm(row[off : off + w * h], w, h, pic["chr"], pic["pal"], pic["ntiles"])
        canvas.paste(cell, (x * 8, y * 8), cell)
        off += w * h
    return canvas


def main() -> None:
    ents = parse_11a(ROM)
    meta = parse_meta(ROM)
    tpls = gen_templates()
    print("templates", len(tpls))

    for eid in (25, 7, 1, 4, 9, 20, 43):
        if eid not in ents or eid not in meta:
            continue
        pic = load(eid, ents, meta)
        fr, nz = densest(pic)
        row = pic["words"][fr * 121 : (fr + 1) * 121]
        results: list[tuple[float, int, str, Image.Image]] = []

        for tw in range(1, 16):
            th = 121 // tw
            if tw * th == 0:
                continue
            stream = row[: tw * th]
            for order, fn in (("cm", blit_cm), ("rm", blit_rm)):
                im = fn(stream, tw, th, pic["chr"], pic["pal"], pic["ntiles"])
                sc, n = edge_score(im)
                results.append((sc, n, f"nt_{tw}x{th}_{order}", im))

        for name, sprites in tpls:
            used = sum(w * h for _, _, w, h in sprites)
            if used > 121:
                continue
            max_x = max(x + w for x, y, w, h in sprites) * 8
            max_y = max(y + h for x, y, w, h in sprites) * 8
            if max_x > 320 or max_y > 320:
                continue
            im = compose_sat(row, sprites, pic)
            sc, n = edge_score(im)
            results.append((sc, n, f"sat_{name}", im))

        results.sort(key=lambda t: (t[0], -t[1]))
        print(f"\n=== id {eid} w2={pic['w2']} nz={nz} ===")
        for sc, n, name, im in results[:10]:
            print(f"  {sc:8.1f} n={n:5d} {name} {im.size}")
        for i, (sc, n, name, im) in enumerate(results[:4]):
            im.resize((im.size[0] * 2, im.size[1] * 2), Image.NEAREST).save(
                OUT / f"best_{eid}_{i}_{name}.png"
            )


if __name__ == "__main__":
    main()

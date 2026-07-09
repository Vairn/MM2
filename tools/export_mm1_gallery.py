#!/usr/bin/env python3
"""Export MM1 2D map renders + WALLPIX art for the wiki gallery."""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    raise SystemExit("pip install pillow")

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

from mm1_maps import MM1_MAP_SLUGS, iter_mm1_screens, mm1_map_title  # noqa: E402
from mm1_wallpix import (  # noqa: E402
    DEFAULT_WALLPIX_EXPORT,
    WALLSLICES,
    WALLPIX_BIOME,
    list_available_wall_sets,
    slice_wall_sheet,
    wall_sheet_path,
)
from mm2_gfx_export import contact_sheet, save_png  # noqa: E402

DEFAULT_MAZEDATA = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 1\MAZEDATA.DTA")
DEFAULT_MM1_EXE = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 1\MM.EXE")

GRID = 16
WALL_EDGE: dict[int, tuple[int, int, int, int] | None] = {
    0: None,
    1: (72, 72, 88, 255),
    2: (220, 176, 56, 255),
    3: (96, 196, 112, 255),
}

MM1_OVERVIEW_GROUPS: list[tuple[str, str, list[int], int]] = [
    ("overview-towns", "Towns", list(range(0, 5)), 5),
    ("overview-caves", "Caves 1–9", list(range(5, 14)), 3),
    ("overview-overland", "Overland A1–E4", list(range(14, 34)), 5),
    ("overview-late", "Late dungeons", list(range(34, 55)), 5),
]

# MM1 overland sector layout (same world grid as MM2).
MM1_OVERLAND_GRID: list[list[int]] = [
    [14, 18, 22, 26, 30],  # A1..E1
    [15, 19, 23, 27, 31],  # A2..E2
    [16, 20, 24, 28, 32],  # A3..E3
    [17, 21, 25, 29, 33],  # A4..E4
]


WALL_DIR_SHIFT = (6, 4, 2, 0)  # N, E, S, W — ScummVM DIRMASK / view3d DIR_PAIR_SHIFT


def _wall_nibble(v: int, direction: int) -> int:
    return (v >> WALL_DIR_SHIFT[direction & 3]) & 0x3


def _collision_blocked(c: int) -> bool:
    return any((c >> (d * 2)) & 1 for d in range(4))


def render_topdown(
    visual: bytes,
    collision: bytes,
    *,
    title: str = "",
    cell: int = 10,
    scale: int = 2,
) -> Image.Image:
    """North-up 16×16 wall map from MAZEDATA page 0 (+ event dots from page 1)."""
    gap = 1
    margin = 30 if title else 6
    inner = GRID * cell + (GRID - 1) * gap
    w = h = margin * 2 + inner
    img = Image.new("RGBA", (w, h), (13, 3, 3, 255))
    draw = ImageDraw.Draw(img)
    if title:
        draw.text((8, 8), title, fill=(242, 237, 237, 255))

    ox, oy = margin, margin
    wall_w = max(2, cell // 4)

    for sy in range(GRID):
        map_y = GRID - 1 - sy
        for x in range(GRID):
            idx = (map_y << 4) | x
            v = visual[idx]
            c = collision[idx]
            x0 = ox + x * (cell + gap)
            y0 = oy + sy * (cell + gap)
            x1 = x0 + cell
            y1 = y0 + cell

            floor = (40, 28, 28, 255) if _collision_blocked(c) else (56, 40, 40, 255)
            draw.rectangle((x0, y0, x1, y1), fill=floor)

            n, e, s, wv = (_wall_nibble(v, d) for d in range(4))
            if n:
                col = WALL_EDGE[n]
                draw.rectangle((x0, y0, x1, y0 + wall_w - 1), fill=col)
            if e:
                col = WALL_EDGE[e]
                draw.rectangle((x1 - wall_w + 1, y0, x1, y1), fill=col)
            if s:
                col = WALL_EDGE[s]
                draw.rectangle((x0, y1 - wall_w + 1, x1, y1), fill=col)
            if wv:
                col = WALL_EDGE[wv]
                draw.rectangle((x0, y0, x0 + wall_w - 1, y1), fill=col)

            if c & 0x80:
                r = max(2, cell // 5)
                draw.ellipse((x1 - r - 2, y0 + 2, x1 - 2, y0 + r + 2), fill=(235, 56, 56, 255))

    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def export_maps(
    mazedata: Path,
    out: Path,
    *,
    exe: Path | None = None,
    scale: int = 2,
) -> dict[int, Image.Image]:
    maps_dir = out / "maps"
    maps_dir.mkdir(parents=True, exist_ok=True)
    thumbs: dict[int, Image.Image] = {}

    for sid, slug, title, visual, collision, _entry, _env in iter_mm1_screens(
        mazedata, exe=exe
    ):
        label = f"{sid:02d} {title}"
        grid = render_topdown(visual, collision, title=label, scale=scale)
        fname = f"{sid:02d}-{slug}.png"
        save_png(grid, maps_dir / fname)
        thumbs[sid] = grid
        print(f"  map {sid:02d} {slug} ({title})")

    for slug, title, screens, cols in MM1_OVERVIEW_GROUPS:
        cells = [thumbs[s] for s in screens if s in thumbs]
        labels = [
            f"{s:02d} {mm1_map_title(MM1_MAP_SLUGS[s])}" for s in screens if s in thumbs
        ]
        if not cells:
            continue
        sheet = contact_sheet(cells, cols=cols, pad=4, labels=labels, label_h=12)
        save_png(sheet, maps_dir / f"{slug}.png")
        print(f"  overview {slug}: {len(cells)} screens")

    # World overland stitch (native scale thumbs).
    native: dict[int, Image.Image] = {}
    for sid, slug, title, visual, collision, _e, _env in iter_mm1_screens(mazedata, exe=exe):
        if not slug.startswith("area"):
            continue
        native[sid] = render_topdown(visual, collision, title="", cell=10, scale=1)

    if native:
        tw = max(im.width for im in native.values())
        th = max(im.height for im in native.values())
        world = Image.new("RGBA", (5 * tw, 4 * th), (13, 3, 3, 255))
        draw = ImageDraw.Draw(world)
        for row_idx, row in enumerate(MM1_OVERLAND_GRID):
            for col_idx, sid in enumerate(row):
                im = native.get(sid)
                if im is None:
                    continue
                world.paste(im, (col_idx * tw, row_idx * th), im)
                sector = MM1_MAP_SLUGS[sid][4:].upper()
                draw.text((col_idx * tw + 4, row_idx * th + 4), sector, fill=(242, 237, 237, 255))
        save_png(world, maps_dir / "world-overland-5x4.png")
        print(f"  world overland 5×4: {world.width}×{world.height} px")

    return thumbs


def export_wall_slices(export_dir: Path, out: Path) -> list[int]:
    walls_dir = out / "walls"
    slices_dir = walls_dir / "slices"
    walls_dir.mkdir(parents=True, exist_ok=True)
    slices_dir.mkdir(parents=True, exist_ok=True)
    exported: list[int] = []
    for n in list_available_wall_sets(export_dir):
        src = wall_sheet_path(export_dir, n)
        if not src.is_file():
            continue
        shutil.copy2(src, walls_dir / src.name)
        sheet = Image.open(src)
        slices = slice_wall_sheet(sheet)
        labeled: list[Image.Image] = []
        labels: list[str] = []
        for sl, img in zip(WALLSLICES, slices):
            pad = 4
            cell = Image.new("RGBA", (img.width + pad * 2, img.height + pad * 2 + 12), (13, 3, 3, 255))
            cell.paste(img, (pad, pad), img)
            labeled.append(cell)
            labels.append(sl.name)
        atlas = contact_sheet(labeled, cols=4, pad=2, labels=labels, label_h=0)
        save_png(atlas, slices_dir / f"wall{n:02d}-slices.png")
        exported.append(n)
        print(f"  wall{n:02d}: sheet + slice atlas")
    return exported


def write_maps_markdown(
    out_docs: Path,
    *,
    image_prefix: str = "/gallery/mm1",
    links: dict[str, str] | None = None,
    use_frontmatter: bool = True,
) -> None:
    out_docs.parent.mkdir(parents=True, exist_ok=True)
    lk = links or {}
    mm1_overview = lk.get("mm1_overview", "/docs/reverse-engineering/50-mm1-overview")
    mazedata_fmt = lk.get("mazedata_format", "MM1-MAZEDATA-Format")
    map_carto = lk.get("maps", "Map-Cartography")
    map_walker = lk.get("map_walker", "MM1-Map-Walker")
    mm1_outdoor = lk.get("mm1_outdoor", "MM1-to-MM2-Outdoor")

    lines: list[str] = []
    if use_frontmatter:
        lines += ["---", "title: MM1 map gallery", "---", ""]

    lines += [
        "# MM1 2D map renders",
        "",
        "Each screen is a **16×16** top-down wall map from `MAZEDATA.DTA` page 0 "
        "(visual walls) with event triggers from page 1 (collision `0x80`).",
        "",
        "**North is at the top** (disk row 15); row 0 on disk is south — same convention as "
        f"[Map Cartography]({map_carto}) / MM2 `map.dat`.",
        "",
        "Wall types (N/E/S/W, 2 bits each): **gray** = wall, **gold** = torch, **green** = door. "
        "Red dots = collision event flag `0x80`.",
        "",
        f"See [MM1 Overview]({mm1_overview}) · [MAZEDATA format]({mazedata_fmt}) · "
        f"[MM1 map walker ↗]({map_walker})",
        "",
        "## Overview by area type",
        "",
        "All **55** map screens (`MAZEDATA.DTA` indices 0–54), grouped like the game world.",
        "",
    ]

    overview_sections = [
        ("overview-towns", "Towns (0–4)", "Five starting towns: Sorpigal through Erliquin."),
        ("overview-caves", "Caves 1–9 (5–13)", "Early dungeon screens linked from the overland."),
        ("overview-overland", "Outdoor overland — 5×4 sectors (A1–E4)", (
            "Twenty surface maps in **column A–E, row 1–4** order (north at top). "
            f"Same world grid as MM2 — see [MM1 to MM2 outdoor]({mm1_outdoor})."
        )),
        ("overview-late", "Late dungeons (34–54)", (
            "Endgame areas: Doom, Black Ridge, Queen's Pyramids, Rainbow Road, "
            "Enchanted Forest, Dragadune, Demon, Alamar, Protection Point, Astral Plane."
        )),
    ]
    for slug, heading, blurb in overview_sections:
        title = next(t for s, t, _, _ in MM1_OVERVIEW_GROUPS if s == slug)
        lines += [
            f"### {heading}",
            "",
            blurb,
            "",
            f"![{title}]({image_prefix}/maps/{slug}.png)",
            "",
        ]

    lines += [
        "## World overland map",
        "",
        "Full **5×4** stitch at native render resolution — center surface sectors **A1–E4** "
        "(town interiors 0–4 are separate screens, not shown here).",
        "",
        f"![MM1 overland 5×4]({image_prefix}/maps/world-overland-5x4.png)",
        "",
        "## Per-screen maps",
        "",
    ]
    for sid, slug in enumerate(MM1_MAP_SLUGS):
        name = mm1_map_title(slug)
        lines += [
            f"### {sid:02d} — {name}",
            "",
            f"![{slug}]({image_prefix}/maps/{sid:02d}-{slug}.png)",
            "",
        ]
    out_docs.write_text("\n".join(lines), encoding="utf-8")
    print(f"  markdown -> {out_docs}")


def write_walls_markdown(
    out_docs: Path,
    exported_sets: list[int],
    *,
    image_prefix: str = "/gallery/mm1",
) -> None:
    out_docs.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "---",
        "title: MM1 WALLPIX sheets",
        "---",
        "",
        "# MM1 WALLPIX art",
        "",
        "496×128 lagdotcom exports from `WALLPIX.DTA`. "
        "See [MM1 art and graphics](/docs/reverse-engineering/51-mm1-art-and-graphics).",
        "",
    ]
    if not exported_sets:
        lines += [
            "::: warning No local exports",
            f"Place lagdotcom `wall*.png` under `{DEFAULT_WALLPIX_EXPORT}`.",
            ":::",
            "",
        ]
    else:
        overland = [n for n in sorted(exported_sets) if n in WALLPIX_BIOME]
        indoor = [n for n in sorted(exported_sets) if n not in WALLPIX_BIOME]
        if overland:
            lines += ["## Overland biome horizons (entries 7–14)", ""]
            for n in overland:
                biome = WALLPIX_BIOME[n].replace("_", " ")
                lines += [
                    f"### wall{n:02d} — {biome}",
                    "",
                    f"![wall{n:02d}]({image_prefix}/walls/wall{n:02d}.png)",
                    "",
                    f"![wall{n:02d} slices]({image_prefix}/walls/slices/wall{n:02d}-slices.png)",
                    "",
                ]
        if indoor:
            lines += ["## Indoor / dungeon wall sets", ""]
            for n in indoor:
                lines += [
                    f"### wall{n:02d}",
                    "",
                    f"![wall{n:02d}]({image_prefix}/walls/wall{n:02d}.png)",
                    "",
                    f"![wall{n:02d} slices]({image_prefix}/walls/slices/wall{n:02d}-slices.png)",
                    "",
                ]
    out_docs.write_text("\n".join(lines), encoding="utf-8")
    print(f"  markdown -> {out_docs}")


def write_index_markdown(out_docs: Path) -> None:
    lines = [
        "---",
        "title: MM1 gallery",
        "---",
        "",
        "# Might and Magic I — gallery",
        "",
        "| Section | Page |",
        "|---------|------|",
        "| 2D maps (55 screens) | [MM1 maps](/docs/gallery/mm1-maps) |",
        "| WALLPIX wall art | [MM1 walls](/docs/gallery/mm1-walls) |",
        "| Docs hub | [MM1 overview](/docs/reverse-engineering/50-mm1-overview) |",
        "| Map walker | [GitHub Pages ↗](https://vairn.github.io/MM2/mm1-maze-walker/) |",
        "",
    ]
    out_docs.write_text("\n".join(lines), encoding="utf-8")


def export_mm1_gallery(
    out: Path,
    *,
    mazedata: Path | None = None,
    exe: Path | None = None,
    wallpix_dir: Path | None = None,
    docs_out: Path | None = None,
    image_prefix: str = "/gallery/mm1",
    links: dict[str, str] | None = None,
    use_frontmatter: bool = True,
    scale: int = 2,
) -> None:
    out.mkdir(parents=True, exist_ok=True)
    md_root = docs_out or (ROOT / "wiki" / "docs" / "gallery")

    if mazedata and mazedata.is_file():
        print("Exporting MM1 maps…")
        export_maps(mazedata, out, exe=exe if exe and exe.is_file() else None, scale=scale)
        write_maps_markdown(
            md_root / "mm1-maps.md",
            image_prefix=image_prefix,
            links=links,
            use_frontmatter=use_frontmatter,
        )
    else:
        print(f"skip MM1 maps: missing MAZEDATA ({mazedata})")

    wp = wallpix_dir or DEFAULT_WALLPIX_EXPORT
    if wp.is_dir() and any(wp.glob("wall*.png")):
        print("Exporting MM1 WALLPIX…")
        exported = export_wall_slices(wp, out)
        write_walls_markdown(md_root / "mm1-walls.md", exported, image_prefix=image_prefix)
    else:
        print(f"skip MM1 walls: no lagdotcom exports in {wp}")
        write_walls_markdown(md_root / "mm1-walls.md", [], image_prefix=image_prefix)

    write_index_markdown(md_root / "mm1-index.md")
    print(f"Done -> {out}")


def main() -> int:
    ap = argparse.ArgumentParser(description="Export MM1 2D maps + WALLPIX gallery PNGs")
    ap.add_argument(
        "--out",
        type=Path,
        default=ROOT / "wiki" / "public" / "gallery" / "mm1",
        help="Output directory",
    )
    ap.add_argument("--mazedata", type=Path, default=DEFAULT_MAZEDATA)
    ap.add_argument("--exe", type=Path, default=DEFAULT_MM1_EXE)
    ap.add_argument("--wallpix", type=Path, default=None, help="lagdotcom exported/ dir")
    ap.add_argument("--docs-out", type=Path, default=None, help="Gallery markdown dir")
    ap.add_argument("--scale", type=int, default=2, help="Map render scale")
    args = ap.parse_args()
    export_mm1_gallery(
        args.out,
        mazedata=args.mazedata,
        exe=args.exe,
        wallpix_dir=args.wallpix,
        docs_out=args.docs_out,
        scale=args.scale,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

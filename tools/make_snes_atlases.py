#!/usr/bin/env python3
"""Build labeled SNES MM2 graphics atlases (Genesis-style contact sheets).

Packs composed scenes + monster sprites into overview PNGs under
EXTRACTED/snes/gfx/atlases/.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    raise SystemExit("pip install pillow")


ROOT = Path(__file__).resolve().parents[1]
SCENES = ROOT / "EXTRACTED" / "snes" / "gfx" / "scenes"
MONSTERS = ROOT / "EXTRACTED" / "snes" / "gfx" / "monsters_assembled"
OUT = ROOT / "EXTRACTED" / "snes" / "gfx" / "atlases"

BG = (18, 18, 24)
LABEL_BG = (28, 28, 36)
FG = (220, 220, 230)
ACCENT = (180, 160, 100)
PAD = 8
LABEL_H = 18
TITLE_H = 28


def font(size: int = 14):
    for name in ("consola.ttf", "Consolas.ttf", "DejaVuSansMono.ttf", "cour.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def load(path: Path) -> Image.Image | None:
    if not path.is_file():
        return None
    return Image.open(path).convert("RGBA")


def fit(img: Image.Image, max_w: int, max_h: int) -> Image.Image:
    w, h = img.size
    scale = min(max_w / w, max_h / h, 1.0)
    if scale < 1.0:
        img = img.resize((max(1, int(w * scale)), max(1, int(h * scale))), Image.NEAREST)
    return img


def cell(img: Image.Image, label: str, max_w: int, max_h: int, fnt) -> Image.Image:
    thumb = fit(img, max_w, max_h)
    cw, ch = max_w, max_h + LABEL_H
    out = Image.new("RGBA", (cw, ch), (*BG, 255))
    # center thumb
    ox = (cw - thumb.width) // 2
    oy = (max_h - thumb.height) // 2
    out.paste(thumb, (ox, oy), thumb if thumb.mode == "RGBA" else None)
    draw = ImageDraw.Draw(out)
    draw.rectangle((0, max_h, cw, ch), fill=LABEL_BG)
    draw.text((4, max_h + 2), label, fill=FG, font=fnt)
    return out


def pack_rows(
    title: str,
    rows: list[list[tuple[Image.Image, str]]],
    out_path: Path,
    cell_w: int,
    cell_h: int,
) -> None:
    fnt = font(13)
    title_fnt = font(16)
    row_imgs: list[Image.Image] = []
    max_row_w = 0
    for row in rows:
        cells = [cell(im, lab, cell_w, cell_h, fnt) for im, lab in row]
        rw = sum(c.width for c in cells) + PAD * (len(cells) + 1)
        rh = cells[0].height + PAD * 2 if cells else 0
        row_img = Image.new("RGBA", (rw, rh), (*BG, 255))
        x = PAD
        for c in cells:
            row_img.paste(c, (x, PAD), c)
            x += c.width + PAD
        row_imgs.append(row_img)
        max_row_w = max(max_row_w, rw)

    total_h = TITLE_H + sum(r.height for r in row_imgs)
    atlas = Image.new("RGBA", (max_row_w, total_h), (*BG, 255))
    draw = ImageDraw.Draw(atlas)
    draw.text((PAD, 6), title, fill=ACCENT, font=title_fnt)
    y = TITLE_H
    for r in row_imgs:
        atlas.paste(r, (0, y), r)
        y += r.height
    out_path.parent.mkdir(parents=True, exist_ok=True)
    atlas.convert("RGB").save(out_path)
    print(f"{out_path.name}: {atlas.size[0]}x{atlas.size[1]}")


def atlas_titles(out: Path) -> None:
    items = [
        ("title_screen.png", "credits $06:BA5C 16x16"),
        ("title_bg1_only.png", "BG1 logo/text"),
        ("title_bg2_only.png", "BG2 stone"),
        ("boot_title_screen.png", "boot 8bpp $1C/$08/$1F"),
    ]
    row = []
    for name, lab in items:
        im = load(SCENES / name)
        if im:
            row.append((im, lab))
    if row:
        pack_rows("SNES MM2 — Title screens", [row], out / "atlas_titles.png", 280, 280)


def atlas_wall_faces(out: Path) -> None:
    """Indoor faces with locked pals: town/cavern $13:8034, castle $13:8014."""
    walls = SCENES / "walls"
    items = [
        ("town_cavern_wall.png", "town=cavern wall $13:8034"),
        ("town_cavern_door.png", "door $13:8034"),
        ("castle_wall.png", "castle wall $13:8014"),
        ("castle_door.png", "castle door $13:8014"),
    ]
    row = []
    for name, lab in items:
        im = load(walls / name)
        if im:
            row.append((im, lab))
    if row:
        pack_rows(
            "SNES MM2 — Indoor faces (ASM pals)",
            [row],
            out / "atlas_wall_faces.png",
            220,
            180,
        )


def atlas_outdoor_faces(out: Path) -> None:
    walls = SCENES / "walls"
    items = [
        ("outdoor_mountains.png", "mountains $13:8034"),
        ("outdoor_trees_A.png", "trees A $13:8054"),
        ("outdoor_trees_B.png", "trees B $13:8054"),
    ]
    row = []
    for name, lab in items:
        im = load(walls / name)
        if im:
            row.append((im, lab))
    if row:
        pack_rows(
            "SNES MM2 — Outdoor faces ($00EE8D)",
            [row],
            out / "atlas_outdoor_faces.png",
            240,
            200,
        )


def atlas_env_faces(out: Path) -> None:
    """Full env contact: walls, doors, floors, outdoor."""
    walls = SCENES / "walls"
    row1 = []
    for name, lab in [
        ("town_cavern_wall.png", "wall $13:8034"),
        ("town_cavern_door.png", "door $13:8034"),
        ("castle_wall.png", "castle $13:8014"),
        ("castle_door.png", "castle door $13:8014"),
    ]:
        im = load(walls / name)
        if im:
            row1.append((im, lab))
    row2 = []
    for name, lab in [
        ("floor_town_cavern_day.png", "floor day $13:8034"),
        ("floor_town_cavern_night.png", "floor night $1D:808A"),
        ("floor_castle_day.png", "floor castle $13:8014"),
        ("town_cavern_wall_night.png", "wall night $1D:808A"),
    ]:
        im = load(walls / name)
        if im:
            row2.append((im, lab))
    row3 = []
    for name, lab in [
        ("outdoor_mountains.png", "mountains"),
        ("outdoor_trees_A.png", "trees A"),
        ("outdoor_trees_B.png", "trees B"),
        ("outdoor_floor_terr5.png", "floor $13:8054"),
    ]:
        im = load(walls / name)
        if im:
            row3.append((im, lab))
    rows = [r for r in (row1, row2, row3) if r]
    if rows:
        pack_rows(
            "SNES MM2 — Env faces (CGRAM $40 masonry)",
            rows,
            out / "atlas_env_faces.png",
            180,
            140,
        )


def atlas_doors_floors(out: Path) -> None:
    walls = SCENES / "walls"
    row1 = []
    for name, lab in [
        ("town_cavern_door.png", "door $13:8034"),
        ("town_cavern_door_night.png", "door night $1D:808A"),
        ("castle_door.png", "castle door $13:8014"),
    ]:
        im = load(walls / name)
        if im:
            row1.append((im, lab))
    row2 = []
    for name, lab in [
        ("floor_town_cavern_day.png", "floor A day"),
        ("floor_castle_day.png", "floor B day"),
        ("outdoor_floor_terr5.png", "outdoor t5 $13:8054"),
        ("outdoor_floor_terr8.png", "outdoor t8 $13:8054"),
    ]:
        im = load(walls / name)
        if im:
            row2.append((im, lab))
    rows = [r for r in (row1, row2) if r]
    if rows:
        pack_rows(
            "SNES MM2 — Doors + floors (CGRAM $40)",
            rows,
            out / "atlas_doors_floors.png",
            220,
            170,
        )


def atlas_env_sheets(out: Path) -> None:
    walls = SCENES / "walls"
    row1 = []
    for name, lab in [
        ("wallA_20x7.png", "floorA $13:8034"),
        ("wallB_20x7.png", "floorB $13:8014"),
        ("sky_11x11_day.png", "sky day $1B:8000"),
        ("sky_11x11_dusk.png", "sky dusk $1D:806A"),
    ]:
        im = load(walls / name)
        if im:
            row1.append((im, lab))
    row2 = []
    for name, lab in [
        ("wallA_map_E2E0.png", "map E2E0"),
        ("wallA_map_E424.png", "map E424"),
        ("wallB_map_E540.png", "map E540"),
        ("sky_map_108000_day.png", "sky map"),
    ]:
        im = load(walls / name)
        if im:
            row2.append((im, lab))
    rows = [r for r in (row1, row2) if r]
    if rows:
        pack_rows(
            "SNES MM2 — Wall/sky sheets (locked pals)",
            rows,
            out / "atlas_env_sheets.png",
            280,
            160,
        )


def atlas_explore(out: Path) -> None:
    row = []
    for name, lab in [
        ("explore_A2_0.png", "explore A2=0 stack"),
        ("explore_A2_2.png", "explore A2=2 stack"),
        ("explore_viewport_crop.png", "WRAM map crop"),
    ]:
        im = load(SCENES / name)
        if im:
            row.append((im, lab))
    if row:
        pack_rows(
            "SNES MM2 — Explore stacks",
            [row],
            out / "atlas_explore.png",
            300,
            280,
        )


def atlas_monsters(out: Path, cols: int = 10, thumb: int = 96) -> None:
    if not MONSTERS.is_dir():
        print("no monsters_assembled/; skip monster atlas")
        return
    # Prefer base sprites (monster_NN.png), skip frame sheets
    files = sorted(
        p
        for p in MONSTERS.glob("monster_*.png")
        if re.fullmatch(r"monster_\d+\.png", p.name)
    )
    if not files:
        # fallback: any without 'frames' in name
        files = sorted(p for p in MONSTERS.glob("*.png") if "frame" not in p.name.lower())
    cells: list[tuple[Image.Image, str]] = []
    for p in files:
        im = load(p)
        if im:
            m = re.search(r"(\d+)", p.stem)
            lab = f"#{int(m.group(1)):02d}" if m else p.stem
            cells.append((im, lab))
    rows: list[list[tuple[Image.Image, str]]] = []
    for i in range(0, len(cells), cols):
        rows.append(cells[i : i + cols])
    if rows:
        pack_rows(
            f"SNES MM2 — Monsters ({len(cells)} sprites, $14:8060 / $00F62E)",
            rows,
            out / "atlas_monsters.png",
            thumb,
            thumb,
        )


def atlas_monsters_frames(out: Path, limit: int = 24, thumb: int = 140) -> None:
    """Contact sheet of anim frame strips (first N with _frames in name)."""
    if not MONSTERS.is_dir():
        return
    files = sorted(MONSTERS.glob("monster_*frame*.png"))[:limit]
    if not files:
        files = sorted(MONSTERS.glob("*frames*.png"))[:limit]
    cells = []
    for p in files:
        im = load(p)
        if im:
            cells.append((im, p.stem.replace("monster_", "#")))
    rows = [cells[i : i + 4] for i in range(0, len(cells), 4)]
    if rows:
        pack_rows(
            f"SNES MM2 — Monster anim sheets (first {len(cells)})",
            rows,
            out / "atlas_monster_anims.png",
            thumb * 2,
            thumb,
        )


def atlas_master(out: Path) -> None:
    """Overview with locked-palette assets."""
    walls = SCENES / "walls"
    row1 = []
    for name, lab in [
        ("boot_title_screen.png", "boot title"),
        ("title_screen.png", "credits title"),
    ]:
        im = load(SCENES / name)
        if im:
            row1.append((im, lab))
    row2 = []
    for name, lab in [
        ("town_cavern_wall.png", "town/cavern"),
        ("town_cavern_door.png", "door"),
        ("castle_wall.png", "castle"),
        ("castle_door.png", "castle door"),
    ]:
        im = load(walls / name)
        if im:
            row2.append((im, lab))
    row3 = []
    for name, lab in [
        ("outdoor_mountains.png", "mountains"),
        ("outdoor_trees_A.png", "trees A"),
        ("outdoor_trees_B.png", "trees B"),
        ("sky_11x11_day.png", "sky day"),
    ]:
        im = load(walls / name)
        if im:
            row3.append((im, lab))
    row4 = []
    for name, lab, folder in [
        ("floor_town_cavern_day.png", "floor A", walls),
        ("floor_castle_day.png", "floor B", walls),
        ("explore_A2_0.png", "explore town", SCENES),
        ("explore_A2_2.png", "explore castle", SCENES),
    ]:
        im = load(folder / name)
        if im:
            row4.append((im, lab))
    rows = [r for r in (row1, row2, row3, row4) if r]
    if rows:
        pack_rows(
            "SNES MM2 — Master (locked pals)",
            rows,
            out / "atlas_master.png",
            200,
            160,
        )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--outdir", type=Path, default=OUT)
    args = ap.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)

    atlas_master(args.outdir)
    atlas_titles(args.outdir)
    atlas_wall_faces(args.outdir)
    atlas_outdoor_faces(args.outdir)
    atlas_env_faces(args.outdir)
    atlas_doors_floors(args.outdir)
    atlas_env_sheets(args.outdir)
    atlas_explore(args.outdir)
    atlas_monsters(args.outdir)
    atlas_monsters_frames(args.outdir)
    print(f"-> {args.outdir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

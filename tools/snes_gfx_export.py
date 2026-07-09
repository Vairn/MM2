#!/usr/bin/env python3
"""Export SNES MM2 graphics for the VitePress / GitHub Wiki gallery."""
from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

GALLERY_CACHE_VERSION = 2

VITEPRESS_LINKS = {
    "index": "/docs/gallery/snes-index",
    "atlases": "/docs/gallery/snes-atlases",
    "walls": "/docs/gallery/snes-walls",
    "monsters": "/docs/gallery/snes-monsters",
    "titles": "/docs/gallery/snes-titles",
    "explore": "/docs/gallery/snes-explore",
    "format": "/docs/reverse-engineering/snes-graphics",
    "gallery_home": "/docs/gallery/",
}

GITHUB_WIKI_LINKS = {
    "index": "SNES-Gallery",
    "atlases": "SNES-Atlases",
    "walls": "SNES-Walls",
    "monsters": "SNES-Monsters",
    "titles": "SNES-Titles",
    "explore": "SNES-Explore",
    "format": "SNES-Graphics",
    "gallery_home": "Gallery",
}

VITEPRESS_PAGES = {
    "index": "snes-index.md",
    "atlases": "snes-atlases.md",
    "walls": "snes-walls.md",
    "monsters": "snes-monsters.md",
    "titles": "snes-titles.md",
    "explore": "snes-explore.md",
}

GITHUB_WIKI_PAGES = {
    "index": "SNES-Gallery.md",
    "atlases": "SNES-Atlases.md",
    "walls": "SNES-Walls.md",
    "monsters": "SNES-Monsters.md",
    "titles": "SNES-Titles.md",
    "explore": "SNES-Explore.md",
}

# Prefer uppercase SNES tree; fall back to lowercase snes (same on Windows).
SNES_GFX_CANDIDATES = [
    ROOT / "EXTRACTED" / "SNES" / "gfx" / "export",
    ROOT / "EXTRACTED" / "snes" / "gfx" / "export",
]

ATLAS_LABELS = {
    "atlas_master.png": "Master overview",
    "atlas_titles.png": "Title / boot screens",
    "atlas_wall_faces.png": "Indoor wall face sheets",
    "atlas_outdoor_faces.png": "Outdoor terrain faces",
    "atlas_env_faces.png": "Environment faces",
    "atlas_doors_floors.png": "Doors, floors, sky",
    "atlas_env_sheets.png": "Environment CHR sheets",
    "atlas_explore.png": "Exploration UI",
    "atlas_monsters.png": "Monster portraits",
    "atlas_monster_anims.png": "Monster animation frames",
}


def resolve_snes_export() -> Path | None:
    for path in SNES_GFX_CANDIDATES:
        if path.is_dir() and (path / "manifest.json").is_file():
            return path
    return None


def img(path: str, *, prefix: str) -> str:
    return f"{prefix.rstrip('/')}/{path.lstrip('/')}?v={GALLERY_CACHE_VERSION}"


def _write_page(path: Path, text: str) -> None:
    """Write markdown with exact filename casing (Windows is case-insensitive)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    desired = path.name
    for p in path.parent.iterdir():
        if p.is_file() and p.suffix == ".md" and p.name.lower() == desired.lower():
            p.unlink()
    tmp = path.parent / f"__wiki_tmp_{desired}"
    if tmp.exists():
        tmp.unlink()
    tmp.write_text(text, encoding="utf-8")
    tmp.rename(path.parent / desired)


def sync_to_gallery(snes_export: Path, gallery_out: Path) -> Path:
    """Copy EXTRACTED/SNES/gfx/export tree into wiki/public/gallery/snes/."""
    dest = gallery_out / "snes"
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(snes_export, dest)
    return dest


def write_snes_monster_thumbs(gallery_snes: Path) -> None:
    """Tight-cropped portrait thumbs for gallery / compare (trim transparent sheet padding)."""
    try:
        from PIL import Image
    except ImportError:
        return
    from gen_gfx_compare_html import (  # noqa: E402
        flatten_rgba,
        pad_rgba,
        pil_save,
        prepare_snes_monster_thumb,
    )

    src_dir = gallery_snes / "monsters" / "sprites"
    dst_dir = gallery_snes / "monsters" / "thumbs"
    if not src_dir.is_dir():
        return
    if dst_dir.exists():
        shutil.rmtree(dst_dir)
    dst_dir.mkdir(parents=True)
    for sprite in sorted(src_dir.glob("monster_*.png")):
        im = prepare_snes_monster_thumb(Image.open(sprite))
        pil_save(flatten_rgba(pad_rgba(im)), dst_dir / sprite.name, scale=1)


def _link(target: str, *, links: dict[str, str], label: str | None = None) -> str:
    href = links[target]
    text = label or href
    if href.startswith("/"):
        return f"[{text}]({href})"
    return f"[{text}]({href})"


def _fm(title: str, *, use_frontmatter: bool) -> list[str]:
    if not use_frontmatter:
        return []
    return ["---", f"title: {title}", "---", ""]


def write_index_markdown(
    docs_out: Path,
    *,
    image_prefix: str,
    has_assets: bool,
    links: dict[str, str],
    use_frontmatter: bool,
    page_names: dict[str, str],
) -> None:
    lines = [
        *_fm("SNES graphics", use_frontmatter=use_frontmatter),
        "# SNES graphics gallery",
        "",
        "CHR sheets and composed scenes from the European SNES cartridge — decoded via",
        "`tools/export_snes_gfx.py` (RAW 4bpp tiles + BGR555 palettes, no on-cartridge",
        "decompressor).",
        "",
        f"Format reference: {_link('format', links=links, label='SNES graphics')}.",
        "",
    ]
    if not has_assets:
        if use_frontmatter:
            lines += [
                "::: warning No local exports",
                "Run `python tools/export_snes_gfx.py` after composing atlases.",
                ":::",
                "",
            ]
        else:
            lines += [
                "> **No local exports** — run `python tools/export_snes_gfx.py` after composing atlases.",
                "",
            ]
    else:
        lines += [
            "| Gallery | Description |",
            "|---------|-------------|",
            f"| {_link('atlases', links=links, label='Atlases')} | Contact-sheet previews |",
            f"| {_link('walls', links=links, label='Wall faces')} | Indoor/outdoor sheets + panel crops |",
            f"| {_link('monsters', links=links, label='Monsters')} | 77 combat portraits + frame strips |",
            f"| {_link('titles', links=links, label='Titles')} | Title and boot screens |",
            f"| {_link('explore', links=links, label='Explore UI')} | Field exploration overlays |",
            "",
            "## Output layout",
            "",
            "- **walls/panels_native/** — per-depth face crops @ 8 px/tile (rewrite-ready)",
            "- **walls/sheets_x2/** — full 20×16 face sheets @ 16 px/tile",
            "- **monsters/sprites/** — assembled portrait per table index",
            "- **monsters/frames/** — horizontal animation strip per monster",
            "- **atlases/** — labeled montages from `make_snes_atlases.py`",
            "",
        ]
    _write_page(docs_out / page_names["index"], "\n".join(lines))


def write_atlases_markdown(
    docs_out: Path,
    gallery_snes: Path,
    *,
    image_prefix: str,
    links: dict[str, str],
    use_frontmatter: bool,
    page_names: dict[str, str],
) -> None:
    lines = [
        *_fm("SNES atlases", use_frontmatter=use_frontmatter),
        "# SNES atlas contact sheets",
        "",
        f"[← SNES gallery]({links['index']})",
        "",
        "Preview montages built from ROM CHR + palette tables. Regenerate with",
        "`python tools/make_snes_atlases.py`.",
        "",
    ]
    atlas_dir = gallery_snes / "atlases"
    if atlas_dir.is_dir():
        for name in sorted(ATLAS_LABELS):
            if not (atlas_dir / name).is_file():
                continue
            label = ATLAS_LABELS[name]
            lines += [
                f"## {label}",
                "",
                f"![{label}]({img(f'atlases/{name}', prefix=image_prefix)})",
                "",
            ]
    _write_page(docs_out / page_names["atlases"], "\n".join(lines))


def write_walls_markdown(
    docs_out: Path,
    gallery_snes: Path,
    snes_export: Path,
    *,
    image_prefix: str,
    links: dict[str, str],
    use_frontmatter: bool,
    page_names: dict[str, str],
) -> None:
    manifest_faces: list[dict] = []
    manifest_path = snes_export / "manifest.json"
    if manifest_path.is_file():
        try:
            manifest_faces = json.loads(manifest_path.read_text(encoding="utf-8")).get("faces", [])
        except json.JSONDecodeError:
            pass
    face_by_name = {f["name"]: f for f in manifest_faces}

    lines = [
        *_fm("SNES wall faces", use_frontmatter=use_frontmatter),
        "# SNES wall / terrain faces",
        "",
        f"[← SNES gallery]({links['index']})",
        "",
        "Face sheets are 20×16 tiles (160×128 px native). **panels_native/** crops",
        "individual depth strips; `amiga_frame_hint` in `manifest.json` is a size/role",
        "guess only — SNES paint order ≠ Amiga `town.32` frame ladder.",
        "",
    ]

    sheets_dir = gallery_snes / "walls" / "sheets_x2"
    panels_root = gallery_snes / "walls" / "panels_native"
    if sheets_dir.is_dir():
        stems = sorted(
            p.stem
            for p in sheets_dir.glob("*.png")
            if not p.stem.endswith("_alpha")
        )
        for stem in stems:
            meta = face_by_name.get(stem, {})
            notes = meta.get("notes", "")
            lines += [f"## `{stem}`" + (f" — {notes}" if notes else ""), ""]
            sheet_rel = f"walls/sheets_x2/{stem}.png"
            if (gallery_snes / sheet_rel).is_file():
                lines.append(f"![{stem} sheet]({img(sheet_rel, prefix=image_prefix)})")
                lines.append("")

            panel_dir = panels_root / stem
            if panel_dir.is_dir():
                panels = sorted(panel_dir.glob("*.png"))[:12]
                if panels:
                    lines += ["| Panel | Preview |", "|-------|---------|"]
                    for panel in panels:
                        rel = f"walls/panels_native/{stem}/{panel.name}"
                        lines.append(
                            f"| `{panel.stem}` | ![{panel.stem}]({img(rel, prefix=image_prefix)}) |"
                        )
                    total = len(list(panel_dir.glob("*.png")))
                    if total > len(panels):
                        lines.append("")
                        lines.append(f"*{total - len(panels)} more panels in `panels_native/{stem}/`*")
                    lines.append("")

    for sub, title in (
        ("floors_x2", "Floors"),
        ("sky_x2", "Sky bands"),
    ):
        sub_dir = gallery_snes / "walls" / sub
        if not sub_dir.is_dir():
            continue
        lines += [f"## {title}", ""]
        for png in sorted(sub_dir.glob("*.png")):
            rel = f"walls/{sub}/{png.name}"
            lines += [f"### `{png.stem}`", "", f"![{png.stem}]({img(rel, prefix=image_prefix)})", ""]

    _write_page(docs_out / page_names["walls"], "\n".join(lines))


def write_monsters_markdown(
    docs_out: Path,
    gallery_snes: Path,
    *,
    image_prefix: str,
    links: dict[str, str],
    use_frontmatter: bool,
    page_names: dict[str, str],
) -> None:
    lines = [
        *_fm("SNES monsters", use_frontmatter=use_frontmatter),
        "# SNES combat monsters",
        "",
        f"[← SNES gallery]({links['index']})",
        "",
        "77 assembled portraits from monster table `$14:8060` (indices 0–76).",
        "Frame strips show per-slot animation CHR laid out horizontally.",
        "",
    ]
    atlas = gallery_snes / "atlases" / "atlas_monsters.png"
    if atlas.is_file():
        lines += [
            "## Overview",
            "",
            f"![monster atlas]({img('atlases/atlas_monsters.png', prefix=image_prefix)})",
            "",
        ]
    anim_atlas = gallery_snes / "atlases" / "atlas_monster_anims.png"
    if anim_atlas.is_file():
        lines += [
            f"![monster anims]({img('atlases/atlas_monster_anims.png', prefix=image_prefix)})",
            "",
        ]

    sprites_dir = gallery_snes / "monsters" / "sprites"
    thumbs_dir = gallery_snes / "monsters" / "thumbs"
    frames_dir = gallery_snes / "monsters" / "frames"
    if sprites_dir.is_dir():
        lines += ["## By index", "", "| # | Portrait | Frames |", "|---|----------|--------|"]
        for sprite in sorted(sprites_dir.glob("monster_*.png")):
            idx = sprite.stem.replace("monster_", "")
            thumb = thumbs_dir / sprite.name
            sprite_rel = (
                f"monsters/thumbs/{sprite.name}"
                if thumb.is_file()
                else f"monsters/sprites/{sprite.name}"
            )
            frame_name = f"monster_{idx}_frames.png"
            frame_rel = f"monsters/frames/{frame_name}"
            frame_cell = ""
            if (frames_dir / frame_name).is_file():
                frame_cell = f"![f{idx}]({img(frame_rel, prefix=image_prefix)})"
            lines.append(
                f"| {idx} | ![{idx}]({img(sprite_rel, prefix=image_prefix)}) | {frame_cell} |"
            )
        lines.append("")

    _write_page(docs_out / page_names["monsters"], "\n".join(lines))


def write_simple_image_page(
    docs_out: Path,
    gallery_snes: Path,
    *,
    page_key: str,
    title: str,
    image_prefix: str,
    blurb: str,
    links: dict[str, str],
    use_frontmatter: bool,
    page_names: dict[str, str],
    subdir: str,
) -> None:
    lines = [
        *_fm(title, use_frontmatter=use_frontmatter),
        f"# {title}",
        "",
        f"[← SNES gallery]({links['index']})",
        "",
        blurb,
        "",
    ]
    src = gallery_snes / subdir
    if src.is_dir():
        for png in sorted(src.glob("*.png")):
            rel = f"{subdir}/{png.name}"
            lines += [f"## `{png.stem}`", "", f"![{png.stem}]({img(rel, prefix=image_prefix)})", ""]
    _write_page(docs_out / page_names[page_key], "\n".join(lines))


def patch_main_gallery_index(docs_out: Path) -> None:
    main_index = docs_out / "index.md"
    if not main_index.is_file():
        return
    text = main_index.read_text(encoding="utf-8")
    link = "| [SNES](/docs/gallery/snes-index) | SNES CHR sheets — walls, monsters, UI |"
    if "snes-index" not in text:
        needle = "| [3D Views]"
        if needle in text:
            text = text.replace(needle, link + "\n" + needle)
            main_index.write_text(text, encoding="utf-8")


def write_gallery_markdown(
    docs_out: Path,
    gallery_snes: Path,
    snes_export: Path,
    *,
    image_prefix: str = "/gallery/snes",
    links: dict[str, str] | None = None,
    use_frontmatter: bool = True,
    page_names: dict[str, str] | None = None,
) -> None:
    docs_out.mkdir(parents=True, exist_ok=True)
    link_map = links or VITEPRESS_LINKS
    pages = page_names or VITEPRESS_PAGES
    has_assets = gallery_snes.is_dir()
    write_index_markdown(
        docs_out,
        image_prefix=image_prefix,
        has_assets=has_assets,
        links=link_map,
        use_frontmatter=use_frontmatter,
        page_names=pages,
    )
    if not has_assets:
        for key in ("atlases", "walls", "monsters", "titles", "explore"):
            stub = docs_out / pages[key]
            if not stub.is_file():
                stub.write_text(
                    ("---\n" if use_frontmatter else "")
                    + ("title: SNES gallery\n---\n\n" if use_frontmatter else "# SNES gallery\n\n")
                    + "No SNES exports found. Run `python tools/export_snes_gfx.py`.\n",
                    encoding="utf-8",
                )
        return

    write_atlases_markdown(
        docs_out,
        gallery_snes,
        image_prefix=image_prefix,
        links=link_map,
        use_frontmatter=use_frontmatter,
        page_names=pages,
    )
    write_walls_markdown(
        docs_out,
        gallery_snes,
        snes_export,
        image_prefix=image_prefix,
        links=link_map,
        use_frontmatter=use_frontmatter,
        page_names=pages,
    )
    write_monsters_markdown(
        docs_out,
        gallery_snes,
        image_prefix=image_prefix,
        links=link_map,
        use_frontmatter=use_frontmatter,
        page_names=pages,
    )
    write_simple_image_page(
        docs_out,
        gallery_snes,
        page_key="titles",
        title="SNES title screens",
        image_prefix=image_prefix,
        blurb="Title and boot screens composed from ROM CHR.",
        links=link_map,
        use_frontmatter=use_frontmatter,
        page_names=pages,
        subdir="titles",
    )
    write_simple_image_page(
        docs_out,
        gallery_snes,
        page_key="explore",
        title="SNES explore UI",
        image_prefix=image_prefix,
        blurb="Exploration overlay samples (field UI).",
        links=link_map,
        use_frontmatter=use_frontmatter,
        page_names=pages,
        subdir="explore",
    )
    if use_frontmatter:
        patch_main_gallery_index(docs_out)


def export_all(
    gallery_out: Path,
    docs_out: Path,
    *,
    snes_export: Path | None = None,
    image_prefix: str = "/gallery/snes",
    skip_extract: bool = False,
    links: dict[str, str] | None = None,
    use_frontmatter: bool = True,
    page_names: dict[str, str] | None = None,
) -> int:
    src = snes_export or resolve_snes_export()
    if src is None and not skip_extract:
        try:
            from export_snes_gfx import export as run_export  # noqa: E402

            print("SNES export missing — running tools/export_snes_gfx.py…")
            run_export(SNES_GFX_CANDIDATES[0])
            src = resolve_snes_export()
        except Exception as exc:
            print(f"SNES gfx export failed: {exc}", file=sys.stderr)

    gallery_snes = gallery_out / "snes"
    if src is not None:
        sync_to_gallery(src, gallery_out)
        print(f"Gallery synced -> {gallery_snes}")
        write_snes_monster_thumbs(gallery_snes)
    else:
        print("skip SNES gallery: no exports in EXTRACTED/SNES/gfx/export", file=sys.stderr)
        gallery_snes = gallery_out / "snes"

    write_gallery_markdown(
        docs_out,
        gallery_snes if gallery_snes.is_dir() else Path("__missing__"),
        src or Path("__missing__"),
        image_prefix=image_prefix,
        links=links,
        use_frontmatter=use_frontmatter,
        page_names=page_names,
    )
    print(f"Markdown -> {docs_out}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Export SNES gfx gallery for wiki")
    ap.add_argument(
        "--snes-export",
        type=Path,
        default=None,
        help="EXTRACTED/SNES/gfx/export root",
    )
    ap.add_argument(
        "--gallery",
        type=Path,
        default=ROOT / "wiki" / "public" / "gallery",
    )
    ap.add_argument(
        "--docs",
        type=Path,
        default=ROOT / "wiki" / "docs" / "gallery",
    )
    ap.add_argument("--skip-extract", action="store_true")
    ap.add_argument(
        "--image-prefix",
        default="/gallery/snes",
        help="URL prefix for markdown image paths",
    )
    ap.add_argument(
        "--github-wiki",
        action="store_true",
        help="Write GitHub Wiki page names/links (no YAML frontmatter)",
    )
    args = ap.parse_args()
    links = GITHUB_WIKI_LINKS if args.github_wiki else None
    pages = GITHUB_WIKI_PAGES if args.github_wiki else None
    prefix = args.image_prefix
    if args.github_wiki and prefix == "/gallery/snes":
        prefix = "images/gallery/snes"
    return export_all(
        args.gallery,
        args.docs,
        snes_export=args.snes_export,
        image_prefix=prefix,
        skip_extract=args.skip_extract,
        links=links,
        use_frontmatter=not args.github_wiki,
        page_names=pages,
    )


if __name__ == "__main__":
    raise SystemExit(main())

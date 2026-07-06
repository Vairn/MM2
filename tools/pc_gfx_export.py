#!/usr/bin/env python3
"""Export PC DOS (.4 / .16) graphics for the VitePress wiki gallery."""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

from decode_pc_gfx import (  # noqa: E402
    DEFAULT_PC_GFX_OUT,
    batch_extract_gog,
    extract_monsters_atlas,
    extract_wall_sheet,
    is_monsters_file,
    wall_out_dir,
)

GALLERY_CACHE_VERSION = 1

# Wall/sprite sheets worth highlighting in the gallery index.
WALL_SHEET_STEMS = [
    "sky", "townf", "castle", "town", "cave", "throw", "master", "endgame",
    "desert", "ocean", "tundra", "swamp", "outdoor1", "outdoor2", "outdoor3",
]

GOG_DEFAULT = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")


def resolve_gog(gog: Path | None) -> Path | None:
    if gog and gog.is_dir():
        return gog
    if GOG_DEFAULT.is_dir() and any(GOG_DEFAULT.glob("*.4")):
        return GOG_DEFAULT
    if (ROOT / "CASTLE.4").is_file():
        return ROOT
    return None


def sync_to_gallery(pc_gfx: Path, gallery_out: Path) -> None:
    """Copy EXTRACTED/pc_gfx tree into wiki/public/gallery/pc/."""
    dest = gallery_out / "pc"
    if dest.exists():
        shutil.rmtree(dest)
    if pc_gfx.exists():
        shutil.copytree(pc_gfx, dest)


def img(path: str) -> str:
    return f"/gallery/pc/{path.lstrip('/')}?v={GALLERY_CACHE_VERSION}"


def write_gallery_markdown(gallery_out: Path, docs_out: Path, pc_gfx: Path) -> None:
    docs_out.mkdir(parents=True, exist_ok=True)

    def has_variant(variant: str, rel: str) -> bool:
        return (pc_gfx / variant / rel).exists()

    # --- index ---
    index_lines = [
        "---",
        "title: PC DOS Graphics",
        "---",
        "",
        "# PC DOS graphics gallery",
        "",
        "CGA (`.4`) and EGA (`.16`) assets from the GOG PC port — decoded from",
        "`MM2.EXE` + `CGA.DRV` / `EGA.DRV` assembly.",
        "",
        "Format reference: [PC DOS graphics formats](/docs/reverse-engineering/54-pc-dos-graphics-formats).",
        "",
        "| Gallery | Description |",
        "|---------|-------------|",
        "| [CGA wall sheets](/docs/gallery/pc-cga) | `.4` environment / UI sprites |",
        "| [EGA wall sheets](/docs/gallery/pc-ega) | `.16` environment / UI sprites |",
        "| [CGA monsters](/docs/gallery/pc-monsters-cga) | Combat sprites + animations |",
        "| [EGA monsters](/docs/gallery/pc-monsters-ega) | Combat sprites + animations |",
        "| [Platform index (numbered)](/docs/gallery/platform-compare) | All slots: monsters + walls, Amiga/CGA/EGA |",
        "",
        "## Output layout",
        "",
        "**Wall sheets:** `raw/frameNN.png` (per slot) + `sheet.png` (montage).",
        "",
        "**Monsters:** `NN.gif` + `NN/` folders matching repo `NN.anm` indices.",
        "96×96 base/steps, and per-script GIFs.",
        "",
    ]
    (docs_out / "pc-index.md").write_text("\n".join(index_lines), encoding="utf-8")

    for variant, title, doc_name in (
        ("cga", "CGA (.4)", "pc-cga"),
        ("ega", "EGA (.16)", "pc-ega"),
    ):
        lines = [
            "---",
            f"title: {title} wall sheets",
            "---",
            "",
            f"# {title} wall / sprite sheets",
            "",
            f"[← PC gallery](/docs/gallery/pc-index)",
            "",
            "Each sheet: **raw/** = individual frames; **sheet.png** = reconstructed atlas.",
            "",
        ]
        var_dir = pc_gfx / variant
        if var_dir.is_dir():
            stems = sorted(
                p.name for p in var_dir.iterdir()
                if p.is_dir() and (p / "raw").is_dir()
            )
            for stem in stems:
                sheet = f"{variant}/{stem}/sheet.png"
                raw0 = f"{variant}/{stem}/raw/frame00.png"
                thumb = raw0 if has_variant(variant, f"{stem}/raw/frame00.png") else sheet
                if not (gallery_out / "pc" / thumb.split("?", 1)[0]).exists():
                    continue
                lines += [
                    f"## `{stem.upper()}`",
                    "",
                    f"![{stem} sheet]({img(sheet)})",
                    "",
                ]
                raw_dir = var_dir / stem / "raw"
                frames = sorted(raw_dir.glob("frame*.png"))[:8]
                if frames:
                    lines.append("| Frame | Preview |")
                    lines.append("|-------|---------|")
                    for fr in frames:
                        rel = f"{variant}/{stem}/raw/{fr.name}"
                        idx = fr.stem.replace("frame", "")
                        lines.append(f"| {idx} | ![f{idx}]({img(rel)}) |")
                    if len(list(raw_dir.glob("frame*.png"))) > 8:
                        lines.append("")
                        lines.append(f"*+{len(list(raw_dir.glob('frame*.png'))) - 8} more in raw/*")
                    lines.append("")
        (docs_out / f"{doc_name}.md").write_text("\n".join(lines), encoding="utf-8")

    for variant, title, doc_name in (
        ("cga", "CGA", "pc-monsters-cga"),
        ("ega", "EGA", "pc-monsters-ega"),
    ):
        lines = [
            "---",
            f"title: {title} combat monsters",
            "---",
            "",
            f"# {title} combat monsters",
            "",
            f"[← PC gallery](/docs/gallery/pc-index)",
            "",
            "Indices match repo **`NN.anm`** files (72 combat sprites).",
            "",
            "- **`NN.gif`** — showcase animation (variant root)",
            "- **`NN/frames/`** — patch frames (frame 0 = body, others = overlays)",
            "- **`NN/base.png`**, script steps, and script GIFs",
            "",
        ]
        variant_dir = pc_gfx / "monsters" / variant
        if variant_dir.is_dir():
            for gf in sorted(variant_dir.glob("[0-9][0-9].gif")):
                anim = gf.stem
                anim_dir = variant_dir / anim
                lines += [f"## Anim {anim} (`{anim}.anm`)", ""]
                lines.append(f"![{anim}]({img(f'monsters/{variant}/{gf.name}')})")
                lines.append("")
                base = anim_dir / "base.png"
                if base.is_file():
                    lines.append(
                        f"Base: ![{anim} base]({img(f'monsters/{variant}/{anim}/base.png')})"
                    )
                    lines.append("")
                script_gifs = sorted(anim_dir.glob("s[0-9]*.gif"))[:3]
                for sg in script_gifs:
                    lines.append(
                        f"![{sg.stem}]({img(f'monsters/{variant}/{anim}/{sg.name}')})"
                    )
                if script_gifs:
                    lines.append("")
        (docs_out / f"{doc_name}.md").write_text("\n".join(lines), encoding="utf-8")

    # Patch main gallery index if present
    main_index = docs_out / "index.md"
    if main_index.is_file():
        text = main_index.read_text(encoding="utf-8")
        link = "| [PC DOS (CGA/EGA)](/docs/gallery/pc-index) | `.4` / `.16` wall sheets and combat monsters |"
        if "pc-index" not in text and "## Galleries" in text:
            text = text.replace("## Galleries", "## Galleries\n\n" + link + "\n")
            main_index.write_text(text, encoding="utf-8")


def export_all(
    gog_dir: Path | None,
    pc_gfx_out: Path,
    gallery_out: Path,
    docs_out: Path,
    *,
    skip_extract: bool = False,
    cga_palette: int = 1,
) -> int:
    if not skip_extract:
        gog = resolve_gog(gog_dir)
        if gog is None:
            print("No GOG dir found — using existing EXTRACTED/pc_gfx if present", file=sys.stderr)
        else:
            print(f"Extracting from {gog} -> {pc_gfx_out}")
            batch_extract_gog(gog, pc_gfx_out, cga_palette=cga_palette)

    sync_to_gallery(pc_gfx_out, gallery_out)
    write_gallery_markdown(gallery_out, docs_out, pc_gfx_out)
    print(f"Gallery synced -> {gallery_out / 'pc'}")
    print(f"Markdown -> {docs_out}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Export PC DOS gfx gallery for wiki")
    ap.add_argument("--gog", type=Path, default=None, help="GOG MM2 install dir")
    ap.add_argument(
        "--pc-gfx",
        type=Path,
        default=DEFAULT_PC_GFX_OUT,
        help="decode_pc_gfx output root",
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
    ap.add_argument("--cga-palette", type=int, choices=(0, 1), default=1)
    args = ap.parse_args()
    return export_all(
        args.gog,
        args.pc_gfx,
        args.gallery,
        args.docs,
        skip_extract=args.skip_extract,
        cga_palette=args.cga_palette,
    )


if __name__ == "__main__":
    raise SystemExit(main())

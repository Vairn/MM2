#!/usr/bin/env python3
"""Export MM2 docs + sprite gallery for GitHub Wiki (Home.md, _Sidebar.md, images/)."""
from __future__ import annotations

import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WIKI = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from mm2_gfx_export import export_all, resolve_data_dir  # noqa: E402

OUT = WIKI / "gh-wiki"
REPO = "Vairn/MM2"
WIKI_GIT = f"https://github.com/{REPO}.wiki.git"

# Source markdown → GitHub Wiki page title (becomes Page-Title.md)
DOC_SOURCES: list[tuple[str, str]] = [
    ("EXTRACTED/docs/00-overview.md", "Overview"),
    ("EXTRACTED/docs/01-startup-init.md", "Startup-and-Init"),
    ("EXTRACTED/docs/02-runtime-memory-map.md", "Runtime-Memory-Map"),
    ("EXTRACTED/docs/03-main-loop-and-map.md", "Main-Loop-and-Map"),
    ("EXTRACTED/docs/04-party-and-session.md", "Party-and-Session"),
    ("EXTRACTED/docs/05-open-questions.md", "Open-Questions"),
    ("EXTRACTED/docs/06-gfx-loading.md", "GFX-Loading"),
    ("EXTRACTED/docs/06-event-dat-format.md", "event-dat-Format"),
    ("EXTRACTED/docs/06-roster-format.md", "roster-dat-Format"),
    ("EXTRACTED/docs/07-anm-tv-format.md", "ANM-TV-Format"),
    ("EXTRACTED/docs/07-dat-files-and-formats.md", "dat-Files-and-Formats"),
    ("EXTRACTED/docs/07-event-script-opcodes.md", "Event-Script-Opcodes"),
    ("EXTRACTED/docs/12-attrib-dat-format.md", "attrib-dat-Format"),
    ("EXTRACTED/docs/21-map-dat-format.md", "map-dat-Format"),
    ("EXTRACTED/docs/13-time-era-calendar.md", "Time-Era-Calendar"),
    ("EXTRACTED/docs/14-game-state-struct.md", "Game-State-Struct"),
    ("EXTRACTED/docs/15-3d-view-and-game-screen.md", "3D-View-and-Game-Screen"),
    ("EXTRACTED/docs/16-monster-ability-format.md", "monsters-dat-Format"),
    ("EXTRACTED/docs/17-combat-system.md", "Combat-System"),
    ("EXTRACTED/docs/18-items-dat-format.md", "items-dat-Format"),
    ("EXTRACTED/docs/19-spells-and-item-use.md", "Spells-and-Item-Use"),
    ("EXTRACTED/docs/20-copy-protection-table.md", "Copy-Protection"),
    ("EXTRACTED/mm2-ANALYSIS.md", "Full-Analysis"),
    ("tools/RE-TOOLS.md", "RE-Tools"),
    ("editor/README.md", "MM2ED-Editor"),
    ("EXTRACTED/decomp/README.md", "C-Decomp-Scaffold"),
    ("CLAUDE.md", "Workspace-Notes"),
]

GALLERY_PAGES = {
    "index": "Gallery",
    "monsters": "Monster-Sprites",
    "tilesets": "Tilesets",
    "maps": "Map-Cartography",
    "map_dat": "Map-dat-Grids",
    "views": "3D-View-Graphics",
}


@dataclass
class PageRef:
    title: str
    source: str | None = None


def build_link_map() -> dict[str, str]:
    """Map VitePress-style paths and source stems → GitHub Wiki page names."""
    m: dict[str, str] = {}
    for src, title in DOC_SOURCES:
        stem = Path(src).stem
        m[f"/docs/reverse-engineering/{stem}"] = title
        m[stem] = title
    m["/docs/tools/re-tools"] = "RE-Tools"
    m["/docs/editor/mm2ed"] = "MM2ED-Editor"
    m["/docs/decomp/readme"] = "C-Decomp-Scaffold"
    m["/docs/reference/workspace-notes"] = "Workspace-Notes"
    m["/docs/reverse-engineering/mm2-ANALYSIS"] = "Full-Analysis"
    for key, title in GALLERY_PAGES.items():
        m[f"/docs/gallery/{key}"] = title
        if key == "index":
            m["/docs/gallery/"] = title
    m["Home"] = "Home"
    m["Gallery"] = "Gallery"
    return m


LINK_MAP = build_link_map()


def strip_frontmatter(text: str) -> str:
    if text.startswith("---\n"):
        end = text.find("\n---\n", 4)
        if end != -1:
            return text[end + 5 :]
    return text


def rewrite_links(text: str) -> str:
    """Convert VitePress paths and .md file refs to GitHub Wiki page links."""

    def repl_md(m: re.Match[str]) -> str:
        label, target = m.group(1), m.group(2)
        target = target.split("#")[0].strip()
        if target.startswith("http"):
            return m.group(0)
        page = LINK_MAP.get(target)
        if not page:
            # try basename without extension
            base = Path(target).stem
            page = LINK_MAP.get(base) or LINK_MAP.get(f"/docs/reverse-engineering/{base}")
        if page:
            return f"[{label}]({page})"
        return m.group(0)

    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", repl_md, text)

    # Wiki-style [[Page]] → [Page](Page)
    text = re.sub(r"\[\[([^\]|]+)\]\]", r"[\1](\1)", text)
    return text


def convert_doc(source: Path, title: str) -> str:
    body = strip_frontmatter(source.read_text(encoding="utf-8"))
    body = rewrite_links(body)
    return f"# {title.replace('-', ' ')}\n\n{body.lstrip()}"


def write_home() -> None:
    logo = "images/book-f00.png"
    text = f"""# Might & Magic II — Reverse Engineering

![spellbook]({logo})

Amiga **Might and Magic II** reverse-engineering notes: data formats, 68k runtime,
combat, events, graphics codecs, and the MM2ED editor.

## Quick start

| Topic | Page |
|-------|------|
| Project overview | [Overview](Overview) |
| `.dat` format inventory | [dat Files and Formats](dat-Files-and-Formats) |
| Monster abilities | [monsters.dat Format](monsters-dat-Format) |
| Combat engine | [Combat System](Combat-System) |
| Sprite gallery | [Gallery](Gallery) |
| RE tooling | [RE Tools](RE-Tools) |
| Data editor | [MM2ED Editor](MM2ED-Editor) |

## Endianness

MM2 `.dat` multibyte fields are **little-endian on disk**. The 68000 runtime may
byte-swap after load — trace ASM before assuming otherwise.

---
*Wiki synced from [{REPO}](https://github.com/{REPO}). Regenerate with*
*`python wiki/scripts/export-github-wiki.py`.*
"""
    (OUT / "Home.md").write_text(text, encoding="utf-8")


def write_sidebar() -> None:
    text = """**[Home](Home)**

### Getting started
- [Overview](Overview)
- [Workspace Notes](Workspace-Notes)
- [Open Questions](Open-Questions)
- [Full Analysis](Full-Analysis)

### Runtime
- [Startup and Init](Startup-and-Init)
- [Runtime Memory Map](Runtime-Memory-Map)
- [Main Loop and Map](Main-Loop-and-Map)
- [Party and Session](Party-and-Session)
- [Game State Struct](Game-State-Struct)
- [Time Era Calendar](Time-Era-Calendar)

### Graphics
- [GFX Loading](GFX-Loading)
- [ANM TV Format](ANM-TV-Format)
- [3D View and Game Screen](3D-View-and-Game-Screen)

### Data formats
- [dat Files and Formats](dat-Files-and-Formats)
- [items.dat](items-dat-Format)
- [monsters.dat](monsters-dat-Format)
- [roster.dat](roster-dat-Format)
- [attrib.dat](attrib-dat-Format)
- [map.dat](map-dat-Format)
- [spells and item use](Spells-and-Item-Use)
- [event.dat](event-dat-Format)
- [Event Script Opcodes](Event-Script-Opcodes)

### Systems
- [Combat System](Combat-System)
- [Copy Protection](Copy-Protection)

### Gallery
- [Gallery home](Gallery)
- [Monster sprites](Monster-Sprites)
- [Tilesets](Tilesets)
- [Map cartography](Map-Cartography)
- [map.dat grids](Map-dat-Grids)
- [3D view graphics](3D-View-Graphics)

### Tools
- [RE Tools](RE-Tools)
- [C Decomp Scaffold](C-Decomp-Scaffold)
- [MM2ED Editor](MM2ED-Editor)
"""
    (OUT / "_Sidebar.md").write_text(text, encoding="utf-8")


def write_footer() -> None:
    (OUT / "_Footer.md").write_text(
        f"[{REPO} on GitHub](https://github.com/{REPO}) · "
        "Research notes — not affiliated with New World Computing",
        encoding="utf-8",
    )


def export_book_logo(dest: Path) -> None:
    script = WIKI / "scripts" / "export-book-logo.py"
    dest.parent.mkdir(parents=True, exist_ok=True)
    import subprocess

    out_public = WIKI / "public" / "book-f00.png"
    subprocess.run([sys.executable, str(script)], cwd=ROOT, check=False)
    if out_public.exists():
        shutil.copy2(out_public, dest)


def export_docs() -> None:
    for rel, title in DOC_SOURCES:
        src = ROOT / rel
        if not src.exists():
            print(f"skip missing doc: {rel}")
            continue
        page = convert_doc(src, title)
        (OUT / f"{title}.md").write_text(page, encoding="utf-8")
        print(f"  doc {title}")


def export_gallery(*, skip_gallery: bool) -> None:
    if skip_gallery:
        print("Skipping sprite gallery (no game assets or --skip-gallery)")
        stub = OUT / "Gallery.md"
        stub.write_text(
            "# Sprite gallery\n\n"
            "Gallery images are generated from game `.32` / `.anm` assets.\n\n"
            "Run `python wiki/scripts/export-github-wiki.py` locally with the MM2\n"
            "data files present, then publish.\n",
            encoding="utf-8",
        )
        return

    data = resolve_data_dir()
    gallery_img = OUT / "images" / "gallery"
    export_all(
        gallery_img,
        docs_out=OUT,
        data_dir=data,
        image_prefix="images/gallery",
        gallery_md_opts={
            "use_frontmatter": False,
            "page_names": {k: f"{v}.md" for k, v in GALLERY_PAGES.items()},
            "page_links": GALLERY_PAGES,
        },
    )
    export_book_logo(OUT / "images" / "book-f00.png")


def has_game_assets() -> bool:
    d = resolve_data_dir()
    return (d / "monsters.dat").exists() and (d / "book.32").exists()


def export_github_wiki(*, skip_gallery: bool = False) -> Path:
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    print(f"Exporting GitHub Wiki -> {OUT}")
    write_home()
    write_sidebar()
    write_footer()
    print("Exporting docs…")
    export_docs()
    export_gallery(skip_gallery=skip_gallery or not has_game_assets())
    print("Done.")
    return OUT


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="Build GitHub Wiki tree under wiki/gh-wiki/")
    ap.add_argument("--skip-gallery", action="store_true", help="Skip PNG export (docs only)")
    args = ap.parse_args()
    export_github_wiki(skip_gallery=args.skip_gallery)
    print(f"\nNext: python wiki/scripts/publish-github-wiki.py")
    print(f"Or browse: https://github.com/{REPO}/wiki")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

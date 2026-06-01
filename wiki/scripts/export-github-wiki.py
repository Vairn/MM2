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
WIKI_BASE = f"https://github.com/{REPO}/wiki"

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
    "monster_sprites": "Monster-Sprites",
    "world_sprites": "World-Sprites",
    "monsters": "Monsters",
    "items": "Items-Catalog",
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


def _wiki_badge(label: str, page: str, *, style: str = "for-the-badge") -> str:
    """Shields.io button linking to a GitHub Wiki page."""
    slug = label.replace(" ", "_").replace("→", "")
    return (
        f"[![{label}](https://img.shields.io/badge/{slug}-731a1a?style={style})]"
        f"({WIKI_BASE}/{page})"
    )


def _gallery_preview_row(out: Path) -> str:
    """Optional sprite strip when gallery PNGs were exported."""
    candidates = [
        ("images/gallery/monsters/01.gif", "Creepy Crawler", "Monster-Sprites"),
        ("images/gallery/views/castle-frames.png", "3D castle view", "3D-View-Graphics"),
        ("images/gallery/maps/overview-towns.png", "Town auto-maps", "Map-Cartography"),
    ]
    cells: list[str] = []
    for rel, alt, page in candidates:
        if (out / rel).is_file():
            cells.append(
                f'<td align="center" width="33%" valign="top">'
                f'<a href="{page}"><img src="{rel}" width="160" alt="{alt}"/></a><br/>'
                f'<sub><a href="{page}">{alt}</a></sub></td>'
            )
    if not cells:
        return ""
    return (
        "\n<br/>\n\n<table align=\"center\"><tr>\n"
        + "\n".join(cells)
        + "\n</tr></table>\n"
    )


def write_home(*, out: Path = OUT) -> None:
    logo = "images/book-f00.png"
    pages_url = f"https://{REPO.split('/')[0]}.github.io/{REPO.split('/')[1]}/maze-walker/"
    preview = _gallery_preview_row(out)
    badges = " ".join(
        [
            _wiki_badge("Overview", "Overview"),
            _wiki_badge("Data Formats", "dat-Files-and-Formats"),
            _wiki_badge("Gallery", "Gallery"),
            _wiki_badge("MM2ED", "MM2ED-Editor"),
        ]
    )
    text = f"""<div align="center">

<img src="{logo}" width="112" alt="Spellbook icon (book.32 frame 0)"/>

# Might & Magic II

**Amiga reverse-engineering wiki**

<sub>Data formats · 68k runtime · combat · events · graphics · MM2ED</sub>

<br/>

[![GitHub](https://img.shields.io/badge/repo-Vairn%2FMM2-120303?style=for-the-badge&logo=github)](https://github.com/{REPO})
[![Map Walker](https://img.shields.io/badge/map_walker-GitHub_Pages-731a1a?style=for-the-badge&logo=googlemaps)]({pages_url})

</div>

<br/>

<div align="center">

{badges}

</div>
{preview}
---

<table>
<tr>
<td width="50%" valign="top">

<h3 align="left">📦 Data formats</h3>

<p>ASM-confirmed <code>.dat</code> layouts with round-trip Python &amp; C codecs.</p>

<ul>
<li><a href="{WIKI_BASE}/items-dat-Format">items.dat</a> — 256 items, class masks, use spells</li>
<li><a href="{WIKI_BASE}/monsters-dat-Format">monsters.dat</a> — abilities, attacks, sprites</li>
<li><a href="{WIKI_BASE}/map-dat-Format">map.dat</a> — 60 screens × 512 bytes</li>
<li><a href="{WIKI_BASE}/event-dat-Format">event.dat</a> — 71 locations, script VM</li>
<li><a href="{WIKI_BASE}/roster-dat-Format">roster.dat</a> · <a href="{WIKI_BASE}/Spells-and-Item-Use">spells</a> · <a href="{WIKI_BASE}/attrib-dat-Format">attrib.dat</a></li>
</ul>

<p><b><a href="{WIKI_BASE}/dat-Files-and-Formats">→ Format inventory</a></b></p>

</td>
<td width="50%" valign="top">

<h3 align="left">⚔️ Game systems</h3>

<p>68000 traces for combat, world simulation, and scripts.</p>

<ul>
<li><a href="{WIKI_BASE}/Combat-System">Combat engine</a> — round loop, AI, rewards</li>
<li><a href="{WIKI_BASE}/3D-View-and-Game-Screen">3D view &amp; collision</a> — wall pages, auto-map</li>
<li><a href="{WIKI_BASE}/Event-Script-Opcodes">Event opcodes</a> — <code>0x00…0x32</code> reference</li>
<li><a href="{WIKI_BASE}/Main-Loop-and-Map">Main loop &amp; map</a> · <a href="{WIKI_BASE}/Copy-Protection">copy protection</a></li>
</ul>

<p><b><a href="{WIKI_BASE}/Full-Analysis">→ Full analysis</a></b></p>

</td>
</tr>
<tr>
<td width="50%" valign="top">

<h3 align="left">🐉 Sprite gallery</h3>

<p>Decoded <code>.32</code> tilesets and <code>.anm</code> combat animations from original assets.</p>

<ul>
<li><a href="{WIKI_BASE}/Monster-Sprites">Monster .anm</a> — GIF + contact sheets</li>
<li><a href="{WIKI_BASE}/World-Sprites">World sprites</a> · <a href="{WIKI_BASE}/Tilesets">tilesets</a></li>
<li><a href="{WIKI_BASE}/Monsters">Monsters catalog</a> · <a href="{WIKI_BASE}/Items-Catalog">items catalog</a></li>
<li><a href="{WIKI_BASE}/Map-Cartography">Auto-map cartography</a> · <a href="{WIKI_BASE}/3D-View-Graphics">3D views</a></li>
</ul>

<p><b><a href="{WIKI_BASE}/Gallery">→ Gallery home</a></b></p>

</td>
<td width="50%" valign="top">

<h3 align="left">🛠️ Tools &amp; code</h3>

<p>Decoders, disassembly helpers, and the ImGui data editor.</p>

<ul>
<li><a href="{WIKI_BASE}/RE-Tools">RE Tools</a> — Python disassemblers &amp; codecs</li>
<li><a href="{WIKI_BASE}/MM2ED-Editor">MM2ED</a> — edit every major <code>.dat</code> file</li>
<li><a href="{WIKI_BASE}/C-Decomp-Scaffold">C lift scaffold</a> — typed structs &amp; loaders</li>
<li><a href="{WIKI_BASE}/Map-Walker">Map walker ↗</a> — interactive HTML5 on <a href="{pages_url}">GitHub Pages</a></li>
</ul>

<p><b><a href="{WIKI_BASE}/Open-Questions">→ Open questions</a></b></p>

</td>
</tr>
</table>

---

<div align="center">

<h2>By the numbers</h2>

<table>
<tr>
<td align="center" width="20%"><h1>256</h1><sub>items</sub></td>
<td align="center" width="20%"><h1>256</h1><sub>monsters</sub></td>
<td align="center" width="20%"><h1>96</h1><sub>spells</sub></td>
<td align="center" width="20%"><h1>60</h1><sub>map screens</sub></td>
<td align="center" width="20%"><h1>71</h1><sub>event locations</sub></td>
</tr>
</table>

</div>

---

> [!TIP]
> **New here?** Read [Overview](Overview), then the [format inventory](dat-Files-and-Formats).
> Editing data files? Jump to [MM2ED Editor](MM2ED-Editor).
> Tracing combat or scripts? [Combat System](Combat-System) · [Event Script Opcodes](Event-Script-Opcodes).

> [!IMPORTANT]
> **Endianness** — MM2 `.dat` multibyte fields are **little-endian on disk**
> (matching our codecs and Blitz3D editors). The 68000 runtime may byte-swap
> after load — trace ASM before assuming otherwise.

<details>
<summary><b>📑 All quick links</b></summary>

| If you want to… | Start here |
|:---|:---|
| Understand the project | [Overview](Overview) → [dat Files and Formats](dat-Files-and-Formats) |
| Edit `.dat` files | [MM2ED Editor](MM2ED-Editor) + sidebar *Data formats* |
| Trace combat or scripts | [Combat System](Combat-System) · [Event Script Opcodes](Event-Script-Opcodes) |
| Browse decoded art | [Gallery](Gallery) |
| See what's still unknown | [Open Questions](Open-Questions) |
| Boot &amp; memory map | [Startup and Init](Startup-and-Init) · [Runtime Memory Map](Runtime-Memory-Map) |
| RE tooling | [RE Tools](RE-Tools) |

</details>

---

<div align="center">

<sub>Synced from <a href="https://github.com/{REPO}">{REPO}</a> ·
<code>python wiki/scripts/export-github-wiki.py</code></sub>

</div>
"""
    (out / "Home.md").write_text(text, encoding="utf-8")


def write_sidebar() -> None:
    text = """**[🏠 Home](Home)**

---

#### Getting started
- [Overview](Overview)
- [Workspace Notes](Workspace-Notes)
- [Open Questions](Open-Questions)
- [Full Analysis](Full-Analysis)

#### Runtime & engine
- [Startup and Init](Startup-and-Init)
- [Runtime Memory Map](Runtime-Memory-Map)
- [Main Loop and Map](Main-Loop-and-Map)
- [Party and Session](Party-and-Session)
- [Game State Struct](Game-State-Struct)
- [Time Era Calendar](Time-Era-Calendar)

#### Graphics
- [GFX Loading](GFX-Loading)
- [ANM TV Format](ANM-TV-Format)
- [3D View and Game Screen](3D-View-and-Game-Screen)

#### Data formats (.dat)
- [Format inventory](dat-Files-and-Formats)
- [items.dat](items-dat-Format)
- [monsters.dat](monsters-dat-Format)
- [roster.dat](roster-dat-Format)
- [attrib.dat](attrib-dat-Format)
- [map.dat](map-dat-Format)
- [spells & item use](Spells-and-Item-Use)
- [event.dat](event-dat-Format)
- [Event Script Opcodes](Event-Script-Opcodes)

#### Game systems
- [Combat System](Combat-System)
- [Copy Protection](Copy-Protection)

#### Gallery
- [Gallery home](Gallery)
- [Monster sprites](Monster-Sprites)
- [World sprites](World-Sprites)
- [Monsters catalog](Monsters)
- [Items catalog](Items-Catalog)
- [Tilesets](Tilesets)
- [Map cartography](Map-Cartography)
- [3D view graphics](3D-View-Graphics)
- [map.dat reference](Map-dat-Grids)
- [Map walker ↗](Map-Walker)

#### Tools
- [RE Tools](RE-Tools)
- [C Decomp Scaffold](C-Decomp-Scaffold)
- [MM2ED Editor](MM2ED-Editor)
"""
    (OUT / "_Sidebar.md").write_text(text, encoding="utf-8")


def write_footer() -> None:
    (OUT / "_Footer.md").write_text(
        f"**[🏠 Home](Home)** · "
        f"[{REPO}](https://github.com/{REPO}) · "
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


def clear_output_dir(path: Path) -> None:
    """Remove prior export tree; tolerate Windows file locks on the root folder."""
    if not path.exists():
        return
    try:
        shutil.rmtree(path)
    except OSError as exc:
        print(f"  rmtree {path}: {exc} — clearing contents instead")
        for child in path.iterdir():
            if child.is_dir():
                shutil.rmtree(child, ignore_errors=True)
            else:
                child.unlink(missing_ok=True)


def export_gallery(*, skip_gallery: bool) -> None:
    if skip_gallery:
        print("Skipping sprite gallery (no game assets or --skip-gallery)")
        stub = OUT / "Gallery.md"
        stub.write_text(
            "<div align=\"center\">\n\n"
            "<img src=\"images/book-f00.png\" width=\"64\" alt=\"Spellbook\"/>\n\n"
            "# 🐉 Sprite gallery\n\n"
            "<sub>Decoded <code>.32</code> tilesets and <code>.anm</code> combat animations</sub>\n\n"
            "</div>\n\n"
            "---\n\n"
            "Gallery images are generated from original game assets.\n\n"
            "```powershell\n"
            "python wiki/scripts/export-github-wiki.py\n"
            "python wiki/scripts/publish-github-wiki.py\n"
            "```\n\n"
            "<table>\n"
            "<tr>\n"
            "<td width=\"50%\" valign=\"top\">\n\n"
            "<h3>Combat &amp; world</h3>\n\n"
            "<ul>\n"
            "<li><a href=\"Monster-Sprites\">Monster .anm</a> — GIF animations</li>\n"
            "<li><a href=\"World-Sprites\">World sprites</a> — town &amp; overland</li>\n"
            "<li><a href=\"Monsters\">Monsters catalog</a> · "
            "<a href=\"Items-Catalog\">Items catalog</a></li>\n"
            "</ul>\n\n"
            "</td>\n"
            "<td width=\"50%\" valign=\"top\">\n\n"
            "<h3>Maps &amp; tiles</h3>\n\n"
            "<ul>\n"
            "<li><a href=\"Tilesets\">Tilesets</a> — planar <code>.32</code> sheets</li>\n"
            "<li><a href=\"Map-Cartography\">Auto-map cartography</a></li>\n"
            "<li><a href=\"3D-View-Graphics\">3D view compositing</a></li>\n"
            "</ul>\n\n"
            "</td>\n"
            "</tr>\n"
            "</table>\n",
            encoding="utf-8",
        )
        return

    print("Exporting sprite gallery…")
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


def export_maze_walker(out_root: Path) -> None:
    """Copy HTML walker + export maps.json (GitHub Pages, not Wiki markdown)."""
    src = WIKI / "maze-walker"
    # Wiki export only adds a link page; static app lives in repo wiki/maze-walker/
    walker_page = OUT / "Map-Walker.md"
    pages_url = f"https://{REPO.split('/')[0]}.github.io/{REPO.split('/')[1]}/maze-walker/"
    walker_page.write_text(
        "# Map walker\n\n"
        "Interactive **HTML5** first-person 3D explorer for all 60 `map.dat` screens — "
        "ASM frustum wall rendering, outdoor terrain lanes, cartography minimap.\n\n"
        "**GitHub Wiki cannot run JavaScript.** Open the walker on GitHub Pages:\n\n"
        f"**[{pages_url}]({pages_url})**\n\n"
        "Controls: **W/S** forward/back, **A/D** turn. "
        "See [map.dat reference](Map-dat-Grids) for tile/event data.\n\n"
        "### Local dev\n\n"
        "```powershell\n"
        "python tools/export_map_walker.py\n"
        "cd wiki/maze-walker\n"
        "python -m http.server 8080\n"
        "# open http://localhost:8080/\n"
        "```\n",
        encoding="utf-8",
    )
    print(f"  wiki page Map-Walker.md -> {pages_url}")

    if not src.is_dir():
        print("  skip maze-walker: missing wiki/maze-walker/")
        return

    try:
        from export_map_walker import export_map_walker as export_json

        export_json(src)
    except Exception as exc:
        print(f"  maze-walker maps.json: {exc}")


def has_game_assets() -> bool:
    d = resolve_data_dir()
    return (d / "monsters.dat").exists() and (d / "book.32").exists()


def export_github_wiki(*, skip_gallery: bool = False) -> Path:
    clear_output_dir(OUT)
    OUT.mkdir(parents=True, exist_ok=True)

    print(f"Exporting GitHub Wiki -> {OUT}")
    write_sidebar()
    write_footer()
    print("Exporting docs…")
    export_docs()
    export_gallery(skip_gallery=skip_gallery or not has_game_assets())
    export_book_logo(OUT / "images" / "book-f00.png")
    export_maze_walker(OUT)
    write_home(out=OUT)
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

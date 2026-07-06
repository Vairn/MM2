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
sys.path.insert(0, str(WIKI))

from mm2_gfx_export import export_all, resolve_data_dir  # noqa: E402
from wiki_enrichments import enrich_page, getting_started_page  # noqa: E402

OUT = WIKI / "gh-wiki"
REPO = "Vairn/MM2"
WIKI_GIT = f"https://github.com/{REPO}.wiki.git"
WIKI_BASE = f"https://github.com/{REPO}/wiki"

# Source markdown → GitHub Wiki page title (becomes Page-Title.md)
DOC_SOURCES: list[tuple[str, str]] = [
    ("EXTRACTED/docs/README.md", "Docs-Wiki-Hub"),
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
    ("EXTRACTED/docs/54-pc-dos-graphics-formats.md", "PC-DOS-Graphics-Formats"),
    ("EXTRACTED/docs/07-dat-files-and-formats.md", "dat-Files-and-Formats"),
    ("EXTRACTED/docs/07-event-script-opcodes.md", "Event-Script-Opcodes"),
    ("EXTRACTED/docs/42-event-dsl-format.md", "Event-Script-DSL"),
    ("EXTRACTED/docs/44-event-text-rendering.md", "Event-Text-Rendering"),
    ("EXTRACTED/docs/45-event-graphics-opcodes.md", "Event-Graphics-Opcodes"),
    ("EXTRACTED/docs/46-scripted-scene-graphics.md", "Scripted-Scene-Graphics"),
    ("EXTRACTED/docs/47-amiga-3d-render-process.md", "Amiga-3D-Render-Process"),
    ("EXTRACTED/docs/48-amiga-cpp-play-screen-draw-walkthrough.md", "Amiga-Cpp-Play-Screen-Draw"),
    ("EXTRACTED/docs/08-event-runtime.md", "Event-Runtime"),
    ("EXTRACTED/docs/12-attrib-dat-format.md", "attrib-dat-Format"),
    ("EXTRACTED/docs/21-map-dat-format.md", "map-dat-Format"),
    ("EXTRACTED/docs/13-time-era-calendar.md", "Time-Era-Calendar"),
    ("EXTRACTED/docs/14-game-state-struct.md", "Game-State-Struct"),
    ("EXTRACTED/docs/49-data-hunk-mm2_data_00.md", "Data-Hunk-mm2-data-00"),
    ("EXTRACTED/docs/43-exploration-input-and-ingame-options.md", "Exploration-Input-and-Options"),
    ("EXTRACTED/docs/15-3d-view-and-game-screen.md", "3D-View-and-Game-Screen"),
    ("EXTRACTED/docs/16-monster-ability-format.md", "monsters-dat-Format"),
    ("EXTRACTED/docs/17-combat-system.md", "Combat-System"),
    ("EXTRACTED/docs/26-combat-overview.md", "Combat-Overview"),
    ("EXTRACTED/docs/26-spell-cast-asm.md", "Spell-Cast-ASM"),
    ("EXTRACTED/docs/35-encounter-tables.md", "Encounter-Tables"),
    ("EXTRACTED/docs/25-audio-sounds-music.md", "Audio-Sounds-Music"),
    ("EXTRACTED/docs/25-mm2-music-format.md", "MM2-Music-Format"),
    ("EXTRACTED/docs/26-audio-callpaths-title-death-shared.md", "Audio-Title-Death-Paths"),
    ("EXTRACTED/docs/27-mm2-known-songs-catalog.md", "Known-Songs-Catalog"),
    ("EXTRACTED/docs/28-town-services.md", "Town-Services"),
    ("EXTRACTED/docs/29-embedded-exe-strings.md", "Embedded-Exe-Strings"),
    ("EXTRACTED/docs/30-event-to-string-path.md", "Event-to-String-Path"),
    ("EXTRACTED/docs/31-spell-sources.md", "Spell-Sources"),
    ("EXTRACTED/docs/32-character-mechanics.md", "Character-Mechanics"),
    ("EXTRACTED/docs/33-skills-and-hirelings.md", "Skills-and-Hirelings"),
    ("EXTRACTED/docs/34-commerce-formulas.md", "Commerce-Formulas"),
    ("EXTRACTED/docs/34-commerce-and-world-services.md", "Commerce-World-Services"),
    ("EXTRACTED/docs/36-class-quest-hp-bug.md", "Class-Quest-HP-Bug"),
    ("EXTRACTED/docs/37-mount-farview-class-quest-event.md", "Mount-Farview-Class-Quest"),
    ("EXTRACTED/docs/53-nordon-nordonna-quests.md", "Nordon-Nordonna-Quests"),
    ("EXTRACTED/docs/38-title-screen-and-intro-assets.md", "Title-Screen-Assets"),
    ("EXTRACTED/docs/39-title-screen-animation.md", "Title-Screen-Animation"),
    ("EXTRACTED/docs/39-character-ui-view-create.md", "Character-UI-View-Create"),
    ("EXTRACTED/docs/40-events-by-location.md", "Events-By-Location-Hub"),
    ("EXTRACTED/docs/18-items-dat-format.md", "items-dat-Format"),
    ("EXTRACTED/docs/19-spells-and-item-use.md", "Spells-and-Item-Use"),
    ("EXTRACTED/docs/20-copy-protection-table.md", "Copy-Protection"),
    ("EXTRACTED/docs/22-mm1-mazedata-format.md", "MM1-MAZEDATA-Format"),
    ("EXTRACTED/docs/23-mm1-to-mm2-outdoor.md", "MM1-to-MM2-Outdoor"),
    ("EXTRACTED/docs/24-mm1-outdoor-wallpix-by-sector.md", "MM1-Outdoor-WALLPIX"),
    ("EXTRACTED/docs/50-mm1-overview.md", "MM1-Overview"),
    ("EXTRACTED/docs/51-mm1-art-and-graphics.md", "MM1-Art-and-Graphics"),
    ("EXTRACTED/docs/52-mm1-items-monsters-events.md", "MM1-Items-Monsters-Events"),
    ("game/README.md", "Game-Remake"),
    ("EXTRACTED/mm2-ANALYSIS.md", "Full-Analysis"),
    ("tools/RE-TOOLS.md", "RE-Tools"),
    ("editor/README.md", "MM2ED-Editor"),
    ("EXTRACTED/decomp/README.md", "C-Decomp-Scaffold"),
    ("CLAUDE.md", "Workspace-Notes"),
]

DEFAULT_MM1_MAZEDATA = Path(
    r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 1\MAZEDATA.DTA"
)
EVENTS_SRC_DIR = "EXTRACTED/docs/events"
EVENTS_HUB_TITLE = "Events-by-Location"

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


def event_loc_wiki_title(stem: str) -> str:
    """loc_34_d2 → Event-Loc-34-D2 (GitHub Wiki page name)."""
    m = re.match(r"loc_(\d+)_(.+)$", stem)
    if not m:
        return f"Event-{stem.replace('_', '-')}"
    num, slug = m.group(1), m.group(2)
    part = "-".join(s.capitalize() for s in slug.split("_"))
    return f"Event-Loc-{num}-{part}"


def build_event_link_map() -> dict[str, str]:
    """Map events/README and loc_*.md paths → wiki page titles."""
    m: dict[str, str] = {}
    events_dir = ROOT / EVENTS_SRC_DIR
    hub = f"{EVENTS_SRC_DIR}/README.md"
    m[hub] = EVENTS_HUB_TITLE
    m["events/README.md"] = EVENTS_HUB_TITLE
    m["events"] = EVENTS_HUB_TITLE
    m["40-events-by-location"] = "Events-By-Location-Hub"
    m["40-events-by-location.md"] = "Events-By-Location-Hub"
    for path in sorted(events_dir.glob("loc_*.md")):
        title = event_loc_wiki_title(path.stem)
        rel = f"{EVENTS_SRC_DIR}/{path.name}"
        m[rel] = title
        m[f"events/{path.name}"] = title
        m[path.name] = title
        m[path.stem] = title
    return m


def build_link_map() -> dict[str, str]:
    """Map VitePress-style paths and source stems → GitHub Wiki page names."""
    m: dict[str, str] = {}
    m.update(build_event_link_map())
    for src, title in DOC_SOURCES:
        stem = Path(src).stem
        m[f"/docs/reverse-engineering/{stem}"] = title
        m[stem] = title
        m[f"{stem}.md"] = title
        # Numeric doc prefix aliases (01-startup-init → Startup-and-Init)
        if len(stem) >= 3 and stem[2] == "-" and stem[:2].isdigit():
            m[stem[3:]] = title
    m["/docs/tools/re-tools"] = "RE-Tools"
    m["/docs/editor/mm2ed"] = "MM2ED-Editor"
    m["editor/README.md"] = "MM2ED-Editor"
    m["../../editor/README.md"] = "MM2ED-Editor"
    m["/docs/decomp/readme"] = "C-Decomp-Scaffold"
    m["/docs/reference/workspace-notes"] = "Workspace-Notes"
    m["/docs/reverse-engineering/mm2-ANALYSIS"] = "Full-Analysis"
    for key, title in GALLERY_PAGES.items():
        m[f"/docs/gallery/{key}"] = title
        if key == "index":
            m["/docs/gallery/"] = title
    m["Home"] = "Home"
    m["Gallery"] = "Gallery"
    for _, title in DOC_SOURCES:
        m[title] = title
    for title in GALLERY_PAGES.values():
        m[title] = title
    m["Map-Walker"] = "Map-Walker"
    m["MM1-Map-Walker"] = "MM1-Map-Walker"
    m["Monster-Variants"] = "Monster-Variants"
    m["Platform-Graphics-Index"] = "Platform-Graphics-Index"
    m["Platform-Monsters-01-74"] = "Platform-Monsters-01-74"
    m["Getting-Started"] = "Getting-Started"
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


def _strip_source_h1(body: str) -> str:
    """Drop the source markdown H1; the wiki export supplies the page title."""
    lines = body.lstrip().split("\n")
    if lines and lines[0].startswith("# "):
        return "\n".join(lines[1:]).lstrip()
    return body.lstrip()


def convert_doc(source: Path, title: str) -> str:
    from wiki_enrichments import BODY_REPLACEMENTS

    if title in BODY_REPLACEMENTS:
        return enrich_page(title, BODY_REPLACEMENTS[title])

    body = strip_frontmatter(source.read_text(encoding="utf-8"))
    body = rewrite_links(body)
    body = _strip_source_h1(body)
    heading = f"# {title.replace('-', ' ')}\n\n"
    return enrich_page(title, heading + body)


def _wiki_badge(label: str, page: str, *, style: str = "for-the-badge") -> str:
    """Shields.io button linking to a GitHub Wiki page."""
    slug = label.replace(" ", "_").replace("→", "")
    return (
        f"[![{label}](https://img.shields.io/badge/{slug}-731a1a?style={style})]"
        f"({WIKI_BASE}/{page})"
    )


def _architecture_mermaid() -> str:
    """Inline Mermaid map for the Home page (rendered natively by GitHub Wiki)."""
    return (
        "\n## How it all fits together\n\n"
        "```mermaid\n"
        "flowchart LR\n"
        '  map["map.dat<br/>geometry"] --> r3d["3D view 0x2ECE"]\n'
        '  attrib["attrib.dat<br/>environment"] -- tileset --> r3d\n'
        '  map -- bit 0x80 --> event["event.dat<br/>scripts"]\n'
        '  event --> vm["script VM 0x172CA"]\n'
        '  vm -- OP_12 --> combat["combat 0x12A22"]\n'
        '  combat --> monsters["monsters.dat"]\n'
        '  roster["roster.dat<br/>party"] -- equips --> items["items.dat"]\n'
        '  items -- use byte --> spells["spells.dat"]\n'
        "```\n\n"
        "<div align=\"center\"><sub>"
        "<a href=\"Getting-Started\">Getting Started</a> walks each of these paths."
        "</sub></div>\n"
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
    preview = _architecture_mermaid() + _gallery_preview_row(out)
    badges = " ".join(
        [
            _wiki_badge("Start Here", "Getting-Started"),
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
<li><a href="{WIKI_BASE}/event-dat-Format">event.dat</a> — container format</li>
<li><a href="{WIKI_BASE}/Events-by-Location">Events by location</a> — all 71 scripts</li>
<li><a href="{WIKI_BASE}/roster-dat-Format">roster.dat</a> · <a href="{WIKI_BASE}/Spells-and-Item-Use">spells</a> · <a href="{WIKI_BASE}/attrib-dat-Format">attrib.dat</a></li>
</ul>

<p><b><a href="{WIKI_BASE}/dat-Files-and-Formats">→ Format inventory</a></b></p>

</td>
<td width="50%" valign="top">

<h3 align="left">⚔️ Game systems</h3>

<p>68000 traces for combat, world simulation, and scripts.</p>

<ul>
<li><a href="{WIKI_BASE}/Combat-Overview">Combat overview</a> · <a href="{WIKI_BASE}/Combat-System">full ASM trace</a></li>
<li><a href="{WIKI_BASE}/Town-Services">Town services</a> — pub, temple, guild, training</li>
<li><a href="{WIKI_BASE}/Spell-Sources">Spell sources</a> — where every spell is obtained</li>
<li><a href="{WIKI_BASE}/Audio-Sounds-Music">Audio / SFX / music</a> — <code>audio.device</code>, Controls menu</li>
<li><a href="{WIKI_BASE}/3D-View-and-Game-Screen">3D view &amp; collision</a> — wall pages, auto-map · <a href="{WIKI_BASE}/Exploration-Input-and-Options">exploration keys</a></li>
<li><a href="{WIKI_BASE}/Events-by-Location">Events per map</a> · <a href="{WIKI_BASE}/Event-Script-Opcodes">opcodes</a> · <a href="{WIKI_BASE}/Event-Text-Rendering">text rendering</a> · <a href="{WIKI_BASE}/Event-to-String-Path">script → text</a></li>
<li><a href="{WIKI_BASE}/Data-Hunk-mm2-data-00">Data hunk</a> — static <code>A4</code> tables · <a href="{WIKI_BASE}/Town-Services">town services</a></li>
<li><a href="{WIKI_BASE}/Main-Loop-and-Map">Main loop &amp; map</a> · <a href="{WIKI_BASE}/Copy-Protection">copy protection</a></li>
<li><a href="{WIKI_BASE}/Title-Screen-Animation">Title screen</a> · <a href="{WIKI_BASE}/Character-UI-View-Create">character UI</a></li>
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
<li><a href="{WIKI_BASE}/Monster-Variants">Monster variants</a> — Amiga vs CGA/EGA</li>
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
> **New here?** Start with [Getting Started](Getting-Started) → [Overview](Overview) → [format inventory](dat-Files-and-Formats).
> Editing data files? Jump to [MM2ED Editor](MM2ED-Editor).
> Tracing combat or scripts? [Combat Overview](Combat-Overview) · [Combat System](Combat-System) · [Event Script Opcodes](Event-Script-Opcodes).
> Audio? [Audio Sounds Music](Audio-Sounds-Music).

> [!IMPORTANT]
> **Endianness** — MM2 `.dat` multibyte fields are **little-endian on disk**
> (matching our codecs and Blitz3D editors). The 68000 runtime may byte-swap
> after load — trace ASM before assuming otherwise.

<details>
<summary><b>📑 All quick links</b></summary>

| If you want to… | Start here |
|:---|:---|
| Understand the project | [Getting Started](Getting-Started) → [Overview](Overview) → [dat Files and Formats](dat-Files-and-Formats) |
| Edit `.dat` files | [MM2ED Editor](MM2ED-Editor) + sidebar *Data formats* |
| Trace combat or scripts | [Combat Overview](Combat-Overview) · [Events by location](Events-by-Location) · [Event Script Opcodes](Event-Script-Opcodes) |
| Town shops &amp; services | [Town Services](Town-Services) · [Spell Sources](Spell-Sources) · [Event to String Path](Event-to-String-Path) |
| Audio / walk beep / SFX | [Audio Sounds Music](Audio-Sounds-Music) |
| Exe-embedded UI strings | [Embedded Exe Strings](Embedded-Exe-Strings) |
| MM1 (DOS) maps &amp; art | [MM1 Overview](MM1-Overview) · [MAZEDATA](MM1-MAZEDATA-Format) · [MM1 map walker ↗](MM1-Map-Walker) |
| Browse decoded art | [Gallery](Gallery) |
| See what's still unknown | [Open Questions](Open-Questions) |
| Boot &amp; memory map | [Startup and Init](Startup-and-Init) · [Runtime Memory Map](Runtime-Memory-Map) · [Data Hunk](Data-Hunk-mm2-data-00) |
| Exploration keys &amp; options | [Exploration Input and Options](Exploration-Input-and-Options) |
| Event script authoring | [Event Script DSL](Event-Script-DSL) · [Event Text Rendering](Event-Text-Rendering) |
| 3D renderer deep-dive | [Amiga 3D Render Process](Amiga-3D-Render-Process) · [Scripted Scene Graphics](Scripted-Scene-Graphics) |
| Remake draw path | [Amiga Cpp Play Screen Draw](Amiga-Cpp-Play-Screen-Draw) · [Game Remake](Game-Remake) |
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
- [Docs Wiki Hub](Docs-Wiki-Hub) ← **full doc index**
- [Getting Started](Getting-Started) ← **read this first**
- [Overview](Overview)
- [Workspace Notes](Workspace-Notes)
- [Open Questions](Open-Questions)
- [Full Analysis](Full-Analysis)
- [Game Remake](Game-Remake)

#### Runtime & engine
- [Startup and Init](Startup-and-Init)
- [Runtime Memory Map](Runtime-Memory-Map)
- [Main Loop and Map](Main-Loop-and-Map)
- [Exploration Input and Options](Exploration-Input-and-Options)
- [Party and Session](Party-and-Session)
- [Game State Struct](Game-State-Struct)
- [Data Hunk mm2 data 00](Data-Hunk-mm2-data-00)
- [Time Era Calendar](Time-Era-Calendar)

#### Graphics
- [GFX Loading](GFX-Loading)
- [ANM TV Format](ANM-TV-Format)
- [3D View and Game Screen](3D-View-and-Game-Screen)
- [Amiga 3D Render Process](Amiga-3D-Render-Process)
- [Scripted Scene Graphics](Scripted-Scene-Graphics)
- [Event Graphics Opcodes](Event-Graphics-Opcodes)
- [Title Screen Assets](Title-Screen-Assets)
- [Title Screen Animation](Title-Screen-Animation)

#### UI
- [Character UI View Create](Character-UI-View-Create)
- [Amiga Cpp Play Screen Draw](Amiga-Cpp-Play-Screen-Draw)

#### Data formats (.dat)
- [Format inventory](dat-Files-and-Formats)
- [items.dat](items-dat-Format)
- [monsters.dat](monsters-dat-Format)
- [roster.dat](roster-dat-Format)
- [attrib.dat](attrib-dat-Format)
- [map.dat](map-dat-Format)
- [spells & item use](Spells-and-Item-Use)
- [event.dat](event-dat-Format)
- [Events by location](Events-by-Location) ← **71 maps**
- [Events hub (numbered)](Events-By-Location-Hub)
- [Event Script Opcodes](Event-Script-Opcodes)
- [Event Script DSL](Event-Script-DSL) ← **`.mm2evt` authoring**
- [Event Text Rendering](Event-Text-Rendering)
- [Event Graphics Opcodes](Event-Graphics-Opcodes)

#### Combat
- [Combat Overview](Combat-Overview)
- [Combat System](Combat-System)
- [Encounter Tables](Encounter-Tables)
- [Spell Cast ASM](Spell-Cast-ASM)
- [monsters.dat abilities](monsters-dat-Format)
- [Spells and item use](Spells-and-Item-Use)

#### Audio
- [Audio, sounds, music](Audio-Sounds-Music)
- [MM2 music format](MM2-Music-Format)
- [Title/death audio paths](Audio-Title-Death-Paths)
- [Known songs catalog](Known-Songs-Catalog)

#### Game systems
- [Town Services](Town-Services)
- [Spell Sources](Spell-Sources)
- [Character Mechanics](Character-Mechanics)
- [Skills and Hirelings](Skills-and-Hirelings)
- [Commerce Formulas](Commerce-Formulas)
- [Commerce World Services](Commerce-World-Services)
- [Mount Farview Class Quest](Mount-Farview-Class-Quest)
- [Class Quest HP Bug](Class-Quest-HP-Bug)
- [Event Runtime](Event-Runtime)
- [Event to String Path](Event-to-String-Path)
- [Embedded Exe Strings](Embedded-Exe-Strings)
- [Copy Protection](Copy-Protection)
- [Time Era Calendar](Time-Era-Calendar)
- [Game State Struct](Game-State-Struct)

#### Might and Magic I (DOS)
- [MM1 Overview](MM1-Overview) ← **hub + decode status**
- [MM1 MAZEDATA format](MM1-MAZEDATA-Format)
- [MM1 to MM2 outdoor](MM1-to-MM2-Outdoor)
- [MM1 WALLPIX by sector](MM1-Outdoor-WALLPIX)
- [MM1 art &amp; graphics](MM1-Art-and-Graphics)
- [MM1 items / monsters (status)](MM1-Items-Monsters-Events)
- [MM1 map walker ↗](MM1-Map-Walker)
- [MM1 2D maps gallery](MM1-Maps-Gallery)
- [MM1 WALLPIX gallery](MM1-Walls-Gallery)

#### Gallery
- [Gallery home](Gallery)
- [Monster sprites](Monster-Sprites)
- [Monster variants (Amiga / PC)](Monster-Variants)
- [Platform graphics index](Platform-Graphics-Index)
- [Platform monsters 01–74](Platform-Monsters-01-74)
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


def write_getting_started() -> None:
    (OUT / "Getting-Started.md").write_text(getting_started_page(), encoding="utf-8")
    print("  page Getting-Started")


def export_diagrams() -> None:
    script = WIKI / "scripts" / "export-wiki-diagrams.py"
    import subprocess

    subprocess.run(
        [sys.executable, str(script)],
        cwd=ROOT,
        check=False,
    )


def export_docs() -> None:
    write_getting_started()
    for rel, title in DOC_SOURCES:
        src = ROOT / rel
        if not src.exists():
            print(f"skip missing doc: {rel}")
            continue
        page = convert_doc(src, title)
        (OUT / f"{title}.md").write_text(page, encoding="utf-8")
        print(f"  doc {title}")


def export_event_locations() -> None:
    """Export events/README hub + all 71 loc_*.md pages to GitHub Wiki."""
    events_dir = ROOT / EVENTS_SRC_DIR
    if not events_dir.is_dir():
        print(f"skip events: missing {EVENTS_SRC_DIR}")
        return

    hub = events_dir / "README.md"
    if hub.is_file():
        page = convert_doc(hub, EVENTS_HUB_TITLE)
        (OUT / f"{EVENTS_HUB_TITLE}.md").write_text(page, encoding="utf-8")
        print(f"  events hub {EVENTS_HUB_TITLE}")

    loc_files = sorted(events_dir.glob("loc_*.md"))
    for path in loc_files:
        title = event_loc_wiki_title(path.stem)
        page = convert_doc(path, title)
        (OUT / f"{title}.md").write_text(page, encoding="utf-8")
    print(f"  events {len(loc_files)} location pages")


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
            "<li><a href=\"Monster-Variants\">Monster variants</a> — Amiga vs CGA/EGA</li>\n"
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
    export_monster_variants_wiki(OUT)
    export_platform_compare_wiki(OUT)


def export_platform_compare_wiki(out_root: Path) -> None:
    """Numbered monster + wall slot compare under images/gallery/platform-compare/."""
    from gen_gfx_compare_html import DEFAULT_GOG, main as compare_main  # noqa: E402

    gog = DEFAULT_GOG
    if not (gog / "MONSTERS.4").is_file():
        print("  skip platform compare: GOG MONSTERS.4/.16 not found")
        return
    gallery = out_root / "images" / "gallery" / "platform-compare"
    print("Exporting platform graphics index…")
    rc = compare_main(
        [
            "-o",
            str(gallery),
            "--github-wiki-dir",
            str(out_root),
            "--data",
            str(ROOT),
            "--gog",
            str(gog),
        ]
    )
    if rc != 0:
        print(f"  platform compare export failed (rc={rc})", file=sys.stderr)


def export_monster_variants_wiki(out_root: Path) -> None:
    """Amiga / CGA / EGA side-by-side under images/gallery/monster-variants/."""
    from export_monster_variants import DEFAULT_GOG, main as export_variants_main  # noqa: E402

    gog = DEFAULT_GOG
    if not (gog / "MONSTERS.4").is_file() or not (gog / "MONSTERS.16").is_file():
        print("  skip monster variants: GOG MONSTERS.4/.16 not found")
        return
    gallery = out_root / "images" / "gallery" / "monster-variants"
    md_out = out_root / "Monster-Variants.md"
    print("Exporting monster variants (Amiga / CGA / EGA)…")
    rc = export_variants_main(
        [
            "-o",
            str(gallery),
            "--markdown",
            str(md_out),
            "--image-prefix",
            "images/gallery/monster-variants",
            "--github-wiki",
            "--no-html",
            "--data",
            str(ROOT),
            "--monsters",
            str(ROOT / "monsters.dat"),
            "--gog",
            str(gog),
        ]
    )
    if rc != 0:
        print(f"  monster variants export failed (rc={rc})", file=sys.stderr)


def export_mm1_maze_walker(out_root: Path) -> None:
    """Wiki link page for the MM1 HTML walker (GitHub Pages)."""
    pages_url = f"https://{REPO.split('/')[0]}.github.io/{REPO.split('/')[1]}/mm1-maze-walker/"
    walker_page = OUT / "MM1-Map-Walker.md"
    walker_page.write_text(
        "# MM1 map walker\n\n"
        "Interactive **HTML5** first-person 3D explorer for all **55** MM1 `MAZEDATA.DTA` "
        "screens — indoor dungeons/towns plus converted overland sectors A1–E4.\n\n"
        "**GitHub Wiki cannot run JavaScript.** Open the walker on GitHub Pages:\n\n"
        f"**[{pages_url}]({pages_url})**\n\n"
        "Controls: **W/S** forward/back, **A/D** turn. "
        "Indoor walls use MM2 `.32` stand-ins; outdoor horizons follow MM1 WALLPIX biomes.\n\n"
        "See [MM1 Overview](MM1-Overview) · [MAZEDATA format](MM1-MAZEDATA-Format) · "
        "[outdoor conversion](MM1-to-MM2-Outdoor).\n\n"
        "### Local dev\n\n"
        "```powershell\n"
        "python tools/export_mm1_map_walker.py\n"
        "cd wiki/mm1-maze-walker\n"
        "python -m http.server 8081\n"
        "# open http://localhost:8081/\n"
        "```\n",
        encoding="utf-8",
    )
    print(f"  wiki page MM1-Map-Walker.md -> {pages_url}")


def export_mm1_gallery_wiki(out_root: Path) -> None:
    """MM1 2D maps + WALLPIX PNGs under images/gallery/mm1/."""
    if not DEFAULT_MM1_MAZEDATA.is_file():
        print("  skip MM1 gallery: MAZEDATA.DTA not found")
        return
    from export_mm1_gallery import export_mm1_gallery  # noqa: E402

    img_out = out_root / "images" / "gallery" / "mm1"
    export_mm1_gallery(
        img_out,
        mazedata=DEFAULT_MM1_MAZEDATA,
        docs_out=out_root,
        image_prefix="images/gallery/mm1",
    )
    renames = {
        "mm1-index.md": "MM1-Gallery.md",
        "mm1-maps.md": "MM1-Maps-Gallery.md",
        "mm1-walls.md": "MM1-Walls-Gallery.md",
    }
    for src_name, title in renames.items():
        src = out_root / src_name
        if src.is_file():
            (out_root / f"{title}.md").write_text(src.read_text(encoding="utf-8"), encoding="utf-8")
            src.unlink()
            print(f"  MM1 gallery page {title}")


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
    print("Exporting diagrams…")
    export_diagrams()
    print("Exporting docs…")
    export_docs()
    print("Exporting event locations…")
    export_event_locations()
    export_gallery(skip_gallery=skip_gallery or not has_game_assets())
    export_mm1_gallery_wiki(OUT)
    export_book_logo(OUT / "images" / "book-f00.png")
    export_maze_walker(OUT)
    export_mm1_maze_walker(OUT)
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

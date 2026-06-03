"""Per-page enrichments for the GitHub Wiki export.

Adds: navigation breadcrumb, an inline Mermaid diagram (rendered natively by
GitHub) or a PNG byte-strip, an auto table of contents on long pages, and a
"See also" footer. Flowcharts are Mermaid text (maintainable + crisp); only
byte-layout strips and the file-size chart stay as PNGs.
"""
from __future__ import annotations

SECTIONS: dict[str, str] = {
    "Overview": "Getting started",
    "Workspace-Notes": "Getting started",
    "Open-Questions": "Getting started",
    "Full-Analysis": "Getting started",
    "Getting-Started": "Getting started",
    "Startup-and-Init": "Runtime & engine",
    "Runtime-Memory-Map": "Runtime & engine",
    "Main-Loop-and-Map": "Runtime & engine",
    "Party-and-Session": "Runtime & engine",
    "Game-State-Struct": "Runtime & engine",
    "Time-Era-Calendar": "Runtime & engine",
    "GFX-Loading": "Graphics",
    "ANM-TV-Format": "Graphics",
    "3D-View-and-Game-Screen": "Graphics",
    "Title-Screen-Assets": "Title screen & UI",
    "Title-Screen-Animation": "Title screen & UI",
    "Character-UI-View-Create": "Title screen & UI",
    "Game-Remake": "Title screen & UI",
    "Docs-Wiki-Hub": "Getting started",
    "dat-Files-and-Formats": "Data formats",
    "items-dat-Format": "Data formats",
    "monsters-dat-Format": "Data formats",
    "roster-dat-Format": "Data formats",
    "attrib-dat-Format": "Data formats",
    "map-dat-Format": "Data formats",
    "Spells-and-Item-Use": "Data formats",
    "event-dat-Format": "Data formats",
    "Events-by-Location": "Data formats",
    "Events-By-Location-Hub": "Data formats",
    "Event-Script-Opcodes": "Data formats",
    "Combat-Overview": "Combat",
    "Combat-System": "Combat",
    "Encounter-Tables": "Combat",
    "Spell-Cast-ASM": "Combat",
    "Audio-Sounds-Music": "Audio",
    "MM2-Music-Format": "Audio",
    "Audio-Title-Death-Paths": "Audio",
    "Known-Songs-Catalog": "Audio",
    "Copy-Protection": "Game systems",
    "Town-Services": "Game systems",
    "Spell-Sources": "Game systems",
    "Event-to-String-Path": "Game systems",
    "Embedded-Exe-Strings": "Game systems",
    "Event-Runtime": "Game systems",
    "Character-Mechanics": "Game systems",
    "Skills-and-Hirelings": "Game systems",
    "Commerce-Formulas": "Game systems",
    "Commerce-World-Services": "Game systems",
    "Mount-Farview-Class-Quest": "Game systems",
    "Class-Quest-HP-Bug": "Game systems",
    "MM1-MAZEDATA-Format": "MM1 cross-walk",
    "MM1-to-MM2-Outdoor": "MM1 cross-walk",
    "MM1-Outdoor-WALLPIX": "MM1 cross-walk",
    "RE-Tools": "Tools",
    "MM2ED-Editor": "Tools",
    "C-Decomp-Scaffold": "Tools",
}

RELATED: dict[str, list[tuple[str, str]]] = {
    "Overview": [
        ("Docs-Wiki-Hub", "Full documentation index"),
        ("Getting-Started", "Reading roadmap with diagrams"),
        ("dat-Files-and-Formats", "All .dat layouts"),
        ("Full-Analysis", "Complete narrative"),
    ],
    "Title-Screen-Animation": [
        ("Title-Screen-Assets", "Boot graph and ASM blit traces"),
        ("Character-UI-View-Create", "P/C keys and character sheet"),
        ("Game-Remake", "Build and run the C++ port"),
        ("GFX-Loading", ".32 loader path"),
    ],
    "Title-Screen-Assets": [
        ("Title-Screen-Animation", "Overlay coords and peekers"),
        ("Copy-Protection", "Logo fade A4-$6476 table"),
        ("Audio-Title-Death-Paths", "Title music 0x12D"),
    ],
    "Character-UI-View-Create": [
        ("Title-Screen-Animation", "Title menu flow"),
        ("roster-dat-Format", "Party record layout"),
        ("Game-Remake", "AmigaClassic vs Stub UI"),
    ],
    "dat-Files-and-Formats": [
        ("items-dat-Format", "256 items"),
        ("map-dat-Format", "Visual + collision pages"),
        ("Events-by-Location", "71 per-map script pages"),
        ("event-dat-Format", "event.dat container"),
        ("MM2ED-Editor", "Edit in GUI"),
    ],
    "items-dat-Format": [
        ("Spells-and-Item-Use", "Use-byte spell index"),
        ("Items-Catalog", "Gallery with thumbs"),
        ("monsters-dat-Format", "Combat counterparts"),
    ],
    "monsters-dat-Format": [
        ("Combat-Overview", "Round loop hub"),
        ("Monster-Sprites", ".anm animations"),
        ("Monsters", "Named catalog"),
    ],
    "roster-dat-Format": [
        ("items-dat-Format", "Equipped item ids"),
        ("Spells-and-Item-Use", "Spellbook bytes"),
        ("Game-State-Struct", "Party in memory"),
    ],
    "map-dat-Format": [
        ("3D-View-and-Game-Screen", "Frustum renderer"),
        ("attrib-dat-Format", "Per-screen env"),
        ("event-dat-Format", "Event flag 0x80"),
        ("Map-Walker", "Interactive explorer"),
    ],
    "event-dat-Format": [
        ("Events-by-Location", "All 71 location scripts"),
        ("Event-Script-Opcodes", "0x00..0x32 reference"),
        ("map-dat-Format", "Collision triggers"),
        ("MM2ED-Editor", "Node-graph editor"),
    ],
    "Events-by-Location": [
        ("Events-By-Location-Hub", "Numbered doc series entry"),
        ("event-dat-Format", "Container layout"),
        ("Event-Script-Opcodes", "Opcode reference"),
        ("map-dat-Format", "60 map screens"),
        ("Mount-Farview-Class-Quest", "Class quests (loc 34)"),
    ],
    "Events-By-Location-Hub": [
        ("Events-by-Location", "Full index + 71 pages"),
        ("event-dat-Format", "event.dat format"),
        ("Event-Runtime", "Collision 0x80 → VM"),
    ],
    "Combat-Overview": [
        ("Combat-System", "Full ASM trace"),
        ("monsters-dat-Format", "Ability bytes"),
        ("Event-Script-Opcodes", "OP_12 encounters"),
    ],
    "Combat-System": [
        ("Combat-Overview", "Summary + mermaid"),
        ("Spells-and-Item-Use", "Combat spell flags"),
        ("3D-View-and-Game-Screen", "Encounter sprites"),
    ],
    "3D-View-and-Game-Screen": [
        ("GFX-Loading", "Asset table"),
        ("map-dat-Format", "Page 0 wall bits"),
        ("3D-View-Graphics", "Gallery composites"),
        ("Map-Walker", "Walk every screen"),
    ],
    "Audio-Sounds-Music": [
        ("Main-Loop-and-Map", "World simulation"),
        ("RE-Tools", "Audio export scripts"),
    ],
    "Startup-and-Init": [
        ("Runtime-Memory-Map", "A4 workspace"),
        ("Main-Loop-and-Map", "Scheduler"),
    ],
    "MM2ED-Editor": [
        ("dat-Files-and-Formats", "What each section edits"),
        ("RE-Tools", "Python codecs"),
    ],
    "Town-Services": [
        ("Spell-Sources", "Where temple/guild spells come from"),
        ("Event-to-String-Path", "OP_0E vs event strings"),
        ("Event-Script-Opcodes", "OP_0E / OP_0B / OP_24"),
        ("Embedded-Exe-Strings", "Exe-embedded shop prompts"),
    ],
    "Spell-Sources": [
        ("Town-Services", "Temple & mage guild handlers"),
        ("Spells-and-Item-Use", "spells.dat flags & item use byte"),
        ("items-dat-Format", "Item use-power @ 0x0F"),
    ],
    "Event-to-String-Path": [
        ("Town-Services", "OP_0E service handlers"),
        ("event-dat-Format", "Per-location string banks"),
        ("Embedded-Exe-Strings", "PEA PC-relative exe text"),
        ("Event-Script-Opcodes", "OP_01..OP_06, OP_0B"),
    ],
    "Embedded-Exe-Strings": [
        ("Event-to-String-Path", "When exe text vs str.dat"),
        ("Town-Services", "Inn & service prompts"),
    ],
}

# PNG byte-strips / charts (path relative to wiki root, alt text)
DIAGRAMS: dict[str, tuple[str, str]] = {
    "dat-Files-and-Formats": ("images/diagrams/dat-inventory.png", ".dat file sizes and roles"),
    "items-dat-Format": ("images/diagrams/items-record.png", "20-byte items.dat record layout"),
    "roster-dat-Format": ("images/diagrams/roster-record.png", "130-byte roster.dat record layout"),
}

# Inline Mermaid diagrams — rendered natively by GitHub Wiki.
MERMAID: dict[str, str] = {
    "Overview": """flowchart LR
  bin["Amiga binary<br/>mm2 (68000)"] --> dis["Disassembly<br/>IRA + Capstone"]
  dis --> docs["EXTRACTED/docs<br/>format specs - ASM traces"]
  docs --> cod["Codecs<br/>Python + C round-trip"]
  docs --> ed["MM2ED editor"]
  docs --> gal["Sprite gallery"]
  docs --> walk["Map walker (HTML5)"]""",
    "dat-Files-and-Formats": """flowchart LR
  subgraph world["World data"]
    map["map.dat"]
    attrib["attrib.dat"]
    event["event.dat"]
  end
  subgraph entities["Entities & rules"]
    monsters["monsters.dat"]
    items["items.dat"]
    spells["spells.dat"]
    roster["roster.dat"]
  end
  attrib -- selects tileset --> map
  map -- bit 0x80 --> event
  event -- OP_12 --> monsters
  items -- use byte --> spells
  roster -- equips --> items
  roster -- knows --> spells""",
    "map-dat-Format": """flowchart TD
  screen["map.dat screen<br/>512 bytes"]
  screen --> p0["Page 0 - visual<br/>2-bit walls N/E/S/W"]
  screen --> p1["Page 1 - collision<br/>(dark&lt;&lt;1)|wall - bit 0x80 = event"]
  p0 --> r3d["3D renderer @ 0x2900"]
  p1 --> ev["event.dat triplets"]
  attrib["attrib.dat (per screen)"] -- selects --> r3d
  attrib --> tiles[".32 tileset + carto mode"]
  attrib --> nb["neighbour links"]""",
    "event-dat-Format": """flowchart LR
  mv["Party moves"] --> chk{"Collision<br/>bit 0x80 set?"}
  chk -- no --> mv
  chk -- yes --> scan["Scan triplets<br/>pos + flags &amp; facing"]
  scan --> seg["Load location<br/>71-entry header"]
  seg --> vm["Script VM @ 0x172CA<br/>opcodes 0x00..0x32"]
  vm --> fx["Effects: text - items<br/>teleport - OP_12 combat"]""",
    "Startup-and-Init": """flowchart TD
  dos["DOS/Exec hunk load"] --> load["Load .dat + .32 assets"]
  load --> a4["A4 workspace<br/>data_base + 0x7FFE"]
  a4 --> loop["Main loop LAB_1280"]
  loop --> vp["Viewport refresh 0x5382"]
  vp --> v3d["3D view 0x2ECE"]""",
    "3D-View-and-Game-Screen": """flowchart TD
  refresh["session refresh 0x5436"] --> vp["viewport 0x5382"]
  vp --> mode{"view mode -$79E2"}
  mode -- "0" --> persp["3D first-person 0x2ECE"]
  mode -- else --> alt["auto-map / text -$7D34"]
  persp --> floor["floor backdrop"]
  persp --> sky["sky / ceiling backdrop"]
  persp --> cells["wall-cell builder 0x2900"]
  cells --> paint["paint walls back-to-front"]""",
    "Audio-Sounds-Music": """flowchart TD
  step["Party step / UI pump"] --> beep{"Walk Beep<br/>flag -$79AF?"}
  beep -- on --> thunk["JSR -$7FD4(A4)<br/>sound dispatch"]
  beep -- off --> silent["(no click)"]
  snd{"Sounds master<br/>flag -$79B0?"} --> thunk
  thunk --> dev["audio.device<br/>9 named channels"]
  ctrl["Controls menu keys 1-4"] -.toggles.-> snd
  ctrl -.toggles.-> beep""",
    "Town-Services": """flowchart TD
  tile["Tile trigger<br/>collision 0x80"] --> scan["Scanner 0x1754A"]
  scan --> vm["Script VM 0x172CA"]
  vm --> op0e["OP_0E selector byte"]
  op0e --> svc{"Service handler"}
  svc --> pub["Pub 0x1A132"]
  svc --> inn["Inn -$7CD4"]
  svc --> temp["Temple 0x1D208"]
  svc --> guild["Mage guild"]
  svc --> train["Training 0x9BCA"]
  pub --> str["str.dat UI strings"]
  inn --> exe["exe-embedded ASCII"]
  temp --> spells["3 cleric spells/town"]
  guild --> sorc["4 sorcerer spells/town"]""",
    "Event-to-String-Path": """flowchart LR
  script["event.dat script"] --> op{"opcode"}
  op -- OP_01..06 --> evs["Location string bank<br/>A4-$47C8"]
  op -- OP_0B --> evs
  op -- OP_0E --> hdl["Service handler"]
  hdl --> str["str.dat A4-$703e"]
  hdl --> exe["Code hunk ASCII"]""",
}

TOC_PAGES = {
    "Combat-System",
    "3D-View-and-Game-Screen",
    "Event-Script-Opcodes",
    "Full-Analysis",
    "Runtime-Memory-Map",
    "Audio-Sounds-Music",
    "Town-Services",
    "Spell-Sources",
    "Event-to-String-Path",
    "Embedded-Exe-Strings",
    "monsters-dat-Format",
    "items-dat-Format",
    "event-dat-Format",
    "MM2ED-Editor",
    "dat-Files-and-Formats",
}

_OVERVIEW_BODY = """# MM2 Reverse-Engineering Overview

This wiki splits the big [Full Analysis](Full-Analysis) narrative into focused pages
that are easier to evolve while reversing and decompiling.

## Document map

| Topic | Page | Contents |
|-------|------|----------|
| Boot | [Startup and Init](Startup-and-Init) | Hunk entry, DOS/Exec, MANX heap |
| Memory | [Runtime Memory Map](Runtime-Memory-Map) | `A4` workspace, thunk tables |
| World | [Main Loop and Map](Main-Loop-and-Map) | Scheduler `LAB_1280`, overland renderer |
| Party | [Party and Session](Party-and-Session) | New game, session restart |
| Open items | [Open Questions](Open-Questions) | Unknowns and TODOs |
| Graphics | [GFX Loading](GFX-Loading) | `.32`/`.dat` asset table |
| Data | [dat Files and Formats](dat-Files-and-Formats) | All `.dat` layouts |

## Reading order

New here? Start with [Getting Started](Getting-Started), then [dat Files and Formats](dat-Files-and-Formats).

Editing data? Jump to [MM2ED Editor](MM2ED-Editor).

Tracing combat or scripts? [Combat Overview](Combat-Overview) - [Event Script Opcodes](Event-Script-Opcodes).

Town shops, temples, and spell acquisition? [Town Services](Town-Services) - [Spell Sources](Spell-Sources).

## Address rules

- Capstone addresses are code-hunk-relative (segment 0 base 0).
- IRA listing tends to be offset by `+0x20` from Capstone in early code.
- Always verify with raw bytes in `mm2` when labels disagree.

## Source files (in repo)

- `EXTRACTED/mm2.capstone.asm` - Capstone disassembly
- `EXTRACTED/mm2.asm` - IRA listing
- `EXTRACTED/mm2-ANALYSIS.md` - full running narrative -> [Full Analysis](Full-Analysis)
"""

BODY_REPLACEMENTS: dict[str, str] = {
    "Overview": _OVERVIEW_BODY,
}


def _nav_box(title: str, section: str) -> str:
    home = "[Home](Home)"
    if title == "Getting-Started":
        return f"> **Navigation:** {home} - You are here: **Getting Started**\n\n"
    if title == "Home":
        return ""
    sec = section or "Wiki"
    label = title.replace("-", " ")
    return f"> **Navigation:** {home} - **{sec}** - {label}\n\n"


def _diagram_block(title: str) -> str:
    if title in MERMAID:
        return f"```mermaid\n{MERMAID[title]}\n```\n\n"
    if title in DIAGRAMS:
        path, alt = DIAGRAMS[title]
        return (
            f'<p align="center">\n'
            f'<img src="{path}" width="720" alt="{alt}"/>\n'
            f"</p>\n\n"
        )
    return ""


def _related_footer(title: str) -> str:
    links = RELATED.get(title)
    if not links:
        return ""
    rows = "\n".join(
        f"| [{page.replace('-', ' ')}]({page}) | {desc} |" for page, desc in links
    )
    return (
        "\n---\n\n"
        "### See also\n\n"
        "| Page | Why |\n"
        "|------|-----|\n"
        f"{rows}\n"
    )


def _inject_toc(body: str, title: str) -> str:
    if title not in TOC_PAGES or "[TOC]" in body:
        return body
    lines = body.split("\n")
    insert_at = 0
    for i, line in enumerate(lines):
        if line.startswith("#"):
            insert_at = i + 1
            break
    while insert_at < len(lines) and lines[insert_at].strip() == "":
        insert_at += 1
    # skip the nav-quote + diagram block to land after the lead paragraph
    while insert_at < len(lines) and (
        lines[insert_at].strip() == ""
        or lines[insert_at].startswith(">")
        or lines[insert_at].startswith("```")
        or lines[insert_at].startswith("<")
    ):
        if lines[insert_at].startswith("```"):
            insert_at += 1
            while insert_at < len(lines) and not lines[insert_at].startswith("```"):
                insert_at += 1
        insert_at += 1
    while insert_at < len(lines) and lines[insert_at].strip() != "":
        insert_at += 1
    lines.insert(insert_at + 1, "\n[TOC]\n")
    return "\n".join(lines)


def enrich_page(title: str, body: str) -> str:
    if title in BODY_REPLACEMENTS:
        body = BODY_REPLACEMENTS[title]

    section = SECTIONS.get(title, "")
    lines = body.lstrip().split("\n")
    if lines and lines[0].startswith("# "):
        h1 = lines[0] + "\n\n"
        rest = "\n".join(lines[1:]).lstrip()
        out = h1 + _nav_box(title, section) + _diagram_block(title) + rest
    else:
        out = _nav_box(title, section) + _diagram_block(title) + body

    out = _inject_toc(out, title)
    out += _related_footer(title)
    return out


def getting_started_page() -> str:
    return """# Getting Started

> **Navigation:** [Home](Home) - You are here: **Getting Started**

Welcome to the **Might & Magic II (Amiga)** reverse-engineering wiki. Everything here
is rebuilt from the original 68000 disassembly. Pick a path below based on what you
want to do.

```mermaid
flowchart LR
  bin["Amiga binary<br/>mm2 (68000)"] --> dis["Disassembly<br/>IRA + Capstone"]
  dis --> docs["EXTRACTED/docs<br/>format specs - ASM traces"]
  docs --> cod["Codecs (Python + C)"]
  docs --> ed["MM2ED editor"]
  docs --> gal["Sprite gallery"]
  docs --> walk["Map walker"]
```

---

## Choose your path

<table>
<tr>
<td width="33%" valign="top">

### Understand the project

1. [Overview](Overview) - doc map & address rules
2. [dat Files and Formats](dat-Files-and-Formats) - every `.dat` file
3. [Runtime Memory Map](Runtime-Memory-Map) - `A4` workspace
4. [Full Analysis](Full-Analysis) - long-form narrative

</td>
<td width="33%" valign="top">

### Edit game data

1. [MM2ED Editor](MM2ED-Editor) - ImGui editor build
2. [items.dat](items-dat-Format) - [monsters.dat](monsters-dat-Format)
3. [map.dat](map-dat-Format) - [event.dat](event-dat-Format)
4. [RE Tools](RE-Tools) - Python round-trip codecs

</td>
<td width="33%" valign="top">

### Trace game logic

1. [Main Loop and Map](Main-Loop-and-Map) - scheduler
2. [Combat Overview](Combat-Overview) -> [Combat System](Combat-System)
3. [Events by location](Events-by-Location) - all 71 map scripts
4. [Event Script Opcodes](Event-Script-Opcodes) - script VM
5. [Town Services](Town-Services) - pub, temple, guild, training
6. [3D View and Game Screen](3D-View-and-Game-Screen) - renderer

</td>
</tr>
</table>

---

## How the data fits together

The world is `map.dat` (geometry) + `attrib.dat` (environment) + `event.dat` (scripts).
Entities and rules live in `monsters.dat`, `items.dat`, `spells.dat`, and `roster.dat`.

```mermaid
flowchart TD
  map["map.dat screen<br/>512 bytes"] --> p0["Page 0 - visual walls"]
  map --> p1["Page 1 - collision<br/>bit 0x80 = event"]
  p0 --> r3d["3D renderer 0x2900"]
  p1 --> ev["event.dat triplets"]
  ev --> vm["script VM 0x172CA"]
  vm -- OP_12 --> combat["combat round loop 0x12A22"]
  attrib["attrib.dat"] -- tileset/env --> r3d
```

Combat is a round loop kicked off by event opcode **OP_12** - see
[Combat Overview](Combat-Overview) for the full chart and the command keys
(**A/F/S/C/U/B/R/E/V**).

---

## Interactive tools

| Tool | Link | What it does |
|------|------|--------------|
| **Map walker** | [GitHub Pages](https://Vairn.github.io/MM2/maze-walker/) | First-person 3D - all 60 screens |
| **Sprite gallery** | [Gallery](Gallery) | Decoded `.32` tilesets + `.anm` combat art |
| **MM2ED** | [Editor docs](MM2ED-Editor) | Edit `.dat` files with live hex + previews |

---

> [!IMPORTANT]
> **Endianness** - MM2 `.dat` multibyte fields are **little-endian on disk** (matching
> Blitz3D editors and our codecs). The 68000 runtime may byte-swap after load - always
> trace ASM before assuming otherwise.

## What's still unknown?

[Open Questions](Open-Questions) tracks unresolved fields and next ASM trace targets.
"""

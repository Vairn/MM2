#!/usr/bin/env python3
"""Export embedded map walker bundle (map.dat + attrib.dat + .32 sprites)."""
from __future__ import annotations

import base64
import io
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

from attrib_codec import AttribRecord  # noqa: E402
from decode_event import (  # noqa: E402
    read_header,
    decode_location,
    decode_strings,
    split_script_segments,
    parse_segment_stream_nodes,
    decompile_op,
    try_load_default_items,
    looks_like_text_record,
)
from mm2_gfx_export import (  # noqa: E402
    area_name,
    frame_count_32,
    load_attrib,
    load_frame,
    load_map_screens,
    resolve_asset,
    resolve_data_dir,
)
from render_view_refs import ORIGIN_X, FLOOR_Y, SKY_Y, VIEW_H, VIEW_W  # noqa: E402
from view3d_indoor import K_CARTO_TILE  # noqa: E402

WALKER_SHEETS = (
    "town.32",
    "townf.32",
    "townt.32",
    "cave.32",
    "cavef.32",
    "cavet.32",
    "castle.32",
    "castlef.32",
    "castlet.32",
    "sky.32",
    "outf.32",
    "outdoor1.32",
    "outdoor2.32",
    "outdoor3.32",
    "desert.32",
    "ocean.32",
    "tundra.32",
    "swamp.32",
    "townb.32",
    "outb.32",
)

DATA_BIN = ROOT / "EXTRACTED" / "ghidra" / "mm2_data_00.bin"
A4 = 0x7FFE
BUNDLE_NAME = "walker-bundle.js"

DEFAULT_GOG = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")

# OP_0E default-range overlay categories (asm 0x15EDC bins -> event.dat loc 60..70).
OVERLAY_CATEGORIES = list(range(0x3C, 0x47))


def string_looks_like_bytecode(seg: bytes) -> bool:
    """Mirror eventVmStringLooksLikeBytecode @ EventVmHelpers.cpp."""
    if not seg:
        return False
    op = seg[0]
    if op == 0 or op >= 0x33:
        return False
    if op == 0x12:
        return len(seg) >= 12
    if op == 0x13:
        return len(seg) >= 10
    if len(seg) >= 2 and seg[1] < 0x20:
        return True
    return False


def overlay_string_anchor(blob: bytes) -> int:
    """LE u16 at work_buf[0..1] — ASM queued dispatch @ 0x176C2 rebuilds this."""
    if len(blob) < 2:
        return 0
    return int(blob[0]) | (int(blob[1]) << 8)


def pool_seek_segments(blob: bytes, start: int = 2) -> list[bytes]:
    """FF-delimited pool from parse_pos (ASM pool_seek @ 0x17262).

    Default-range overlays memcpy the whole location raw into work_buf, then
    pool_seek from offset 2 with QUEUED_EVENT_ID. Do NOT index the codec string
    table — loc 60 embeds bytecode in early pool slots and prose after the LE
    string anchor (Corak/Nordon/Nordonna/Feldecarb).
    """
    segs: list[bytes] = []
    pos = start
    while pos < len(blob):
        seg_start = pos
        while pos < len(blob) and blob[pos] != 0xFF:
            pos += 1
        segs.append(blob[seg_start:pos])
        if pos >= len(blob):
            break
        pos += 1
    return segs


def decode_overlay_slot(
    blob: bytes, strings: list[str], index: int, items, loc_id: int
) -> dict | None:
    """Decode one default-range overlay slot for JS runDefaultRangeOverlay."""
    segments = pool_seek_segments(blob, 2)
    if index < 0 or index >= len(segments):
        return None
    seg = segments[index]
    if not seg:
        return None

    if looks_like_text_record(seg):
        text = seg.decode("ascii", errors="replace").replace("@", "\n")
        return {"kind": "text", "text": text}

    if string_looks_like_bytecode(seg):
        nodes = []
        pseudo = []
        for node in parse_segment_stream_nodes(seg):
            op = int(node["op"])
            args = [int(x) for x in node["args"]]
            line = decompile_op(op, args, strings, items, loc_id)
            pseudo.append(line)
            nodes.append({"op": op, "args": args, "pseudo": line})
        return {"kind": "bytecode", "nodes": nodes, "pseudo": pseudo}

    try:
        text = seg.decode("ascii", errors="replace").replace("@", "\n").strip()
    except Exception:
        text = ""
    if text:
        return {"kind": "text", "text": text}
    return None


def build_overlays_payload(event_data: bytes, event_header: list, items) -> dict:
    """Export event.dat overlay locations for OP_0E default-range VM re-entry."""
    overlays: dict = {}
    for cat in OVERLAY_CATEGORIES:
        if cat >= len(event_header):
            continue
        off, length = event_header[cat]
        blob = event_data[off : off + length]
        loc = decode_location(blob, cat)
        # Prefer ASM LE string anchor over codec string_table_offset — the latter
        # can mis-parse when 00 00 00 appears inside early pool bytecode (loc 60).
        anchor = overlay_string_anchor(blob)
        strings = (
            decode_strings(blob, anchor)
            if anchor < len(blob)
            else loc["strings"]
        )
        segments = pool_seek_segments(blob, 2)
        slots: dict = {}
        for idx in range(len(segments)):
            slot = decode_overlay_slot(blob, strings, idx, items, cat)
            if slot:
                slots[str(idx)] = slot
        overlays[str(cat)] = {
            "id": cat,
            "kind": loc.get("record_kind", "unknown"),
            "strings": strings,
            "slots": slots,
        }
    return overlays


def sheet_key(name: str) -> str:
    return name.replace(".", "_")


def load_terrain_lookup() -> list[int]:
    if not DATA_BIN.is_file():
        return [0] * 256
    blob = DATA_BIN.read_bytes()
    off = A4 - 0x720A
    if off < 0 or off + 256 > len(blob):
        return [0] * 256
    return list(blob[off : off + 256])


def load_item_gold(data_dir: Path) -> list[int]:
    from decode_items import decode_record

    for candidate in (data_dir / "items.dat", ROOT / "items.dat", ROOT / "EXTRACTED" / "items.dat"):
        if candidate.is_file():
            raw = candidate.read_bytes()
            rec_size = 0x14
            gold = []
            for i in range(len(raw) // rec_size):
                rec = raw[i * rec_size : (i + 1) * rec_size]
                gold.append(decode_record(rec).gold)
            return gold
    return []


def load_default_party(data_dir: Path) -> dict | None:
    """First six roster slots (0..5) — default Goto-Town party in roster.dat."""
    import struct

    from decode_roster import decode_record, load_roster_bytes

    for candidate in (
        data_dir / "roster.dat",
        data_dir / "ROSTER.DAT",
        ROOT / "roster.dat",
        ROOT / "EXTRACTED" / "roster.dat",
    ):
        if not candidate.is_file():
            continue
        try:
            data = load_roster_bytes(candidate)
        except ValueError:
            continue
        members = []
        roster_slots = []
        for slot in range(6):
            rec = decode_record(data, slot)
            if rec is None:
                break
            off = slot * 130
            hp_aux = struct.unpack_from("<H", data, off + 0x60)[0]
            sb = rec["stats_base"]
            members.append(
                {
                    "rosterIndex": slot,
                    "name": rec["name"],
                    "sex": data[off + 0x0C],
                    "alignmentBase": data[off + 0x6A],
                    "race": data[off + 0x0E],
                    "classId": data[off + 0x0F],
                    "level": rec["level"],
                    "spellLevel": rec["spell_lvl"],
                    "gold": rec["gold"],
                    "gems": rec["gems"],
                    "experience": rec["xp"],
                    "hpCurrent": rec["hp_cur"],
                    "hpMax": rec["hp_max"],
                    "hpAux": hp_aux,
                    "spCurrent": rec["sp_cur"],
                    "spMax": rec["sp_max"],
                    "condition": rec["condition"],
                    "armorClass": rec["ac"],
                    "age": rec["age"],
                    "food": rec["food"],
                    "endurance": sb["End"],
                    "intelligence": sb["Int"],
                    "personality": sb["Per"],
                    "might": sb["Might"],
                    "speed": sb["Spd"],
                    "accuracy": sb["Acc"],
                    "luck": sb["Luck"],
                    "thievery": data[off + 0x16],
                    "unlockSkill": data[off + 0x1E],
                    "homeTown": data[off + 0x0B] & 0x7F,
                    "classQuestGuildMask": data[off + 0x79],
                    "skillPack": data[off + 0x50],
                    "spellBook": list(data[off + 0x4C : off + 0x58]),
                    "pubMeal": data[off + 0x78],
                    "backpack": [
                        {"id": item_id, "charges": ch, "flags": fl}
                        for item_id, ch, fl in rec["backpack"]
                    ],
                    "equipped": [
                        {"id": item_id, "charges": ch, "flags": fl}
                        for item_id, ch, fl in rec["equip"]
                    ],
                }
            )
            roster_slots.append(slot)
        if not members:
            return None
        return {
            "partyCount": len(members),
            "rosterSlots": roster_slots,
            "members": members,
            "source": str(candidate.name),
        }
    return None


def load_font() -> list[list[int]]:
    import re
    content = (ROOT / "game" / "src" / "gfx" / "Mm2Font8x8.inc").read_text()
    matches = re.findall(r'\{([0-9a-fA-Fx, ]+)\}', content)
    font = []
    for m in matches:
        font.append([int(x.strip(), 16) for x in m.split(',') if x.strip()])
    return font


def collect_sprites(data_dir: Path) -> tuple[dict, dict[str, str]]:
    """Build manifest + base64 data-URL sprite table."""
    from mm2_gfx_export import load_anm_asset, composite_anm_frame, load_monster_records
    
    manifest: dict = {
        "view": {
            "w": VIEW_W,
            "h": VIEW_H,
            "originX": ORIGIN_X,
            "skyY": SKY_Y,
            "floorY": FLOOR_Y,
        },
        "sheets": {},
        "anm": {},
        "monsters": load_monster_records(data_dir),
        "items": try_load_default_items(),
        "itemsGold": load_item_gold(data_dir),
        "defaultParty": load_default_party(data_dir),
        "terrainLookup": load_terrain_lookup(),
        "cartoTile": list(K_CARTO_TILE),
        "font": load_font(),
    }
    sprites: dict[str, str] = {}
    exported = 0
    for sheet in WALKER_SHEETS:
        if not resolve_asset(data_dir, sheet):
            continue
        key = sheet_key(sheet)
        frames_meta: dict[str, dict] = {}
        n = frame_count_32(sheet, data_dir)
        for fi in range(n):
            try:
                img = load_frame(sheet, fi, data_dir)
            except Exception:
                break
            buf = io.BytesIO()
            img.save(buf, format="PNG")
            b64 = base64.b64encode(buf.getvalue()).decode("ascii")
            cache_key = f"{key}:{fi}"
            sprites[cache_key] = f"data:image/png;base64,{b64}"
            frames_meta[str(fi)] = {"w": img.width, "h": img.height}
            exported += 1
        if frames_meta:
            manifest["sheets"][key] = {"file": sheet, "frames": frames_meta}

    for anm_idx in range(1, 75):
        sheet = f"{anm_idx:02d}.anm"
        p = resolve_asset(data_dir, sheet)
        if not p:
            continue
        key = sheet_key(sheet)
        frames_meta: dict[str, dict] = {}
        asset = load_anm_asset(p)
        for fi in range(asset.frame_count):
            img = composite_anm_frame(asset, fi)
            if not img:
                continue
            buf = io.BytesIO()
            img.save(buf, format="PNG")
            b64 = base64.b64encode(buf.getvalue()).decode("ascii")
            cache_key = f"{key}:{fi}"
            sprites[cache_key] = f"data:image/png;base64,{b64}"
            frames_meta[str(fi)] = {"w": img.width, "h": img.height}
            exported += 1
        if frames_meta:
            manifest["anm"][key] = {
                "file": sheet,
                "frames": frames_meta,
                "flipbookDelay": 5,
            }
            
    print(f"  sprites: {exported} frames across {len(manifest['sheets'])} sheets and {len(manifest['anm'])} anms")
    return manifest, sprites


_PC_SHEET_CACHE: dict[tuple[str, str], dict] = {}

# Indoor ceiling/torch overlays: full CGA pen 0/1 silhouette colour-key.
_PC_OVERLAY_SHEETS = frozenset({
    "townt.32",
    "cavet.32",
    "castlet.32",
})

# Outdoor horizon mountains/trees: same paired void mask on every frame (front + side).
_PC_OUTDOOR_HORIZON_SHEETS = frozenset({
    "outdoor1.32",
    "outdoor2.32",
    "outdoor3.32",
})

# Outdoor floor only — front frames still use pen-0 void where present.
_PC_OUTDOOR_OVERLAY_SHEETS = frozenset({
    "outf.32",
})

# Biome decor + minimap border sheets (wall layout, outdoor front key=0).
_PC_OUTDOOR_WALL_SHEETS = frozenset({
    "desert.32",
    "ocean.32",
    "tundra.32",
    "swamp.32",
    "outb.32",
})


def _resolve_pc_blob(stem: str, variant: str, gog_dir: Path, data_dir: Path) -> Path | None:
    ext = ".4" if variant == "cga" else ".16"
    for base in (gog_dir, data_dir, ROOT):
        path = base / f"{stem.upper()}{ext}"
        if path.is_file():
            return path
    return None


def _load_pc_sheet_info(variant: str, stem: str, path: Path) -> dict:
    from decode_pc_gfx import parse_wall_sheet  # noqa: E402

    key = (variant, stem)
    if key not in _PC_SHEET_CACHE:
        _PC_SHEET_CACHE[key] = parse_wall_sheet(path)
    return _PC_SHEET_CACHE[key]


def _cga_silhouette_for_frame(
    variant: str,
    stem: str,
    frame: int,
    pc_path: Path,
    data_dir: Path,
) -> bytes | None:
    if variant != "ega":
        return None
    cga_path = _resolve_pc_blob(stem, "cga", pc_path.parent, data_dir)
    if cga_path is None:
        return None
    cga_sheet = _load_pc_sheet_info("cga", stem, cga_path)
    cga_frames = cga_sheet["frames"]
    if frame >= len(cga_frames):
        return None
    return cga_frames[frame].pixels


def _ega_silhouette_for_frame(
    variant: str,
    stem: str,
    frame: int,
    pc_path: Path,
    data_dir: Path,
) -> bytes | None:
    if variant != "cga":
        return None
    ega_path = _resolve_pc_blob(stem, "ega", pc_path.parent, data_dir)
    if ega_path is None:
        return None
    ega_sheet = _load_pc_sheet_info("ega", stem, ega_path)
    ega_frames = ega_sheet["frames"]
    if frame >= len(ega_frames):
        return None
    return ega_frames[frame].pixels


def _render_pc_walker_frame(
    variant: str,
    stem: str,
    frame: int,
    pc_path: Path,
    amiga_sheet: str,
    data_dir: Path,
):
    from decode_pc_gfx import (  # noqa: E402
        render_outdoor_frame_rgba,
        render_outdoor_wall_frame_rgba,
        render_overlay_frame_rgba,
        render_pc_wall_frame_rgba,
        render_sky_frame_rgba,
        row_bytes,
    )
    from PIL import Image  # noqa: E402

    sheet = _load_pc_sheet_info(variant, stem, pc_path)
    frames = sheet["frames"]
    if frame >= len(frames):
        raise FileNotFoundError(f"{variant}/{stem} frame {frame}")
    fr = frames[frame]
    w, h, pix, bpp = fr.width, fr.height, fr.pixels, sheet["bpp"]
    rb = row_bytes(w, bpp)
    if len(pix) < rb * h:
        raise ValueError(f"{variant}/{stem} frame {frame}: short pixel run")

    cga_sil = _cga_silhouette_for_frame(variant, stem, frame, pc_path, data_dir)
    ega_sil = _ega_silhouette_for_frame(variant, stem, frame, pc_path, data_dir)

    if amiga_sheet in _PC_OVERLAY_SHEETS:
        rgba = render_overlay_frame_rgba(
            w, h, pix, bpp, cga_silhouette=cga_sil if cga_sil is not None else pix
        )
    elif amiga_sheet == "sky.32":
        rgba = render_sky_frame_rgba(
            w, h, pix, bpp, cga_silhouette=cga_sil, ega_silhouette=ega_sil
        )
    elif amiga_sheet in _PC_OUTDOOR_HORIZON_SHEETS:
        rgba = render_outdoor_wall_frame_rgba(
            w,
            h,
            pix,
            bpp,
            frame=frame,
            cga_silhouette=cga_sil,
            ega_silhouette=ega_sil,
        )
    elif amiga_sheet in _PC_OUTDOOR_OVERLAY_SHEETS:
        rgba = render_outdoor_frame_rgba(
            w,
            h,
            pix,
            bpp,
            frame=frame,
            cga_silhouette=cga_sil,
            ega_silhouette=ega_sil,
        )
    else:
        outdoor = amiga_sheet in _PC_OUTDOOR_WALL_SHEETS
        rgba = render_pc_wall_frame_rgba(
            w,
            h,
            pix,
            bpp,
            frame=frame,
            outdoor=outdoor,
            cga_silhouette=cga_sil,
            ega_silhouette=ega_sil,
        )
    img = Image.new("RGBA", (w, h))
    img.putdata(rgba)
    return img


def _export_pc_walker_monsters(
    variant_dir: Path,
    variant: str,
    gog: Path,
    data_dir: Path,
) -> dict:
    """Export MONSTERS.4 / MONSTERS.16 combat sprites keyed by requested picture id."""
    from export_monster_variants import resolve_pc_picture  # noqa: E402
    from decode_pc_gfx import (  # noqa: E402
        COMBAT_CANVAS_H,
        COMBAT_CANVAS_W,
        composite_pc_combat_frame,
        parse_monster_picture_blob,
    )
    from PIL import Image  # noqa: E402

    ext = ".4" if variant == "cga" else ".16"
    raw_path = _resolve_pc_blob("monsters", variant, gog, data_dir)
    if raw_path is None:
        return {}
    raw = raw_path.read_bytes()
    anm_meta: dict[str, dict] = {}
    frame_total = 0
    for picture_id in range(1, 75):
        resolved = resolve_pc_picture(raw, picture_id)
        if resolved is None:
            continue
        used_id, file_off = resolved
        blob = parse_monster_picture_blob(raw, file_off, used_id, ext)
        if blob is None or not blob.frames:
            continue
        key = f"{picture_id:02d}_{variant}"
        frames_meta: dict[str, dict] = {}
        by_idx = {fr.frame_index: fr for fr in blob.frames}
        for fidx in sorted(by_idx):
            rgba = composite_pc_combat_frame(by_idx, fidx)
            img = Image.new("RGBA", (COMBAT_CANVAS_W, COMBAT_CANVAS_H))
            img.putdata(rgba)
            rel = f"sprites/{key}/{fidx}.png"
            out_png = variant_dir / rel
            out_png.parent.mkdir(parents=True, exist_ok=True)
            img.save(out_png, format="PNG")
            frames_meta[str(fidx)] = {
                "w": COMBAT_CANVAS_W,
                "h": COMBAT_CANVAS_H,
                "path": rel,
            }
            frame_total += 1
        anm_meta[key] = {
            "file": raw_path.name,
            "pictureId": picture_id,
            "pcPictureId": used_id,
            "frames": frames_meta,
            "flipbookDelay": 5,
        }
    if anm_meta:
        print(
            f"  pc/{variant} monsters: {len(anm_meta)} pictures, {frame_total} frames"
        )
    return anm_meta


def export_pc_walker_gfx(out_dir: Path, data_dir: Path, gog_dir: Path | None) -> None:
    """Write wiki/maze-walker/pc/{cga,ega}/sprites + manifest.json (lazy-loaded in browser)."""
    gog = gog_dir or (DEFAULT_GOG if DEFAULT_GOG.is_dir() else ROOT)
    pc_root = out_dir / "pc"
    total = 0
    for variant in ("cga", "ega"):
        variant_dir = pc_root / variant
        sprite_root = variant_dir / "sprites"
        sprite_root.mkdir(parents=True, exist_ok=True)
        sheets_meta: dict[str, dict] = {}
        for sheet in WALKER_SHEETS:
            if not resolve_asset(data_dir, sheet):
                continue
            stem = sheet.replace(".32", "")
            pc_path = _resolve_pc_blob(stem, variant, gog, data_dir)
            if pc_path is None:
                continue
            key = f"{stem}_{variant}"
            frames_meta: dict[str, dict] = {}
            pc_info = _load_pc_sheet_info(variant, stem, pc_path)
            n = len(pc_info["frames"])
            for fi in range(n):
                try:
                    img = _render_pc_walker_frame(variant, stem, fi, pc_path, sheet, data_dir)
                except (FileNotFoundError, ValueError):
                    break
                rel = f"sprites/{key}/{fi}.png"
                out_png = variant_dir / rel
                out_png.parent.mkdir(parents=True, exist_ok=True)
                img.save(out_png, format="PNG")
                frames_meta[str(fi)] = {"w": img.width, "h": img.height, "path": rel}
                total += 1
            if frames_meta:
                sheets_meta[key] = {"file": pc_path.name, "frames": frames_meta}
        anm_meta = _export_pc_walker_monsters(variant_dir, variant, gog, data_dir)
        manifest = {
            "view": {
                "w": VIEW_W,
                "h": VIEW_H,
                "originX": ORIGIN_X,
                "skyY": SKY_Y,
                "floorY": FLOOR_Y,
            },
            "variant": variant,
            "sheets": sheets_meta,
            "anm": anm_meta,
        }
        (variant_dir / "manifest.json").write_text(
            json.dumps(manifest, separators=(",", ":")), encoding="utf-8"
        )
        wall_frames = sum(len(s["frames"]) for s in sheets_meta.values())
        print(f"  pc/{variant}: {len(sheets_meta)} sheets, {wall_frames} wall frames")
    if total == 0:
        print("  pc: skipped (no GOG .4/.16 blobs found — pass --pc-gog)")


def build_maps_payload(attrib: list[bytes], screens: list[tuple[bytes, bytes]], data_dir: Path) -> dict:
    """map.dat pages + attrib.dat per-screen fields (env, neighbours, roof ceiling mask)."""
    event_path = data_dir / "event.dat"
    if event_path.is_file():
        event_data = event_path.read_bytes()
        event_header = read_header(event_data)
        items = try_load_default_items()
    else:
        event_header = []
        items = []

    payload: dict = {"version": 3, "screens": []}
    for sid in range(60):
        visual, collision = screens[sid]
        a = attrib[sid]
        rec = AttribRecord(bytearray(a))
        
        screen_events = {}
        if sid < len(event_header):
            off, length = event_header[sid]
            blob = event_data[off : off + length]
            loc = decode_location(blob, sid)
            script = blob[loc["script_offset"] : loc["string_table_offset"]]
            segments = split_script_segments(script)
            
            by_evt = {}
            for pos, evt, flags in loc["triplets"]:
                by_evt.setdefault(evt, []).append({"pos": pos, "flags": flags})
                
            for evt, triggers in by_evt.items():
                if evt < len(segments):
                    seg = segments[evt]
                    if looks_like_text_record(seg):
                        txt = seg.decode("ascii", errors="replace").replace("@", "\n")
                        pseudo = [f'text_record: "{txt}"']
                        nodes = [{"op": -1, "text": txt}]
                    else:
                        parsed_nodes = parse_segment_stream_nodes(seg)
                        pseudo = []
                        nodes = []
                        for node in parsed_nodes:
                            op = int(node["op"])
                            args = [int(x) for x in node["args"]]
                            line = decompile_op(op, args, loc["strings"], items, sid)
                            pseudo.append(line)
                            nodes.append({"op": op, "args": args, "pseudo": line})
                    screen_events[evt] = {
                        "triggers": triggers,
                        "script": pseudo,
                        "nodes": nodes
                    }

        payload["screens"].append(
            {
                "id": sid,
                "name": area_name(sid, attrib),
                "outdoor": rec.is_outdoor,
                "env": rec.env_name,
                "mapCategory": rec.map_category,
                "surface": rec.surface_flag,
                "entry": [rec.entry_coord[0], rec.entry_coord[1]],
                "neighbors": list(rec.neighbors),
                "encounter": {
                    "stepRate": a[0x09],
                    "groupGate": a[0x0A],
                    "maxMonsters": a[0x0B],
                    "minMonsters": a[0x0C],
                    "retreat": a[0x0D],
                },
                "doorStrength": a[0x12],
                "doorTrap": a[0x13],
                "flags1A": a[0x1A],  # attrib flags → A4-$5600; bit7 = map-wide dark
                "sector": a[0x15],
                # attrib +0x20..+0x3F: 256 roof bits → sky.32 frame 1 (ceiling) vs 0 (open)
                "roof": list(a[0x20:0x40]),
                "visual": list(visual),
                "collision": list(collision),
                "events": screen_events,
                "strings": loc["strings"] if sid < len(event_header) else [],
            }
        )
    return payload


def write_embedded_bundle(out_dir: Path, maps: dict, manifest: dict, sprites: dict[str, str], overlays: dict) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    bundle_path = out_dir / BUNDLE_NAME
    body = "\n".join(
        [
            "// Auto-generated by tools/export_map_walker.py — commit this file for GitHub Pages.",
            f"export const WALKER_MAPS = {json.dumps(maps, separators=(',', ':'))};",
            f"export const WALKER_MANIFEST = {json.dumps(manifest, separators=(',', ':'))};",
            f"export const WALKER_OVERLAYS = {json.dumps(overlays, separators=(',', ':'))};",
            f"export const WALKER_SPRITES = {json.dumps(sprites, separators=(',', ':'))};",
            "",
        ]
    )
    bundle_path.write_text(body, encoding="utf-8")
    kb = bundle_path.stat().st_size / 1024
    print(f"  bundle: {bundle_path} ({kb:.0f} KiB)")
    return bundle_path


def export_map_walker(
    out_dir: Path,
    data_dir: Path | None = None,
    *,
    split: bool = False,
    pc_gog: Path | None = None,
    skip_pc: bool = False,
) -> dict:
    data = resolve_data_dir(data_dir)
    if not (data / "map.dat").is_file():
        raise FileNotFoundError(f"map.dat not found under {data}")
    if not (data / "attrib.dat").is_file():
        raise FileNotFoundError(
            f"attrib.dat not found under {data} (required for env, neighbours, roof/ceiling masks)"
        )
    attrib = load_attrib(data)
    screens = load_map_screens(data)
    maps = build_maps_payload(attrib, screens, data)
    manifest, sprites = collect_sprites(data)
    event_path = data / "event.dat"
    overlays: dict = {}
    if event_path.is_file():
        event_data = event_path.read_bytes()
        event_header = read_header(event_data)
        items = try_load_default_items()
        overlays = build_overlays_payload(event_data, event_header, items)
    write_embedded_bundle(out_dir, maps, manifest, sprites, overlays)

    if split:
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "maps.json").write_text(json.dumps(maps, separators=(",", ":")), encoding="utf-8")
        sprite_root = out_dir / "sprites"
        for cache_key, data_url in sprites.items():
            key, fi = cache_key.rsplit(":", 1)
            dir_path = sprite_root / key
            dir_path.mkdir(parents=True, exist_ok=True)
            raw = base64.b64decode(data_url.split(",", 1)[1])
            (dir_path / f"{fi}.png").write_bytes(raw)
        split_manifest = json.loads(json.dumps(manifest))
        for key, sheet in split_manifest["sheets"].items():
            for fi, fr in sheet["frames"].items():
                fr["path"] = f"sprites/{key}/{fi}.png"
        for key, sheet in split_manifest.get("anm", {}).items():
            for fi, fr in sheet["frames"].items():
                fr["path"] = f"sprites/{key}/{fi}.png"
        (out_dir / "sprites.json").write_text(
            json.dumps(split_manifest, separators=(",", ":")), encoding="utf-8"
        )
        print(f"  split: maps.json + sprites.json + {len(sprites)} PNGs")

    if not skip_pc:
        export_pc_walker_gfx(out_dir, data, pc_gog)

    return maps


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="Export embedded bundle for the HTML5 map walker")
    ap.add_argument(
        "-o",
        "--out",
        type=Path,
        default=ROOT / "wiki" / "maze-walker",
    )
    ap.add_argument("--data", type=Path, default=None)
    ap.add_argument(
        "--split",
        action="store_true",
        help="Also write maps.json, sprites.json, and loose PNGs (local dev)",
    )
    ap.add_argument(
        "--pc-gog",
        type=Path,
        default=None,
        help="GOG install dir with TOWN.4 / TOWN.16 (default: standard GOG path or repo root)",
    )
    ap.add_argument(
        "--skip-pc",
        action="store_true",
        help="Do not export PC CGA/EGA sprite trees under pc/",
    )
    args = ap.parse_args()
    try:
        export_map_walker(
            args.out,
            args.data,
            split=args.split,
            pc_gog=args.pc_gog,
            skip_pc=args.skip_pc,
        )
    except FileNotFoundError as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

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
            manifest["anm"][key] = {"file": sheet, "frames": frames_meta}
            
    print(f"  sprites: {exported} frames across {len(manifest['sheets'])} sheets and {len(manifest['anm'])} anms")
    return manifest, sprites


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
                "surface": rec.surface_flag,
                "entry": [rec.entry_coord[0], rec.entry_coord[1]],
                "neighbors": list(rec.neighbors),
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


def write_embedded_bundle(out_dir: Path, maps: dict, manifest: dict, sprites: dict[str, str]) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    bundle_path = out_dir / BUNDLE_NAME
    body = "\n".join(
        [
            "// Auto-generated by tools/export_map_walker.py — commit this file for GitHub Pages.",
            f"export const WALKER_MAPS = {json.dumps(maps, separators=(',', ':'))};",
            f"export const WALKER_MANIFEST = {json.dumps(manifest, separators=(',', ':'))};",
            f"export const WALKER_SPRITES = {json.dumps(sprites, separators=(',', ':'))};",
            "",
        ]
    )
    bundle_path.write_text(body, encoding="utf-8")
    kb = bundle_path.stat().st_size / 1024
    print(f"  bundle: {bundle_path} ({kb:.0f} KiB)")
    return bundle_path


def export_map_walker(out_dir: Path, data_dir: Path | None = None, *, split: bool = False) -> dict:
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
    write_embedded_bundle(out_dir, maps, manifest, sprites)

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
    args = ap.parse_args()
    try:
        export_map_walker(args.out, args.data, split=args.split)
    except FileNotFoundError as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Build side-by-side platform compare pages: monsters + wall/sprite sheets.

Exports numbered thumbs for:
  - Combat monsters: picture ids 01–74 (MONSTERS.4 / .16 / NN.anm / SNES table)
  - Wall sheets: per-frame CGA / EGA / Amiga .32 (TOWN, CASTLE, SKY, …)

Usage:
  python tools/gen_gfx_compare_html.py
  python tools/gen_gfx_compare_html.py -o wiki/public/gallery/platform-compare
  python tools/gen_gfx_compare_html.py --skip-walls
"""
from __future__ import annotations

import argparse
import html
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GOG = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")
DEFAULT_OUT = ROOT / "EXTRACTED" / "pc_gfx" / "compare"
COMPARE_CACHE_VERSION = 6

SNES_MONSTER_SPRITE_DIRS = [
    ROOT / "EXTRACTED" / "SNES" / "gfx" / "export" / "monsters" / "sprites",
    ROOT / "EXTRACTED" / "SNES" / "gfx" / "monsters_assembled",
    ROOT / "EXTRACTED" / "snes" / "gfx" / "export" / "monsters" / "sprites",
    ROOT / "EXTRACTED" / "snes" / "gfx" / "monsters_assembled",
]

MAC_RSRC = ROOT / "EXTRACTED" / "MAC" / "Might_and_Magic_II_0" / "extracted_rsrc"
MAC_COLOR_LZPP = MAC_RSRC / "Color" / "images" / "lzpp"
MAC_COLOR_FRAMES = MAC_COLOR_LZPP / "frames"
MAC_BW_PPT_FRAMES = (
    MAC_RSRC / "Data" / "images" / "ppt" / "frames",
    MAC_RSRC / "BW" / "images" / "ppt" / "frames",
    MAC_RSRC / "BW_ART" / "frames",
)
MAC_BW_VIEW_ENV = MAC_RSRC / "BW_ART" / "view_env"

sys.path.insert(0, str(ROOT / "tools"))

from decode_pc_gfx import (  # noqa: E402
    COMBAT_CANVAS_H,
    COMBAT_CANVAS_W,
    _decompress_monster_blob,
    _monster_table_end,
    _parse_monster_picture,
    composite_monster_layers,
    decode_wall_frame_rgb,
    parse_wall_sheet,
    write_png_rgb,
)
from decode_monsters import decode_record  # noqa: E402
from mm2_gfx_export import (  # noqa: E402
    NON_GFX_32,
    composite_anm_frame,
    flatten_rgba,
    frame_count_32,
    load_anm_asset,
    load_frame,
    resolve_asset,
    scale_nearest,
)

try:
    from PIL import Image
except ImportError:
    Image = None  # type: ignore

MONSTERS_DAT = ROOT / "monsters.dat"
RECORD_SIZE = 26
WIKI_BG = (13, 3, 3, 255)
MONSTER_SCALE = 3
WALL_SCALE = 2

SKIP_PC_STEMS = frozenset({"monsters", "xfer"})
SKIP_WALL_STEMS = frozenset({"disk", "globe"})

WALL_STEM_ORDER = [
    "sky",
    "townf",
    "town",
    "townb",
    "townt",
    "castlef",
    "castle",
    "castleb",
    "castlet",
    "cavef",
    "cave",
    "caveb",
    "cavet",
    "outf",
    "outb",
    "outdoor1",
    "outdoor2",
    "outdoor3",
    "desert",
    "ocean",
    "tundra",
    "swamp",
    "throw",
    "master",
    "endgame",
    "book",
    "nwcp",
]


def _blob_ok(raw: bytes, fo: int, table_end: int) -> bool:
    if fo <= 0 or fo < table_end:
        return False
    dec = _decompress_monster_blob(raw, fo)
    return dec is not None and (dec[0] & 0x3F) > 0


def resolve_monster_slot(raw: bytes, picture_id: int) -> tuple[str | None, int]:
    """Picture id ``N`` (1-based) -> blob file offset via header entry ``N-1`` (0-based)."""
    table_end = _monster_table_end(raw)
    entry = picture_id - 1
    if entry < 0 or entry * 4 + 4 > table_end:
        return None, 0
    fo = struct.unpack_from("<I", raw, entry * 4)[0]
    if fo and _blob_ok(raw, fo, table_end):
        return "u32", fo
    return None, 0


def header_u32(raw: bytes, picture_id: int) -> int:
    entry = picture_id - 1
    if entry < 0 or entry * 4 + 4 > len(raw):
        return 0
    return struct.unpack_from("<I", raw, entry * 4)[0]


def load_monsters_by_picture(path: Path) -> dict[int, list[str]]:
    if not path.is_file():
        return {}
    raw = path.read_bytes()
    out: dict[int, list[str]] = {}
    for idx in range(len(raw) // RECORD_SIZE):
        rec = raw[idx * RECORD_SIZE : (idx + 1) * RECORD_SIZE]
        if len(rec) < RECORD_SIZE:
            break
        pic = decode_record(idx, rec)["picture"] & 0x7F
        if pic == 0:
            continue
        name = decode_record(idx, rec)["name"].strip()
        if not name:
            continue
        out.setdefault(pic, []).append(name)
    return out


def format_monster_list(names: list[str], max_show: int = 6) -> str:
    if not names:
        return ""
    if len(names) <= max_show:
        return ", ".join(names)
    shown = ", ".join(names[:max_show])
    return f"{shown}, +{len(names) - max_show} more"


def pc_slot_note(u32: int, hit: tuple[str, int] | None) -> str:
    if hit:
        return f"{hit[0]} @{hit[1]}"
    if u32 == 0:
        return "u32=0 (empty)"
    return f"u32={u32} invalid"


def pil_save(im: Image.Image, path: Path, scale: int = 1) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if scale > 1:
        w, h = im.size
        im = im.resize((w * scale, h * scale), Image.NEAREST)
    im.save(path)


def pad_rgba(im: Image.Image) -> Image.Image:
    pad = Image.new("RGBA", (im.width + 4, im.height + 4), WIKI_BG)
    pad.paste(im, (2, 2), im)
    return pad


def rgba_list_to_pil(flat: list, w: int, h: int) -> Image.Image:
    return Image.frombytes("RGBA", (w, h), bytes(c for px in flat for c in px))


def export_monster_thumb(
    raw: bytes, slot: int, ext: str, out: Path, cga_palette: int = 1
) -> tuple[str | None, int] | None:
    if Image is None:
        return None
    method, fo = resolve_monster_slot(raw, slot)
    if method is None:
        return None
    frames = _parse_monster_picture(raw, fo, slot, slot, ext)
    if not frames:
        return None
    by_idx = {fr.frame_index: fr for fr in frames}
    rgba = composite_monster_layers(by_idx, [0], cga_palette=cga_palette)
    im = rgba_list_to_pil(rgba, COMBAT_CANVAS_W, COMBAT_CANVAS_H)
    pil_save(flatten_rgba(pad_rgba(im)), out, scale=MONSTER_SCALE)
    return method, fo


def resolve_snes_monster_sprite(picture_id: int) -> Path | None:
    """Picture id ``N`` (1-based) → SNES table index ``N-1`` → ``monster_XX.png``.

    Validated by RGB similarity vs Amiga ``NN.anm`` thumbs (2026-07): ``N-1`` wins
    34/36 sampled slots over direct ``N`` indexing.
    """
    if picture_id < 1:
        return None
    idx = picture_id - 1
    name = f"monster_{idx:02d}.png"
    for base in SNES_MONSTER_SPRITE_DIRS:
        path = base / name
        if path.is_file():
            return path
    return None


def crop_rgba_content(im: Image.Image) -> Image.Image:
    """Trim fully transparent borders."""
    bbox = im.getbbox()
    if not bbox:
        return im
    return im.crop(bbox)


def prepare_snes_monster_thumb(im: Image.Image) -> Image.Image:
    """Tight-crop SNES assembled sprite; scale longest side to combat canvas (×MONSTER_SCALE)."""
    im = crop_rgba_content(im.convert("RGBA"))
    if im.width == 0 or im.height == 0:
        return im
    target = COMBAT_CANVAS_W * MONSTER_SCALE
    scale = target / max(im.width, im.height)
    nw = max(1, int(im.width * scale))
    nh = max(1, int(im.height * scale))
    if nw != im.width or nh != im.height:
        im = im.resize((nw, nh), Image.NEAREST)
    return im


def export_snes_monster_thumb(picture_id: int, out: Path) -> bool:
    src = resolve_snes_monster_sprite(picture_id)
    if src is None or Image is None:
        return False
    im = prepare_snes_monster_thumb(Image.open(src))
    pil_save(flatten_rgba(pad_rgba(im)), out, scale=1)
    return True


def _mac_color_frame0(picture_id: int) -> Path | None:
    """Resolve Color LZpp frame 0 path for picture id."""
    if not MAC_COLOR_FRAMES.is_dir():
        return None
    matches = sorted(MAC_COLOR_FRAMES.glob(f"{picture_id}_*_00.png"))
    if not matches:
        matches = sorted(MAC_COLOR_FRAMES.glob(f"{picture_id}_00.png"))
    return matches[0] if matches else None


def _mac_bw_frame0(picture_id: int) -> Path | None:
    for frames_dir in MAC_BW_PPT_FRAMES:
        if not frames_dir.is_dir():
            continue
        p = frames_dir / f"{picture_id}_00.png"
        if p.is_file():
            return p
    for pack in ("Data", "BW"):
        p = MAC_RSRC / pack / "images" / "ppt" / f"{picture_id}_frame0.png"
        if p.is_file():
            return p
    return None


def export_mac_monster_thumb(src: Path | None, out: Path) -> bool:
    if src is None or not src.is_file() or Image is None:
        return False
    im = Image.open(src).convert("RGBA")
    im = crop_rgba_content(im)
    pil_save(flatten_rgba(pad_rgba(im)), out, scale=MONSTER_SCALE)
    return True


def export_anm_thumb(anm_path: Path, out: Path) -> bool:
    if not anm_path.is_file() or Image is None:
        return False
    asset = load_anm_asset(anm_path)
    comp = composite_anm_frame(asset, 0)
    if comp is None:
        return False
    pil_save(flatten_rgba(pad_rgba(comp.convert("RGBA"))), out, scale=MONSTER_SCALE)
    return True


def pc_wall_frame_count(gog: Path, stem: str, ext: str) -> int:
    path = gog / f"{stem.upper()}{ext}"
    if not path.is_file():
        return 0
    try:
        return len(parse_wall_sheet(path)["frames"])
    except Exception:
        return 0


def amiga_wall_frame_count(data_dir: Path, stem: str) -> int:
    sheet = f"{stem}.32"
    if sheet in NON_GFX_32 or not resolve_asset(data_dir, sheet):
        return 0
    try:
        n = frame_count_32(sheet, data_dir)
        return n if 0 < n <= 256 else 0
    except Exception:
        return 0


def export_pc_wall_thumb(
    gog: Path, stem: str, ext: str, frame_idx: int, out: Path, cga_palette: int = 1
) -> bool:
    path = gog / f"{stem.upper()}{ext}"
    if not path.is_file():
        return False
    try:
        sheet = parse_wall_sheet(path)
        frames = sheet["frames"]
        if frame_idx >= len(frames):
            return False
        fr = frames[frame_idx]
        rgb = decode_wall_frame_rgb(
            fr.width, fr.height, fr.pixels, sheet["bpp"], cga_palette=cga_palette
        )
        write_png_rgb(rgb, fr.width, fr.height, out)
        if Image is not None and WALL_SCALE > 1:
            pil_save(Image.open(out).convert("RGB"), out, scale=WALL_SCALE)
        return True
    except Exception:
        return False


def export_amiga_wall_thumb(data_dir: Path, stem: str, frame_idx: int, out: Path) -> bool:
    sheet = f"{stem}.32"
    if sheet in NON_GFX_32 or not resolve_asset(data_dir, sheet):
        return False
    try:
        im = load_frame(sheet, frame_idx, data_dir)
        pil_save(im.convert("RGBA"), out, scale=WALL_SCALE)
        return True
    except Exception:
        return False


def discover_wall_stems(gog: Path, data_dir: Path) -> list[str]:
    pc = {
        p.stem.lower()
        for p in gog.glob("*.4")
        if p.stem.lower() not in SKIP_PC_STEMS
    }
    amiga = {
        p.stem.lower()
        for p in data_dir.glob("*.32")
        if p.name not in NON_GFX_32
    }
    stems = sorted((pc | amiga) - SKIP_WALL_STEMS)
    ordered = [s for s in WALL_STEM_ORDER if s in stems]
    ordered += [s for s in stems if s not in ordered]
    return ordered


def export_format_sample(
    label: str,
    out: Path,
    *,
    gog: Path,
    data_dir: Path,
    cga_palette: int,
) -> dict[str, str]:
    meta: dict[str, str] = {"label": label, "status": "missing"}
    out.parent.mkdir(parents=True, exist_ok=True)

    if label.endswith(".4"):
        src = gog / "TOWN.4"
        if src.is_file():
            sheet = parse_wall_sheet(src)
            fr = sheet["frames"][0]
            rgb = decode_wall_frame_rgb(fr.width, fr.height, fr.pixels, 2, cga_palette)
            write_png_rgb(rgb, fr.width, fr.height, out)
            meta = {
                "label": label,
                "status": "ok",
                "source": str(src),
                "frame": f"0 / {len(sheet['frames'])}",
                "size": f"{fr.width}×{fr.height}",
            }
    elif label.endswith(".16"):
        src = gog / "TOWN.16"
        if src.is_file():
            sheet = parse_wall_sheet(src)
            fr = sheet["frames"][0]
            rgb = decode_wall_frame_rgb(fr.width, fr.height, fr.pixels, 4, cga_palette)
            write_png_rgb(rgb, fr.width, fr.height, out)
            meta = {
                "label": label,
                "status": "ok",
                "source": str(src),
                "frame": f"0 / {len(sheet['frames'])}",
                "size": f"{fr.width}×{fr.height}",
            }
    elif label.endswith(".32"):
        src_name = "town.32"
        src = data_dir / src_name
        if src.is_file() and Image is not None:
            try:
                im = load_frame(src_name, 0, data_dir)
                pil_save(im, out, scale=1)
                meta = {
                    "label": label,
                    "status": "ok",
                    "source": str(src),
                    "frame": "0",
                    "size": f"{im.size[0]}×{im.size[1]}",
                }
            except Exception as exc:
                meta = {"label": label, "status": f"error: {exc}", "source": str(src)}
    return meta


def build_html(
    monster_rows: list[dict],
    wall_sections: list[dict],
    format_samples: list[dict],
    out_html: Path,
) -> None:

    def cell_img(path: Path | None, alt: str, *, wall: bool = False) -> str:
        if path and path.is_file():
            rel_path = path.relative_to(out_html.parent).as_posix()
            cls = "wall-thumb" if wall else ""
            return (
                f'<a href="{html.escape(rel_path)}" target="_blank" class="{cls}">'
                f'<img src="{html.escape(rel_path)}" alt="{html.escape(alt)}" loading="lazy"></a>'
            )
        return '<span class="empty">—</span>'

    fmt_block = ""
    for fs in format_samples:
        img_path = Path(fs.get("img", ""))
        fmt_block += f"""
        <div class="format-card">
          <h3>{html.escape(fs.get("label", "?"))}</h3>
          {cell_img(img_path if img_path.is_file() else None, fs.get("label", ""))}
          <dl>
            <dt>Source</dt><dd><code>{html.escape(fs.get("source", ""))}</code></dd>
            <dt>Frame</dt><dd>{html.escape(fs.get("frame", ""))}</dd>
            <dt>Size</dt><dd>{html.escape(fs.get("size", ""))}</dd>
            <dt>Status</dt><dd>{html.escape(fs.get("status", ""))}</dd>
          </dl>
        </div>"""

    monster_body = ""
    for r in monster_rows:
        row_class = "warn" if r.get("warn") else ""
        monster_body += f"""
        <tr class="{row_class}">
          <td class="slot">{r["slot"]:02d}</td>
          <td class="monsters">{html.escape(r["monsters"])}</td>
          <td class="meta"><code>u32={r["u32_cga"]}</code><br><code>u32={r["u32_ega"]}</code></td>
          <td class="meta">{html.escape(r["cga_note"])}</td>
          <td class="thumb">{cell_img(r.get("cga_img"), f"CGA {r['slot']:02d}")}</td>
          <td class="meta">{html.escape(r["ega_note"])}</td>
          <td class="thumb">{cell_img(r.get("ega_img"), f"EGA {r['slot']:02d}")}</td>
          <td class="thumb">{cell_img(r.get("anm_img"), f"ANM {r['slot']:02d}")}</td>
          <td class="thumb">{cell_img(r.get("snes_img"), f"SNES {r['slot']:02d}")}</td>
          <td class="thumb">{cell_img(r.get("mac_color_img"), f"Mac Color {r['slot']:02d}")}</td>
          <td class="thumb">{cell_img(r.get("mac_bw_img"), f"Mac BW {r['slot']:02d}")}</td>
          <td class="meta">{html.escape(r["anm_note"])}<br>{html.escape(r.get("snes_note", ""))}<br>MacC {html.escape(r.get("mac_color_note", ""))} · MacBW {html.escape(r.get("mac_bw_note", ""))}</td>
        </tr>"""

    nav_links = '<a href="#monsters">Monsters 01–74</a>'
    wall_html = ""
    for sec in wall_sections:
        stem = sec["stem"]
        anchor = f"wall-{stem}"
        nav_links += f' <a href="#{anchor}">{html.escape(stem.upper())}</a>'
        rows_html = ""
        for r in sec["rows"]:
            rows_html += f"""
        <tr>
          <td class="slot">{r["frame"]:02d}</td>
          <td class="thumb">{cell_img(r.get("cga_img"), f"{stem} CGA {r['frame']:02d}", wall=True)}</td>
          <td class="thumb">{cell_img(r.get("ega_img"), f"{stem} EGA {r['frame']:02d}", wall=True)}</td>
          <td class="thumb">{cell_img(r.get("amiga_img"), f"{stem} Amiga {r['frame']:02d}", wall=True)}</td>
        </tr>"""
        wall_html += f"""
  <section class="wall" id="{anchor}">
    <h2>{html.escape(stem.upper())}</h2>
    <p class="sheet-meta">
      <code>{stem}.32</code> ({sec["amiga_n"]} frames) ·
      <code>{stem.upper()}.4</code> ({sec["cga_n"]}) ·
      <code>{stem.upper()}.16</code> ({sec["ega_n"]})
    </p>
    <table>
      <thead>
        <tr>
          <th>Frame</th>
          <th>CGA</th>
          <th>EGA</th>
          <th>Amiga</th>
        </tr>
      </thead>
      <tbody>{rows_html}
      </tbody>
    </table>
  </section>"""

    doc = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>MM2 platform graphics index</title>
  <style>
    :root {{ font-family: system-ui, sans-serif; background: #1a1a22; color: #e8e8ec; }}
    body {{ margin: 0 0 3rem; }}
    header {{ padding: 1rem 1.5rem 0.5rem; max-width: 1600px; }}
    h1 {{ font-size: 1.35rem; font-weight: 600; margin: 0 0 0.5rem; }}
    p {{ color: #aaa; line-height: 1.45; max-width: 90ch; }}
    nav.toc {{
      position: sticky; top: 0; z-index: 2;
      background: #1a1a22ee; backdrop-filter: blur(6px);
      border-bottom: 1px solid #444;
      padding: 0.5rem 1.5rem;
      display: flex; flex-wrap: wrap; gap: 0.35rem 0.75rem;
      font-size: 0.78rem;
    }}
    nav.toc a {{ color: #8cf; text-decoration: none; }}
    nav.toc a:hover {{ text-decoration: underline; }}
    main {{ padding: 0 1.5rem; max-width: 1600px; }}
    .formats {{ display: flex; flex-wrap: wrap; gap: 1rem; margin: 1rem 0 1.5rem; }}
    .format-card {{ background: #252530; border: 1px solid #444; border-radius: 6px; padding: 0.75rem; max-width: 320px; }}
    .format-card h3 {{ margin: 0 0 0.5rem; font-size: 1rem; }}
    .format-card img {{ max-width: 280px; max-height: 200px; image-rendering: pixelated; background: #111; }}
    .format-card dl {{ margin: 0.5rem 0 0; font-size: 0.8rem; }}
    .format-card dt {{ color: #888; display: inline; }}
    .format-card dd {{ display: inline; margin: 0 0 0 0.25rem; }}
    .format-card dd::after {{ content: ""; display: block; margin-bottom: 0.25rem; }}
    section {{ margin-top: 2rem; }}
    section.wall h2 {{ font-size: 1.1rem; margin: 0 0 0.25rem; }}
    .sheet-meta {{ font-size: 0.82rem; margin: 0 0 0.75rem; color: #999; }}
    table {{ border-collapse: collapse; width: 100%; font-size: 0.85rem; margin-bottom: 1rem; }}
    th, td {{ border: 1px solid #444; padding: 0.35rem 0.5rem; vertical-align: middle; }}
    th {{ background: #2a2a36; position: sticky; top: 2.2rem; z-index: 1; }}
    tr:nth-child(even) {{ background: #22222c; }}
    .slot {{ font-weight: 700; text-align: center; width: 3rem; }}
    .thumb img {{ max-height: 96px; max-width: 120px; image-rendering: pixelated; background: #111; display: block; }}
    .wall .thumb img {{ max-height: 160px; max-width: 240px; }}
    .meta {{ font-size: 0.75rem; color: #bbb; min-width: 6rem; }}
    .monsters {{ font-size: 0.78rem; max-width: 14rem; line-height: 1.35; }}
    tr.warn {{ background: #3a2a1a !important; }}
    tr.warn .monsters {{ color: #f0c080; }}
    .empty {{ color: #666; }}
    code {{ font-size: 0.72rem; word-break: break-all; }}
  </style>
</head>
<body>
  <header>
    <h1>MM2 platform graphics index</h1>
    <p>
      Numbered slots side-by-side: <strong>Amiga</strong> (planar <code>.32</code> / combat <code>.anm</code>)
      vs <strong>PC CGA</strong> (<code>.4</code>) vs <strong>PC EGA</strong> (<code>.16</code>)
      vs <strong>SNES</strong> (table <code>$14:8060</code>)
      vs <strong>Mac Color</strong> (LZpp) vs <strong>Mac B&amp;W</strong> (¶PPT / <code>%B6PPT</code>).
      Monster picture id <strong>N</strong> uses header entry <code>N−1</code> in <code>MONSTERS.*</code>
      and SNES sprite <code>monster_&#123;N−1&#125;.png</code>.
      Wall sheets share the same stem (<code>TOWN</code> ↔ <code>town.32</code>); frame counts may differ by port.
      Mac thumbs are frame&nbsp;0; full cel strips live under
      <code>EXTRACTED/MAC/.../atlases/{{color,bw}}_monsters/</code>.
      BW view-env CBMP pieces are separate (<code>BW_ART/view_env/</code>).
    </p>
  </header>
  <nav class="toc">{nav_links}</nav>
  <main>
    <section class="formats">
      <h2 style="width:100%;margin:0;font-size:1.1rem;">Format samples</h2>
      {fmt_block}
    </section>
    <section id="monsters">
      <h2>Combat monsters — picture ids 01–74</h2>
      <p>
        Row <strong>N</strong> = <code>monsters.dat</code> picture byte <code>&amp; 0x7F</code>.
        Orange: named monster uses this id but no PC thumb decodes.
        <code>NN.anm</code> is Amiga combat art (same index when ported).
        SNES CHR is assembled from table <code>$14:8060</code> (index <code>N−1</code>).
        Mac Color = LZpp frame&nbsp;0; Mac B&amp;W = Data/BW ¶PPT frame&nbsp;0 (Sqnc/FRMS ground truth for cel counts).
      </p>
      <table>
        <thead>
          <tr>
            <th>Pic<br>id</th>
            <th>monsters.dat</th>
            <th>Header u32<br><small>CGA / EGA</small></th>
            <th>MONSTERS.4</th>
            <th>thumb</th>
            <th>MONSTERS.16</th>
            <th>thumb</th>
            <th>Amiga .anm</th>
            <th>SNES</th>
            <th>Mac Color</th>
            <th>Mac BW</th>
            <th>notes</th>
          </tr>
        </thead>
        <tbody>
          {monster_body}
        </tbody>
      </table>
    </section>
    {wall_html}
  </main>
</body>
</html>
"""
    out_html.write_text(doc, encoding="utf-8")


def build_markdown(
    out_md: Path,
    *,
    html_path: str = "/gallery/platform-compare/index.html",
    embed_iframe: bool = True,
) -> None:
    bust = f"?v={COMPARE_CACHE_VERSION}"
    lines = [
        "---",
        "title: Platform Graphics Index",
        "---",
        "",
        "# Platform graphics index",
        "",
        "Every **numbered slot** for combat monsters and wall/sprite sheets —",
        "**Amiga** (`.32` / `.anm`) vs **PC CGA** (`.4`) vs **PC EGA** (`.16`) vs **SNES**",
        "vs **Mac Color** (LZpp) vs **Mac B&W** (¶PPT).",
        "",
    ]
    if embed_iframe:
        lines += [
            "Format reference: [PC DOS graphics formats](/docs/reverse-engineering/54-pc-dos-graphics-formats)",
            " · [ANM/TV format](/docs/reverse-engineering/07-anm-tv-format)",
            " · [Monster variants by name](/docs/gallery/monster-variants).",
            "",
            "Mac thumbs are **frame 0**; full cel atlases:",
            "`EXTRACTED/MAC/Might_and_Magic_II_0/extracted_rsrc/atlases/color_monsters/` and",
            "`…/bw_monsters/`. BW view-env CBMP: `…/BW_ART/view_env/`.",
            "",
            f'<iframe src="{html_path}{bust}" class="platform-compare-frame" title="Platform graphics compare"></iframe>',
            "",
        ]
    else:
        lines += [
            "Sticky-nav HTML table: monsters **01–74** plus every wall sheet frame",
            "(`TOWN`, `CASTLE`, `SKY`, `THROW`, outdoor biomes, …).",
            "",
            f"**[Open compare page →]({html_path})**",
            "",
            "GitHub Wiki cannot embed the interactive table — use the link above "
            "(downloads/opens `index.html` from the wiki repo).",
            "",
        ]
    lines += [
        f'[Open full page ↗]({html_path}{bust})',
        "",
    ]
    out_md.parent.mkdir(parents=True, exist_ok=True)
    out_md.write_text("\n".join(lines), encoding="utf-8")


def build_github_wiki_pages(
    wiki_dir: Path,
    monster_rows: list[dict],
    wall_sections: list[dict],
    assets_dir: Path,
    *,
    image_prefix: str = "images/gallery/platform-compare/assets",
) -> list[str]:
    """GitHub Wiki markdown with embedded PNGs (no raw HTML links)."""
    prefix = image_prefix.rstrip("/")
    bust = f"?v={COMPARE_CACHE_VERSION}"

    def img_cell(path: Path | None) -> str:
        if path and path.is_file():
            rel = path.relative_to(assets_dir).as_posix()
            return f"![]({prefix}/{rel}{bust})"
        return "—"

    pages: list[str] = []

    hub: list[str] = [
        "# Platform graphics index",
        "",
        "Numbered slots side-by-side: **Amiga** (`.32` / `.anm`) vs **PC CGA** (`.4`) vs **PC EGA** (`.16`) vs **SNES** vs **Mac Color** (LZpp) vs **Mac B&W** (¶PPT).",
        "",
        "Format: [PC DOS graphics](PC-DOS-Graphics-Formats) · [ANM/TV](ANM-TV-Format) · "
        "[SNES graphics](SNES-Graphics) · [Monster variants by name](Monster-Variants).",
        "",
        "## Combat monsters",
        "",
        "[Picture ids 01–74](Platform-Monsters-01-74) — CGA / EGA / `.anm` / SNES / Mac Color / Mac BW thumbs.",
        "",
        "## Wall / sprite sheets",
        "",
    ]
    for sec in wall_sections:
        stem = sec["stem"]
        page = f"Platform-Walls-{stem.upper()}"
        pages.append(page)
        hub.append(
            f"- [{stem.upper()}]({page}) — "
            f"{sec['amiga_n']} Amiga · {sec['cga_n']} CGA · {sec['ega_n']} EGA frames"
        )
    wiki_dir.mkdir(parents=True, exist_ok=True)
    (wiki_dir / "Platform-Graphics-Index.md").write_text("\n".join(hub) + "\n", encoding="utf-8")

    mon = [
        "# Combat monsters — picture ids 01–74",
        "",
        "[← Platform index](Platform-Graphics-Index)",
        "",
        "Row **N** = `monsters.dat` picture byte `& 0x7F`. Header entry **`N−1`** in `MONSTERS.*`.",
        "SNES table `$14:8060` index **`N−1`** → `monster_XX.png` (validated vs Amiga thumbs).",
        "Mac Color = LZpp frame 0; Mac B&W = ¶PPT frame 0 (Data/BW).",
        "",
        "| Pic | monsters.dat | CGA | EGA | Amiga | SNES | Mac Color | Mac BW |",
        "|----:|--------------|-----|-----|-------|------|-----------|--------|",
    ]
    for r in monster_rows:
        names = r["monsters"].replace("|", "\\|")
        mon.append(
            f"| {r['slot']:02d} | {names} | "
            f"{img_cell(r.get('cga_img'))} | {img_cell(r.get('ega_img'))} | "
            f"{img_cell(r.get('anm_img'))} | {img_cell(r.get('snes_img'))} | "
            f"{img_cell(r.get('mac_color_img'))} | {img_cell(r.get('mac_bw_img'))} |"
        )
    (wiki_dir / "Platform-Monsters-01-74.md").write_text("\n".join(mon) + "\n", encoding="utf-8")
    pages.append("Platform-Monsters-01-74")

    for sec in wall_sections:
        stem = sec["stem"]
        page = f"Platform-Walls-{stem.upper()}"
        lines = [
            f"# {stem.upper()} — `{stem}.32` / `{stem.upper()}.4` / `{stem.upper()}.16`",
            "",
            "[← Platform index](Platform-Graphics-Index)",
            "",
            f"**{sec['amiga_n']}** Amiga · **{sec['cga_n']}** CGA · **{sec['ega_n']}** EGA frames.",
            "",
            "| Frame | CGA | EGA | Amiga |",
            "|------:|-----|-----|-------|",
        ]
        for r in sec["rows"]:
            lines.append(
                f"| {r['frame']:02d} | {img_cell(r.get('cga_img'))} | "
                f"{img_cell(r.get('ega_img'))} | {img_cell(r.get('amiga_img'))} |"
            )
        (wiki_dir / f"{page}.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    return ["Platform-Graphics-Index", *pages]


def export_monster_rows(
    *,
    cga_raw: bytes,
    ega_raw: bytes,
    by_picture: dict[int, list[str]],
    assets: Path,
    data_dir: Path,
    cga_palette: int,
) -> list[dict]:
    rows: list[dict] = []
    for slot in range(1, 75):
        names = by_picture.get(slot, [])
        row: dict = {
            "slot": slot,
            "monsters": format_monster_list(names) if names else "—",
            "u32_cga": header_u32(cga_raw, slot),
            "u32_ega": header_u32(ega_raw, slot),
            "cga_note": "",
            "ega_note": "",
            "anm_note": "",
            "snes_note": "",
            "mac_color_note": "",
            "mac_bw_note": "",
        }

        cga_img = assets / "monsters" / "cga" / f"{slot:02d}.png"
        cga_hit = export_monster_thumb(cga_raw, slot, ".4", cga_img, cga_palette)
        if cga_hit:
            row["cga_img"] = cga_img
        row["cga_note"] = pc_slot_note(row["u32_cga"], cga_hit)

        ega_img = assets / "monsters" / "ega" / f"{slot:02d}.png"
        ega_hit = export_monster_thumb(ega_raw, slot, ".16", ega_img, cga_palette)
        if ega_hit:
            row["ega_img"] = ega_img
        row["ega_note"] = pc_slot_note(row["u32_ega"], ega_hit)

        if names and not cga_hit and not ega_hit:
            row["warn"] = True

        anm_path = data_dir / f"{slot:02d}.anm"
        anm_img = assets / "monsters" / "anm" / f"{slot:02d}.png"
        if anm_path.is_file():
            if export_anm_thumb(anm_path, anm_img):
                row["anm_img"] = anm_img
                row["anm_note"] = anm_path.name
            else:
                row["anm_note"] = f"{anm_path.name} decode fail"
        else:
            row["anm_note"] = "no file"

        snes_img = assets / "monsters" / "snes" / f"{slot:02d}.png"
        if export_snes_monster_thumb(slot, snes_img):
            row["snes_img"] = snes_img
            row["snes_note"] = f"idx {slot - 1:02d}"
        else:
            row["snes_note"] = "no sprite"

        mac_c_img = assets / "monsters" / "mac_color" / f"{slot:02d}.png"
        mac_c_src = _mac_color_frame0(slot)
        if export_mac_monster_thumb(mac_c_src, mac_c_img):
            row["mac_color_img"] = mac_c_img
            row["mac_color_note"] = "LZpp"
        else:
            row["mac_color_note"] = "—"

        mac_bw_img = assets / "monsters" / "mac_bw" / f"{slot:02d}.png"
        mac_bw_src = _mac_bw_frame0(slot)
        if export_mac_monster_thumb(mac_bw_src, mac_bw_img):
            row["mac_bw_img"] = mac_bw_img
            row["mac_bw_note"] = "¶PPT"
        else:
            row["mac_bw_note"] = "—"

        rows.append(row)
    return rows


def export_wall_sections(
    *,
    gog: Path,
    data_dir: Path,
    assets: Path,
    cga_palette: int,
) -> list[dict]:
    sections: list[dict] = []
    for stem in discover_wall_stems(gog, data_dir):
        cga_n = pc_wall_frame_count(gog, stem, ".4")
        ega_n = pc_wall_frame_count(gog, stem, ".16")
        amiga_n = amiga_wall_frame_count(data_dir, stem)
        if cga_n == 0 and ega_n == 0 and amiga_n == 0:
            continue

        frame_rows: list[dict] = []
        max_frames = max(cga_n, ega_n, amiga_n)
        for fi in range(max_frames):
            row: dict = {"frame": fi}
            cga_out = assets / "walls" / stem / "cga" / f"{fi:02d}.png"
            if export_pc_wall_thumb(gog, stem, ".4", fi, cga_out, cga_palette):
                row["cga_img"] = cga_out
            ega_out = assets / "walls" / stem / "ega" / f"{fi:02d}.png"
            if export_pc_wall_thumb(gog, stem, ".16", fi, ega_out, cga_palette):
                row["ega_img"] = ega_out
            amiga_out = assets / "walls" / stem / "amiga" / f"{fi:02d}.png"
            if export_amiga_wall_thumb(data_dir, stem, fi, amiga_out):
                row["amiga_img"] = amiga_out
            frame_rows.append(row)

        sections.append(
            {
                "stem": stem,
                "cga_n": cga_n,
                "ega_n": ega_n,
                "amiga_n": amiga_n,
                "rows": frame_rows,
            }
        )
        print(f"  wall {stem}: {max_frames} rows (cga={cga_n} ega={ega_n} amiga={amiga_n})")
    return sections


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Generate MM2 platform graphics compare HTML")
    ap.add_argument("--gog", type=Path, default=DEFAULT_GOG, help="GOG MM2 directory")
    ap.add_argument("--data", type=Path, default=ROOT, help="Amiga .anm / .32 directory")
    ap.add_argument("-o", "--output", type=Path, default=DEFAULT_OUT)
    ap.add_argument(
        "--markdown",
        type=Path,
        default=None,
        help="Also write VitePress markdown with embedded iframe",
    )
    ap.add_argument(
        "--html-prefix",
        default="/gallery/platform-compare/index.html",
        help="iframe src for --markdown",
    )
    ap.add_argument(
        "--github-wiki-dir",
        type=Path,
        default=None,
        help="Write GitHub Wiki markdown pages (embedded images, no HTML links)",
    )
    ap.add_argument("--cga-palette", type=int, choices=(0, 1), default=1)
    ap.add_argument("--skip-walls", action="store_true")
    args = ap.parse_args(argv)

    out = args.output
    assets = out / "assets"
    gog = args.gog
    cga_path = gog / "MONSTERS.4"
    ega_path = gog / "MONSTERS.16"
    if not cga_path.is_file() or not ega_path.is_file():
        print(f"Need MONSTERS.4 and MONSTERS.16 under {gog}", file=sys.stderr)
        return 1
    if Image is None:
        print("Warning: pip install pillow for .anm / .32 thumbs", file=sys.stderr)

    cga_raw = cga_path.read_bytes()
    ega_raw = ega_path.read_bytes()
    monsters_path = args.data / "monsters.dat"
    if not monsters_path.is_file():
        monsters_path = MONSTERS_DAT
    by_picture = load_monsters_by_picture(monsters_path)

    format_samples: list[dict] = []
    for fmt in (".4", ".16", ".32"):
        img = assets / "format" / f"sample{fmt.replace('.', '_')}.png"
        meta = export_format_sample(fmt, img, gog=gog, data_dir=args.data, cga_palette=args.cga_palette)
        meta["img"] = str(img)
        format_samples.append(meta)

    print("Exporting combat monsters 01–74…")
    monster_rows = export_monster_rows(
        cga_raw=cga_raw,
        ega_raw=ega_raw,
        by_picture=by_picture,
        assets=assets,
        data_dir=args.data,
        cga_palette=args.cga_palette,
    )

    wall_sections: list[dict] = []
    if not args.skip_walls:
        print("Exporting wall / sprite sheets…")
        wall_sections = export_wall_sections(
            gog=gog,
            data_dir=args.data,
            assets=assets,
            cga_palette=args.cga_palette,
        )

    html_path = out / "index.html"
    build_html(monster_rows, wall_sections, format_samples, html_path)
    if args.markdown:
        build_markdown(args.markdown, html_path=args.html_prefix, embed_iframe=True)
    if args.github_wiki_dir:
        pages = build_github_wiki_pages(
            args.github_wiki_dir,
            monster_rows,
            wall_sections,
            assets,
        )
        print(f"Wrote {len(pages)} GitHub Wiki pages -> {args.github_wiki_dir}")

    cga_n = sum(1 for r in monster_rows if r.get("cga_img"))
    ega_n = sum(1 for r in monster_rows if r.get("ega_img"))
    anm_n = sum(1 for r in monster_rows if r.get("anm_img"))
    snes_n = sum(1 for r in monster_rows if r.get("snes_img"))
    mac_c_n = sum(1 for r in monster_rows if r.get("mac_color_img"))
    mac_bw_n = sum(1 for r in monster_rows if r.get("mac_bw_img"))
    wall_frames = sum(len(s["rows"]) for s in wall_sections)
    print(f"Wrote {html_path}")
    print(
        f"  monsters: CGA {cga_n}/74 · EGA {ega_n}/74 · .anm {anm_n}/74 · "
        f"SNES {snes_n}/74 · MacC {mac_c_n}/74 · MacBW {mac_bw_n}/74"
    )
    print(f"  wall sheets: {len(wall_sections)} stems · {wall_frames} frame rows")
    if args.markdown:
        print(f"Wrote {args.markdown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

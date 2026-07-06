#!/usr/bin/env python3
"""Export per-monster combat sprite GIFs: Amiga .anm, PC CGA, PC EGA.

Reads ``monsters.dat`` (256×26). For each named monster with a picture id,
writes ``<Name>_amiga.gif``, ``<Name>_cga.gif``, ``<Name>_ega.gif`` when the
source resolves.

Usage:
  python tools/export_monster_variants.py
  python tools/export_monster_variants.py -o EXTRACTED/monster_variants
"""
from __future__ import annotations

import argparse
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GOG = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")
DEFAULT_OUT = ROOT / "EXTRACTED" / "monster_variants"
VARIANTS_CACHE_VERSION = 4

sys.path.insert(0, str(ROOT / "tools"))

from decode_monsters import RECORD_COUNT, RECORD_SIZE, decode_record  # noqa: E402
from decode_pc_gfx import (  # noqa: E402
    COMBAT_CANVAS_H,
    COMBAT_CANVAS_W,
    _pick_primary_script,
    composite_pc_combat_frame,
    parse_monster_picture_blob,
    render_monster_frame_rgba,
)
from gen_gfx_compare_html import resolve_monster_slot  # noqa: E402
from mm2_gfx_export import (  # noqa: E402
    ANM_GIF_DURATION_MS,
    composite_anm_frame,
    contact_sheet,
    flatten_rgba,
    load_anm_asset,
    save_anm_gif,
    save_png as save_pil_png,
    scale_nearest,
)

try:
    from PIL import Image
except ImportError:
    Image = None  # type: ignore

THUMB_SCALE = 3
SHEET_COLS = 8
WIKI_BG = (13, 3, 3, 255)


def sanitize_name(name: str) -> str:
    s = name.strip()
    s = re.sub(r"[^\w\s-]", "", s, flags=re.UNICODE)
    s = re.sub(r"\s+", "_", s)
    return s or "unnamed"


def resolve_pc_picture(raw: bytes, picture_id: int, max_id: int = 75) -> tuple[int, int] | None:
    """Game-accurate resolve: picture id ``N`` -> ``(used_id, file_off)``.

    Entry ``N-1`` holds the blob offset; if it is ``0`` (blob absent from this
    PC file, e.g. Merchant = pic 34), the DOS build advances to the next
    non-empty picture (confirmed in-game: Merchant renders picture 35). Returns
    ``None`` if nothing resolves before ``max_id``.
    """
    pid = picture_id
    while pid <= max_id:
        method, file_off = resolve_monster_slot(raw, pid)
        if method is not None:
            return pid, file_off
        pid += 1
    return None


def rgba_flat_to_pil(flat: list[tuple[int, int, int, int]], w: int, h: int) -> "Image.Image":
    assert Image is not None
    return Image.frombytes("RGBA", (w, h), bytes(c for px in flat for c in px))


def pad_thumb(im: "Image.Image", scale: int = THUMB_SCALE) -> "Image.Image":
    """Padded RGBA thumb on wiki background (matches monster-sprites gallery)."""
    assert Image is not None
    pad = Image.new("RGBA", (im.width + 4, im.height + 4), WIKI_BG)
    pad.paste(im, (2, 2), im)
    return scale_nearest(pad, scale)


def _save_rgb_gif(frames: list["Image.Image"], path: Path, *, duration_ms: int = ANM_GIF_DURATION_MS) -> bool:
    """Write GIF from RGB frames (disposal=2, same as Amiga combat export)."""
    if not frames:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    dur = max(40, min(duration_ms, 2000))
    if len(frames) == 1:
        frames[0].save(path, optimize=True)
        return True
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=dur,
        loop=0,
        disposal=2,
        optimize=True,
    )
    return True


def _pc_composite_rgb(
    flat: list[tuple[int, int, int, int]], w: int, h: int, *, scale: int = THUMB_SCALE
) -> "Image.Image":
    """96×96 PC combat buffer → padded, upscaled RGB (matches ``save_anm_gif``)."""
    return flatten_rgba(pad_thumb(rgba_flat_to_pil(flat, w, h), scale=scale))


def export_amiga_assets(anm_path: Path, out_dir: Path, slug: str) -> bool:
    """Write ``{slug}_amiga.gif``, ``.png`` thumb, ``-sheet`` + ``-raw-sheet``."""
    if not anm_path.is_file() or Image is None:
        return False
    asset = load_anm_asset(anm_path)
    gif_path = out_dir / f"{slug}_amiga.gif"
    png_path = out_dir / f"{slug}_amiga.png"
    sheet_path = out_dir / f"{slug}_amiga-sheet.png"
    raw_path = out_dir / f"{slug}_amiga-raw-sheet.png"

    if not save_anm_gif(asset, gif_path, scale=THUMB_SCALE):
        return False

    comp0 = composite_anm_frame(asset, 0)
    if comp0:
        save_pil_png(pad_thumb(comp0.convert("RGBA")), png_path)

    composed = [composite_anm_frame(asset, i) for i in range(asset.frame_count)]
    composed = [c.convert("RGBA") for c in composed if c]
    if composed:
        save_pil_png(contact_sheet(composed, cols=SHEET_COLS, pad=4), sheet_path)

    if asset.frames:
        save_pil_png(
            contact_sheet(asset.frames, cols=SHEET_COLS, pad=4, labels=[f"f{i:02d}" for i in range(len(asset.frames))], label_h=10),
            raw_path,
        )
    return gif_path.is_file()


def export_pc_assets(
    raw: bytes,
    picture_id: int,
    ext: str,
    out_dir: Path,
    slug: str,
    *,
    cga_palette: int = 1,
) -> int | None:
    """Write ``{slug}_cga|ega`` gif/png/sheets; return picture id used (fallback)."""
    resolved = resolve_pc_picture(raw, picture_id)
    if resolved is None:
        return None
    used_id, file_off = resolved
    blob = parse_monster_picture_blob(raw, file_off, used_id, ext)
    if blob is None or not blob.frames:
        return None

    tag = "cga" if ext == ".4" else "ega"
    gif_path = out_dir / f"{slug}_{tag}.gif"
    png_path = out_dir / f"{slug}_{tag}.png"
    sheet_path = out_dir / f"{slug}_{tag}-sheet.png"
    raw_path = out_dir / f"{slug}_{tag}-raw-sheet.png"

    by_idx = {fr.frame_index: fr for fr in blob.frames}
    primary_si = _pick_primary_script(blob.scripts)

    # Animated GIF (primary script or base) — same 3× pad/scale as Amiga .anm export
    rgb_frames: list[Image.Image] = []
    if primary_si is not None and primary_si < len(blob.scripts):
        script = blob.scripts[primary_si]
        for _step, (fidx, _delay) in enumerate(script):
            rgba = composite_pc_combat_frame(by_idx, fidx, cga_palette=cga_palette)
            rgb_frames.append(_pc_composite_rgb(rgba, COMBAT_CANVAS_W, COMBAT_CANVAS_H))
    else:
        rgba = composite_pc_combat_frame(by_idx, 0, cga_palette=cga_palette)
        rgb_frames.append(_pc_composite_rgb(rgba, COMBAT_CANVAS_W, COMBAT_CANVAS_H))
    if rgb_frames:
        _save_rgb_gif(rgb_frames, gif_path, duration_ms=150)

    if Image is None:
        return used_id if gif_path.is_file() else None

    # Composed frame 0 thumb
    base_rgba = composite_pc_combat_frame(by_idx, 0, cga_palette=cga_palette)
    save_pil_png(
        pad_thumb(rgba_flat_to_pil(base_rgba, COMBAT_CANVAS_W, COMBAT_CANVAS_H)),
        png_path,
    )

    # Composed states c0..cN
    composed_imgs: list[Image.Image] = []
    for fidx in sorted(by_idx):
        rgba = composite_pc_combat_frame(by_idx, fidx, cga_palette=cga_palette)
        composed_imgs.append(rgba_flat_to_pil(rgba, COMBAT_CANVAS_W, COMBAT_CANVAS_H))
    if composed_imgs:
        save_pil_png(
            contact_sheet(
                composed_imgs,
                cols=SHEET_COLS,
                pad=2,
                labels=[f"c{fidx:02d}" for fidx in sorted(by_idx)],
                label_h=10,
            ),
            sheet_path,
        )

    # Raw stored patches (frame bitmaps as in blob)
    patch_imgs: list[Image.Image] = []
    patch_labels: list[str] = []
    for fr in sorted(blob.frames, key=lambda f: f.frame_index):
        flat = render_monster_frame_rgba(fr, cga_palette=cga_palette)
        patch_imgs.append(rgba_flat_to_pil(flat, fr.width, fr.height))
        patch_labels.append(f"p{fr.frame_index:02d}")
    if patch_imgs:
        save_pil_png(
            contact_sheet(patch_imgs, cols=SHEET_COLS, pad=4, labels=patch_labels, label_h=10),
            raw_path,
        )

    return used_id if gif_path.is_file() else None


def _attach_variant_files(row: dict, variant: str, slug: str, out_dir: Path) -> None:
    gif = out_dir / f"{slug}_{variant}.gif"
    if gif.is_file():
        row[variant] = gif
    png = out_dir / f"{slug}_{variant}.png"
    if png.is_file():
        row[f"{variant}_png"] = png
    sheet = out_dir / f"{slug}_{variant}-sheet.png"
    if sheet.is_file():
        row[f"{variant}_sheet"] = sheet
    raw = out_dir / f"{slug}_{variant}-raw-sheet.png"
    if raw.is_file():
        row[f"{variant}_raw"] = raw


def _variant_cell_html(r: dict, variant: str, out_html: Path) -> str:
    path = r.get(variant)
    parts: list[str] = []
    if path and path.is_file():
        rel = html.escape(path.relative_to(out_html.parent).as_posix())
        parts.append(
            f'<img src="{rel}" alt="{html.escape(r["name"])} {variant}" loading="lazy">'
        )
    subst = r.get("subst", {}).get(variant)
    if subst:
        parts.append(f'<div class="subst">→ pic {subst:02d}</div>')
    return "".join(parts) if parts else '<span class="missing">—</span>'


def build_html(rows: list[dict], out_html: Path, stats: dict[str, int]) -> None:
    body = ""
    for r in rows:
        miss_pc = not r.get("cga") or not r.get("ega")
        row_class = "warn" if miss_pc and r.get("amiga") else ""
        body += f"""
        <tr class="{row_class}">
          <td class="idx">{r['idx']:3d}</td>
          <td class="name">{html.escape(r['name'])}</td>
          <td class="pic">{r['picture']:02d}</td>
          <td class="plat">{_variant_cell_html(r, 'amiga', out_html)}</td>
          <td class="plat">{_variant_cell_html(r, 'cga', out_html)}</td>
          <td class="plat">{_variant_cell_html(r, 'ega', out_html)}</td>
        </tr>"""

    doc = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>MM2 monster variants — Amiga / CGA / EGA</title>
  <style>
    :root {{ font-family: system-ui, sans-serif; background: #1a1a22; color: #e8e8ec; }}
    body {{ margin: 1rem 1.5rem 3rem; }}
    h1 {{ font-size: 1.35rem; }}
    p {{ color: #aaa; max-width: 90ch; line-height: 1.45; }}
    p a {{ color: #8cf; }}
    table {{ border-collapse: collapse; width: 100%; font-size: 0.85rem; }}
    th, td {{ border: 1px solid #444; padding: 0.35rem 0.5rem; vertical-align: middle; }}
    th {{ background: #2a2a36; position: sticky; top: 0; z-index: 1; }}
    tr:nth-child(even) {{ background: #22222c; }}
    tr.warn {{ background: #3a2a1a; }}
    tr.warn .name {{ color: #f0c080; }}
    .idx {{ text-align: right; color: #888; width: 2.5rem; }}
    .pic {{ text-align: center; font-family: monospace; }}
    .plat img {{ max-height: 72px; max-width: 96px; image-rendering: pixelated; background: #111; display: inline-block; margin: 1px; }}
    .plat a {{ display: inline-block; }}
    .subst {{ font-size: 0.68rem; color: #f0a050; margin-top: 2px; }}
    .missing {{ color: #666; }}
    .stats {{ color: #888; margin-bottom: 1rem; }}
  </style>
</head>
<body>
  <h1>Monster combat sprites — Amiga vs PC</h1>
  <p class="stats">
    {stats['monsters']} monsters · amiga {stats['amiga']} · cga {stats['cga']} · ega {stats['ega']}
    · orange = missing CGA or EGA GIF
  </p>
  <p>
    One row per <code>monsters.dat</code> record — animated combat sprite per platform.
    PC uses header entry <code>id-1</code>; empty slots fall forward to the next picture
    (<span class="subst">→ pic NN</span>).
  </p>
  <table>
    <thead>
      <tr>
        <th>#</th>
        <th>Name</th>
        <th>Pic</th>
        <th>Amiga .anm</th>
        <th>PC CGA</th>
        <th>PC EGA</th>
      </tr>
    </thead>
    <tbody>{body}
    </tbody>
  </table>
</body>
</html>
"""
    out_html.write_text(doc, encoding="utf-8")


def build_markdown(
    rows: list[dict],
    out_md: Path,
    stats: dict[str, int],
    *,
    image_prefix: str = "/gallery/monster-variants",
    github_wiki: bool = False,
) -> None:
    """VitePress or GitHub Wiki page: one row per ``monsters.dat`` record."""
    bust = "" if github_wiki else f"?v={VARIANTS_CACHE_VERSION}"
    prefix = image_prefix.rstrip("/")

    def variant_cell(r: dict, variant: str) -> str:
        parts: list[str] = []
        gif = r.get(variant)
        if gif:
            parts.append(f'![]({prefix}/{gif.name}{bust})')
        subst = r.get("subst", {}).get(variant)
        if subst:
            parts.append(f"<small>→ pic {subst:02d}</small>")
        return "<br>".join(parts) if parts else "—"

    lines: list[str] = []
    if not github_wiki:
        lines.extend(
            [
                "---",
                "title: Monster Variants (Amiga / PC)",
                "---",
                "",
            ]
        )
    lines.extend(
        [
            "# Monster combat sprites — Amiga vs PC",
            "",
            f"**{stats['monsters']}** named monsters from "
            + (
                "[`monsters.dat`](monsters-dat-Format)"
                if github_wiki
                else "[`monsters.dat`](/docs/reverse-engineering/16-monster-ability-format)"
            ),
            "(picture byte @ `0x15 & 0x7F`).",
            "",
            "Each platform column shows the **animated combat sprite** (composed GIF loop).",
            "",
            "PC lookup: header entry **`picture_id − 1`**. Empty slots advance to the next",
            "picture (`→ pic NN`). Some slots differ from Amiga (e.g. Mountain Man pic 43).",
            "",
            "Slot-level compare: "
            + (
                "[Picture ids 01–74](Platform-Monsters-01-74)."
                if github_wiki
                else "[PC picture ids 01–74](/docs/gallery/platform-compare)."
            ),
            "",
        ]
    )
    if not github_wiki:
        lines.extend(['<div class="monster-variants-wrap">', ""])
    lines.extend(
        [
            "| # | Name | Pic | Amiga | CGA | EGA |",
            "|---:|------|----:|-------|-----|-----|",
        ]
    )

    for r in rows:
        idx = r["idx"]
        name = r["name"].replace("|", "\\|")
        pic = r["picture"]
        lines.append(
            f"| {idx} | {name} | {pic:02d} | "
            f"{variant_cell(r, 'amiga')} | {variant_cell(r, 'cga')} | {variant_cell(r, 'ega')} |"
        )

    if not github_wiki:
        lines.extend(["", "</div>", ""])
    out_md.parent.mkdir(parents=True, exist_ok=True)
    out_md.write_text("\n".join(lines), encoding="utf-8")


def collect_rows_from_disk(out_dir: Path, mon_raw: bytes) -> tuple[list[dict], dict[str, int]]:
    """Rebuild gallery rows from existing GIFs (``--html-only``)."""
    used_slugs: dict[str, int] = {}
    rows: list[dict] = []
    stats = {"monsters": 0, "amiga": 0, "cga": 0, "ega": 0, "skipped": 0}
    for idx in range(RECORD_COUNT):
        rec = mon_raw[idx * RECORD_SIZE : (idx + 1) * RECORD_SIZE]
        if len(rec) < RECORD_SIZE:
            break
        d = decode_record(idx, rec)
        name = d["name"].strip()
        pic = d["picture"] & 0x7F
        if not name or pic == 0:
            stats["skipped"] += 1
            continue
        base = sanitize_name(name)
        if base in used_slugs:
            used_slugs[base] += 1
            base = f"{base}_{idx:03d}"
        else:
            used_slugs[base] = 1
        row: dict = {"idx": idx, "name": name, "picture": pic, "slug": base}
        for variant in ("amiga", "cga", "ega"):
            _attach_variant_files(row, variant, base, out_dir)
            if row.get(variant):
                stats[variant] += 1
        rows.append(row)
        stats["monsters"] += 1
    return rows, stats


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Export monster GIF variants (Amiga/CGA/EGA)")
    ap.add_argument("--monsters", type=Path, default=ROOT / "monsters.dat")
    ap.add_argument("--data", type=Path, default=ROOT, help="Amiga .anm directory")
    ap.add_argument("--gog", type=Path, default=DEFAULT_GOG)
    ap.add_argument("-o", "--output", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--cga-palette", type=int, choices=(0, 1), default=1)
    ap.add_argument(
        "--html-only",
        action="store_true",
        help="Regenerate index.html from GIFs already in --output",
    )
    ap.add_argument(
        "--markdown",
        type=Path,
        default=None,
        help="Also write VitePress markdown (wiki gallery page)",
    )
    ap.add_argument(
        "--image-prefix",
        default="/gallery/monster-variants",
        help="URL prefix for GIFs in --markdown (default: /gallery/monster-variants)",
    )
    ap.add_argument(
        "--no-html",
        action="store_true",
        help="Skip index.html (wiki export only needs GIFs + markdown)",
    )
    ap.add_argument(
        "--github-wiki",
        action="store_true",
        help="GitHub Wiki markdown (no YAML frontmatter, wiki links)",
    )
    args = ap.parse_args(argv)

    if not args.monsters.is_file():
        print(f"Not found: {args.monsters}", file=sys.stderr)
        return 1

    mon_raw = args.monsters.read_bytes()
    out_dir = args.output
    out_dir.mkdir(parents=True, exist_ok=True)

    def finish(rows: list[dict], stats: dict[str, int]) -> int:
        if not args.no_html:
            build_html(rows, out_dir / "index.html", stats)
            print(f"Wrote {out_dir / 'index.html'}")
        if args.markdown:
            build_markdown(
                rows,
                args.markdown,
                stats,
                image_prefix=args.image_prefix,
                github_wiki=args.github_wiki,
            )
            print(f"Wrote {args.markdown}")
        print(f"Wrote {out_dir}")
        print(
            f"  {stats['monsters']} monsters: "
            f"amiga={stats['amiga']} cga={stats['cga']} ega={stats['ega']}"
        )
        return 0

    if args.html_only:
        rows, stats = collect_rows_from_disk(out_dir, mon_raw)
        return finish(rows, stats)

    cga_path = args.gog / "MONSTERS.4"
    ega_path = args.gog / "MONSTERS.16"
    if not cga_path.is_file() or not ega_path.is_file():
        print(f"Need MONSTERS.4 and MONSTERS.16 under {args.gog}", file=sys.stderr)
        return 1

    cga_raw = cga_path.read_bytes()
    ega_raw = ega_path.read_bytes()

    used_slugs: dict[str, int] = {}
    stats = {"monsters": 0, "amiga": 0, "cga": 0, "ega": 0, "skipped": 0}
    rows: list[dict] = []

    for idx in range(RECORD_COUNT):
        rec = mon_raw[idx * RECORD_SIZE : (idx + 1) * RECORD_SIZE]
        if len(rec) < RECORD_SIZE:
            break
        d = decode_record(idx, rec)
        name = d["name"].strip()
        pic = d["picture"] & 0x7F
        if not name or pic == 0:
            stats["skipped"] += 1
            continue

        base = sanitize_name(name)
        if base in used_slugs:
            used_slugs[base] += 1
            base = f"{base}_{idx:03d}"
        else:
            used_slugs[base] = 1

        stats["monsters"] += 1
        anm_path = args.data / f"{pic:02d}.anm"
        row: dict = {"idx": idx, "name": name, "picture": pic, "slug": base}

        try:
            if export_amiga_assets(anm_path, out_dir, base):
                stats["amiga"] += 1
        except Exception as exc:
            print(f"#{idx} {name} amiga: {exc}", file=sys.stderr)

        for variant, raw_bytes, ext in (
            ("cga", cga_raw, ".4"),
            ("ega", ega_raw, ".16"),
        ):
            try:
                used = export_pc_assets(
                    raw_bytes, pic, ext, out_dir, base, cga_palette=args.cga_palette
                )
                if used:
                    stats[variant] += 1
                    if used != pic:
                        row.setdefault("subst", {})[variant] = used
            except Exception as exc:
                print(f"#{idx} {name} {variant}: {exc}", file=sys.stderr)

        _attach_variant_files(row, "amiga", base, out_dir)
        _attach_variant_files(row, "cga", base, out_dir)
        _attach_variant_files(row, "ega", base, out_dir)
        rows.append(row)

        if stats["monsters"] % 50 == 0:
            print(f"  ... {stats['monsters']} monsters processed")

    manifest = out_dir / "manifest.txt"
    manifest.write_text(
        "\n".join(
            [
                f"# monsters.dat -> {out_dir.name}",
                f"monsters={stats['monsters']} skipped_empty={stats['skipped']}",
                f"amiga_gifs={stats['amiga']} cga_gifs={stats['cga']} ega_gifs={stats['ega']}",
                "# naming: <Name>_{amiga,cga,ega}.{gif,png} + -sheet.png + -raw-sheet.png",
            ]
        ),
        encoding="utf-8",
    )
    return finish(rows, stats)


if __name__ == "__main__":
    raise SystemExit(main())

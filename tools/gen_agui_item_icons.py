#!/usr/bin/env python3
"""Generate Agui item icons with an OpenRouter image model.

The prompt spec is NOT duplicated here — everything (master prompt, negative
prompt, the 12 batches and all 255 item names) is parsed live out of
`game/data/ui/agui/ITEM_ICON_PROMPT.md`, so editing that doc changes what this
script asks for.

Pipeline
    python tools/gen_agui_item_icons.py --list                 # parse check, no API
    python tools/gen_agui_item_icons.py --batch 1 --dry-run    # see exact prompts
    python tools/gen_agui_item_icons.py --batch 1              # generate
    python tools/check_agui_pixel_grid.py <out-dir>            # local QA
    python tools/ingest_agui_item_icons.py --from-dir <out-dir>
    python tools/pack_agui_ui.py
    python tools/ui_pack_preview.py

Common invocations
    --all                       every batch, 255 icons (resumable)
    --batch 1 2 3               selected batches
    --ids 0x01-0x18,0x3e        explicit id ranges / singles
    --mode sheet                one 8-column sheet per batch instead of per item
    --jobs 4                    parallel requests
    --retries 2                 auto-regenerate icons that fail the grid check
    --style-ref                 attach icons/use.png + faces/face_00.png as refs
    --force                     redo icons that already exist

Outputs `iXX.png` (lowercase hex) so `ingest_agui_item_icons.py --from-dir`
picks them up unchanged, plus a `manifest.json` recording model, prompt hash,
cost and QA verdict per icon.

Needs an OpenRouter key — see openrouter_client.py for how it is discovered.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import re
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from openrouter_client import (  # noqa: E402
    DEFAULT_IMAGE_MODEL,
    GeneratedImage,
    OpenRouterClient,
    OpenRouterError,
)

ROOT = Path(__file__).resolve().parents[1]
AGUI = ROOT / "game" / "data" / "ui" / "agui"
PROMPT_MD = AGUI / "ITEM_ICON_PROMPT.md"
PALETTE_PATH = AGUI / "palette.json"
DEFAULT_OUT = AGUI / "_ai_exports"

STYLE_REFS = [AGUI / "icons" / "use.png", AGUI / "faces" / "face_00.png"]

CELL = 96          # 12x12 logical @ 8x
SHEET_COLS = 8
# Models render a fixed square canvas (Gemini: 1024). Sheet cells must divide it
# evenly or the split lands mid-icon, so only power-of-two column counts qualify.
SHEET_CANVAS = 1024
SHEET_COL_CHOICES = (2, 4, 8, 16)


def sheet_layout(count: int, canvas: int = SHEET_CANVAS,
                 cols: int | str = "auto") -> tuple[int, int, int]:
    """Pick (cols, rows, cell_px) for `count` icons on a square canvas.

    Fewer columns means more pixels per logical pixel, which is what actually
    decides whether an icon survives the reduction to 12x12 — so use the
    coarsest grid the batch fits in.
    """
    if cols != "auto":
        c = int(cols)
        return c, (count + c - 1) // c, canvas // c
    for c in SHEET_COL_CHOICES:
        if count <= c * c:
            return c, (count + c - 1) // c, canvas // c
    c = SHEET_COL_CHOICES[-1]
    return c, (count + c - 1) // c, canvas // c

_print_lock = threading.Lock()


def say(*parts: object) -> None:
    with _print_lock:
        print(*parts, flush=True)


# ---------------------------------------------------------------------------
# ITEM_ICON_PROMPT.md parsing
# ---------------------------------------------------------------------------


@dataclass
class Item:
    id: int
    name: str
    batch: int

    @property
    def hex(self) -> str:
        return f"{self.id:02X}"

    @property
    def filename(self) -> str:
        return f"i{self.id:02x}.png"

    def __str__(self) -> str:
        return f"{self.hex} {self.name}"


@dataclass
class Batch:
    number: int
    title: str
    first: int
    last: int
    body: str
    items: list[Item] = field(default_factory=list)


@dataclass
class Spec:
    master: str
    negative: str
    batches: list[Batch]

    @property
    def items(self) -> list[Item]:
        return [it for b in self.batches for it in b.items]

    def batch(self, number: int) -> Batch:
        for b in self.batches:
            if b.number == number:
                return b
        raise SystemExit(f"no batch {number} in {PROMPT_MD.name}")


_FENCE = re.compile(r"```[a-zA-Z0-9_-]*\n(.*?)\n```", re.S)
# "## Batch 01 — mundane 1H (ids 01–18)" — accepts em/en/hyphen dashes.
_BATCH_HEAD = re.compile(
    r"^##\s+Batch\s+(\d+)\s*[—–-]\s*(.*?)\s*\(ids\s*([0-9A-Fa-f]{2})\s*[—–-]\s*([0-9A-Fa-f]{2})\)\s*$",
    re.M,
)


def _block_after(text: str, heading_pattern: str) -> str:
    m = re.search(heading_pattern, text, re.M)
    if not m:
        raise SystemExit(f"{PROMPT_MD.name}: could not find heading /{heading_pattern}/")
    fence = _FENCE.search(text, m.end())
    if not fence:
        raise SystemExit(f"{PROMPT_MD.name}: no code block after /{heading_pattern}/")
    return fence.group(1).strip()


def _parse_items(body: str, first: int, last: int, batch_no: int) -> list[Item]:
    """Pull `HH Name` entries out of a batch block.

    Driven by the declared id range: for each expected id we look for that exact
    token, so item names containing digits ("Admit 8 Pass", "J-26 Fluxer",
    "+7 Loincloth") and inline hex colours ("#40C040") cannot be mistaken for ids.
    """
    ids = list(range(first, last + 1))
    hits: list[tuple[int, int, int]] = []  # (id, match_start, match_end)
    cursor = 0
    for value in ids:
        pat = re.compile(rf"(?:^|[\s])({value:02X})[ \t]+(?=\S)", re.M)
        m = pat.search(body, cursor)
        if not m:
            say(f"  ! batch {batch_no}: id {value:02X} not found in the doc text")
            continue
        hits.append((value, m.start(1), m.end()))
        cursor = m.end()

    items: list[Item] = []
    for i, (value, _, end) in enumerate(hits):
        stop = hits[i + 1][1] if i + 1 < len(hits) else len(body)
        # Entries never wrap: cut at the newline so trailing batch prose
        # ("Plain steel / wood, no magic glow.") is not glued onto the last name.
        raw = body[end:stop].split("\n", 1)[0]
        name = " ".join(raw.split()).strip(" ,;")
        if not name:
            say(f"  ! batch {batch_no}: id {value:02X} has an empty name")
            continue
        items.append(Item(id=value, name=name, batch=batch_no))
    return items


def load_spec(path: Path = PROMPT_MD) -> Spec:
    if not path.is_file():
        raise SystemExit(f"prompt spec not found: {path}")
    text = path.read_text(encoding="utf-8")

    master = _block_after(text, r"^##\s+Master prompt")
    negative = " ".join(_block_after(text, r"^##\s+Negative prompt").split())

    heads = list(_BATCH_HEAD.finditer(text))
    if not heads:
        raise SystemExit(f"{path.name}: no '## Batch NN — … (ids XX–YY)' headings found")

    batches: list[Batch] = []
    for i, m in enumerate(heads):
        number = int(m.group(1))
        title = m.group(2)
        first = int(m.group(3), 16)
        last = int(m.group(4), 16)
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        section = text[m.end():end]
        fence = _FENCE.search(section)
        body = fence.group(1).strip() if fence else section.strip()
        batch = Batch(number=number, title=title, first=first, last=last, body=body)
        batch.items = _parse_items(body, first, last, number)
        batches.append(batch)
    return Spec(master=master, negative=negative, batches=batches)


# ---------------------------------------------------------------------------
# prompt construction
# ---------------------------------------------------------------------------


def item_prompt(spec: Spec, item: Item) -> str:
    return (
        f"{spec.master}\n"
        f"\nRENDER EXACTLY ONE ITEM — nothing else in the image:\n"
        f"  {item.hex} {item.name}\n"
        f"\nOUTPUT (overrides any pixel dimensions above): the canvas size does not\n"
        f"matter — render whatever resolution you natively produce. What matters is\n"
        f"that the ARTWORK is a 12x12 grid of large, perfectly flat, hard-edged\n"
        f"blocks: 144 big squares of solid colour, each square one uniform colour\n"
        f"edge to edge. Think of it as a 12x12 image blown up huge, never as a\n"
        f"detailed drawing. Content fills the inner 10x10 with a 1-block margin.\n"
        f"Transparent background if you can; otherwise a single flat solid colour\n"
        f"used nowhere in the artwork. No sprite sheet, no grid lines, no labels,\n"
        f"no border, no scene, no shading within a block.\n"
        f"\nAVOID (negative prompt): {spec.negative}"
    )


def sheet_prompt(spec: Spec, batch: Batch, cols: int, rows: int, cell: int) -> str:
    listing = "\n".join(f"  row {i // cols + 1} col {i % cols + 1}: {it.hex} {it.name}"
                        for i, it in enumerate(batch.items))
    return (
        f"{spec.master}\n"
        f"\nSPRITE SHEET on a single square canvas divided into a {cols} x {cols}\n"
        f"grid of equal square cells ({cell}x{cell} each). Zero gutter, no grid\n"
        f"lines, no labels, no numbers, no frames, no separators of any kind.\n"
        f"Each cell holds exactly ONE item, drawn as a 12x12 grid of large flat\n"
        f"hard-edged blocks filling the inner 10x10 of that cell with a 1-block\n"
        f"margin. Every block is one uniform colour edge to edge — no shading,\n"
        f"no anti-aliasing, no detail smaller than a block.\n"
        f"Transparent background if you can; otherwise ONE flat solid colour used\n"
        f"nowhere in the artwork.\n"
        f"\nFill cells row-major. Rows {rows + 1}-{cols} stay completely empty:\n{listing}\n"
        f"\nBatch note: {batch.title}. Every item must be distinguishable from the\n"
        f"others at a glance — silhouette first, material second.\n"
        f"\nAVOID (negative prompt): {spec.negative}"
    )


def prompt_hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:12]


# ---------------------------------------------------------------------------
# id selection
# ---------------------------------------------------------------------------


def parse_id_spec(text: str) -> set[int]:
    """'0x01-0x18,3e,0xa0-0xbb' -> {1..24, 62, 160..187}"""
    out: set[int] = set()
    for chunk in text.replace(" ", "").split(","):
        if not chunk:
            continue
        if "-" in chunk[1:]:
            lo_s, _, hi_s = chunk.partition("-") if not chunk.startswith("0x") else (
                chunk[: chunk.index("-", 2)], "-", chunk[chunk.index("-", 2) + 1:]
            )
            lo, hi = int(lo_s, 16), int(hi_s, 16)
            if lo > hi:
                lo, hi = hi, lo
            out.update(range(lo, hi + 1))
        else:
            out.add(int(chunk, 16))
    bad = {v for v in out if not 0x01 <= v <= 0xFF}
    if bad:
        raise SystemExit(f"ids out of range 0x01-0xFF: {sorted(hex(v) for v in bad)}")
    return out


# ---------------------------------------------------------------------------
# generation
# ---------------------------------------------------------------------------


@dataclass
class Result:
    item: Item | None
    path: Path | None
    ok: bool
    note: str = ""
    attempts: int = 0
    cost: float = 0.0


def _save_first(images: Sequence[GeneratedImage], dest: Path) -> Path:
    dest.parent.mkdir(parents=True, exist_ok=True)
    images[0].save(dest)
    return dest


def _quantize(raw: Path, dest: Path, palette, args) -> tuple[bool, str]:
    """Reduce the raw model output to an exact 12x12-derived icon and grade it.

    Image models emit a fixed canvas (Gemini: 1024x1024) and cannot be made to
    output exact 8x blocks, so the grid is recovered here rather than demanded
    from the model. Returns (accept, note).
    """
    if palette is None:
        return True, "no palette — copied raw"
    try:
        from check_agui_pixel_grid import quantize_file
    except ImportError:
        return True, "quantize-skipped (check_agui_pixel_grid.py missing)"
    try:
        st = quantize_file(raw, palette, dest, emit=args.emit,
                           content_cells=args.content_cells)
    except Exception as exc:  # noqa: BLE001
        return False, f"quantize failed: {exc}"

    bits = [f"purity {st.purity:.2f}", f"fill {st.filled}", f"pens {st.pens}"]
    if st.bg_stripped:
        bits.append("bg-keyed")
    note = ", ".join(bits)

    if st.filled < 6:
        return False, f"{note} — almost nothing survives 12x12"
    if st.filled > 132:
        return False, f"{note} — fills the whole canvas (background not keyed)"
    if st.purity < args.min_purity:
        return False, f"{note} — too detailed/soft to reduce (< {args.min_purity:.2f})"
    if args.strict_qa and st.purity < 0.9:
        return False, f"{note} — strict mode wants purity >= 0.90"
    return True, note


def generate_item(
    client: OpenRouterClient,
    spec: Spec,
    item: Item,
    out_dir: Path,
    args: argparse.Namespace,
    palette,
    refs: Sequence[str],
) -> Result:
    dest = out_dir / item.filename
    if dest.exists() and not args.force:
        return Result(item, dest, True, "skipped (exists)")

    prompt = item_prompt(spec, item)
    if args.dry_run:
        say(f"\n--- {item} -> {dest.name} ---\n{prompt}")
        return Result(item, dest, True, "dry-run")

    last_note = ""
    for attempt in range(1, args.retries + 2):
        before = client.usage.cost
        try:
            images = client.generate_image(
                prompt,
                n=1,
                aspect_ratio=args.aspect,
                reference_images=refs,
                transport=args.transport,
            )
        except OpenRouterError as exc:
            last_note = str(exc)
            say(f"  {item.hex} attempt {attempt} failed: {last_note}")
            if attempt <= args.retries:
                time.sleep(1.5 * attempt)
                continue
            return Result(item, None, False, last_note, attempt, client.usage.cost - before)

        cost = client.usage.cost - before
        if not images:
            last_note = "model returned no image"
            if attempt <= args.retries:
                continue
            return Result(item, None, False, last_note, attempt, cost)

        raw_dir = out_dir / "raw"
        raw = raw_dir / item.filename
        _save_first(images, raw)
        ok, note = _quantize(raw, dest, palette, args)
        last_note = note
        if ok or attempt > args.retries:
            say(f"  {'OK ' if ok else 'QA?'} {item.hex} {item.name[:34]:<34} {dest.name}  {note}")
            return Result(item, dest, ok, note, attempt, cost)
        say(f"  retry {item.hex} ({note})")
        dest.unlink(missing_ok=True)
        raw.unlink(missing_ok=True)

    return Result(item, None, False, last_note, args.retries + 1)


def generate_sheet(
    client: OpenRouterClient,
    spec: Spec,
    batch: Batch,
    out_dir: Path,
    args: argparse.Namespace,
    palette,
    refs: Sequence[str],
) -> list[Result]:
    """One request per batch: render a grid, then split and quantize each cell.

    Far cheaper than per-item (1 call instead of ~28) but the model has to keep
    every item distinct in a single render, so expect a lower hit rate. Cells
    that fail quantization are reported individually and can be re-run in
    per-item mode with --ids.
    """
    cols, rows, cell = sheet_layout(len(batch.items), args.canvas, args.sheet_cols)
    sheet_path = out_dir / "raw" / f"batch_{batch.number:02d}.png"

    prompt = sheet_prompt(spec, batch, cols, rows, cell)
    if args.dry_run:
        say(f"\n--- batch {batch.number:02d}: {len(batch.items)} items, "
            f"{cols}x{cols} grid, {cell}px cells -> {sheet_path.name} ---\n{prompt}")
        return [Result(None, sheet_path, True, "dry-run")]

    before = client.usage.cost
    if sheet_path.is_file() and not args.force:
        say(f"  reusing existing sheet {sheet_path.name} (--force to regenerate)")
    else:
        try:
            images = client.generate_image(
                prompt, n=1, aspect_ratio="1:1",
                reference_images=refs, transport=args.transport,
            )
        except OpenRouterError as exc:
            return [Result(None, None, False, str(exc), 1, client.usage.cost - before)]
        if not images:
            return [Result(None, None, False, "model returned no image", 1,
                           client.usage.cost - before)]
        _save_first(images, sheet_path)

    cost = client.usage.cost - before
    return split_sheet(sheet_path, batch, out_dir, args, palette,
                       cols=cols, cell=cell, cost=cost)


def split_sheet(
    sheet_path: Path,
    batch: Batch,
    out_dir: Path,
    args: argparse.Namespace,
    palette,
    *,
    cols: int,
    cell: int,
    cost: float = 0.0,
) -> list[Result]:
    """Cut a sheet into cells and quantize each one into iXX.png."""
    try:
        from PIL import Image
        from check_agui_pixel_grid import quantize_to_logical
    except ImportError as exc:
        return [Result(None, sheet_path, False, f"pillow/codec missing: {exc}")]

    sheet = Image.open(sheet_path).convert("RGBA")
    w, h = sheet.size
    # Trust the grid, not the nominal cell size: the model may have returned a
    # different canvas than we asked for.
    cw = w / cols
    ch = h / cols
    say(f"  sheet {w}x{h} -> {cols}x{cols} grid, {cw:.0f}x{ch:.0f} cells")

    results: list[Result] = []
    per_cell_cost = cost / max(1, len(batch.items))
    for i, item in enumerate(batch.items):
        dest = out_dir / item.filename
        if dest.exists() and not args.force:
            results.append(Result(item, dest, True, "skipped (exists)"))
            continue
        col, row = i % cols, i // cols
        box = (int(round(col * cw)), int(round(row * ch)),
               int(round((col + 1) * cw)), int(round((row + 1) * ch)))
        crop = sheet.crop(box)
        raw_cell = out_dir / "raw" / item.filename
        raw_cell.parent.mkdir(parents=True, exist_ok=True)
        crop.save(raw_cell)

        if palette is None:
            results.append(Result(item, dest, True, "no palette — raw cell kept",
                                  1, per_cell_cost))
            continue
        try:
            native, st = quantize_to_logical(crop, palette,
                                             content_cells=args.content_cells)
        except Exception as exc:  # noqa: BLE001
            results.append(Result(item, None, False, f"quantize failed: {exc}",
                                  1, per_cell_cost))
            continue

        note = f"purity {st.purity:.2f}, fill {st.filled}, pens {st.pens}"
        ok = True
        if st.filled < 6:
            ok, note = False, f"{note} — cell looks empty"
        elif st.filled > 132:
            ok, note = False, f"{note} — cell has no transparent margin"
        elif st.purity < args.min_purity:
            ok, note = False, f"{note} — below min purity {args.min_purity:.2f}"

        if ok:
            scale = max(1, args.emit // 12)
            dest.parent.mkdir(parents=True, exist_ok=True)
            native.resize((12 * scale, 12 * scale), Image.NEAREST).save(dest)
        say(f"  {'OK ' if ok else 'BAD'} {item.hex} {item.name[:32]:<32} "
            f"r{row + 1}c{col + 1}  {note}")
        results.append(Result(item, dest if ok else None, ok, note, 1, per_cell_cost))
    return results


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sel = ap.add_argument_group("selection")
    sel.add_argument("--all", action="store_true", help="every batch (0x01-0xFF)")
    sel.add_argument("--batch", type=int, nargs="+", metavar="N", help="batch numbers")
    sel.add_argument("--ids", help="hex ids/ranges, e.g. 0x01-0x18,3e,a0-bb")
    sel.add_argument("--list", action="store_true",
                     help="print the parsed batches/items and exit (no API key needed)")

    out = ap.add_argument_group("output")
    out.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    out.add_argument("--mode", choices=["per-item", "sheet"], default="per-item")
    out.add_argument("--sheet-cols", default="auto",
                     help="columns on a batch sheet: auto, 2, 4, 8 or 16 "
                          "(must divide the canvas evenly)")
    out.add_argument("--canvas", type=int, default=SHEET_CANVAS,
                     help="expected model canvas size for sheets (default 1024)")
    out.add_argument("--split-sheet", type=Path,
                     help="skip generation: split an existing sheet for --batch N")
    out.add_argument("--force", action="store_true", help="regenerate existing files")
    out.add_argument("--dry-run", action="store_true", help="print prompts, make no calls")

    api = ap.add_argument_group("model")
    api.add_argument("--model", default=DEFAULT_IMAGE_MODEL)
    api.add_argument("--api-key")
    api.add_argument("--transport", choices=["auto", "images", "chat"], default="auto")
    api.add_argument("--aspect", default="1:1")
    api.add_argument("--style-ref", action="store_true",
                     help="attach icons/use.png + faces/face_00.png as style references")
    api.add_argument("--ref", action="append", default=[], help="extra reference image (repeatable)")
    api.add_argument("--jobs", type=int, default=1, help="parallel requests (default 1)")
    api.add_argument("--retries", type=int, default=1,
                     help="regenerations per icon when QA fails (default 1)")
    api.add_argument("--timeout", type=float, default=300.0)
    api.add_argument("--strict-qa", action="store_true",
                     help="require a near-perfect flat-block source (purity >= 0.90)")
    api.add_argument("--emit", type=int, default=96, choices=[12, 96, 192],
                     help="size of the quantized icon written for ingest (default 96)")
    api.add_argument("--content-cells", type=int, default=10,
                     help="logical cells the artwork is fitted into (default 10, "
                          "leaving the spec's 1-cell margin)")
    api.add_argument("--min-purity", type=float, default=0.55,
                     help="reject/retry below this flat-block score (default 0.55)")
    api.add_argument("-v", "--verbose", action="store_true")
    return ap


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    spec = load_spec()

    if args.list:
        total = 0
        for b in spec.batches:
            print(f"\nBatch {b.number:02d}  {b.title}  "
                  f"(0x{b.first:02X}-0x{b.last:02X}, {len(b.items)} parsed)")
            for it in b.items:
                print(f"   {it.hex}  {it.name}")
            total += len(b.items)
        print(f"\n{total} items across {len(spec.batches)} batches")
        print(f"master prompt: {len(spec.master)} chars | negative: {len(spec.negative)} chars")
        missing = sorted(set(range(0x01, 0x100)) - {it.id for it in spec.items})
        if missing:
            print(f"ids with no entry: {', '.join(f'{v:02X}' for v in missing)}")
        return 0

    if args.split_sheet:
        if not args.batch or len(args.batch) != 1:
            print("--split-sheet needs exactly one --batch N", file=sys.stderr)
            return 2
        batch = spec.batch(args.batch[0])
        args.out_dir.mkdir(parents=True, exist_ok=True)
        try:
            from check_agui_pixel_grid import load_palette
            palette = load_palette(PALETTE_PATH)
        except Exception as exc:  # noqa: BLE001
            say(f"! palette unavailable ({exc})")
            palette = None
        cols, _rows, cell = sheet_layout(len(batch.items), args.canvas, args.sheet_cols)
        res = split_sheet(args.split_sheet, batch, args.out_dir, args, palette,
                          cols=cols, cell=cell)
        bad = [r for r in res if not r.ok]
        say(f"\n{len(res) - len(bad)}/{len(res)} cells accepted")
        if bad:
            say("re-run these per-item:  --force --ids "
                + ",".join(f"0x{r.item.hex}" for r in bad if r.item))
        return 1 if bad else 0

    if not (args.all or args.batch or args.ids):
        build_parser().print_help()
        print("\npick a selection: --all, --batch N, or --ids 0x01-0x18", file=sys.stderr)
        return 2

    # Resolve the work list.
    if args.all:
        batches = list(spec.batches)
    elif args.batch:
        batches = [spec.batch(n) for n in args.batch]
    else:
        wanted = parse_id_spec(args.ids)
        batches = []
        for b in spec.batches:
            picked = [it for it in b.items if it.id in wanted]
            if picked:
                batches.append(Batch(b.number, b.title, b.first, b.last, b.body, picked))

    items = [it for b in batches for it in b.items]
    if not items:
        print("nothing selected", file=sys.stderr)
        return 2

    args.out_dir.mkdir(parents=True, exist_ok=True)

    refs = [str(p) for p in args.ref]
    if args.style_ref:
        refs += [str(p) for p in STYLE_REFS if p.is_file()]
        if not refs:
            say("! --style-ref: no reference images found under game/data/ui/agui")

    palette = None
    if not args.dry_run:
        try:
            from check_agui_pixel_grid import load_palette
            palette = load_palette(PALETTE_PATH)
        except Exception as exc:  # noqa: BLE001
            say(f"! palette QA disabled ({exc})")

    client = None
    if not args.dry_run:
        try:
            client = OpenRouterClient(
                api_key=args.api_key, model=args.model,
                timeout=args.timeout, verbose=args.verbose,
            )
        except OpenRouterError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1
    else:
        class _Null:  # dry-run stand-in so the code paths stay identical
            model = args.model

            class usage:  # noqa: N801
                cost = 0.0
        client = _Null()  # type: ignore[assignment]

    say(f"model {args.model} | mode {args.mode} | out {args.out_dir}")
    if args.mode == "sheet":
        say(f"{len(batches)} sheet(s)" + (" [dry-run]" if args.dry_run else ""))
    else:
        say(f"{len(items)} icon(s)" + (" [dry-run]" if args.dry_run else "")
            + (f" | {args.jobs} jobs" if args.jobs > 1 else ""))
    if refs:
        say(f"reference images: {', '.join(Path(r).name for r in refs)}")

    started = time.time()
    results: list[Result] = []

    if args.mode == "sheet":
        for b in batches:
            results.extend(generate_sheet(client, spec, b, args.out_dir, args,  # type: ignore[arg-type]
                                          palette, refs))
    elif args.jobs > 1 and not args.dry_run:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {
                pool.submit(generate_item, client, spec, it, args.out_dir, args, palette, refs): it
                for it in items
            }
            for fut in concurrent.futures.as_completed(futures):
                results.append(fut.result())
    else:
        for it in items:
            results.append(generate_item(client, spec, it, args.out_dir, args, palette, refs))  # type: ignore[arg-type]

    if args.dry_run:
        say(f"\ndry-run: {len(results)} prompt(s) shown, no API calls made")
        return 0

    ok = [r for r in results if r.ok]
    bad = [r for r in results if not r.ok]
    elapsed = time.time() - started

    manifest_path = args.out_dir / "manifest.json"
    existing: dict = {}
    if manifest_path.is_file():
        try:
            existing = json.loads(manifest_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            existing = {}
    entries: dict = existing.get("icons", {})
    for r in results:
        key = r.item.hex if r.item else (r.path.name if r.path else "?")
        entries[key] = {
            "name": r.item.name if r.item else None,
            "batch": r.item.batch if r.item else None,
            "file": r.path.name if r.path else None,
            "ok": r.ok,
            "note": r.note,
            "attempts": r.attempts,
            "cost_usd": round(r.cost, 5),
            "model": args.model,
            "prompt_sha": prompt_hash(item_prompt(spec, r.item)) if r.item else None,
            "generated": time.strftime("%Y-%m-%dT%H:%M:%S"),
        }
    manifest_path.write_text(json.dumps({
        "spec": str(PROMPT_MD.relative_to(ROOT)),
        "model": args.model,
        "mode": args.mode,
        "updated": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "icons": entries,
    }, indent=2), encoding="utf-8")

    say(f"\n{len(ok)}/{len(results)} ok in {elapsed:.0f}s | "
        f"{client.usage} | manifest -> {manifest_path.name}")  # type: ignore[union-attr]
    if bad:
        say("failed / needs another pass:")
        for r in bad:
            label = str(r.item) if r.item else (r.path.name if r.path else "?")
            say(f"  {label}: {r.note}")
        say(f"\nre-run just those:  --force --ids "
            f"{','.join(f'0x{r.item.hex}' for r in bad if r.item)}")

    say("\nnext:")
    say(f"  raw model output kept in {args.out_dir / 'raw'} for re-quantizing")
    say(f"  python tools/check_agui_pixel_grid.py {args.out_dir}")
    if args.mode == "per-item":
        say(f"  python tools/ingest_agui_item_icons.py --from-dir {args.out_dir}")
    say("  python tools/pack_agui_ui.py && python tools/ui_pack_preview.py")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())

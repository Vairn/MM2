#!/usr/bin/env python3
"""Assemble SNES MM2 monster/portrait gfx from metadata + animation scripts.

**Honest status:** MM2 stores monster/portrait graphics as *scattered 8x8 tiles*
referenced by per-monster animation scripts. The game composes them at runtime via
$00F62E (WRAM staging) and $00F23C (multi-layer VRAM placement). This tool
produces **best-effort frame previews** — recognizable parts (knight armour, hooded
figures, mechanical bits) but **not bit-exact assembled sprites** until the full
F62E row stride, F148 8bpp bitmask path, and F23C quadrant layout are replicated.

Metadata (table $14:8060, stride index*3):
  +0 bank, +4 palette ptr, +6 CHR-base ptr (F4), +8 anim script ptr, +0xA dims
Anim tokens >= $8000: absolute tile ROM address in bank; else F4 + (token-1)*32.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from snes_decompress import bgr555_to_rgb, decode_4bpp_tile, lorom_to_file  # noqa: E402

try:
    from PIL import Image
except ImportError:
    Image = None  # type: ignore

TABLE_SNES = (0x14, 0x8060)


def word_le(buf: bytes, off: int) -> int:
    return buf[off] | (buf[off + 1] << 8)


def resolve_tile(bank: int, tok: int, f4: int, rom: bytes) -> bytes | None:
  if tok == 0:
    return None
  if tok >= 0x8000:
    off = lorom_to_file(bank, tok)
  else:
    off = lorom_to_file(bank, f4 + (tok - 1) * 32)
  if off + 32 > len(rom):
    return None
  return rom[off : off + 32]


def read_meta(rom: bytes, index: int) -> dict | None:
    tbase = lorom_to_file(*TABLE_SNES)
    eoff = tbase + index * 3
    if eoff + 3 > len(rom):
        return None
    lo, hi, bk = rom[eoff : eoff + 3]
    if lo == 0 and hi == 0 and bk == 0:
        return None
    addr = (hi << 8) | lo
    moff = lorom_to_file(bk, addr)
    if moff + 0x10 > len(rom):
        return None
    raw = rom[moff : moff + 0x20]
    bank = raw[0]
    pal_addr = word_le(raw, 4)
    ptr_chr = lorom_to_file(bank, word_le(raw, 6))
    ptr_anim = lorom_to_file(bank, word_le(raw, 8))
    ptr_dims = lorom_to_file(bank, word_le(raw, 0x0A))
    pal_file = lorom_to_file(bank, pal_addr) if pal_addr >= 0x8000 else None
    if pal_file is None or pal_file + 32 > len(rom):
        return None
    f4 = word_le(rom, ptr_chr)
    cols = rom[ptr_dims]
    rows = rom[ptr_dims + 2] if rom[ptr_dims + 2] else rom[ptr_dims + 1]
    if cols == 0 or rows == 0:
        cols, rows = max(cols, rows, 1), max(cols, rows, 1)
    return {
        "index": index,
        "bank": bank,
        "meta_file": moff,
        "palette_file": pal_file,
        "script_file": ptr_anim,
        "f4": f4,
        "cols": cols,
        "rows": rows,
        "bpp8": bool(raw[2] & 0x80),
    }


def parse_frames(rom: bytes, meta: dict, max_frames: int = 1) -> list[list[int]]:
    """Return up to max_frames tile-token lists (frame 0 only by default)."""
    pos = meta["script_file"]
    end = min(len(rom), pos + 0x200)
    need = meta["cols"] * meta["rows"]
    frames: list[list[int]] = []
    cur: list[int] = []
    while pos + 2 <= end and len(frames) < max_frames:
        tok = word_le(rom, pos)
        pos += 2
        if tok == 0:
            if cur:
                frames.append(cur[:need])
                if len(frames) >= max_frames:
                    break
                cur = []
            continue
        cur.append(tok)
        if len(cur) >= need:
            frames.append(cur[:need])
            if len(frames) >= max_frames:
                break
            cur = cur[need:]
    if not frames and cur:
        frames.append(cur[:need])
    return frames


def render_frame(rom: bytes, meta: dict, tokens: list[int], scale: int) -> Image.Image:
    words = [struct.unpack_from("<H", rom, meta["palette_file"] + i * 2)[0] for i in range(16)]
    pal = [bgr555_to_rgb(w) for w in words]
    cols, rows = meta["cols"], meta["rows"]
    img = Image.new("RGBA", (cols * 8, rows * 8), (0, 0, 0, 0))
    px = img.load()
    for i, tok in enumerate(tokens[: cols * rows]):
        block = resolve_tile(meta["bank"], tok, meta["f4"], rom)
        if not block:
            continue
        tile = decode_4bpp_tile(block)
        ox, oy = (i % cols) * 8, (i // cols) * 8
        for y in range(8):
            for x in range(8):
                c = tile[y][x]
                if c == 0:
                    continue
                px[ox + x, oy + y] = (*pal[c], 255)
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def assemble_monster(rom: bytes, index: int, scale: int, max_frames: int = 1) -> tuple[dict, list[Image.Image]]:
    meta = read_meta(rom, index)
    if not meta:
        return {"index": index, "error": "no_meta"}, []
    frames = parse_frames(rom, meta, max_frames)
    imgs = [render_frame(rom, meta, fr, scale) for fr in frames if fr]
    meta["frame_count"] = len(imgs)
    meta["tokens_per_frame"] = meta["cols"] * meta["rows"]
    return meta, imgs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--out", type=Path, default=Path("EXTRACTED/snes/analysis/monsters.json"))
    ap.add_argument("--render-dir", type=Path, default=Path("EXTRACTED/snes/gfx/monsters_assembled"))
    ap.add_argument("--max", type=int, default=128)
    ap.add_argument("--max-frames", type=int, default=1, help="Frames per monster (default 1)")
    ap.add_argument("--scale", type=int, default=4)
    args = ap.parse_args()

    if Image is None:
        raise SystemExit("pip install pillow")

    rom = args.rom.read_bytes()
    args.render_dir.mkdir(parents=True, exist_ok=True)
    records: list[dict] = []
    rendered = 0

    for i in range(args.max):
        info, frames = assemble_monster(rom, i, args.scale, args.max_frames)
        if info.get("error"):
            break
        records.append(info)
        if not frames:
            continue
        out = args.render_dir / f"monster_{i:02d}_preview.png"
        frames[0].save(out)
        info["png"] = str(out).replace("\\", "/")
        info["note"] = "partial tile assembly — not a validated full sprite"
        rendered += 1
        print(f"monster {i:02d}: {info['cols']}x{info['rows']} preview")

    payload = {"table": f"${TABLE_SNES[0]:02X}:{TABLE_SNES[1]:04X}", "assembled": rendered, "records": records}
    args.out.write_text(json.dumps(payload, indent=2))
    print(f"Wrote {args.out} ({rendered} assembled)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

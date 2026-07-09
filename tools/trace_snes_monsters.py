#!/usr/bin/env python3
"""Assemble SNES MM2 monster/portrait sprites via $00F62E + multi-layer $00F23C.

Layer 0 (FE=0): overwrite blit ($F0E7) of F8×FA byte-token script into a
16-wide WRAM sheet at $7E:4000.

Layers 1+ (FE=1): for each tile, build inverted opacity mask ($F200/$F148),
AND-clear dest, then OR-blit source ($F066). Animation frame AE indexes into
per-layer F4/EB/dims word tables via selector byte tables; selector value 1
skips the layer. Terminator bytes at +0E/+17/+20 (bit7 set) end the chain.

Metadata (bank = meta[0]):
  +04  palette addr (direct)
  +06/+08/+0A  ptrs -> F4 / EB / dims word tables (indexed by frame)
  +0C  selector for layer-0 FE=1 redraw
  +0E  terminator (BMI => done)
  +0F/+11/+13  layer-1 F4/EB/dims tables; +15 selector; +17 terminator
  +18/+1A/+1C  layer-2; +1E selector; +20 terminator
  +21/+23/+25  layer-3; +27 selector
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
D421: list[int] = []
D439: list[int] = []
D46B: list[int] = []

# Layer groups: (f4_off, eb_off, dims_off, sel_off)
# Terminators at +0E/+17/+20 are checked *before* entering the next layer.
LAYERS = [
    (0x06, 0x08, 0x0A, 0x0C),  # base / anim redraw
    (0x0F, 0x11, 0x13, 0x15),
    (0x18, 0x1A, 0x1C, 0x1E),
    (0x21, 0x23, 0x25, 0x27),
]
# Terminator byte offset checked before entering layer i (None = always enter)
LAYER_TERM_BEFORE = [None, 0x0E, 0x17, 0x20]


def word_le(buf: bytes, off: int) -> int:
    return buf[off] | (buf[off + 1] << 8)


def load_layout_tables(rom: bytes) -> None:
    global D421, D439, D46B
    D421 = [word_le(rom, lorom_to_file(0, 0xD421) + i * 2) for i in range(16)]
    D439 = [word_le(rom, lorom_to_file(0, 0xD439) + i * 2) for i in range(16)]
    D46B = [word_le(rom, lorom_to_file(0, 0xD46B) + i * 2) for i in range(16)]


def tile_opacity_mask(tile: bytes) -> bytes:
    """Faithful $00F200: 16-byte inverted OR of all 4 bitplanes (2 bytes/row)."""
    out = bytearray(16)
    for r in range(8):
        m = tile[r * 2] | tile[r * 2 + 1] | tile[16 + r * 2] | tile[16 + r * 2 + 1]
        m = (~m) & 0xFF
        out[r * 2] = m
        out[r * 2 + 1] = m
    return bytes(out)


def blit_overwrite(wram: bytearray, ee: int, tile: bytes) -> None:
    if ee + 32 <= len(wram):
        wram[ee : ee + 32] = tile


def blit_mask_or(wram: bytearray, ee: int, tile: bytes) -> None:
    """$F148 AND-mask then $F066 OR — composite overlay tile onto WRAM."""
    if ee + 32 > len(wram):
        return
    mask = tile_opacity_mask(tile)
    # F148: AND each dest word-byte with mask (16 words = 32 bytes, mask repeats per row pair)
    for i in range(32):
        wram[ee + i] &= mask[i % 16]
    # F066: OR source onto dest
    for i in range(32):
        wram[ee + i] |= tile[i]


def read_meta_raw(rom: bytes, index: int) -> tuple[int, int, bytes] | None:
    tbase = lorom_to_file(*TABLE_SNES)
    eoff = tbase + index * 3
    if eoff + 3 > len(rom):
        return None
    lo, hi, bk = rom[eoff : eoff + 3]
    if lo == 0 and hi == 0 and bk == 0:
        return None
    addr = (hi << 8) | lo
    moff = lorom_to_file(bk, addr)
    if moff + 0x30 > len(rom):
        return None
    return bk, moff, rom[moff : moff + 0x30]


def table_word_at(rom: bytes, bank: int, table_addr: int, byte_off: int) -> int | None:
    """Read word at table_addr + byte_off (ASM: TXY; LDA [$F1],Y — Y is byte offset)."""
    if table_addr < 0x8000:
        return None
    fo = lorom_to_file(bank, table_addr + byte_off)
    if fo + 2 > len(rom):
        return None
    return word_le(rom, fo)


def selector_byte(rom: bytes, bank: int, sel_addr: int, ae: int) -> int | None:
    if sel_addr < 0x8000:
        return None
    fo = lorom_to_file(bank, sel_addr + ae)
    if fo >= len(rom):
        return None
    return rom[fo]


def stage_script(rom: bytes, bank: int, f4: int, script_addr: int,
                 f6: int, f7: int, wram: bytearray, fe: bool) -> tuple[int, int]:
    """Run one $00F62E pass. Returns (F8, FA)."""
    if script_addr < 0x8000 or f4 < 0x8000:
        return 0, 0
    sf = lorom_to_file(bank, script_addr)
    if sf + 2 > len(rom):
        return 0, 0
    f8, fa = rom[sf], rom[sf + 1]
    if f8 == 0 or fa == 0 or f8 > 15 or fa > 32:
        return 0, 0
    if f6 > 15 or f7 > 15:
        return 0, 0

    need = ((f7 + fa) * 16 + f6 + f8) * 32
    if len(wram) < need:
        wram.extend(b"\x00" * (need - len(wram)))

    ee = D421[f7] + D439[f6]
    fc = D46B[f8]
    pos = sf + 2
    blit = blit_mask_or if fe else blit_overwrite

    for _row in range(fa):
        row_start = ee
        for _col in range(f8):
            if pos >= len(rom):
                break
            tok = rom[pos]
            pos += 1
            if tok != 0:
                tile_addr = f4 + (tok - 1) * 32
                fo = lorom_to_file(bank, tile_addr)
                if fo + 32 <= len(rom):
                    blit(wram, ee, rom[fo : fo + 32])
            ee += 0x20
        ee = row_start + f8 * 0x20 + fc + 0x20
    return f8, fa


def assemble_frame(rom: bytes, bank: int, meta: bytes, ae: int = 0) -> tuple[bytearray, dict]:
    """Compose all layers for animation frame AE into one WRAM sheet."""
    wram = bytearray(16 * 16 * 32)  # 16×16 tiles max
    info: dict = {"ae": ae, "layers": []}
    bbox = {"f6": 15, "f7": 15, "f8": 0, "fa": 0}  # track used region

    def expand_bbox(f6: int, f7: int, f8: int, fa: int) -> None:
        if f8 == 0 or fa == 0:
            return
        x0 = min(bbox["f6"], f6)
        y0 = min(bbox["f7"], f7)
        x1 = max(bbox["f6"] + bbox["f8"], f6 + f8)
        y1 = max(bbox["f7"] + bbox["fa"], f7 + fa)
        bbox["f6"], bbox["f7"] = x0, y0
        bbox["f8"], bbox["fa"] = x1 - x0, y1 - y0

    for li, (f4o, ebo, dimo, selo) in enumerate(LAYERS):
        term = LAYER_TERM_BEFORE[li]
        if term is not None and term < len(meta) and (meta[term] & 0x80):
            break

        f4_table = word_le(meta, f4o)
        eb_table = word_le(meta, ebo)
        dim_table = word_le(meta, dimo)
        sel_addr = word_le(meta, selo)

        if li == 0:
            # Always: FE=0 base from table index 0
            f4 = table_word_at(rom, bank, f4_table, 0)
            eb = table_word_at(rom, bank, eb_table, 0)
            dims = table_word_at(rom, bank, dim_table, 0)
            if f4 is None or eb is None or dims is None:
                info["layers"].append({"i": 0, "error": "bad_base"})
                return wram, info
            f6, f7 = dims & 0xFF, (dims >> 8) & 0xFF
            f8, fa = stage_script(rom, bank, f4, eb, f6, f7, wram, False)
            expand_bbox(f6, f7, f8, fa)
            info["layers"].append({
                "i": 0, "fe": False, "idx": 0,
                "f4": hex(f4), "eb": hex(eb), "f6": f6, "f7": f7, "f8": f8, "fa": fa,
            })
            # FE=1 redraw of layer 0 if selector picks a non-base index
            sel = selector_byte(rom, bank, sel_addr, ae)
            if sel is None or sel == 0 or sel == 1 or sel == 0xFF:
                continue
            frame_idx = sel
            fe = True
        else:
            sel = selector_byte(rom, bank, sel_addr, ae)
            if sel is None or sel == 1:
                info["layers"].append({"i": li, "skip": True, "sel": sel})
                continue
            if sel == 0xFF:
                break
            # sel==0 is a valid table index (ASM only skips on CMP #$01)
            frame_idx = sel
            fe = True

        f4 = table_word_at(rom, bank, f4_table, frame_idx)
        eb = table_word_at(rom, bank, eb_table, frame_idx)
        dims = table_word_at(rom, bank, dim_table, frame_idx)
        if f4 is None or eb is None or dims is None:
            info["layers"].append({"i": li, "error": "bad_table", "idx": frame_idx})
            continue
        f6, f7 = dims & 0xFF, (dims >> 8) & 0xFF
        f8, fa = stage_script(rom, bank, f4, eb, f6, f7, wram, fe)
        expand_bbox(f6, f7, f8, fa)
        info["layers"].append({
            "i": li, "fe": fe, "idx": frame_idx,
            "f4": hex(f4), "eb": hex(eb), "f6": f6, "f7": f7, "f8": f8, "fa": fa,
        })

    info["bbox"] = bbox
    return wram, info


def max_anim_frames(rom: bytes, bank: int, meta: bytes) -> int:
    """Scan selector tables for highest AE before $FF."""
    n = 1
    for _f4o, _ebo, _dimo, selo in LAYERS:
        sel_addr = word_le(meta, selo)
        if sel_addr < 0x8000:
            continue
        fo = lorom_to_file(bank, sel_addr)
        for ae in range(32):
            if fo + ae >= len(rom):
                break
            if rom[fo + ae] == 0xFF:
                n = max(n, ae)
                break
            n = max(n, ae + 1)
    return min(n, 16)


def render_bbox(wram: bytes, bbox: dict, pal: list[tuple[int, int, int]],
                scale: int) -> Image.Image:
    f6, f7, f8, fa = bbox["f6"], bbox["f7"], bbox["f8"], bbox["fa"]
    if f8 <= 0 or fa <= 0:
        return Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    img = Image.new("RGBA", (f8 * 8, fa * 8), (0, 0, 0, 0))
    px = img.load()
    for row in range(fa):
        for col in range(f8):
            off = ((f7 + row) * 16 + (f6 + col)) * 32
            if off + 32 > len(wram):
                continue
            block = wram[off : off + 32]
            if block == b"\x00" * 32:
                continue
            tile = decode_4bpp_tile(block)
            ox, oy = col * 8, row * 8
            for y in range(8):
                for x in range(8):
                    c = tile[y][x]
                    if c:
                        px[ox + x, oy + y] = (*pal[c], 255)
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--out", type=Path, default=Path("EXTRACTED/snes/analysis/monsters.json"))
    ap.add_argument("--render-dir", type=Path, default=Path("EXTRACTED/snes/gfx/monsters_assembled"))
    ap.add_argument("--max", type=int, default=128)
    ap.add_argument("--scale", type=int, default=3)
    ap.add_argument("--frames", type=int, default=0,
                    help="Max anim frames per monster (0=all detected)")
    ap.add_argument("--only", type=int, nargs="*", default=None)
    args = ap.parse_args()
    if Image is None:
        raise SystemExit("pip install pillow")

    rom = args.rom.read_bytes()
    load_layout_tables(rom)
    args.render_dir.mkdir(parents=True, exist_ok=True)

    indices = args.only if args.only is not None else range(args.max)
    records: list[dict] = []
    rendered = 0

    for i in indices:
        got = read_meta_raw(rom, i)
        if got is None:
            break
        bk, moff, meta = got
        bank = meta[0]
        pal_addr = word_le(meta, 4)
        if pal_addr < 0x8000:
            records.append({"index": i, "error": "no_pal"})
            continue
        pal_file = lorom_to_file(bank, pal_addr)
        if pal_file + 32 > len(rom):
            records.append({"index": i, "error": "pal_oob"})
            continue
        words = [struct.unpack_from("<H", rom, pal_file + j * 2)[0] for j in range(16)]
        pal = [bgr555_to_rgb(w) for w in words]

        nframes = max_anim_frames(rom, bank, meta)
        if args.frames > 0:
            nframes = min(nframes, args.frames)

        frame_imgs: list[Image.Image] = []
        frame_infos: list[dict] = []
        for ae in range(nframes):
            try:
                wram, info = assemble_frame(rom, bank, meta, ae)
            except Exception as exc:
                frame_infos.append({"ae": ae, "error": str(exc)})
                continue
            if info["bbox"]["f8"] == 0:
                continue
            img = render_bbox(wram, info["bbox"], pal, args.scale)
            frame_imgs.append(img)
            frame_infos.append(info)

        if not frame_imgs:
            records.append({"index": i, "error": "empty", "meta": hex(moff)})
            print(f"monster {i:02d}: SKIP empty")
            continue

        if len(frame_imgs) == 1:
            out = args.render_dir / f"monster_{i:02d}.png"
            frame_imgs[0].save(out)
        else:
            # Contact sheet of anim frames
            w = max(im.width for im in frame_imgs)
            h = max(im.height for im in frame_imgs)
            sheet = Image.new("RGBA", (w * len(frame_imgs), h), (0, 0, 0, 0))
            for n, im in enumerate(frame_imgs):
                sheet.paste(im, (n * w, 0))
            out = args.render_dir / f"monster_{i:02d}_frames.png"
            sheet.save(out)
            # Also save frame 0 alone
            frame_imgs[0].save(args.render_dir / f"monster_{i:02d}.png")

        rec = {
            "index": i,
            "meta_snes": f"${bk:02X}:{moff & 0xFFFF:04X}" if False else f"${bank:02X}",
            "meta_file": hex(moff),
            "bank": f"${bank:02X}",
            "frames": len(frame_imgs),
            "png": str(out).replace("\\", "/"),
            "frame0_layers": frame_infos[0].get("layers") if frame_infos else [],
        }
        records.append(rec)
        rendered += 1
        print(f"monster {i:02d}: {len(frame_imgs)} frame(s) "
              f"{frame_imgs[0].width//args.scale}x{frame_imgs[0].height//args.scale} -> {out.name}")

    payload = {
        "table": f"${TABLE_SNES[0]:02X}:{TABLE_SNES[1]:04X}",
        "method": "F62E multi-layer (FE=0 overwrite + FE=1 mask-OR) + anim selectors",
        "assembled": rendered,
        "records": records,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(payload, indent=2))
    print(f"Wrote {args.out} ({rendered} assembled)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

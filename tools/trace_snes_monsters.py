#!/usr/bin/env python3
"""Trace SNES MM2 monster/portrait gfx from the $14:8060 metadata table.

ASM path (bank $00):
  $00D803  monster index -> 3-byte ptr in $14:8060 -> metadata record
  $00D849  palette: bank=metadata[0], addr=word@+4; JSR $00E289 (16 words -> CGRAM sub $A0)
  $00F23C  portrait layout + animation via $00F62E
  $00F62E  animation script -> staged CHR in WRAM $7E:4000+

Metadata layout (confirmed on index 10 / $16:8000):
  +0   bank byte (shared by palette + nested ptrs)
  +1,+2  size/flags (bit7 of +2 => 8bpp per $00D82F)
  +4   palette addr (16-bit LE, same bank)
  +6   CHR / tile blob addr (16-bit LE, same bank)
  +8   animation script addr (16-bit LE, same bank)
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


def meta_record(rom: bytes, index: int) -> dict | None:
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
        return {"index": index, "error": "meta_oob", "snes": f"${bk:02X}:{addr:04X}"}

    raw = rom[moff : moff + 0x20]
    bank = raw[0]
    pal_addr = word_le(raw, 4)
    chr_addr = word_le(raw, 6)
    anim_addr = word_le(raw, 8)

    def snes_file(a: int) -> str | None:
        if a < 0x8000 or bank > 0x3F:
            return None
        fo = lorom_to_file(bank, a)
        return hex(fo) if fo < len(rom) else None

    return {
        "index": index,
        "table_offset": hex(eoff),
        "meta_snes": f"${bk:02X}:{addr:04X}",
        "meta_file": hex(moff),
        "bank": f"${bank:02X}",
        "palette_snes": f"${bank:02X}:{pal_addr:04X}",
        "palette_file": snes_file(pal_addr),
        "chr_snes": f"${bank:02X}:{chr_addr:04X}",
        "chr_file": snes_file(chr_addr),
        "anim_snes": f"${bank:02X}:{anim_addr:04X}",
        "anim_file": snes_file(anim_addr),
        "bpp_hint": "8" if raw[2] & 0x80 else "4",
        "header_hex": raw[:0x10].hex(),
    }


def render_chr(rom: bytes, pal_off: int, chr_off: int, size: int, out: Path,
               cols: int = 8, scale: int = 3) -> None:
    if Image is None:
        raise SystemExit("pip install pillow")
    words = [struct.unpack_from("<H", rom, pal_off + i * 2)[0] for i in range(16)]
    pal = [bgr555_to_rgb(w) for w in words]
    data = rom[chr_off : chr_off + size]
    ntiles = len(data) // 32
    rows = max(1, (ntiles + cols - 1) // cols)
    img = Image.new("RGB", (cols * 8, rows * 8), (40, 36, 48))
    px = img.load()
    for t in range(ntiles):
        tile = decode_4bpp_tile(data[t * 32 : t * 32 + 32])
        ox, oy = (t % cols) * 8, (t // cols) * 8
        for y in range(8):
            for x in range(8):
                px[ox + x, oy + y] = pal[tile[y][x]]
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--out", type=Path, default=Path("EXTRACTED/snes/analysis/monsters.json"))
    ap.add_argument("--render-dir", type=Path, default=None)
    ap.add_argument("--chr-size", type=lambda x: int(x, 0), default=0x800)
    ap.add_argument("--max", type=int, default=128)
    ap.add_argument("--scale", type=int, default=3)
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    records: list[dict] = []
    rendered = 0
    for i in range(args.max):
        rec = meta_record(rom, i)
        if rec is None:
            break
        records.append(rec)
        if not args.render_dir or not rec.get("palette_file") or not rec.get("chr_file"):
            continue
        poff = int(rec["palette_file"], 16)
        coff = int(rec["chr_file"], 16)
        if coff + 64 > len(rom):
            continue
        out = args.render_dir / f"monster_{rec['index']:02d}_{rec['bank'].replace('$','')}.png"
        try:
            render_chr(rom, poff, coff, args.chr_size, out, scale=args.scale)
            rec["png"] = str(out).replace("\\", "/")
            rendered += 1
            print(f"rendered {out.name}")
        except Exception as exc:
            rec["render_error"] = str(exc)

    payload = {
        "table": f"${TABLE_SNES[0]:02X}:{TABLE_SNES[1]:04X}",
        "count": len(records),
        "rendered": rendered,
        "records": records,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(payload, indent=2))
    print(f"Wrote {len(records)} records ({rendered} renders) -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

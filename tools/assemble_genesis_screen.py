import importlib.util, struct, sys
from pathlib import Path
from PIL import Image, ImageDraw

spec = importlib.util.spec_from_file_location("lz", "tools/genesis_lz_decompress.py")
lz = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lz)
ROM = Path("EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen")
OUT = Path("EXTRACTED/genesis/gfx/catalog/screens")
OUT.mkdir(parents=True, exist_ok=True)


def cram(word):
    r = (word >> 1) & 7; g = (word >> 5) & 7; b = (word >> 9) & 7
    s = lambda v: v * 255 // 7
    return s(r), s(g), s(b)


def palettes(rom, off):
    _, _, blob = lz.read_resource(rom, off)
    w = [struct.unpack_from(">H", blob, i)[0] for i in range(0, len(blob) & ~1, 2)]
    c = [cram(x) for x in w]
    return [c[i:i + 16] + [(0, 0, 0)] * (16 - len(c[i:i + 16])) for i in range(0, len(c), 16)]


def tile_pixels(blob, idx):
    base = idx * 32
    if base + 32 > len(blob):
        return None
    out = []
    for row in range(8):
        line = []
        for bc in range(4):
            byte = blob[base + row * 4 + bc]
            line.append(byte >> 4); line.append(byte & 0xF)
        out.append(line)
    return out


def assemble(map_blob, chr_blob, pals, width, base_tile=0, use_attr=True):
    words = [struct.unpack_from(">H", map_blob, i)[0] for i in range(0, len(map_blob) & ~1, 2)]
    n = len(words)
    height = (n + width - 1) // width
    img = Image.new("RGB", (width * 8, height * 8), (24, 24, 32))
    px = img.load()
    for i, w in enumerate(words):
        if use_attr:
            tile = w & 0x7FF
            pal_i = (w >> 13) & 3
            hflip = (w >> 11) & 1
            vflip = (w >> 12) & 1
        else:
            tile = w
            pal_i = 0; hflip = vflip = 0
        tp = tile_pixels(chr_blob, tile - base_tile)
        if tp is None:
            continue
        pal = pals[pal_i] if pal_i < len(pals) else pals[0]
        cx = (i % width) * 8; cy = (i // width) * 8
        for ry in range(8):
            for rx in range(8):
                sy = 7 - ry if vflip else ry
                sx = 7 - rx if hflip else rx
                px[cx + rx, cy + ry] = pal[tp[sy][sx]]
    return img


def main():
    rom = ROM.read_bytes()
    map_off = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x37C00
    chr_off = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x41042
    pal_off = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x9389E
    _, _, map_blob = lz.read_resource(rom, map_off)
    _, _, chr_blob = lz.read_resource(rom, chr_off)
    pals = palettes(rom, pal_off)
    n = len(map_blob) // 2
    print(f"map 0x{map_off:X} words={n} chr 0x{chr_off:X} tiles={len(chr_blob)//32}")
    panels = []
    for width in (16, 20, 22, 24, 28, 32, 40):
        img = assemble(map_blob, chr_blob, pals, width, use_attr=True)
        panels.append((f"w{width}", img))
        img2 = assemble(map_blob, chr_blob, pals, width, use_attr=False)
        panels.append((f"w{width}raw", img2))
    scale = 2
    gap = 10
    total_w = sum(p.width * scale + gap for _, p in panels) + 8
    total_h = max(p.height for _, p in panels) * scale + 16
    sheet = Image.new("RGB", (total_w, total_h), (10, 10, 14))
    d = ImageDraw.Draw(sheet)
    x = 4
    for lbl, img in panels:
        up = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        d.text((x, 2), lbl, fill=(220, 220, 120))
        sheet.paste(up, (x, 14)); x += up.width + gap
    path = OUT / f"map{map_off:06X}_chr{chr_off:06X}.png"
    sheet.save(path)
    print("->", path)


if __name__ == "__main__":
    main()

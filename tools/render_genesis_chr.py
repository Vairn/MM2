import importlib.util, json, struct
from pathlib import Path
from PIL import Image, ImageDraw

spec = importlib.util.spec_from_file_location("lz", "tools/genesis_lz_decompress.py")
lz = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lz)
ROM = Path("EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen")
CATALOG = Path("EXTRACTED/genesis/analysis/tile_catalog.json")
OUT = Path("EXTRACTED/genesis/gfx/catalog/chr")
OUT.mkdir(parents=True, exist_ok=True)

# CHR banks (dense 4bpp pixel data), taken from the classified catalog.
CHR = [e["offset"] for e in json.loads(CATALOG.read_text())["tables"]
       if e["class"] == "chr"]


def cram(word):
    r = (word >> 1) & 7; g = (word >> 5) & 7; b = (word >> 9) & 7
    s = lambda v: v * 255 // 7
    return s(r), s(g), s(b)


def pal_from(rom, off, idx):
    _, _, blob = lz.read_resource(rom, off)
    w = [struct.unpack_from(">H", blob, i)[0] for i in range(0, len(blob) & ~1, 2)]
    c = [cram(x) for x in w]
    p = c[idx * 16:idx * 16 + 16]
    return p + [(0, 0, 0)] * (16 - len(p))


def tiles(blob, pal, cols=16):
    n = len(blob) // 32
    rows = (n + cols - 1) // cols
    img = Image.new("RGB", (cols * 8, rows * 8), (40, 40, 56))
    px = img.load()
    for t in range(n):
        base = t * 32; tx = (t % cols) * 8; ty = (t // cols) * 8
        for row in range(8):
            for bc in range(4):
                byte = blob[base + row * 4 + bc]
                px[tx + bc * 2, ty + row] = pal[byte >> 4]
                px[tx + bc * 2 + 1, ty + row] = pal[byte & 0xF]
    return img


def main():
    rom = ROM.read_bytes()
    pals = [pal_from(rom, 0x9389E, i) for i in range(4)] + \
           [pal_from(rom, 0x94F40, i) for i in range(4)]
    for off in CHR:
        _, _, blob = lz.read_resource(rom, off)
        stacks = []
        for i, p in enumerate(pals):
            stacks.append(tiles(blob, p, cols=32))
        w = stacks[0].width
        gap = 3
        H = sum(s.height for s in stacks) + gap * (len(stacks) - 1)
        combo = Image.new("RGB", (w, H), (12, 12, 16))
        y = 0
        for s in stacks:
            combo.paste(s, (0, y)); y += s.height + gap
        combo = combo.resize((combo.width * 3, combo.height * 3), Image.NEAREST)
        combo.save(OUT / f"{off:06X}.png")
        print(f"0x{off:X} tiles={len(blob)//32} -> {OUT}/{off:06X}.png")


if __name__ == "__main__":
    main()

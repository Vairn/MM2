import sys
sys.path.insert(0, 'tools')
from pathlib import Path
from decode_pc_gfx import parse_wall_sheet, render_wall_frame_rgba
from PIL import Image, ImageDraw

GOG = Path(r'C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2')
CHECKER = 20

def checker_bg(w, h):
    img = Image.new('RGBA', (w, h))
    for y in range(h):
        for x in range(w):
            light = ((x // CHECKER) + (y // CHECKER)) % 2 == 0
            img.putpixel((x, y), (180,180,180,255) if light else (110,110,110,255))
    return img

def render_sheet_strip(path, bpp_label, max_frames=12):
    sheet = parse_wall_sheet(path)
    frames = sheet['frames'][:max_frames]
    if not frames:
        return None
    cell_w = max(f.width for f in frames)
    cell_h = max(f.height for f in frames)
    strip = Image.new('RGBA', (len(frames) * (cell_w + 2) + 2, cell_h + 20), (40,40,40,255))
    draw = ImageDraw.Draw(strip)
    for i, fr in enumerate(frames):
        rgba = render_wall_frame_rgba(fr.width, fr.height, fr.pixels, sheet['bpp'], frame=fr.index)
        bg = checker_bg(fr.width, fr.height)
        sp = Image.new('RGBA', (fr.width, fr.height))
        sp.putdata(rgba)
        bg.alpha_composite(sp)
        strip.paste(bg, (2 + i * (cell_w + 2), 0))
        trans = sum(1 for p in rgba if p[3] == 0)
        draw.text((2 + i * (cell_w + 2), cell_h + 2), f'{fr.index}:{trans}', fill=(200,200,200,255))
    return strip

stems = ['TOWN', 'CASTLE', 'CAVE', 'DESERT', 'SWAMP', 'TUNDRA', 'OCEAN', 'OUTB', 'OUTDOOR1', 'OUTDOOR2', 'OUTDOOR3']

rows = []
for stem in stems:
    for ext, label in [('.16', 'EGA'), ('.4', 'CGA')]:
        p = GOG / f'{stem}{ext}'
        if not p.exists():
            continue
        strip = render_sheet_strip(p, label)
        if strip is None:
            continue
        label_bar = Image.new('RGBA', (strip.width, 14), (20,20,20,255))
        ImageDraw.Draw(label_bar).text((2,1), f'{stem}{ext}', fill=(255,220,80,255))
        combined = Image.new('RGBA', (strip.width, strip.height + 14))
        combined.paste(label_bar, (0,0))
        combined.paste(strip, (0,14))
        rows.append(combined)

if rows:
    total_h = sum(r.height for r in rows) + len(rows) * 2
    max_w = max(r.width for r in rows)
    out = Image.new('RGBA', (max_w, total_h), (10,10,10,255))
    y = 0
    for r in rows:
        out.paste(r, (0, y))
        y += r.height + 2
    out.save('test_all_walls.png')
    print(f'Saved test_all_walls.png  ({out.width}x{out.height})')

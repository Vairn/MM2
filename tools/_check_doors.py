import sys
sys.path.insert(0, 'tools')
from pathlib import Path
from decode_pc_gfx import parse_wall_sheet, render_wall_frame_rgba
from PIL import Image, ImageDraw

GOG = Path(r'C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2')
CHECKER = 16

def checker(w, h):
    img = Image.new('RGBA', (w, h))
    for y in range(h):
        for x in range(w):
            light = ((x // CHECKER) + (y // CHECKER)) % 2 == 0
            img.putpixel((x, y), (180, 180, 180, 255) if light else (100, 100, 100, 255))
    return img

sheet = parse_wall_sheet(GOG / 'CASTLE.16')
# wall frame 0 vs door frame 16, side 4 vs door 20
pairs = [(0, 16), (4, 20)]
row_h = max(sheet['frames'][0].height, sheet['frames'][16].height)
out = Image.new('RGBA', (800, row_h * 2 + 40), (30, 30, 30, 255))
draw = ImageDraw.Draw(out)

for row, (wall_fi, door_fi) in enumerate(pairs):
    for col, fi in enumerate([wall_fi, door_fi]):
        fr = sheet['frames'][fi]
        rgba = render_wall_frame_rgba(fr.width, fr.height, fr.pixels, 4, frame=fi)
        bg = checker(fr.width, fr.height)
        sp = Image.new('RGBA', (fr.width, fr.height))
        sp.putdata(rgba)
        bg.alpha_composite(sp)
        trans = sum(1 for p in rgba if p[3] == 0)
        x = 10 + col * 400
        y = 10 + row * (row_h + 20)
        out.paste(bg, (x, y))
        label = f'frame {fi} trans={trans}'
        draw.text((x, y + fr.height + 2), label, fill=(220, 220, 220, 255))

out.save('test_door_vs_wall.png')
print('saved test_door_vs_wall.png')

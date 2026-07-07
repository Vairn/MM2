import sys
sys.path.insert(0, 'tools')
from pathlib import Path
from decode_pc_gfx import parse_wall_sheet, decode_wall_frame_indices
from PIL import Image

gog = Path(r'C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2')

for sheet_name in ['TOWN.16', 'CASTLE.16', 'CAVE.16']:
    p = gog / sheet_name
    if not p.exists():
        continue
    sheet = parse_wall_sheet(p)
    print(f'\n{sheet_name}:')
    for fi in range(min(4, len(sheet['frames']))):
        fr = sheet['frames'][fi]
        idxs = decode_wall_frame_indices(fr.width, fr.height, fr.pixels, 4)
        total = len(idxs)
        idx0 = idxs.count(0)
        idx8 = idxs.count(8)
        # check index-8 in center vs edge
        center8 = sum(
            1 for y in range(fr.height) for x in range(fr.width)
            if idxs[y * fr.width + x] == 8
            and (fr.width // 4 < x < 3 * fr.width // 4)
            and (fr.height // 4 < y < 3 * fr.height // 4)
        )
        edge8 = idx8 - center8
        print(f'  frame {fi}: {fr.width}x{fr.height}  idx0={idx0}  idx8={idx8} (center={center8} edge={edge8})')

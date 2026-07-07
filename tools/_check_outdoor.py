import sys
sys.path.insert(0, 'tools')
from pathlib import Path
from decode_pc_gfx import parse_wall_sheet, decode_wall_frame_indices, render_wall_frame_rgba, render_overlay_frame_rgba

gog = Path(r'C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2')

for stem in ['OUTDOOR1', 'DESERT', 'OUTB', 'OUTF', 'SKY']:
    p = gog / f'{stem}.16'
    if not p.exists():
        continue
    sheet = parse_wall_sheet(p)
    n = len(sheet['frames'])
    print(f'\n{stem}.16: {n} frames')
    for fi in range(min(8, n)):
        fr = sheet['frames'][fi]
        idxs = decode_wall_frame_indices(fr.width, fr.height, fr.pixels, 4)
        rgba_wall = render_wall_frame_rgba(fr.width, fr.height, fr.pixels, 4, frame=fi, outdoor=True)
        trans_wall = sum(1 for p in rgba_wall if p[3] == 0)
        rgba_ov = render_overlay_frame_rgba(fr.width, fr.height, fr.pixels, 4)
        trans_ov = sum(1 for p in rgba_ov if p[3] == 0)
        print(f'  f{fi} {fr.width}x{fr.height}: idx0={idxs.count(0):4d} idx8={idxs.count(8):4d}  wall_trans={trans_wall}  overlay_trans={trans_ov}')

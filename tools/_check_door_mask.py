import sys
sys.path.insert(0, 'tools')
from pathlib import Path
from decode_pc_gfx import (
    parse_wall_sheet,
    decode_wall_frame_indices,
    render_wall_frame_rgba,
    render_overlay_frame_rgba,
)

gog = Path(r'C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2')

for stem in ['TOWN', 'CASTLE']:
    ega = parse_wall_sheet(gog / f'{stem}.16')
    cga = parse_wall_sheet(gog / f'{stem}.4')
    for fi in [4, 20, 16]:
        if fi >= len(ega['frames']):
            continue
        efr = ega['frames'][fi]
        cfr = cga['frames'][fi]
        eidx = decode_wall_frame_indices(efr.width, efr.height, efr.pixels, 4)
        cidx = decode_wall_frame_indices(cfr.width, cfr.height, cfr.pixels, 2)
        wall = render_wall_frame_rgba(efr.width, efr.height, efr.pixels, 4, frame=fi)
        ov = render_overlay_frame_rgba(
            efr.width, efr.height, efr.pixels, 4
        )
        wt = sum(1 for p in wall if p[3] == 0)
        ot = sum(1 for p in ov if p[3] == 0)
        # pixels where wall keys but overlay keeps opaque (door detail saved by overlay)
        saved = 0
        for i, (w, o) in enumerate(zip(wall, ov)):
            if w[3] == 0 and o[3] == 255:
                saved += 1
        print(f'{stem} EGA f{fi}: wall_trans={wt} overlay_trans={ot} overlay_saves={saved}')
        from collections import Counter
        ec = Counter(eidx)
        cc = Counter(cidx)
        print(f'  EGA indices: {dict(sorted(ec.items()))}')
        print(f'  CGA indices: {dict(sorted(cc.items()))}')

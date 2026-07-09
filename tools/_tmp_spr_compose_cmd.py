python -c @"
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import importlib.util
import json
spec = importlib.util.spec_from_file_location('lz', 'tools/genesis_lz_decompress.py')
lz = importlib.util.module_from_spec(spec); spec.loader.exec_module(lz)
rom = Path(r'EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen').read_bytes()
OUT=Path('EXTRACTED/genesis/gfx/catalog/monsters')
ATLAS=Path('EXTRACTED/genesis/gfx/catalog/monsters/atlases')
ATLAS.mkdir(parents=True, exist_ok=True)
PLANE_W=121

def cram(w):
    r=(w>>1)&7; g=(w>>5)&7; b=(w>>9)&7
    s=lambda v:v*255//7
    return s(r),s(g),s(b)

def parse_11a():
    a=0x33D4A
    ents={}
    while True:
        eid=struct.unpack_from('>H',rom,a)[0]
        if eid==0: break
        sz1=struct.unpack_from('>I',rom,a+2)[0]
        sec2=a+sz1+0xA
        sz2=struct.unpack_from('>I',rom,sec2)[0]
        pal=sec2+sz2+8
        nxt=sec2+sz2+0x28
        ents[eid]={'base':a,'sec2':sec2,'pal':pal}
        a=nxt
    return ents

def parse_913():
    a=0x913B4
    ents={}
    while True:
        eid=struct.unpack_from('>H',rom,a)[0]
        if eid==0xFFFF: break
        w2,w3,w4=struct.unpack_from('>HHH',rom,a+2)
        skip=w4*2+8
        ents[eid]=dict(w2=w2,w3=w3,w4=w4)
        a+=skip
    return ents

def blit_cm(stream, tw, th, chr_data, pal, ntiles):
    img=Image.new('RGBA',(tw*8,th*8),(0,0,0,0)); px=img.load()
    for i,w in enumerate(stream[:tw*th]):
        idx=w&0x7FF
        if not idx: continue
        ti=idx-1
        if ti<0 or ti>=ntiles: continue
        flip_h=bool(w&0x0800); flip_v=bool(w&0x1000)
        tile=chr_data[ti*32:(ti+1)*32]
        x,y=i//th, i%th
        for yy in range(8):
            for xx in range(8):
                bi=yy*4+xx//2
                n=(tile[bi]>>4) if (xx&1)==0 else (tile[bi]&0xF)
                if not n: continue
                dx=7-xx if flip_h else xx
                dy=7-yy if flip_v else yy
                px[x*8+dx,y*8+dy]=(*pal[n],255)
    return img

ents=parse_11a(); m913=parse_913()
catalog=[]

# Rip all ids with: th=plane_rows, tw=meta.w2, CM packing, frames=plane rows
for eid in sorted(ents.keys()):
    if eid not in m913: continue
    tw=m913[eid]['w2']
    if tw<=0: continue
    e=ents[eid]
    chr_data=lz.decompress_lz(rom[e['base']+2:])
    layout=lz.decompress_lz(rom[e['sec2']:])
    pal=[cram(struct.unpack_from('>H',rom,e['pal']+i)[0]) for i in range(0,32,2)]
    ntiles=len(chr_data)//32
    words=[struct.unpack_from('>H',layout,i)[0] for i in range(0,len(layout)&~1,2)]
    if len(words)%PLANE_W: continue
    rows=len(words)//PLANE_W
    th=rows
    cell=tw*th
    if cell<=0 or cell>PLANE_W: 
        # can't pack even one sprite per row
        continue
    nspr=PLANE_W//cell
    # Build frame0 contact of all sprites in row 0
    sprites=[]
    for s in range(nspr):
        stream=words[s*cell:(s+1)*cell]
        nz=sum(1 for w in stream if w&0x7FF)
        if nz < cell*0.25: continue
        sprites.append(blit_cm(stream, tw, th, chr_data, pal, ntiles))
    if not sprites: continue
    # anim sheet for sprite 0 across frames
    frames=[]
    for r in range(rows):
        stream=words[r*PLANE_W:r*PLANE_W+cell]
        frames.append(blit_cm(stream, tw, th, chr_data, pal, ntiles))
    # save
    contact=Image.new('RGBA',(tw*8*len(sprites), th*8),(0,0,0,0))
    for i,sp in enumerate(sprites):
        contact.paste(sp,(i*tw*8,0))
    contact.save(OUT/f'spr_{eid:03d}_row0.png')
    anim=Image.new('RGBA',(tw*8*len(frames), th*8),(0,0,0,0))
    for i,f in enumerate(frames):
        anim.paste(f,(i*tw*8,0))
    anim.save(OUT/f'spr_{eid:03d}_anim.png')
    frames[0].resize((tw*8*4, th*8*4), Image.NEAREST).save(OUT/f'spr_{eid:03d}_f0_x4.png')
    catalog.append({
        'id':eid,'tw':tw,'th':th,'tiles':ntiles,'sprites_row0':len(sprites),
        'frames':rows,'base':hex(e['base']),'w3':m913[eid]['w3']
    })
    print(f'id={eid:3d} {tw}x{th} sprites={len(sprites)} frames={rows} tiles={ntiles}')

# Atlas of f0 for ids 1-60
pics=[c for c in catalog if 1<=c['id']<=60]
# normalize cell size for atlas - use max
if pics:
    max_w=max(c['tw'] for c in pics)*8
    max_h=max(c['th'] for c in pics)*8
    cols=10
    rows_a=(len(pics)+cols-1)//cols
    atlas=Image.new('RGB',(cols*(max_w+8), rows_a*(max_h+16)),(16,16,16))
    draw=ImageDraw.Draw(atlas)
    for i,c in enumerate(pics):
        img=Image.open(OUT/f'spr_{c[\"id\"]:03d}_anim.png')
        # first frame
        f0=img.crop((0,0,c['tw']*8,c['th']*8))
        x=(i%cols)*(max_w+8)+4
        y=(i//cols)*(max_h+16)+12
        atlas.paste(f0,(x,y),f0)
        draw.text((x,y-10), str(c['id']), fill=(255,255,0))
    atlas.save(ATLAS/'atlas_monsters_f0.png')
    print('atlas', atlas.size)

Path('EXTRACTED/genesis/analysis/monster_catalog.json').write_text(json.dumps(catalog, indent=2))
print('catalog', len(catalog))
"@
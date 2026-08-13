import struct, sys

def dump_items():
    d = open('items.dat','rb').read()
    print('ITEMS: len', len(d), 'expect 5120 ->', len(d)==5120)
    for i in [0,1,2,3,250,255]:
        r = d[i*20:(i+1)*20]
        name = r[:12]
        gold = struct.unpack('<H', r[18:20])[0]
        print(f'  rec {i}: name={name!r} sep={r[12]:#04x} cls={r[13]:#04x} bonus={r[14]:#04x} use={r[15]:#04x} dmg={r[16]:#04x} pad={r[17]:#04x} gold={gold}')

def dump_monsters():
    d = open('monsters.dat','rb').read()
    print('MONSTERS: len', len(d))
    for recsize in [26, 32, 24, 208]:
        if len(d) % recsize == 0:
            print(f'  divides by recsize {recsize}: {len(d)//recsize} records')
    print('  first 64 bytes:', d[:64].hex(' '))

def dump_roster():
    d = open('roster.dat','rb').read()
    print('ROSTER: len', len(d))
    for recsize in [64, 128, 52, 104, 208]:
        if len(d) % recsize == 0:
            print(f'  divides by recsize {recsize}: {len(d)//recsize} records')
    print('  first 96 bytes:', d[:96].hex(' '))

def dump_spells():
    d = open('spells.dat','rb').read()
    print('SPELLS: len', len(d))
    print('  bytes:', d.hex(' '))

def dump_attrib():
    d = open('attrib.dat','rb').read()
    print('ATTRIB: len', len(d))
    for recsize in [24, 32, 16, 3840//120, 3840//160]:
        if recsize and len(d) % recsize == 0:
            print(f'  divides by recsize {recsize}: {len(d)//recsize} records')
    print('  first 64 bytes:', d[:64].hex(' '))

def dump_str():
    d = open('str.dat','rb').read()
    print('STR: len', len(d))
    print('  first 128 bytes:', d[:128].hex(' '))
    print('  as ascii:', d[:128])

def dump_map():
    d = open('map.dat','rb').read()
    print('MAP: len', len(d), 'expect 30720 ->', len(d)==30720)
    # screen 0 visual page first row, collision page first row
    print('  screen0 visual row0:', d[0:16].hex(' '))
    print('  screen0 collision row0:', d[256:256+16].hex(' '))
    # screen 4 (Middlegate?)
    print('  screen4 visual row0:', d[4*512:4*512+16].hex(' '))

if __name__ == '__main__':
    dump_items(); print()
    dump_monsters(); print()
    dump_roster(); print()
    dump_spells(); print()
    dump_attrib(); print()
    dump_str(); print()
    dump_map()

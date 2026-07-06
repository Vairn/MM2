#!/usr/bin/env python3
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from decode_pc_gfx import lzw_decompress

GOG = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")
b = (GOG / "MONSTERS.4").read_bytes()

dec = lzw_decompress(b[3138:], struct.unpack_from("<I", b, 3134)[0])
fo = 101
frame = dec[fo : fo + 1541]
print("frame0 len", len(frame))
for i in range(0, 32, 2):
    le = struct.unpack_from("<HH", frame, i)
    be = struct.unpack_from(">HH", frame, i)
    print(f"  @{i:02x} LE {le} BE {be}")

t = (GOG / "THROW.4").read_bytes()
td = lzw_decompress(t[4:], struct.unpack_from("<I", t, 0)[0])
print("THROW dec", len(td), "head", td[:24].hex())

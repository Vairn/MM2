from pathlib import Path
import struct

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()

# Any instruction with extension word 0024 (disp $24)
writes = []
idx = 0
while True:
    i = code.find(b"\x00\x24", idx)
    if i < 0:
        break
    if i >= 2:
        op = struct.unpack_from(">H", code, i - 2)[0]
        # write-like: move.b Dx, $24(An); add; sub; clr; ori; andi; etc
        hi = (op >> 12) & 0xF
        # move.b ea, $24(an): 
        print(f"{i-2:06x} op={op:04x} {code[i-2:i+2].hex()}")
    idx = i + 2
    if len(writes) > 200:
        break

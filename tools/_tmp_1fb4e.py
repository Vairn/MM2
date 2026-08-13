from pathlib import Path
import struct

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()
verbs = [
    "sprays poison", "sprays acid", "casts a curse", "breathes fire",
    "breathes lightning", "breathes cold", "breathes energy", "breathes gas",
    "breathes acid", "explodes", "gazes", "drains magic", "drains spell level",
    "vaporizes valuables", "juggles party", "energy blast", "sleep",
    "lightning bolts", "fireballs", "fingers of death", "disintegrate",
    "super shock", "dancing sword", "incinerate", "invokes power", "implosion",
    "inferno", "pain", "silence", "frenzies",
]
# 1FD90: subq.l #2,d0 then table — index = pabil-2
# Confirm 1FD90 bytes
print("1fd90", code[0x1fd90:0x1fda4].hex())
table = 0x1FD56
pc_jmp = 0x1FDA2
known = {
    0x1FC1E: "addq -79A5 curse",
    0x1FC32: "1F912 explode self",
    0x1FC3A: "1F72E cond|$82",
    0x1FC48: "1F94E clr SP all",
    0x1FC50: "1F988 dec spell-lvl",
    0x1FC58: "1F9CC",
    0x1FC60: "1FA3E",
    0x1FC68: "1F6DC(6)+1F632",
    0x1FC7A: "1F72E cond|$10",
    0x1FC88: "1F6DC(6)+1F414",
    0x1FC9A: "1F6DC(6)+1F414",
    0x1FCAC: "1F896 cond|$81",
    0x1FCBA: "1F896 cond|$FF",
    0x1FCC8: "1F6DC($28)+1F632",
    0x1FCDA: "1F6DC($C)+1F414",
    0x1FCEC: "1F6DC($32)+1F632",
    0x1FCFE: "dmg=1000+1F632",
    0x1FD0C: "1F6DC($14)+1F414",
    0x1FD1E: "rng1..15+1 +1F632",
    0x1FD38: "1F72E cond|$02",
    0x1FD44: "1FAFA",
    0x1FD4A: "1F72E cond|$20",
}
print("pabil -> leaf (index=pabil-2):")
for pabil in range(2, 32):
    idx = pabil - 2
    w = struct.unpack_from(">H", code, table + idx * 2)[0]
    name = verbs[pabil] if pabil < len(verbs) else "?"
    if w == 2:
        print(f"pabil {pabil:2d} {name:20s} FAIL/nop")
        continue
    off = struct.unpack(">h", struct.pack(">H", w))[0]
    tgt = pc_jmp + off
    note = known.get(tgt, "")
    print(f"pabil {pabil:2d} {name:20s} -> {tgt:#06x} {note}")

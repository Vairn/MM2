data = open(r'c:\_20260421_\D-REC\development\MM2\EXTRACTED\mm2_data_00.bin','rb').read()
# kCartoTile at A4-0x765E => offset = 0x7FFE - 0x765E = 0x09A0
off = 0x7FFE - 0x765E
print(f'kCartoTile at offset {hex(off)}')
ct = [data[off+i] for i in range(64)]
print(ct)

# Page 0 visual byte layout: index = byte>>2 (6-bit, 0..63)
# The 2-bit fields: but WAIT - are the masks confirmed?
# From CLAUDE.md: N=bits7-6 (0xC0), E=bits5-4 (0x30), S=bits3-2 (0x0C), W=bits1-0 (0x03)
# That means byte>>2 only gives the upper 6 bits (NESW without W low bit),
# but actually byte>>2 = full 8-bit >> 2 = 6 bits: bits 7..2 = N|E|S bits + W high bit.
# kCartoTile index = byte>>2 = (N<<4)|(E<<2)|S|W_hi ... hmm that doesn't map cleanly

# Actually 64 entries means 6-bit index: bits 7-2 of the visual byte
# So W low bit is ignored in the automap lookup
print("\nkCartoTile[idx] for idx=0..63:")
for i in range(64):
    n = (i>>4) & 0x3
    e = (i>>2) & 0x3
    s = i & 0x3  # This is only bits 3-2 shifted... wait
    # Actually if index = byte>>2, and byte = N[7:6]|E[5:4]|S[3:2]|W[1:0]
    # byte>>2 = N[7:6]<<4|E[5:4]<<2|S[3:2]  -- NO, that's wrong
    # byte>>2 shifts everything right 2: so bits7=N_hi, 6=N_lo, 5=E_hi, 4=E_lo, 3=S_hi, 2=S_lo become bits5..0
    # So idx = (N<<4)|(E<<2)|S ... yes that's right
    n2 = (i>>4)&3
    e2 = (i>>2)&3
    s2 = i&3
    print(f"  [{i:2d}] N={n2} E={e2} S={s2} → cartoTile={ct[i]}")

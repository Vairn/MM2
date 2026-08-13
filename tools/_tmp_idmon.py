#!/usr/bin/env python3
"""Disassemble Identify Monster leaf @ 0xB788 and dump string literals."""
import importlib.util
import struct

spec = importlib.util.spec_from_file_location(
    "p", r"c:\_20260421_\D-REC\development\MM2\tools\_trace_spell_picker.py"
)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
mem = mod.build_full_code_image()

s = 0xB788
end = s + 0x200
pc = s
while pc < end:
    op = struct.unpack_from(">H", mem, pc)[0]
    if op == 0x4E5D:
        print(f"{hex(pc)} unlk")
        if pc + 2 < end and struct.unpack_from(">H", mem, pc + 2)[0] == 0x4E75:
            print(f"{hex(pc+2)} rts")
        break
    if op == 0x4E55:
        print(f"{hex(pc)} link #{struct.unpack_from('>h', mem, pc+2)[0]}")
        pc += 4
        continue
    if op == 0x4EBA:
        d = struct.unpack_from(">h", mem, pc + 2)[0]
        tgt = (pc + 2 + d) & 0xFFFFFFFF
        print(f"{hex(pc)} jsr {hex(tgt)}")
        pc += 4
        continue
    if op == 0x4EAC:
        d = struct.unpack_from(">h", mem, pc + 2)[0]
        print(f"{hex(pc)} jsr A4{d:#x}")
        pc += 4
        continue
    if op == 0x3F3C:
        print(f"{hex(pc)} push.w #{struct.unpack_from('>H', mem, pc+2)[0]:#x}")
        pc += 4
        continue
    if op == 0x4267:
        print(f"{hex(pc)} push.w #0")
        pc += 2
        continue
    if op == 0x487A:
        d = struct.unpack_from(">h", mem, pc + 2)[0]
        str_pc = (pc + 2 + d) & 0xFFFFFFFF
        # read C string
        i = str_pc
        chars = []
        while i < len(mem) and mem[i] != 0 and len(chars) < 80:
            chars.append(chr(mem[i]) if 32 <= mem[i] < 127 else f"\\x{mem[i]:02x}")
            i += 1
        print(f"{hex(pc)} pea {hex(str_pc)} \"{''.join(chars)}\"")
        pc += 4
        continue
    if op == 0x0C2D:
        imm = mem[pc + 2]
        off = struct.unpack_from(">h", mem, pc + 4)[0]
        print(f"{hex(pc)} cmpi.b #{imm:#x}, A5{off:#x}")
        pc += 6
        continue
    if op == 0x1B40:
        off = struct.unpack_from(">h", mem, pc + 2)[0]
        print(f"{hex(pc)} move.b d0, A5{off:#x}")
        pc += 4
        continue
    if (op & 0xFF00) == 0x6700:
        if op & 0xFF:
            d = op & 0xFF
            if d >= 0x80:
                d -= 0x100
            print(f"{hex(pc)} beq {hex(pc+2+d)}")
            pc += 2
        else:
            d = struct.unpack_from(">h", mem, pc + 2)[0]
            print(f"{hex(pc)} beq.w {hex(pc+2+d)}")
            pc += 4
        continue
    if op == 0x4A2C:
        off = struct.unpack_from(">h", mem, pc + 2)[0]
        print(f"{hex(pc)} tst.b A4{off:#x}")
        pc += 4
        continue
    if op == 0x197C:
        imm = mem[pc + 2]
        off = struct.unpack_from(">h", mem, pc + 4)[0]
        print(f"{hex(pc)} move.b #{imm:#x}, A4{off:#x}")
        pc += 6
        continue
    print(f"{hex(pc)} ? {op:04x}")
    pc += 2

# Also dump strings near end of leaf
print("\n--- strings near B900-B960 ---")
for off in range(0xB900, 0xB960):
    if mem[off:off+4] == b"Hit " or mem[off:off+2] == b"HP" or mem[off] in (ord("Y"), ord("N")):
        pass
# find printable runs after leaf
for base in range(0xB8E0, 0xB9B0):
    if 32 <= mem[base] < 127 and mem[base + 1] >= 32:
        run = []
        i = base
        while i < base + 40 and mem[i] != 0:
            if 32 <= mem[i] < 127:
                run.append(chr(mem[i]))
            else:
                break
            i += 1
        if len(run) >= 3:
            print(hex(base), "".join(run))

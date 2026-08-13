from pathlib import Path

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()

for pat, name in [
    (b"\xd1\x28\x00\x24", "add.b d0,$24"),
    (b"\x52\x28\x00\x24", "addq.b #1,$24"),
    (b"\xd0\x28\x00\x24", "add.b $24,d0"),
    (b"\x10\x28\x00\x24", "move.b $24,d0"),
]:
    idx = 0
    while True:
        i = code.find(pat, idx)
        if i < 0 or i > 0x20000:
            break
        if 0xA000 <= i <= 0xC800:
            for a in range(i, max(0, i - 0xA0), -2):
                if code[a : a + 2] == b"\x4e\x55":
                    print(f"{name} @{i:#x} func {a:#x}")
                    break
        idx = i + 2

# Search addi.b #imm, $24(a0) = 0628 iiii 0024
idx = 0
while True:
    i = code.find(b"\x06\x28", idx)
    if i < 0 or i > 0x20000:
        break
    if code[i + 4 : i + 6] == b"\x00\x24" and 0xA000 <= i <= 0xC800:
        imm = code[i + 3]
        for a in range(i, max(0, i - 0xA0), -2):
            if code[a : a + 2] == b"\x4e\x55":
                print(f"addi.b #{imm:#x},$24 @{i:#x} func {a:#x}")
                break
    idx = i + 2

# Maybe invis sets GS buff read during AC calc - search addq to -0x799C area unused
print("--- move.b #1 near BD74 ---")
for i in range(0xBD00, 0xBE00):
    if code[i : i + 2] == b"\x19\x7c" and code[i + 3] == 1:
        disp = int.from_bytes(code[i + 4 : i + 6], "big", signed=True)
        print(f"{i:#x} move.b #1, {disp:#x}")

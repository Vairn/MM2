from pathlib import Path

code = Path("EXTRACTED/hunks/mm2_code_00.bin").read_bytes()

# All addi/addq/add to $24 in whole code hunk
for pat, name in [
    (b"\xd1\x28\x00\x24", "add.b d0,$24"),
    (b"\x52\x28\x00\x24", "addq.b #1,$24"),
    (b"\x54\x28\x00\x24", "addq.b #2,$24"),
    (b"\x56\x28\x00\x24", "addq.b #3,$24"),
    (b"\x5c\x28\x00\x24", "addq.b #6,$24"),
]:
    idx = 0
    while True:
        i = code.find(pat, idx)
        if i < 0:
            break
        for a in range(i, max(0, i - 0xC0), -2):
            if code[a : a + 2] == b"\x4e\x55":
                print(f"{name} @{i:#x} func {a:#x}")
                break
        else:
            print(f"{name} @{i:#x}")
        idx = i + 2

idx = 0
while True:
    i = code.find(b"\x06\x28", idx)
    if i < 0:
        break
    if i + 6 <= len(code) and code[i + 4 : i + 6] == b"\x00\x24":
        imm = code[i + 3]
        for a in range(i, max(0, i - 0xC0), -2):
            if code[a : a + 2] == b"\x4e\x55":
                print(f"addi.b #{imm:#x},$24 @{i:#x} func {a:#x}")
                break
    idx = i + 2

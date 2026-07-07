#!/usr/bin/env python3
"""Minimal 6502 disassembler for MM2 Apple/C64 boot RE (not a full emulator).

Usage:
  python tools/disasm_6502.py EXTRACTED/apple2/asm/disk_a_t0s0.bin --org 0x800
"""
from __future__ import annotations

import argparse
from pathlib import Path

# NMOS 6502 — enough for boot tracing; undocumented ops shown as .byte
_OPS: dict[int, tuple[str, int, str]] = {}
# (mnemonic, size, mode)
_RAW = [
    (0x00, "BRK", 1, "imp"),
    (0x01, "ORA", 2, "indx"),
    (0x05, "ORA", 2, "zp"),
    (0x06, "ASL", 2, "zp"),
    (0x08, "PHP", 1, "imp"),
    (0x09, "ORA", 2, "imm"),
    (0x0A, "ASL", 1, "acc"),
    (0x0D, "ORA", 3, "abs"),
    (0x10, "BPL", 2, "rel"),
    (0x18, "CLC", 1, "imp"),
    (0x20, "JSR", 3, "abs"),
    (0x21, "AND", 2, "indx"),
    (0x24, "BIT", 2, "zp"),
    (0x25, "AND", 2, "zp"),
    (0x29, "AND", 2, "imm"),
    (0x2C, "BIT", 3, "abs"),
    (0x30, "BMI", 2, "rel"),
    (0x38, "SEC", 1, "imp"),
    (0x4A, "LSR", 1, "acc"),
    (0x4C, "JMP", 3, "abs"),
    (0x4D, "EOR", 3, "abs"),
    (0x50, "BVC", 2, "rel"),
    (0x60, "RTS", 1, "imp"),
    (0x65, "ADC", 2, "zp"),
    (0x68, "PLA", 1, "imp"),
    (0x69, "ADC", 2, "imm"),
    (0x6C, "JMP", 3, "ind"),
    (0x6D, "ADC", 3, "abs"),
    (0x70, "BVS", 2, "rel"),
    (0x84, "STY", 2, "zp"),
    (0x85, "STA", 2, "zp"),
    (0x86, "STX", 2, "zp"),
    (0x88, "DEY", 1, "imp"),
    (0x8A, "TXA", 1, "imp"),
    (0x8C, "STY", 3, "abs"),
    (0x8D, "STA", 3, "abs"),
    (0x8E, "STX", 3, "abs"),
    (0x90, "BCC", 2, "rel"),
    (0x91, "STA", 2, "indy"),
    (0x94, "STY", 2, "zpx"),
    (0x95, "STA", 2, "zpx"),
    (0x99, "STA", 3, "absy"),
    (0x9A, "TXS", 1, "imp"),
    (0xA0, "LDY", 2, "imm"),
    (0xA2, "LDX", 2, "imm"),
    (0xA4, "LDY", 2, "zp"),
    (0xA5, "LDA", 2, "zp"),
    (0xA6, "LDX", 2, "zp"),
    (0xA8, "TAY", 1, "imp"),
    (0xA9, "LDA", 2, "imm"),
    (0xAA, "TAX", 1, "imp"),
    (0xAC, "LDY", 3, "abs"),
    (0xAD, "LDA", 3, "abs"),
    (0xAE, "LDX", 3, "abs"),
    (0xB0, "BCS", 2, "rel"),
    (0xB1, "LDA", 2, "indy"),
    (0xB5, "LDA", 2, "zpx"),
    (0xB9, "LDA", 3, "absy"),
    (0xBA, "TSX", 1, "imp"),
    (0xBD, "LDA", 3, "absx"),
    (0xC0, "CPY", 2, "imm"),
    (0xC5, "CMP", 2, "zp"),
    (0xC6, "DEC", 2, "zp"),
    (0xC8, "INY", 1, "imp"),
    (0xC9, "CMP", 2, "imm"),
    (0xCA, "DEX", 1, "imp"),
    (0xCD, "CMP", 3, "abs"),
    (0xCE, "DEC", 3, "abs"),
    (0xD0, "BNE", 2, "rel"),
    (0xD8, "CLD", 1, "imp"),
    (0xE0, "CPX", 2, "imm"),
    (0xE5, "SBC", 2, "zp"),
    (0xE6, "INC", 2, "zp"),
    (0xE8, "INX", 1, "imp"),
    (0xEA, "NOP", 1, "imp"),
    (0xEE, "INC", 3, "abs"),
    (0xF0, "BEQ", 2, "rel"),
]
for op, mn, sz, _mode in _RAW:
    _OPS[op] = (mn, sz, _mode)


def disasm(data: bytes, org: int = 0, max_insns: int = 500) -> list[str]:
    lines: list[str] = []
    i = 0
    n = 0
    while i < len(data) and n < max_insns:
        addr = org + i
        op = data[i]
        if op not in _OPS:
            lines.append(f"{addr:04X}:  .byte ${op:02X}")
            i += 1
            n += 1
            continue
        mn, sz, mode = _OPS[op]
        if i + sz > len(data):
            lines.append(f"{addr:04X}:  .byte ${op:02X}        ; truncated")
            break
        operands = data[i + 1 : i + sz]
        if mode == "imp":
            opnd = ""
        elif mode == "acc":
            opnd = "A"
        elif mode == "imm":
            opnd = f"#${operands[0]:02X}"
        elif mode == "zp":
            opnd = f"${operands[0]:02X}"
        elif mode == "zpx":
            opnd = f"${operands[0]:02X},X"
        elif mode == "abs":
            opnd = f"${operands[1]:02X}{operands[0]:02X}"
        elif mode == "absx":
            opnd = f"${operands[1]:02X}{operands[0]:02X},X"
        elif mode == "absy":
            opnd = f"${operands[1]:02X}{operands[0]:02X},Y"
        elif mode == "indx":
            opnd = f"(${operands[0]:02X},X)"
        elif mode == "indy":
            opnd = f"(${operands[0]:02X}),Y"
        elif mode == "ind":
            opnd = f"(${operands[1]:02X}{operands[0]:02X})"
        elif mode == "rel":
            rel = operands[0] if operands[0] < 128 else operands[0] - 256
            target = addr + 2 + rel
            opnd = f"${target:04X}"
        else:
            opnd = operands.hex()
        bytes_s = " ".join(f"{b:02X}" for b in data[i : i + sz])
        lines.append(f"{addr:04X}:  {bytes_s:<12} {mn} {opnd}".rstrip())
        i += sz
        n += 1
    return lines


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("bin", type=Path)
    ap.add_argument("--org", type=lambda x: int(x, 0), default=0x800)
    ap.add_argument("-o", "--out", type=Path)
    ap.add_argument("--limit", type=int, default=500)
    args = ap.parse_args()

    data = args.bin.read_bytes()
    lines = disasm(data, args.org, args.limit)
    text = "\n".join(lines) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"Wrote {args.out}")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()

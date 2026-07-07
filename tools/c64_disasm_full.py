#!/usr/bin/env python3
"""Complete NMOS 6502 disassembler with flow tracing for MM2 C64 RE.

Unlike tools/disasm_6502.py (partial table) this covers all 151 documented
opcodes and can do a recursive flow trace from an entry so we distinguish
real code from data. Undocumented opcodes render as .byte.

Usage:
  python tools/c64_disasm_full.py IMAGE --base 0x0800 --entry 0x080B [--trace]
"""
from __future__ import annotations

import argparse
from pathlib import Path

# mode sizes
SZ = {"imp": 1, "acc": 1, "imm": 2, "zp": 2, "zpx": 2, "zpy": 2,
      "izx": 2, "izy": 2, "abs": 3, "abx": 3, "aby": 3, "ind": 3, "rel": 2}

# opcode -> (mnemonic, mode)
T = {
    0x00: ("BRK", "imp"), 0x01: ("ORA", "izx"), 0x05: ("ORA", "zp"), 0x06: ("ASL", "zp"),
    0x08: ("PHP", "imp"), 0x09: ("ORA", "imm"), 0x0A: ("ASL", "acc"), 0x0D: ("ORA", "abs"),
    0x0E: ("ASL", "abs"), 0x10: ("BPL", "rel"), 0x11: ("ORA", "izy"), 0x15: ("ORA", "zpx"),
    0x16: ("ASL", "zpx"), 0x18: ("CLC", "imp"), 0x19: ("ORA", "aby"), 0x1D: ("ORA", "abx"),
    0x1E: ("ASL", "abx"), 0x20: ("JSR", "abs"), 0x21: ("AND", "izx"), 0x24: ("BIT", "zp"),
    0x25: ("AND", "zp"), 0x26: ("ROL", "zp"), 0x28: ("PLP", "imp"), 0x29: ("AND", "imm"),
    0x2A: ("ROL", "acc"), 0x2C: ("BIT", "abs"), 0x2D: ("AND", "abs"), 0x2E: ("ROL", "abs"),
    0x30: ("BMI", "rel"), 0x31: ("AND", "izy"), 0x35: ("AND", "zpx"), 0x36: ("ROL", "zpx"),
    0x38: ("SEC", "imp"), 0x39: ("AND", "aby"), 0x3D: ("AND", "abx"), 0x3E: ("ROL", "abx"),
    0x40: ("RTI", "imp"), 0x41: ("EOR", "izx"), 0x45: ("EOR", "zp"), 0x46: ("LSR", "zp"),
    0x48: ("PHA", "imp"), 0x49: ("EOR", "imm"), 0x4A: ("LSR", "acc"), 0x4C: ("JMP", "abs"),
    0x4D: ("EOR", "abs"), 0x4E: ("LSR", "abs"), 0x50: ("BVC", "rel"), 0x51: ("EOR", "izy"),
    0x55: ("EOR", "zpx"), 0x56: ("LSR", "zpx"), 0x58: ("CLI", "imp"), 0x59: ("EOR", "aby"),
    0x5D: ("EOR", "abx"), 0x5E: ("LSR", "abx"), 0x60: ("RTS", "imp"), 0x61: ("ADC", "izx"),
    0x65: ("ADC", "zp"), 0x66: ("ROR", "zp"), 0x68: ("PLA", "imp"), 0x69: ("ADC", "imm"),
    0x6A: ("ROR", "acc"), 0x6C: ("JMP", "ind"), 0x6D: ("ADC", "abs"), 0x6E: ("ROR", "abs"),
    0x70: ("BVS", "rel"), 0x71: ("ADC", "izy"), 0x75: ("ADC", "zpx"), 0x76: ("ROR", "zpx"),
    0x78: ("SEI", "imp"), 0x79: ("ADC", "aby"), 0x7D: ("ADC", "abx"), 0x7E: ("ROR", "abx"),
    0x81: ("STA", "izx"), 0x84: ("STY", "zp"), 0x85: ("STA", "zp"), 0x86: ("STX", "zp"),
    0x88: ("DEY", "imp"), 0x8A: ("TXA", "imp"), 0x8C: ("STY", "abs"), 0x8D: ("STA", "abs"),
    0x8E: ("STX", "abs"), 0x90: ("BCC", "rel"), 0x91: ("STA", "izy"), 0x94: ("STY", "zpx"),
    0x95: ("STA", "zpx"), 0x96: ("STX", "zpy"), 0x98: ("TYA", "imp"), 0x99: ("STA", "aby"),
    0x9A: ("TXS", "imp"), 0x9D: ("STA", "abx"), 0xA0: ("LDY", "imm"), 0xA1: ("LDA", "izx"),
    0xA2: ("LDX", "imm"), 0xA4: ("LDY", "zp"), 0xA5: ("LDA", "zp"), 0xA6: ("LDX", "zp"),
    0xA8: ("TAY", "imp"), 0xA9: ("LDA", "imm"), 0xAA: ("TAX", "imp"), 0xAC: ("LDY", "abs"),
    0xAD: ("LDA", "abs"), 0xAE: ("LDX", "abs"), 0xB0: ("BCS", "rel"), 0xB1: ("LDA", "izy"),
    0xB4: ("LDY", "zpx"), 0xB5: ("LDA", "zpx"), 0xB6: ("LDX", "zpy"), 0xB8: ("CLV", "imp"),
    0xB9: ("LDA", "aby"), 0xBA: ("TSX", "imp"), 0xBC: ("LDY", "abx"), 0xBD: ("LDA", "abx"),
    0xBE: ("LDX", "aby"), 0xC0: ("CPY", "imm"), 0xC1: ("CMP", "izx"), 0xC4: ("CPY", "zp"),
    0xC5: ("CMP", "zp"), 0xC6: ("DEC", "zp"), 0xC8: ("INY", "imp"), 0xC9: ("CMP", "imm"),
    0xCA: ("DEX", "imp"), 0xCC: ("CPY", "abs"), 0xCD: ("CMP", "abs"), 0xCE: ("DEC", "abs"),
    0xD0: ("BNE", "rel"), 0xD1: ("CMP", "izy"), 0xD5: ("CMP", "zpx"), 0xD6: ("DEC", "zpx"),
    0xD8: ("CLD", "imp"), 0xD9: ("CMP", "aby"), 0xDD: ("CMP", "abx"), 0xDE: ("DEC", "abx"),
    0xE0: ("CPX", "imm"), 0xE1: ("SBC", "izx"), 0xE4: ("CPX", "zp"), 0xE5: ("SBC", "zp"),
    0xE6: ("INC", "zp"), 0xE8: ("INX", "imp"), 0xE9: ("SBC", "imm"), 0xEA: ("NOP", "imp"),
    0xEC: ("CPX", "abs"), 0xED: ("SBC", "abs"), 0xEE: ("INC", "abs"), 0xF0: ("BEQ", "rel"),
    0xF1: ("SBC", "izy"), 0xF5: ("SBC", "zpx"), 0xF6: ("INC", "zpx"), 0xF8: ("SED", "imp"),
    0xF9: ("SBC", "aby"), 0xFD: ("SBC", "abx"), 0xFE: ("INC", "abx"),
}

BRANCH = {"BPL", "BMI", "BVC", "BVS", "BCC", "BCS", "BNE", "BEQ"}
STOP = {"RTS", "RTI", "JMP", "BRK"}  # end of a linear run when tracing


def fmt_operand(mode: str, b: bytes, addr: int) -> tuple[str, int | None]:
    """Return (text, target-address-if-branch/jump-else-None)."""
    if mode == "imp":
        return "", None
    if mode == "acc":
        return "A", None
    if mode == "imm":
        return f"#${b[1]:02X}", None
    if mode == "zp":
        return f"${b[1]:02X}", None
    if mode == "zpx":
        return f"${b[1]:02X},X", None
    if mode == "zpy":
        return f"${b[1]:02X},Y", None
    if mode == "izx":
        return f"(${b[1]:02X},X)", None
    if mode == "izy":
        return f"(${b[1]:02X}),Y", None
    tgt = b[1] | (b[2] << 8)
    if mode == "abs":
        return f"${tgt:04X}", tgt
    if mode == "abx":
        return f"${tgt:04X},X", None
    if mode == "aby":
        return f"${tgt:04X},Y", None
    if mode == "ind":
        return f"(${tgt:04X})", None
    if mode == "rel":
        off = b[1] if b[1] < 128 else b[1] - 256
        t = (addr + 2 + off) & 0xFFFF
        return f"${t:04X}", t
    return b[1:].hex(), None


def disasm_range(data: bytes, base: int, start: int, end: int) -> list[str]:
    lines = []
    i = start - base
    while i < end - base and i < len(data):
        addr = base + i
        op = data[i]
        info = T.get(op)
        if info is None:
            lines.append(f"{addr:04X}: {op:02X}          .byte ${op:02X}")
            i += 1
            continue
        mn, mode = info
        sz = SZ[mode]
        chunk = data[i : i + sz]
        if len(chunk) < sz:
            lines.append(f"{addr:04X}: {chunk.hex():<10} .byte (trunc)")
            break
        text, _ = fmt_operand(mode, chunk + b"\0\0", addr)
        raw = " ".join(f"{x:02X}" for x in chunk)
        lines.append(f"{addr:04X}: {raw:<10} {mn} {text}".rstrip())
        i += sz
    return lines


def flow_trace(data: bytes, base: int, entry: int, calls_are_stops=False):
    """Recursive descent: follow branches/JSR/JMP, mark reachable insns."""
    n = len(data)
    code = set()  # addresses that begin an instruction
    seen = set()
    stack = [entry]
    jsr_targets = set()
    while stack:
        pc = stack.pop()
        while True:
            off = pc - base
            if off < 0 or off >= n or pc in seen:
                break
            seen.add(pc)
            op = data[off]
            info = T.get(op)
            if info is None:
                break  # data / undocumented -> stop this run
            mn, mode = info
            sz = SZ[mode]
            if off + sz > n:
                break
            code.add(pc)
            chunk = data[off : off + sz]
            _, tgt = fmt_operand(mode, chunk + b"\0\0", pc)
            if mn in BRANCH and tgt is not None:
                stack.append(tgt)
                pc = pc + sz
                continue
            if mn == "JSR" and tgt is not None:
                jsr_targets.add(tgt)
                if base <= tgt < base + n:
                    stack.append(tgt)
                pc = pc + sz
                continue
            if mn == "JMP":
                if mode == "abs" and tgt is not None and base <= tgt < base + n:
                    stack.append(tgt)
                break  # unconditional -> end run
            if mn in ("RTS", "RTI", "BRK"):
                break
            pc = pc + sz
    return code, jsr_targets


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("image", type=Path)
    ap.add_argument("--base", type=lambda x: int(x, 0), default=0x0800)
    ap.add_argument("--entry", type=lambda x: int(x, 0), default=None)
    ap.add_argument("--start", type=lambda x: int(x, 0), default=None)
    ap.add_argument("--end", type=lambda x: int(x, 0), default=None)
    ap.add_argument("--trace", action="store_true")
    ap.add_argument("-o", "--out", type=Path)
    args = ap.parse_args()

    data = args.image.read_bytes()
    base = args.base
    if args.trace:
        entry = args.entry if args.entry is not None else base
        code, jsr = flow_trace(data, base, entry)
        out = [f"; flow trace from ${entry:04X}: {len(code)} reachable insns, "
               f"{len(jsr)} JSR targets"]
        for pc in sorted(code):
            out.extend(disasm_range(data, base, pc, pc + 3)[:1])
        out.append("")
        out.append("; JSR targets: " + " ".join(f"${t:04X}" for t in sorted(jsr)))
        text = "\n".join(out) + "\n"
    else:
        start = args.start if args.start is not None else base
        end = args.end if args.end is not None else base + len(data)
        text = "\n".join(disasm_range(data, base, start, end)) + "\n"

    if args.out:
        args.out.write_text(text, encoding="utf-8")
        print(f"wrote {args.out}")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()

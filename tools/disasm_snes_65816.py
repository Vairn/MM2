#!/usr/bin/env python3
"""65816 disassembler for SNES MM2 gfx path tracing.

Full 256-opcode table with M/X flag width tracking (REP/SEP aware) so that
immediate and accumulator/index-sized operands decode correctly. Not a full
emulator - control flow is linear from the start offset, but width flags are
propagated so LZ/RLE codec routines disassemble cleanly.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Key SNES I/O names for annotation
SNES_IO = {
    0x2100: "INIDISP",
    0x2105: "BGMODE",
    0x2107: "BG1SC",
    0x2108: "BG2SC",
    0x2109: "BG3SC",
    0x210A: "BG4SC",
    0x2115: "VMAIN",
    0x2116: "VMADDL",
    0x2117: "VMADDH",
    0x2118: "VMDATAL",
    0x2119: "VMDATAH",
    0x2121: "CGADD",
    0x2122: "CGDATA",
    0x212C: "TM",
    0x4200: "NMITIMEN",
    0x4212: "HVBJOY",
    0x420B: "MDMAEN",
    0x420C: "HDMAEN",
    0x4300: "DMAP0",
    0x4301: "BBAD0",
    0x4302: "A1T0L",
    0x4303: "A1T0H",
    0x4304: "A1B0",
    0x4305: "DAS0L",
    0x4306: "DAS0H",
}

# Addressing modes and their fixed operand byte counts (excluding opcode).
# Variable-width modes (imm_m / imm_x) are resolved from M/X flags at decode.
_MODE_LEN = {
    "imp": 0,
    "acc": 0,
    "imm8": 1,
    "imm_m": None,  # 1 if M else 2
    "imm_x": None,  # 1 if X else 2
    "dp": 1,
    "dpx": 1,
    "dpy": 1,
    "idp": 1,
    "idpx": 1,
    "idpy": 1,
    "idl": 1,
    "idly": 1,
    "sr": 1,
    "sriy": 1,
    "rel": 1,
    "rell": 2,
    "abs": 2,
    "absx": 2,
    "absy": 2,
    "ind": 2,
    "indx": 2,
    "indl": 2,
    "absl": 3,
    "abslx": 3,
    "move": 2,
}

# opcode -> (mnemonic, mode)
_OPS: dict[int, tuple[str, str]] = {
    0x00: ("BRK", "imm8"), 0x01: ("ORA", "idpx"), 0x02: ("COP", "imm8"), 0x03: ("ORA", "sr"),
    0x04: ("TSB", "dp"), 0x05: ("ORA", "dp"), 0x06: ("ASL", "dp"), 0x07: ("ORA", "idl"),
    0x08: ("PHP", "imp"), 0x09: ("ORA", "imm_m"), 0x0A: ("ASL", "acc"), 0x0B: ("PHD", "imp"),
    0x0C: ("TSB", "abs"), 0x0D: ("ORA", "abs"), 0x0E: ("ASL", "abs"), 0x0F: ("ORA", "absl"),
    0x10: ("BPL", "rel"), 0x11: ("ORA", "idpy"), 0x12: ("ORA", "idp"), 0x13: ("ORA", "sriy"),
    0x14: ("TRB", "dp"), 0x15: ("ORA", "dpx"), 0x16: ("ASL", "dpx"), 0x17: ("ORA", "idly"),
    0x18: ("CLC", "imp"), 0x19: ("ORA", "absy"), 0x1A: ("INC", "acc"), 0x1B: ("TCS", "imp"),
    0x1C: ("TRB", "abs"), 0x1D: ("ORA", "absx"), 0x1E: ("ASL", "absx"), 0x1F: ("ORA", "abslx"),
    0x20: ("JSR", "abs"), 0x21: ("AND", "idpx"), 0x22: ("JSL", "absl"), 0x23: ("AND", "sr"),
    0x24: ("BIT", "dp"), 0x25: ("AND", "dp"), 0x26: ("ROL", "dp"), 0x27: ("AND", "idl"),
    0x28: ("PLP", "imp"), 0x29: ("AND", "imm_m"), 0x2A: ("ROL", "acc"), 0x2B: ("PLD", "imp"),
    0x2C: ("BIT", "abs"), 0x2D: ("AND", "abs"), 0x2E: ("ROL", "abs"), 0x2F: ("AND", "absl"),
    0x30: ("BMI", "rel"), 0x31: ("AND", "idpy"), 0x32: ("AND", "idp"), 0x33: ("AND", "sriy"),
    0x34: ("BIT", "dpx"), 0x35: ("AND", "dpx"), 0x36: ("ROL", "dpx"), 0x37: ("AND", "idly"),
    0x38: ("SEC", "imp"), 0x39: ("AND", "absy"), 0x3A: ("DEC", "acc"), 0x3B: ("TSC", "imp"),
    0x3C: ("BIT", "absx"), 0x3D: ("AND", "absx"), 0x3E: ("ROL", "absx"), 0x3F: ("AND", "abslx"),
    0x40: ("RTI", "imp"), 0x41: ("EOR", "idpx"), 0x42: ("WDM", "imm8"), 0x43: ("EOR", "sr"),
    0x44: ("MVP", "move"), 0x45: ("EOR", "dp"), 0x46: ("LSR", "dp"), 0x47: ("EOR", "idl"),
    0x48: ("PHA", "imp"), 0x49: ("EOR", "imm_m"), 0x4A: ("LSR", "acc"), 0x4B: ("PHK", "imp"),
    0x4C: ("JMP", "abs"), 0x4D: ("EOR", "abs"), 0x4E: ("LSR", "abs"), 0x4F: ("EOR", "absl"),
    0x50: ("BVC", "rel"), 0x51: ("EOR", "idpy"), 0x52: ("EOR", "idp"), 0x53: ("EOR", "sriy"),
    0x54: ("MVN", "move"), 0x55: ("EOR", "dpx"), 0x56: ("LSR", "dpx"), 0x57: ("EOR", "idly"),
    0x58: ("CLI", "imp"), 0x59: ("EOR", "absy"), 0x5A: ("PHY", "imp"), 0x5B: ("TCD", "imp"),
    0x5C: ("JML", "absl"), 0x5D: ("EOR", "absx"), 0x5E: ("LSR", "absx"), 0x5F: ("EOR", "abslx"),
    0x60: ("RTS", "imp"), 0x61: ("ADC", "idpx"), 0x62: ("PER", "rell"), 0x63: ("ADC", "sr"),
    0x64: ("STZ", "dp"), 0x65: ("ADC", "dp"), 0x66: ("ROR", "dp"), 0x67: ("ADC", "idl"),
    0x68: ("PLA", "imp"), 0x69: ("ADC", "imm_m"), 0x6A: ("ROR", "acc"), 0x6B: ("RTL", "imp"),
    0x6C: ("JMP", "ind"), 0x6D: ("ADC", "abs"), 0x6E: ("ROR", "abs"), 0x6F: ("ADC", "absl"),
    0x70: ("BVS", "rel"), 0x71: ("ADC", "idpy"), 0x72: ("ADC", "idp"), 0x73: ("ADC", "sriy"),
    0x74: ("STZ", "dpx"), 0x75: ("ADC", "dpx"), 0x76: ("ROR", "dpx"), 0x77: ("ADC", "idly"),
    0x78: ("SEI", "imp"), 0x79: ("ADC", "absy"), 0x7A: ("PLY", "imp"), 0x7B: ("TDC", "imp"),
    0x7C: ("JMP", "indx"), 0x7D: ("ADC", "absx"), 0x7E: ("ROR", "absx"), 0x7F: ("ADC", "abslx"),
    0x80: ("BRA", "rel"), 0x81: ("STA", "idpx"), 0x82: ("BRL", "rell"), 0x83: ("STA", "sr"),
    0x84: ("STY", "dp"), 0x85: ("STA", "dp"), 0x86: ("STX", "dp"), 0x87: ("STA", "idl"),
    0x88: ("DEY", "imp"), 0x89: ("BIT", "imm_m"), 0x8A: ("TXA", "imp"), 0x8B: ("PHB", "imp"),
    0x8C: ("STY", "abs"), 0x8D: ("STA", "abs"), 0x8E: ("STX", "abs"), 0x8F: ("STA", "absl"),
    0x90: ("BCC", "rel"), 0x91: ("STA", "idpy"), 0x92: ("STA", "idp"), 0x93: ("STA", "sriy"),
    0x94: ("STY", "dpx"), 0x95: ("STA", "dpx"), 0x96: ("STX", "dpy"), 0x97: ("STA", "idly"),
    0x98: ("TYA", "imp"), 0x99: ("STA", "absy"), 0x9A: ("TXS", "imp"), 0x9B: ("TXY", "imp"),
    0x9C: ("STZ", "abs"), 0x9D: ("STA", "absx"), 0x9E: ("STZ", "absx"), 0x9F: ("STA", "abslx"),
    0xA0: ("LDY", "imm_x"), 0xA1: ("LDA", "idpx"), 0xA2: ("LDX", "imm_x"), 0xA3: ("LDA", "sr"),
    0xA4: ("LDY", "dp"), 0xA5: ("LDA", "dp"), 0xA6: ("LDX", "dp"), 0xA7: ("LDA", "idl"),
    0xA8: ("TAY", "imp"), 0xA9: ("LDA", "imm_m"), 0xAA: ("TAX", "imp"), 0xAB: ("PLB", "imp"),
    0xAC: ("LDY", "abs"), 0xAD: ("LDA", "abs"), 0xAE: ("LDX", "abs"), 0xAF: ("LDA", "absl"),
    0xB0: ("BCS", "rel"), 0xB1: ("LDA", "idpy"), 0xB2: ("LDA", "idp"), 0xB3: ("LDA", "sriy"),
    0xB4: ("LDY", "dpx"), 0xB5: ("LDA", "dpx"), 0xB6: ("LDX", "dpy"), 0xB7: ("LDA", "idly"),
    0xB8: ("CLV", "imp"), 0xB9: ("LDA", "absy"), 0xBA: ("TSX", "imp"), 0xBB: ("TYX", "imp"),
    0xBC: ("LDY", "absx"), 0xBD: ("LDA", "absx"), 0xBE: ("LDX", "absy"), 0xBF: ("LDA", "abslx"),
    0xC0: ("CPY", "imm_x"), 0xC1: ("CMP", "idpx"), 0xC2: ("REP", "imm8"), 0xC3: ("CMP", "sr"),
    0xC4: ("CPY", "dp"), 0xC5: ("CMP", "dp"), 0xC6: ("DEC", "dp"), 0xC7: ("CMP", "idl"),
    0xC8: ("INY", "imp"), 0xC9: ("CMP", "imm_m"), 0xCA: ("DEX", "imp"), 0xCB: ("WAI", "imp"),
    0xCC: ("CPY", "abs"), 0xCD: ("CMP", "abs"), 0xCE: ("DEC", "abs"), 0xCF: ("CMP", "absl"),
    0xD0: ("BNE", "rel"), 0xD1: ("CMP", "idpy"), 0xD2: ("CMP", "idp"), 0xD3: ("CMP", "sriy"),
    0xD4: ("PEI", "idp"), 0xD5: ("CMP", "dpx"), 0xD6: ("DEC", "dpx"), 0xD7: ("CMP", "idly"),
    0xD8: ("CLD", "imp"), 0xD9: ("CMP", "absy"), 0xDA: ("PHX", "imp"), 0xDB: ("STP", "imp"),
    0xDC: ("JML", "indl"), 0xDD: ("CMP", "absx"), 0xDE: ("DEC", "absx"), 0xDF: ("CMP", "abslx"),
    0xE0: ("CPX", "imm_x"), 0xE1: ("SBC", "idpx"), 0xE2: ("SEP", "imm8"), 0xE3: ("SBC", "sr"),
    0xE4: ("CPX", "dp"), 0xE5: ("SBC", "dp"), 0xE6: ("INC", "dp"), 0xE7: ("SBC", "idl"),
    0xE8: ("INX", "imp"), 0xE9: ("SBC", "imm_m"), 0xEA: ("NOP", "imp"), 0xEB: ("XBA", "imp"),
    0xEC: ("CPX", "abs"), 0xED: ("SBC", "abs"), 0xEE: ("INC", "abs"), 0xEF: ("SBC", "absl"),
    0xF0: ("BEQ", "rel"), 0xF1: ("SBC", "idpy"), 0xF2: ("SBC", "idp"), 0xF3: ("SBC", "sriy"),
    0xF4: ("PEA", "abs"), 0xF5: ("SBC", "dpx"), 0xF6: ("INC", "dpx"), 0xF7: ("SBC", "idly"),
    0xF8: ("SED", "imp"), 0xF9: ("SBC", "absy"), 0xFA: ("PLX", "imp"), 0xFB: ("XCE", "imp"),
    0xFC: ("JSR", "indx"), 0xFD: ("SBC", "absx"), 0xFE: ("INC", "absx"), 0xFF: ("SBC", "abslx"),
}


def file_to_pc(off: int) -> int:
    bank = off >> 15
    addr = 0x8000 + (off & 0x7FFF)
    return (bank << 16) | addr


def pc_to_file(pc: int) -> int:
    bank = (pc >> 16) & 0x7F
    addr = pc & 0xFFFF
    return (bank << 15) | (addr & 0x7FFF)


def annotate_addr(addr: int) -> str:
    name = SNES_IO.get(addr)
    return f"${addr:04X}" + (f" ; {name}" if name else "")


def _operand_len(mode: str, m_flag: int, x_flag: int) -> int:
    if mode == "imm_m":
        return 1 if m_flag else 2
    if mode == "imm_x":
        return 1 if x_flag else 2
    return _MODE_LEN[mode]


def _format_operand(mode: str, ob: bytes, addr: int, size: int) -> str:
    if mode in ("imp", "acc"):
        return "A" if mode == "acc" else ""
    if mode == "imm8":
        return f"#${ob[0]:02X}"
    if mode in ("imm_m", "imm_x"):
        if len(ob) == 2:
            return f"#${ob[0] | (ob[1] << 8):04X}"
        return f"#${ob[0]:02X}"
    if mode == "dp":
        return f"${ob[0]:02X}"
    if mode == "dpx":
        return f"${ob[0]:02X},X"
    if mode == "dpy":
        return f"${ob[0]:02X},Y"
    if mode == "idp":
        return f"(${ob[0]:02X})"
    if mode == "idpx":
        return f"(${ob[0]:02X},X)"
    if mode == "idpy":
        return f"(${ob[0]:02X}),Y"
    if mode == "idl":
        return f"[${ob[0]:02X}]"
    if mode == "idly":
        return f"[${ob[0]:02X}],Y"
    if mode == "sr":
        return f"${ob[0]:02X},S"
    if mode == "sriy":
        return f"(${ob[0]:02X},S),Y"
    if mode == "rel":
        rel = ob[0] if ob[0] < 128 else ob[0] - 256
        return f"${addr + size + rel:06X}"
    if mode == "rell":
        v = ob[0] | (ob[1] << 8)
        rel = v if v < 0x8000 else v - 0x10000
        return f"${addr + size + rel:06X}"
    if mode == "abs":
        return annotate_addr(ob[0] | (ob[1] << 8))
    if mode == "absx":
        return f"{annotate_addr(ob[0] | (ob[1] << 8))},X"
    if mode == "absy":
        return f"{annotate_addr(ob[0] | (ob[1] << 8))},Y"
    if mode == "ind":
        return f"(${ob[0] | (ob[1] << 8):04X})"
    if mode == "indx":
        return f"(${ob[0] | (ob[1] << 8):04X},X)"
    if mode == "indl":
        return f"[${ob[0] | (ob[1] << 8):04X}]"
    if mode == "absl":
        return f"${ob[0] | (ob[1] << 8) | (ob[2] << 16):06X}"
    if mode == "abslx":
        return f"${ob[0] | (ob[1] << 8) | (ob[2] << 16):06X},X"
    if mode == "move":
        return f"#${ob[0]:02X},#${ob[1]:02X}"
    return ob.hex()


def disasm(data: bytes, org: int = 0, max_insns: int = 400, m_flag: int = 1, x_flag: int = 1) -> list[str]:
    """Linear disassembly with REP/SEP width tracking.

    m_flag/x_flag: 1 = 8-bit, 0 = 16-bit (initial state before first REP/SEP).
    """
    lines: list[str] = []
    i = 0
    n = 0
    while i < len(data) and n < max_insns:
        addr = org + i
        op = data[i]
        mn, mode = _OPS[op]
        olen = _operand_len(mode, m_flag, x_flag)
        size = 1 + olen
        if i + size > len(data):
            lines.append(f"{addr:06X}:  .byte ${op:02X}        ; truncated")
            break
        ob = data[i + 1 : i + size]
        opnd = _format_operand(mode, ob, addr, size)
        # Width tracking via REP/SEP immediate.
        if mn == "REP":
            if ob[0] & 0x20:
                m_flag = 0
            if ob[0] & 0x10:
                x_flag = 0
        elif mn == "SEP":
            if ob[0] & 0x20:
                m_flag = 1
            if ob[0] & 0x10:
                x_flag = 1
        flag_note = ""
        if mn in ("REP", "SEP"):
            flag_note = f"   ; M={m_flag} X={x_flag}"
        bytes_s = " ".join(f"{b:02X}" for b in data[i : i + size])
        text = f"{addr:06X}:  {bytes_s:<14} {mn} {opnd}".rstrip() + flag_note
        lines.append(text)
        i += size
        n += 1
    return lines


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("--offset", type=lambda x: int(x, 0), required=True, help="File offset")
    ap.add_argument("--org", type=lambda x: int(x, 0), default=None, help="SNES PC (default: LoROM from offset)")
    ap.add_argument("--bytes", type=lambda x: int(x, 0), default=0x120)
    ap.add_argument("--insns", type=lambda x: int(x, 0), default=400)
    ap.add_argument("--m", type=int, default=1, choices=(0, 1), help="Initial M flag (1=8-bit)")
    ap.add_argument("--x", type=int, default=1, choices=(0, 1), help="Initial X flag (1=8-bit)")
    ap.add_argument("-o", "--out", type=Path)
    args = ap.parse_args()

    data = args.rom.read_bytes()
    off = args.offset
    org = args.org if args.org is not None else file_to_pc(off)
    chunk = data[off : off + args.bytes]
    lines = disasm(chunk, org, max_insns=args.insns, m_flag=args.m, x_flag=args.x)
    text = "\n".join(lines) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"Wrote {args.out}", file=sys.stderr)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())

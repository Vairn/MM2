#!/usr/bin/env python3
"""Minimal 6502 interpreter for MM2 C64 boot decompression trace.

Not cycle-accurate; enough for $03B7-$03FF and $0800-$09FF RAM paths.
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass
class Cpu:
    a: int = 0
    x: int = 0
    y: int = 0
    sp: int = 0xFD
    pc: int = 0
    p: int = 0x20  # U set
    mem: bytearray | None = None
    steps: int = 0
    stopped: bool = False

    def flag_n(self) -> bool:
        return (self.a & 0x80) != 0

    def flag_z(self) -> bool:
        return (self.a & 0xFF) == 0

    def flag_c(self) -> bool:
        return (self.p & 0x01) != 0

    def set_nz(self, val: int) -> None:
        val &= 0xFF
        self.p &= ~0xC0
        if val & 0x80:
            self.p |= 0x80
        if val == 0:
            self.p |= 0x02

    def push(self, v: int) -> None:
        assert self.mem is not None
        self.mem[0x0100 + self.sp] = v & 0xFF
        self.sp = (self.sp - 1) & 0xFF

    def pull(self) -> int:
        assert self.mem is not None
        self.sp = (self.sp + 1) & 0xFF
        return self.mem[0x0100 + self.sp]

    def read16(self, lo: int) -> int:
        assert self.mem is not None
        return self.mem[lo] | (self.mem[(lo + 1) & 0xFFFF] << 8)

    def read(self, addr: int) -> int:
        assert self.mem is not None
        return self.mem[addr & 0xFFFF]

    def write(self, addr: int, val: int) -> None:
        assert self.mem is not None
        self.mem[addr & 0xFFFF] = val & 0xFF


def step(cpu: Cpu) -> None:
    mem = cpu.mem
    assert mem is not None
    op = mem[cpu.pc]
    pc = cpu.pc

    def imm() -> int:
        return (pc + 1) & 0xFFFF

    def zp() -> int:
        return mem[(pc + 1) & 0xFFFF]

    def abs_() -> int:
        return mem[(pc + 1) & 0xFFFF] | (mem[(pc + 2) & 0xFFFF] << 8)

    def rel() -> int:
        off = mem[(pc + 1) & 0xFFFF]
        if off & 0x80:
            off -= 0x100
        return (pc + 2 + off) & 0xFFFF

    def branch(cond: bool) -> None:
        if cond:
            cpu.pc = rel()
        else:
            cpu.pc = (pc + 2) & 0xFFFF

    if op == 0x00:  # BRK
        cpu.stopped = True
        cpu.pc = (pc + 1) & 0xFFFF
    elif op == 0x4C:  # JMP abs
        cpu.pc = abs_()
    elif op == 0x6C:  # JMP (ind)
        ptr = abs_()
        cpu.pc = mem[ptr] | (mem[(ptr + 1) & 0xFFFF] << 8)
    elif op == 0x20:  # JSR
        ret = (pc + 2) & 0xFFFF
        cpu.push(ret >> 8)
        cpu.push(ret & 0xFF)
        cpu.pc = abs_()
    elif op == 0x60:  # RTS
        lo = cpu.pull()
        hi = cpu.pull()
        cpu.pc = (lo | (hi << 8) + 1) & 0xFFFF
    elif op == 0x78:  # SEI
        cpu.p |= 0x04
        cpu.pc = (pc + 1) & 0xFFFF
    elif op == 0x58:  # CLI
        cpu.p &= ~0x04
        cpu.pc = (pc + 1) & 0xFFFF
    elif op == 0xA2:  # LDX imm
        cpu.x = mem[imm()]
        cpu.set_nz(cpu.x)
        cpu.pc = (pc + 2) & 0xFFFF
    elif op == 0xA0:  # LDY imm
        cpu.y = mem[imm()]
        cpu.set_nz(cpu.y)
        cpu.pc = (pc + 2) & 0xFFFF
    elif op == 0x29:  # AND imm
        cpu.a = cpu.a & mem[imm()]
        cpu.set_nz(cpu.a)
        cpu.pc = (pc + 2) & 0xFFFF
    elif op == 0xA9:  # LDA imm
        cpu.a = mem[imm()]
        cpu.set_nz(cpu.a)
        cpu.pc = (pc + 2) & 0xFFFF
    elif op == 0xAD:  # LDA abs
        cpu.a = mem[abs_()]
        cpu.set_nz(cpu.a)
        cpu.pc = (pc + 3) & 0xFFFF
    elif op == 0xBD:  # LDA abs,x
        addr = (abs_() + cpu.x) & 0xFFFF
        cpu.a = mem[addr]
        cpu.set_nz(cpu.a)
        cpu.pc = (pc + 3) & 0xFFFF
    elif op == 0xB9:  # LDA abs,y
        addr = (abs_() + cpu.y) & 0xFFFF
        cpu.a = mem[addr]
        cpu.set_nz(cpu.a)
        cpu.pc = (pc + 3) & 0xFFFF
    elif op == 0x2C:  # BIT abs
        val = mem[abs_()]
        cpu.p = (cpu.p & ~0xC0) | (val & 0xC0)
        cpu.p = (cpu.p & ~0x02) | (0x02 if (cpu.a & val) == 0 else 0)
        cpu.pc = (pc + 3) & 0xFFFF
    elif op == 0xEA:  # NOP
        cpu.pc = (pc + 1) & 0xFFFF
    elif op == 0xE6:  # INC zp
        addr = zp()
        mem[addr] = (mem[addr] + 1) & 0xFF
        cpu.set_nz(mem[addr])
        cpu.pc = (pc + 2) & 0xFFFF
    elif op == 0xB5:  # LDA zpx
        cpu.a = mem[(zp() + cpu.x) & 0xFF]
        cpu.set_nz(cpu.a)
        cpu.pc = (pc + 2) & 0xFFFF
    elif op == 0x85:  # STA zp
        mem[zp()] = cpu.a & 0xFF
        cpu.pc = (pc + 2) & 0xFFFF
    elif op == 0x8D:  # STA abs
        mem[abs_()] = cpu.a & 0xFF
        cpu.pc = (pc + 3) & 0xFFFF
    elif op == 0x9D:  # STA abs,x
        addr = (abs_() + cpu.x) & 0xFFFF
        mem[addr] = cpu.a & 0xFF
        cpu.pc = (pc + 3) & 0xFFFF
    elif op == 0x99:  # STA abs,y
        addr = (abs_() + cpu.y) & 0xFFFF
        mem[addr] = cpu.a & 0xFF
        cpu.pc = (pc + 3) & 0xFFFF
    elif op == 0xE8:  # INX
        cpu.x = (cpu.x + 1) & 0xFF
        cpu.set_nz(cpu.x)
        cpu.pc = (pc + 1) & 0xFFFF
    elif op == 0xC8:  # INY
        cpu.y = (cpu.y + 1) & 0xFF
        cpu.set_nz(cpu.y)
        cpu.pc = (pc + 1) & 0xFFFF
    elif op == 0xEE:  # INC abs
        addr = abs_()
        mem[addr] = (mem[addr] + 1) & 0xFF
        cpu.set_nz(mem[addr])
        cpu.pc = (pc + 3) & 0xFFFF
    elif op == 0xC9:  # CMP imm
        t = (cpu.a - mem[imm()]) & 0xFF
        cpu.p = (cpu.p & ~0x03) | (0x01 if cpu.a >= mem[imm()] else 0)
        cpu.set_nz(t)
        cpu.pc = (pc + 2) & 0xFFFF
    elif op == 0xD0:  # BNE
        branch(not cpu.flag_z())
    elif op == 0xF0:  # BEQ
        branch(cpu.flag_z())
    elif op == 0x10:  # BPL
        branch(not cpu.flag_n())
    elif op == 0x30:  # BMI
        branch(cpu.flag_n())
    elif op == 0x90:  # BCC
        branch(not cpu.flag_c())
    elif op == 0xB0:  # BCS
        branch(cpu.flag_c())
    else:
        raise NotImplementedError(f"unsupported opcode ${op:02X} at ${pc:04X}")

    cpu.steps += 1


def run_until(cpu: Cpu, stop_pc: int, max_steps: int = 5_000_000) -> None:
    while cpu.steps < max_steps and not cpu.stopped:
        if cpu.pc == stop_pc:
            return
        step(cpu)
    raise RuntimeError(f"stop timeout at PC=${cpu.pc:04X} steps={cpu.steps}")

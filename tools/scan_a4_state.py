#!/usr/bin/env python3
"""Scan the capstone disassembly for A4-relative *data* accesses and aggregate
them into a game-state field map.

A4 is the fixed global-state base (LEA.L $7FFE,A4 at 0x24920). Every
``d(a4)`` displacement is therefore a field at a constant offset. This tool
collects, per offset:

  * access count
  * operand sizes seen (.b/.w/.l)
  * mnemonics seen (move, cmp, add, lea, ...)
  * how often it is a write target (dest of move/clr/st/etc.)
  * whether it is ever used as an array base via ``lea`` + indexed ``(a4,Xn)``
  * a few example instruction addresses

JSR/JMP d(A4) are reported separately -- those are library/jump-table thunks,
not state fields.
"""
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASM = ROOT / "EXTRACTED" / "mm2.capstone.asm"
JSON_OUT = ROOT / "EXTRACTED" / "a4_state_scan.json"
ANCHOR = 0x7FFE

# capstone line: "0069fc  d1fc0000abcd      add.l     #$..., -$79b4(a4)"
LINE_RE = re.compile(
    r"^([0-9a-f]{4,8})\s+[0-9a-f ]+?\s+"      # address + raw bytes
    r"([a-z][a-z0-9._]*)\s+(.*)$",            # mnemonic + operands
    re.I,
)
# a4-relative displacement operand, with optional index register
DISP_RE = re.compile(r"(-?\$[0-9a-f]+)\(a4\)", re.I)
INDEX_RE = re.compile(r"\(a4,\s*([ad][0-7])(\.[wl])?\)", re.I)

WRITE_MNEM = {
    "clr", "st", "sf", "scc", "scs", "seq", "sne", "spl", "smi",
    "sge", "sgt", "sle", "slt", "shi", "sls", "svc", "svs",
}


def signed_off(tok: str) -> int:
    neg = tok.startswith("-")
    val = int(tok.lstrip("-").lstrip("$"), 16)
    return -val if neg else val


class Field:
    __slots__ = ("count", "sizes", "mnems", "writes", "lea", "examples")

    def __init__(self):
        self.count = 0
        self.sizes = set()
        self.mnems = set()
        self.writes = 0
        self.lea = 0
        self.examples = []

    def add(self, addr, mnem, size, is_write, is_lea):
        self.count += 1
        if size:
            self.sizes.add(size)
        self.mnems.add(mnem)
        if is_write:
            self.writes += 1
        if is_lea:
            self.lea += 1
        if len(self.examples) < 4:
            self.examples.append(addr)


def main():
    fields: dict[int, Field] = defaultdict(Field)
    jsr = defaultdict(int)
    indexed = defaultdict(int)  # offsets that lea then get indexed: tracked loosely

    text = ASM.read_text(encoding="utf-8", errors="replace")
    for raw in text.splitlines():
        m = LINE_RE.match(raw)
        if not m:
            continue
        addr, mnem, ops = m.group(1), m.group(2).lower(), m.group(3)
        base = mnem.split(".")[0]
        size = ""
        if "." in mnem:
            size = mnem.rsplit(".", 1)[1]

        disps = DISP_RE.findall(ops)
        if not disps:
            continue

        if base in ("jsr", "jmp"):
            for d in disps:
                jsr[signed_off(d)] += 1
            continue

        # split operands to detect write target (last operand for most ops)
        # capstone uses ", " between the two operands
        parts = [p.strip() for p in ops.split(",")]
        # An a4 disp that contains a comma index won't split cleanly; rejoin.
        dest = parts[-1] if parts else ops

        for d in disps:
            off = signed_off(d)
            is_lea = base == "lea"
            # write if dest operand contains this disp and mnem stores into dest
            disp_in_dest = f"{d}(a4)".lower() in dest.lower()
            is_write = False
            if base == "clr" or base in WRITE_MNEM:
                is_write = True
            elif disp_in_dest and base in (
                "move", "add", "sub", "and", "or", "eor", "addq", "subq",
                "addi", "subi", "andi", "ori", "eori", "move",
                "neg", "not", "asl", "asr", "lsl", "lsr", "rol", "ror",
                "bset", "bclr", "bchg", "mulu", "muls", "divu", "divs",
            ):
                is_write = True
            fields[off].add(addr, base, size, is_write, is_lea)

    # report
    out = []
    out.append(f"# A4-relative data fields  (A4 = ${ANCHOR:04X})")
    out.append(f"# total distinct data offsets: {len(fields)}")
    out.append(f"# total distinct JSR/JMP thunk offsets: {len(jsr)}")
    out.append("# 'word' = raw 16-bit displacement word (how docs name far fields);")
    out.append("# 'offset' = true signed displacement; absEA = A4 + offset.")
    out.append("")
    hdr = (f"{'offset':>10} {'word':>6} {'absEA':>7} {'cnt':>5} {'wr':>4} "
           f"{'lea':>4}  {'sizes':<8} {'mnems'}")
    out.append(hdr)
    out.append("-" * len(hdr))
    records = []
    for off in sorted(fields):
        f = fields[off]
        ea = (ANCHOR + off) & 0xFFFFFFFF
        word = off & 0xFFFF
        offtxt = (f"-${-off:X}" if off < 0 else f"+${off:X}")
        sizes = ",".join(sorted(f.sizes)) or "-"
        mnems = ",".join(sorted(f.mnems))
        out.append(
            f"{offtxt:>10} {word:6X} {ea:7X} {f.count:5d} {f.writes:4d} "
            f"{f.lea:4d}  {sizes:<8} {mnems}"
        )
        records.append({
            "offset": off,
            "word": word,
            "ea": ea,
            "count": f.count,
            "writes": f.writes,
            "lea": f.lea,
            "sizes": sorted(f.sizes),
            "mnems": sorted(f.mnems),
            "examples": f.examples,
        })

    print("\n".join(out))

    JSON_OUT.write_text(json.dumps({
        "anchor": ANCHOR,
        "fields": records,
        "thunks": {f"{o & 0xFFFF:04X}": c for o, c in sorted(jsr.items())},
    }, indent=1), encoding="utf-8")

    # also dump the thunk list separately at the end
    print("\n\n# JSR/JMP d(A4) thunk offsets (library jump table, NOT state):")
    for off in sorted(jsr):
        offtxt = (f"-${-off:X}" if off < 0 else f"+${off:X}")
        print(f"  {offtxt:>10}   x{jsr[off]}")


if __name__ == "__main__":
    main()

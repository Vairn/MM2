#!/usr/bin/env python3
"""Dump MM2 thunk-slot callsites and nearby pushed args from asm listing."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASM_PATH = ROOT / "EXTRACTED" / "mm2.asm"
OUT_PATH = ROOT / "EXTRACTED" / "tmp_mm2_sound_calls.txt"

# Raw opcodes as seen in comments in mm2.asm.
TARGETS = {
    "slot_7c62": "4eac839e",  # JSR -$7C62(A4)
    "slot_7c44": "4eac83bc",  # JSR -$7C44(A4)
    "slot_7bae": "4eac8452",  # JSR -$7BAE(A4)
    "slot_7ba8": "4eac8458",  # JSR -$7BA8(A4)
    "slot_7b9c": "4eac8464",  # JSR -$7B9C(A4)
}

ADDR_RE = re.compile(r";([0-9a-fA-F]{5}):\s*([0-9a-fA-F]+)")
IMM_PUSH_RE = re.compile(r"MOVE\.(W|L)\s+#\$([0-9a-fA-F]+),-\(A7\)")


def parse_addr_and_opcode(line: str) -> tuple[int, str] | None:
    m = ADDR_RE.search(line)
    if not m:
        return None
    return int(m.group(1), 16), m.group(2).lower()


def main() -> None:
    lines = ASM_PATH.read_text(encoding="utf-8", errors="replace").splitlines()

    grouped: dict[str, list[str]] = {k: [] for k in TARGETS}
    unique_word_imms: dict[str, set[int]] = {k: set() for k in TARGETS}

    for i, line in enumerate(lines):
        parsed = parse_addr_and_opcode(line)
        if not parsed:
            continue
        addr, opcode = parsed

        kind = None
        for name, op in TARGETS.items():
            if opcode == op:
                kind = name
                break
        if kind is None:
            continue

        # Capture nearby immediate pushes that likely feed the call.
        pushed: list[str] = []
        ctx: list[str] = []
        for j in range(max(0, i - 8), i):
            t = lines[j].strip()
            if not t:
                continue
            if "-(A7)" in t or "LEA" in t or "MOVEQ" in t:
                ctx.append(t)
            m = IMM_PUSH_RE.search(t)
            if m:
                pushed.append(f"{m.group(1)}#${m.group(2).upper()}")
                if m.group(1).upper() == "W":
                    unique_word_imms[kind].add(int(m.group(2), 16))

        entry = f"0x{addr:05X} pushes=[{', '.join(pushed)}]"
        if ctx:
            entry += "\n    " + "\n    ".join(ctx[-5:])
        grouped[kind].append(entry)

    lines_out: list[str] = []
    lines_out.append(f"Source: {ASM_PATH}")
    lines_out.append("")
    lines_out.append("Legend:")
    lines_out.append("- slot_7c62: JSR -$7C62(A4)")
    lines_out.append("- slot_7c44: JSR -$7C44(A4)")
    lines_out.append("- slot_7bae: JSR -$7BAE(A4)")
    lines_out.append("- slot_7ba8: JSR -$7BA8(A4)")
    lines_out.append("- slot_7b9c: JSR -$7B9C(A4)")
    lines_out.append("")
    for kind in ("slot_7b9c", "slot_7ba8", "slot_7bae", "slot_7c62", "slot_7c44"):
        uniq = " ".join(f"0x{x:X}" for x in sorted(unique_word_imms[kind]))
        lines_out.append(f"unique W immediates {kind}: {uniq}")
    lines_out.append("")

    for kind in ("slot_7b9c", "slot_7ba8", "slot_7bae", "slot_7c62", "slot_7c44"):
        entries = grouped[kind]
        lines_out.append(f"[{kind}] count={len(entries)}")
        lines_out.extend(entries)
        lines_out.append("")

    OUT_PATH.write_text("\n".join(lines_out), encoding="utf-8")
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    main()

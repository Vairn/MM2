#!/usr/bin/env python3
"""Extract the event/spell member-field accessor map from the 68k disassembly.

The field engine ``event_member_field_ptr`` @ 0x17766 maps a *selector byte*
(0x00..0x7F) to a byte offset inside a character record, plus an access width
(1/2/4). Selectors are dispatched through a 0x80-entry word jump table at
0x17FEA: ``target = 0x180FE + int16(table[selector])``. Each target stub does
``movea.l $8(a5),a0; adda.l #N,a0; move.l a0,-$4A8(a4)`` (N = field offset; the
``move.l $8(a5),-$4A8`` form means N=0), or jumps to the computed getters
($177E4 base-HP, $177F2 base-SP @ $181B0).

Width comes from the 0x17766 prologue:
  - long  (4): selectors 0x31, 0x3E
  - word  (2): 0x20 0x28 0x35 0x38 0x3A 0x3C
  - byte  (1): everything else

Output: EXTRACTED/event_field_map.json + a C array printed to stdout.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASM = ROOT / "EXTRACTED" / "mm2.capstone.annotated.asm"
OUT = ROOT / "EXTRACTED" / "event_field_map.json"
HEADER = ROOT / "game" / "include" / "mm2" / "events" / "EventFieldMap.h"

JT_BASE = 0x17FEA
JT_PC = 0x180FE          # jmp $180FE(pc,d0.w)
JT_COUNT = 0x80
COMPUTED = {0x177E4: "base_hp_max", 0x177F2: "base_sp_max"}
WIDTH4 = {0x31, 0x3E}
WIDTH2 = {0x20, 0x28, 0x35, 0x38, 0x3A, 0x3C}

line_re = re.compile(r"^([0-9a-f]{6})\s+([0-9a-f]+)\s+(.*)$")


def load_asm() -> dict[int, tuple[str, str]]:
    """addr -> (raw_hex, text)."""
    rows: dict[int, tuple[str, str]] = {}
    for raw in ASM.read_text(encoding="utf-8", errors="replace").splitlines():
        m = line_re.match(raw)
        if not m:
            continue
        addr = int(m.group(1), 16)
        rows[addr] = (m.group(2), m.group(3).strip())
    return rows


def read_jump_table(rows: dict[int, tuple[str, str]]) -> list[int]:
    """Concatenate raw bytes across [JT_BASE, JT_BASE+0x100) and form words."""
    blob = bytearray()
    addr = JT_BASE
    end = JT_BASE + JT_COUNT * 2
    while addr < end:
        if addr not in rows:
            raise SystemExit(f"jump-table gap at 0x{addr:05X}")
        hexstr = rows[addr][0]
        b = bytes.fromhex(hexstr)
        blob += b
        addr += len(b)
    words = [int.from_bytes(blob[i : i + 2], "big") for i in range(0, JT_COUNT * 2, 2)]
    return words


def stub_offset(rows: dict[int, tuple[str, str]], stub_addr: int) -> tuple[str, int]:
    """Resolve a dispatch stub to (kind, offset)."""
    if stub_addr in COMPUTED:
        return ("computed", -1)
    text = rows.get(stub_addr, ("", ""))[1]
    # offset-0 form: "move.l $8(a5), -$4a8(a4)"
    if text.startswith("move.l") and "$8(a5)" in text and "-$4a8" in text:
        return ("field", 0)
    # "movea.l $8(a5), a0" then next line adda/addq #N
    if text.startswith("movea.l") and "$8(a5)" in text:
        # scan forward a few addresses for the adda/addq into a0
        a = stub_addr
        for _ in range(4):
            a = next_addr(rows, a)
            if a is None:
                break
            t = rows[a][1]
            m = re.search(r"(?:adda\.l|addq\.l)\s+#\$([0-9a-f]+),\s*a0", t)
            if m:
                return ("field", int(m.group(1), 16))
            if "move.l" in t and "-$4a8" in t:
                return ("field", 0)
    return ("unknown", -2)


def next_addr(rows: dict[int, tuple[str, str]], addr: int) -> int | None:
    nxts = [a for a in rows if a > addr]
    return min(nxts) if nxts else None


def main() -> None:
    rows = load_asm()
    words = read_jump_table(rows)

    entries = []
    for sel in range(JT_COUNT):
        w = words[sel]
        target = (JT_PC + ((w ^ 0x8000) - 0x8000)) & 0xFFFFFF  # sign-extend word
        kind, off = stub_offset(rows, target)
        width = 4 if sel in WIDTH4 else 2 if sel in WIDTH2 else 1
        entries.append(
            {
                "selector": sel,
                "target": target,
                "kind": kind,
                "offset": off,
                "width": width,
            }
        )

    OUT.write_text(json.dumps({"entries": entries}, indent=2), encoding="utf-8")

    bad = [e for e in entries if e["kind"] == "unknown"]
    print(f"selectors: {JT_COUNT}  resolved: {JT_COUNT - len(bad)}  unknown: {len(bad)}")
    print()
    print("sel  off  w  kind")
    for e in entries:
        off = "--" if e["offset"] < 0 else f"{e['offset']:02X}"
        print(f"0x{e['selector']:02X}  {off:>3} {e['width']}  {e['kind']}  (-> 0x{e['target']:05X})")

    # Emit a compact C table for the runtime (offset; 0xFF = computed/unknown).
    print("\n/* selector -> {record byte offset, width}; 0xFF offset = computed */")
    cells = []
    for e in entries:
        off = 0xFF if e["offset"] < 0 else e["offset"]
        cells.append(f"{{0x{off:02X}, {e['width']}}}")
    for i in range(0, JT_COUNT, 8):
        print("    " + ", ".join(cells[i : i + 8]) + ",")

    write_header(entries)
    print(f"\nwrote {HEADER.relative_to(ROOT)}")


def write_header(entries: list[dict]) -> None:
    lines: list[str] = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// GENERATED by tools/extract_event_field_map.py -- DO NOT EDIT BY HAND.")
    lines.append("// Source of truth: event_member_field_ptr @ 0x17766 + jump table @ 0x17FEA")
    lines.append("// in EXTRACTED/mm2.capstone.annotated.asm. Regenerate:")
    lines.append("//     python tools/extract_event_field_map.py")
    lines.append("//")
    lines.append("// Maps an event/spell *field selector* byte (0x00..0x7F) to a byte offset")
    lines.append("// inside a 0x82-byte character record (Mm2RosterRecord) plus the natural")
    lines.append("// access width (1/2/4). offset == kEventFieldComputed marks the two")
    lines.append("// computed getters (selector 0x00 = base max HP, 0x01 = base max SP @ 0x181B0)")
    lines.append("// which are not a simple record offset.")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append("namespace mm2::events {")
    lines.append("")
    lines.append("constexpr uint8_t kEventFieldComputed = 0xFF;")
    lines.append("constexpr int kEventFieldSelectorCount = 0x80;")
    lines.append("")
    lines.append("struct EventFieldEntry {")
    lines.append("    uint8_t offset;  // record byte offset, or kEventFieldComputed")
    lines.append("    uint8_t width;   // 1, 2, or 4")
    lines.append("};")
    lines.append("")
    lines.append("// 0x80 entries; index by selector byte (selector & 0x7F).")
    lines.append("inline constexpr EventFieldEntry kEventFieldMap[kEventFieldSelectorCount] = {")
    cells = []
    for e in entries:
        off = 0xFF if e["offset"] < 0 else e["offset"]
        cells.append(f"{{0x{off:02X}, {e['width']}}}")
    for i in range(0, JT_COUNT, 4):
        row = ", ".join(cells[i : i + 4])
        lines.append(f"    {row},  // 0x{i:02X}")
    lines.append("};")
    lines.append("")
    lines.append("// Resolve a field selector to its record byte offset + width.")
    lines.append("// Returns false for out-of-range selectors and the two computed getters")
    lines.append("// (callers needing max HP/SP must special-case selector 0x00/0x01).")
    lines.append("inline bool eventVmResolveMemberField(uint8_t selector, int *offset, int *width) {")
    lines.append("    const uint8_t sel = static_cast<uint8_t>(selector & 0x7F);")
    lines.append("    const EventFieldEntry &e = kEventFieldMap[sel];")
    lines.append("    if (e.offset == kEventFieldComputed) {")
    lines.append("        return false;")
    lines.append("    }")
    lines.append("    if (offset) {")
    lines.append("        *offset = e.offset;")
    lines.append("    }")
    lines.append("    if (width) {")
    lines.append("        *width = e.width;")
    lines.append("    }")
    lines.append("    return true;")
    lines.append("}")
    lines.append("")
    lines.append("}  // namespace mm2::events")
    lines.append("")
    HEADER.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()

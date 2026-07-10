#!/usr/bin/env python3
"""Extract per-stub args pushed before jsr 0x1338E / 0x10894 / 0x133B6."""
from __future__ import annotations

import struct
from pathlib import Path
import importlib.util

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p", HERE / "_trace_spell_picker.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
mem = m.build_full_code_image()

NAMES_S = [
    "Awaken", "Detect Magic", "Energy Blast", "Flame Arrow", "Light", "Location", "Sleep",
    "Eagle Eye", "Electric Arrow", "Identify Monster", "Jump", "Levitate", "Lloyd's Beacon",
    "Protection from Magic", "Acid Stream", "Fly", "Invisibility", "Lightning Bolt", "Web",
    "Wizard Eye", "Cold Beam", "Feeble Mind", "Fire Ball", "Guard Dog", "Shield",
    "Time Distortion", "Disrupt", "Fingers of Death", "Sand Storm", "Shelter", "Teleport",
    "Disintegration", "Entrapment", "Fantastic Freeze", "Recharge Item", "Super Shock",
    "Dancing Sword", "Duplication", "Etherealize", "Prismatic Light", "Incinerate",
    "Mega Volts", "Meteor Shower", "Power Shield", "Implosion", "Inferno", "Star Burst",
    "Enchant Item",
]
NAMES_C = [
    "Apparition", "Awaken", "Bless", "First Aid", "Light", "Power Cure", "Turn Undead",
    "Cure Wounds", "Heroism", "Nature's Gate", "Pain", "Protection From Elements", "Silence",
    "Weaken", "Cold Ray", "Create Food", "Cure Poison", "Immobilize", "Lasting Light",
    "Walk on Water", "Acid Spray", "Air Transmutation", "Cure Disease", "Restore Alignment",
    "Surface", "Holy Bonus", "Air Encasement", "Deadly Swarm", "Frenzy", "Paralyze",
    "Remove Condition", "Earth Transmutation", "Rejuvenate", "Stone to Flesh",
    "Water Encasement", "Water Transmutation", "Earth Encasement", "Fiery Flail", "Moon Ray",
    "Raise Dead", "Fire Encasement", "Fire Transmutation", "Mass Distortion", "Town Portal",
]


def parse_dense(mem, base):
    pairs = []
    off = base
    while off + 8 <= base + 0x200:
        if struct.unpack_from(">H", mem, off)[0] != 0x4EBA:
            break
        disp = struct.unpack_from(">h", mem, off + 2)[0]
        stub = (off + 2 + disp) & 0xFFFFFFFF
        if struct.unpack_from(">H", mem, off + 4)[0] != 0x6000:
            break
        pairs.append(stub)
        off += 8
    return pairs


def scan_stub(addr, n=120):
    """Return list of (kind, info) for interesting ops."""
    i = addr
    end = addr + n
    events = []
    stack_imm = []  # recent move.w #imm,-(a7)
    while i + 2 <= end and i + 2 <= len(mem):
        op = struct.unpack_from(">H", mem, i)[0]
        if op == 0x4E75:
            events.append(("rts", None))
            break
        if op == 0x4EBA and i + 4 <= len(mem):
            d = struct.unpack_from(">h", mem, i + 2)[0]
            tgt = (i + 2 + d) & 0xFFFFFFFF
            events.append(("jsr", tgt, list(stack_imm[-4:])))
            stack_imm.clear()
            i += 4
            continue
        # move.w #imm,-(a7) = 3F3C iiii
        if op == 0x3F3C and i + 4 <= len(mem):
            imm = struct.unpack_from(">H", mem, i + 2)[0]
            stack_imm.append(imm)
            events.append(("push.w", imm))
            i += 4
            continue
        # move.b #imm, d(a4)
        if op in (0x197C, 0x1B7C) and i + 6 <= len(mem):
            imm = mem[i + 2]
            a4 = struct.unpack_from(">h", mem, i + 4)[0]
            events.append(("mv.b", imm, (-a4) & 0xFFFF))
            i += 6
            continue
        if op == 0x522C and i + 4 <= len(mem):
            a4 = struct.unpack_from(">h", mem, i + 2)[0]
            events.append(("addq1", (-a4) & 0xFFFF))
            i += 4
            continue
        # clr.w -(a7) = 4267
        if op == 0x4267:
            stack_imm.append(0)
            events.append(("push.w", 0))
            i += 2
            continue
        i += 2
    return events


def summarize(school, names, stubs):
    print(f"=== {school} ===")
    for i, s in enumerate(stubs[:48]):
        name = names[i] if i < len(names) else "?"
        ev = scan_stub(s, 140)
        jsrs = [e for e in ev if e[0] == "jsr"]
        interesting = []
        for e in jsrs:
            tgt = e[1]
            pushes = e[2] if len(e) > 2 else []
            if tgt in (0x1338E, 0x133B6, 0x10894, 0x108BC, 0xD43C, 0xD464, 0xD3B8):
                interesting.append((hex(tgt), pushes))
        a4w = [e for e in ev if e[0] in ("mv.b", "addq1")]
        if interesting or a4w:
            print(f"{i:2d} {name:24s} @{s:#06x} jsr={interesting} a4={a4w[:6]}")


sorc = parse_dense(mem, 0xD006)
cler = parse_dense(mem, 0xCDC6)
print(f"sorc={len(sorc)} cler={len(cler)}")
# Note: dense index may not equal flat0 — still useful for dice args per stub addr
summarize("SORC dense", NAMES_S, sorc)
summarize("CLER dense", NAMES_C, cler)

# Also dump exact Awaken @ 0xB66C
print("\n=== raw Awaken 0xB66C ===")
for e in scan_stub(0xB66C, 80):
    print(e)
print("\n=== Energy Blast-ish ===")
for e in scan_stub(0xB6F2, 80):
    print(e)

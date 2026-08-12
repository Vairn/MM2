#!/usr/bin/env python3
"""ASM-vs-remake spell audit using Capstone listing bytes (source of truth)."""
from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SORC = [
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
CLER = [
    "Apparition", "Awaken", "Bless", "First Aid", "Light", "Power Cure", "Turn Undead",
    "Cure Wounds", "Heroism", "Nature's Gate", "Pain", "Protection From Elements", "Silence",
    "Weaken", "Cold Ray", "Create Food", "Cure Poison", "Immobilize", "Lasting Light",
    "Walk on Water", "Acid Spray", "Air Transmutation", "Cure Disease", "Restore Alignment",
    "Surface", "Holy Bonus", "Air Encasement", "Deadly Swarm", "Frenzy", "Paralyze",
    "Remove Condition", "Earth Transmutation", "Rejuvenate", "Stone to Flesh",
    "Water Encasement", "Water Transmutation", "Earth Encasement", "Fiery Flail", "Moon Ray",
    "Raise Dead", "Fire Encasement", "Fire Transmutation", "Mass Distortion", "Town Portal",
    "Divine Intervention", "Holy Word", "Resurrection", "Uncurse Item",
]

REMAKE_DIRECT_SORC = {
    0, 1, 4, 5, 7, 10, 11, 12, 13, 15, 16, 19, 23, 24, 25, 29, 30, 32, 34, 37, 38, 43, 47
}
REMAKE_DIRECT_CLER = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 15, 16, 18, 19, 21, 22, 23, 24, 25, 28, 30, 31, 32, 33,
    35, 39, 41, 43, 44, 45, 46, 47
}
REMAKE_AUTO_SORC = {36, 39, 41, 42, 45, 46}
REMAKE_AUTO_CLER = {0, 13, 27, 29, 38}
REMAKE_LETTER_SORC = {2, 3, 6, 8, 9, 14, 17, 18, 20, 21, 22, 26, 27, 28, 31, 33, 35, 40, 44}
REMAKE_LETTER_CLER = {10, 12, 14, 17, 20, 26, 34, 36, 37, 40, 42}
REMAKE_EXPLORE_FAIL_SORC = {25, 32} | REMAKE_AUTO_SORC | REMAKE_LETTER_SORC
REMAKE_EXPLORE_FAIL_CLER = {44} | REMAKE_AUTO_CLER | REMAKE_LETTER_CLER

# Hand-traced stub entries (Capstone) — flat → stub. Prefer jump-table JSR targets
# verified in mm2.capstone.asm; explore/non-combat leaves often live on CDB8 path.
# Where CFF8 table points at a no-op rts, the real body is the preceding link.
STUB_OVERRIDE = {
    # Sorcerer (combat CFF8 / explore CD90 may differ for dual leaves)
    ("S", 16): 0xB9C4,  # Invisibility
    ("S", 24): 0xBB5C,  # Shield (-$799B)
    ("S", 25): 0xBB86,  # Time Distortion
    ("S", 43): 0xBEE2,  # Power Shield (verify)
    ("C", 2): 0xBFC4,  # Bless
    ("C", 6): 0xBFEE,  # Turn Undead
}

HELPER_NAMES = {
    0xD390: "D390_confirm",
    0xD43C: "D43C_letter",
    0xD464: "D464_target",
    0xD29C: "D29C_failmsg",
    0xD25A: "D25A_fx",
    0xD2EA: "D2EA_party",
    0x108BC: "108BC_combat",
    0x10894: "10894_combat",
    0x133B6: "133B6_effect",
    0x1338E: "1338E_effect",
    0x1333A: "1333A_effect",
    0xC028: "C028_turn",
    0xC050: "C050_turn_body",
}


def load_capstone_image() -> bytearray:
    """Rebuild code image from mm2.capstone.asm hex columns (source of truth)."""
    path = ROOT / "EXTRACTED" / "mm2.capstone.asm"
    mem = bytearray(0x30000)
    # 00b9c4  4e550000          link...
    line_re = re.compile(r"^([0-9a-fA-F]{6})\s+([0-9a-fA-F]{2,})(?:\s|$)")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = line_re.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        hx = m.group(2)
        if len(hx) % 2:
            continue
        raw = bytes.fromhex(hx)
        mem[addr : addr + len(raw)] = raw
    return mem


def parse_dense_pairs(mem: bytearray, base: int, limit: int = 0x200) -> list[tuple[int, int]]:
    pairs: list[tuple[int, int]] = []
    off = base
    end = base + limit
    while off + 8 <= end:
        w0 = struct.unpack_from(">I", mem, off)[0]
        w1 = struct.unpack_from(">I", mem, off + 4)[0]
        hi0, lo0 = (w0 >> 16) & 0xFFFF, w0 & 0xFFFF
        hi1 = (w1 >> 16) & 0xFFFF
        if hi0 != 0x4EBA or hi1 != 0x6000:
            break
        disp = struct.unpack(">h", lo0.to_bytes(2, "big"))[0]
        pairs.append((off, (off + 2 + disp) & 0xFFFFFFFF))
        off += 8
    return pairs


def parse_sparse(mem: bytearray, table_base: int, count: int, jmp_base: int):
    rows = []
    for code in range(count):
        w = struct.unpack_from(">h", mem, table_base + code * 2)[0]
        tgt = (jmp_base + w) & 0xFFFFFFFF
        rows.append((code, w, tgt))
    return rows


def dense_stub_at(mem: bytearray, pair_addr: int) -> int | None:
    w0 = struct.unpack_from(">I", mem, pair_addr)[0]
    if ((w0 >> 16) & 0xFFFF) != 0x4EBA:
        return None
    disp = struct.unpack(">h", (w0 & 0xFFFF).to_bytes(2, "big"))[0]
    return (pair_addr + 2 + disp) & 0xFFFFFFFF


def pc_target(addr: int, raw: bytes) -> int | None:
    if len(raw) >= 4 and raw[0:2] == b"\x4e\xba":
        disp = struct.unpack(">h", raw[2:4])[0]
        return (addr + 2 + disp) & 0xFFFFFFFF
    return None


def classify_stub(mem: bytearray, stub: int, limit_bytes: int = 0x120) -> dict:
    """Light scan of Capstone lines from stub..stub+limit for helpers / gates."""
    out = {
        "stub": stub,
        "helpers": [],
        "calls_d390": False,
        "calls_letter": False,
        "calls_combat_apply": False,
        "btst_5600": [],
        "first_insns": [],
    }
    # Walk using known instruction sizes from image: decode via simple scan of 4EBA/4EB9/rts
    addr = stub
    end = stub + limit_bytes
    n = 0
    while addr < end and n < 40:
        op = mem[addr : addr + 2]
        if len(op) < 2:
            break
        word = struct.unpack(">H", op)[0]
        # rts
        if word == 0x4E75 and n > 0:
            out["first_insns"].append(f"{addr:#06x}:rts")
            break
        # link
        if word == 0x4E55:
            out["first_insns"].append(f"{addr:#06x}:link")
            addr += 4
            n += 1
            continue
        # jsr (d16,PC)
        if word == 0x4EBA:
            tgt = pc_target(addr, mem[addr : addr + 4])
            name = HELPER_NAMES.get(tgt, f"{tgt:#06x}" if tgt else "?")
            out["first_insns"].append(f"{addr:#06x}:jsr {name}")
            if name not in out["helpers"] and tgt:
                out["helpers"].append(name)
            if tgt == 0xD390:
                out["calls_d390"] = True
            if tgt in (0xD43C, 0xD464):
                out["calls_letter"] = True
            if tgt in (0x108BC, 0x10894, 0x133B6, 0x1338E, 0x1333A):
                out["calls_combat_apply"] = True
            addr += 4
            n += 1
            continue
        # btst.b #imm, -$5600(a4) = 082c 000k aa00  where aa00 is -5600 encoding
        if word == 0x082C and addr + 6 <= len(mem):
            imm = mem[addr + 3]
            ea = struct.unpack(">H", mem[addr + 4 : addr + 6])[0]
            if ea == 0xAA00:  # -$5600(a4)
                out["btst_5600"].append(imm)
                out["first_insns"].append(f"{addr:#06x}:btst #${imm:x},-$5600")
            addr += 6
            n += 1
            continue
        # unlk
        if word == 0x4E5D:
            out["first_insns"].append(f"{addr:#06x}:unlk")
            addr += 2
            n += 1
            continue
        # default advance 2 (rough)
        addr += 2
        n += 1
    return out


def decode_flags(b0: int, b1: int) -> dict:
    combat = bool(b0 & 0x40)
    noncombat = bool(b0 & 0x80)
    outdoor = bool(b1 & 0x80)
    if combat and not noncombat:
        when = "combat"
    elif noncombat and not combat:
        when = "explore"
    else:
        when = "anytime"
    return {"when": when, "outdoor": outdoor, "b0": f"{b0:02X}", "b1": f"{b1:02X}"}


def remake_path(school: str, flat: int) -> str:
    auto = REMAKE_AUTO_SORC if school == "S" else REMAKE_AUTO_CLER
    letter = REMAKE_LETTER_SORC if school == "S" else REMAKE_LETTER_CLER
    direct = REMAKE_DIRECT_SORC if school == "S" else REMAKE_DIRECT_CLER
    if flat in auto:
        return "autoAoE"
    if flat in letter:
        return "letterPick"
    if flat in direct:
        return "direct"
    return "STUB_FALLTHROUGH"


def remake_explore_ok(school: str, flat: int) -> bool:
    fail = REMAKE_EXPLORE_FAIL_SORC if school == "S" else REMAKE_EXPLORE_FAIL_CLER
    return flat not in fail


def level_number(flat: int) -> tuple[int, int]:
    per = [7, 7, 6, 6, 5, 5, 4, 4, 4]
    base = 0
    for lv, n in enumerate(per, 1):
        if flat < base + n:
            return lv, flat - base + 1
        base += n
    return 0, 0


def resolve_stub(mem: bytearray, school: str, flat: int) -> dict:
    if (school, flat) in STUB_OVERRIDE:
        stub = STUB_OVERRIDE[(school, flat)]
        return {"stub": stub, "via": "override"}

    picker = flat if school == "S" else flat + 0x30
    # Prefer school-typical sparse path
    if school == "S":
        code = picker - 2
        rows = parse_sparse(mem, 0xD1AE, 0x5C, 0xD27C)
        if 0 <= code < len(rows) and rows[code][1] != 2:
            stub = dense_stub_at(mem, rows[code][2])
            if stub and mem[stub : stub + 2] != b"\x4e\x75":
                return {"stub": stub, "via": "cff8", "code": code}
        rows = parse_sparse(mem, 0xCF1E, 0x60, 0xCFF2)
        if picker < len(rows) and rows[picker][1] != 2:
            stub = dense_stub_at(mem, rows[picker][2])
            return {"stub": stub, "via": "cdb8", "code": picker}
    else:
        rows = parse_sparse(mem, 0xCF1E, 0x60, 0xCFF2)
        if picker < len(rows) and rows[picker][1] != 2:
            stub = dense_stub_at(mem, rows[picker][2])
            if stub and mem[stub : stub + 2] != b"\x4e\x75":
                return {"stub": stub, "via": "cdb8", "code": picker}
        code = picker - 2
        rows = parse_sparse(mem, 0xD1AE, 0x5C, 0xD27C)
        if 0 <= code < len(rows) and rows[code][1] != 2:
            stub = dense_stub_at(mem, rows[code][2])
            return {"stub": stub, "via": "cff8", "code": code}
    return {"stub": None, "via": "none"}


def snap_back_to_link(mem: bytearray, stub: int) -> int:
    """If table lands on rts/unlk, walk back to preceding link.w."""
    if stub is None:
        return stub
    if mem[stub : stub + 2] == b"\x4e\x55":
        return stub
    # walk back up to 0x40 bytes for link
    for back in range(2, 0x40, 2):
        a = stub - back
        if a >= 0 and mem[a : a + 2] == b"\x4e\x55":
            return a
    return stub


def main() -> None:
    mem = load_capstone_image()
    # sanity: Invisibility
    assert mem[0xB9C4:0xB9C8] == bytes.fromhex("4e550000"), mem[0xB9C4:0xB9C8].hex()
    assert mem[0xBFC4:0xBFC8] == bytes.fromhex("4e550000"), mem[0xBFC4:0xBFC8].hex()

    dat = (ROOT / "spells.dat").read_bytes()
    rows = []

    for school, names, off in (("S", SORC, 0), ("C", CLER, 48)):
        for flat, name in enumerate(names):
            b0, b1 = dat[(off + flat) * 2], dat[(off + flat) * 2 + 1]
            flags = decode_flags(b0, b1)
            resolved = resolve_stub(mem, school, flat)
            stub = snap_back_to_link(mem, resolved.get("stub"))
            cls = classify_stub(mem, stub) if stub else {}
            lv, num = level_number(flat)
            rpath = remake_path(school, flat)
            rexpl = remake_explore_ok(school, flat)

            issues = []
            # spells.dat gate vs remake explore
            if flags["when"] == "combat" and rexpl:
                issues.append("dat=combat but remake explore succeeds")
            if flags["when"] == "explore" and not rexpl and rpath != "direct":
                # explore-only spelled failed by remake combat lists — bad if somehow cast in explore
                if rpath in ("autoAoE", "letterPick"):
                    issues.append("dat=explore-only but remake uses combat-only path")
            if flags["when"] == "explore" and not rexpl:
                issues.append("dat=explore but remake explore FAILS")
            if cls.get("calls_d390") and rexpl and flags["when"] == "combat":
                issues.append("ASM D390 (combat confirm) but remake explore applies")
            if cls.get("calls_letter") and rpath == "direct":
                # allow explore modals
                modal = (school == "S" and flat in {12, 15, 30, 34, 37, 47}) or (
                    school == "C"
                    and flat
                    in {3, 5, 7, 8, 16, 22, 23, 28, 30, 32, 33, 39, 43, 46, 47}
                )
                if not modal:
                    issues.append("ASM letter/target helper but remake direct")
            if rpath == "STUB_FALLTHROUGH":
                issues.append("remake fallthrough stub message")

            # Comment address bugs in remake
            note = []
            if school == "S" and flat == 24 and stub == 0xBB5C:
                note.append("remake comment cites 0xBB84 (rts); body @ 0xBB5C")
            if school == "C" and flat == 2 and stub == 0xBFC4:
                note.append("remake comment cites 0xBFEC (rts); body @ 0xBFC4")

            rows.append(
                {
                    "tag": f"{school}{lv}/{num}",
                    "school": school,
                    "flat": flat,
                    "name": name,
                    "flags": flags,
                    "stub": stub,
                    "via": resolved.get("via"),
                    "asm": cls,
                    "remake_path": rpath,
                    "remake_explore_ok": rexpl,
                    "issues": issues,
                    "notes": note,
                }
            )

    out_path = ROOT / "EXTRACTED" / "tmp_spell_audit.json"
    out_path.write_text(json.dumps(rows, indent=2), encoding="utf-8")

    issues = [r for r in rows if r["issues"]]
    d390 = [r for r in rows if r["asm"].get("calls_d390")]
    letter = [r for r in rows if r["asm"].get("calls_letter")]
    outdoor = [r for r in rows if r["flags"]["outdoor"]]

    print(f"Wrote {out_path}")
    print(f"Gate/path issues: {len(issues)} / 96")
    print(f"D390 stubs: {len(d390)}  letter stubs: {len(letter)}  outdoor: {len(outdoor)}")

    print("\n=== ISSUES ===")
    for r in issues:
        print(
            f"{r['tag']:8} {r['name']:28} dat={r['flags']['when']:8} "
            f"path={r['remake_path']:10} expl={r['remake_explore_ok']} "
            f"stub={r['stub'] and hex(r['stub'])} d390={r['asm'].get('calls_d390')} "
            f"| {'; '.join(r['issues'])}"
        )

    print("\n=== ALL D390 STUBS ===")
    for r in d390:
        print(
            f"{r['tag']:8} {r['name']:28} dat={r['flags']['when']:8} "
            f"explRem={r['remake_explore_ok']} path={r['remake_path']:10} "
            f"stub={hex(r['stub'])} helpers={r['asm'].get('helpers')} "
            f"btst5600={r['asm'].get('btst_5600')}"
        )

    print("\n=== OUTDOOR FLAGS ===")
    for r in outdoor:
        print(f"{r['tag']:8} {r['name']:28} dat={r['flags']['when']:8} remExpl={r['remake_explore_ok']}")

    # Coverage counts
    from collections import Counter

    c = Counter(r["remake_path"] for r in rows)
    print("\n=== REMAKE PATH COUNTS ===", dict(c))
    dat_c = Counter(r["flags"]["when"] for r in rows)
    print("=== DAT WHEN COUNTS ===", dict(dat_c))


if __name__ == "__main__":
    main()

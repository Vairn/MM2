"""OP_0E default-path selector binning @ asm 0x15EDC (doc 07 SS OP_0E).

CORRECTED 2026-07: a prior pass claimed the DEFAULT-range dispatch (0x15EDC
bins the selector to category 0x3C..0x46, then 0x160AE calls thunk -$7DFA)
always reaches 0x9D76, the "Arena Games" ticket-combat-reward engine. That was
backwards. Byte-reading the A4 vtable trampoline table directly out of
EXTRACTED/ghidra/mm2_data_00.bin (file offset 0x7FFE+disp) settles it:

  -$7DBE (file off 0x0240): 4EF9 00009D76  -> 0x09D76  Arena Games engine
  -$7DFA (file off 0x0204): 4EF9 000092F2  -> 0x092F2  event_dat_loader (!)

So explicit OP_0E selector 0x08 (thunk -$7DBE) is the SOLE path into the
Arena Games engine; the default-range dispatch instead re-enters the SAME
event_dat_loader used to load a normal on-map location, with a synthetic
category/index -- plausibly to run one of event.dat's non-map "string bank"
pseudo-records (e.g. decoder location 60, "Nordon/Nordonna/Corak"). That
reinvocation mechanism is not reverse-engineered yet. The earlier claim made
callers treat every default-range selector (Atlantium Olympic trials
0x12-0x25, Vulcania/Sandsobar arena tiers, post-victory combat tiers
0x26-0x29/0x4A-0x4F, Mount Farview reward 0x97, the game-start Corak
monologue, ...) as reaching the ticket engine, which is wrong.

When explicit selector 0x08 IS reached, the engine unconditionally:

  1) scans every party member's BACKPACK ONLY (record+0x3A..0x3F, NOT
     equipped slots) for a ticket item 0xD0..0xD3 (asm 0x9D9C-0x9DDA);
  2) on miss: "Sorry, but you must have a ticket to compete in these
     games." (str @ code 0xA082/0xA0A7, byte-exact) -- no combat;
  3) on hit: consumes the ticket (thunk -$7F26), shows "The games master
     accepts your ticket.  Let the battle begin!" (0xA0BF/0xA0E5), and arms
     a FIXED encounter (same battle-slot array + combat thunk as OP_12) with
     monster type = ((color*3 + area[screen]) * 16) + rng(1,16) (asm
     0x9E86-0x9EC2, area table @ data hunk 0xE74);
  4) on victory: grants gold from a 4(color) x 3(town tier) table (data hunk
     0xE7A) to the first eligible party member and prints "Winner, you
     receive N gold" (0xA0FC/0xA111) -- plus a documented ROM bug that
     corrupts record+0x79 (doc 36-class-quest-hp-bug.md).

Explicit selector 0x07 (general store, thunk -$7DB8 -> 0xA62C, also
byte-verified) does NOT reach this engine -- a distinct fixed address.
"""

from __future__ import annotations

# (lo, hi, category_byte, subtract) — matches chained -$7E9C checks @ 0x15EDC.
SELECTOR_BINS: tuple[tuple[int, int, int, int], ...] = (
    (0x09, 0x10, 0x3C, 0x08),
    (0x11, 0x37, 0x3D, 0x10),
    (0x38, 0x4B, 0x3E, 0x37),
    (0x4C, 0x54, 0x3F, 0x4B),
    (0x56, 0x5B, 0x40, 0x55),
    (0x5C, 0x5E, 0x41, 0x5B),
    (0x65, 0x69, 0x42, 0x64),
    (0x6A, 0x7C, 0x43, 0x69),
    (0x97, 0x98, 0x44, 0x96),
    (0xE3, 0xF3, 0x45, 0xE2),
    (0xF4, 0xFB, 0x46, 0xF3),
)

# Data hunk 0xE74 (A4-$718A): per-screen difficulty add-in, indexed by map
# screen 0..4 (Middlegate/Atlantium/Tundara/Vulcania/Sandsobar).
ARENA_AREA_INDEX: tuple[int, ...] = (0, 2, 0, 0, 1)

# Data hunk 0xE7A (A4-$7184): 4 ticket colors (Green/Yellow/Red/Black) x 3
# town tiers (tier = min(screen, 2): Middlegate=0, Atlantium=1, else 2).
ARENA_GOLD_REWARD: tuple[tuple[int, int, int], ...] = (
    (200, 1500, 500),
    (2000, 5000, 3000),
    (7000, 15000, 10000),
    (20000, 50000, 30000),
)

ARENA_TICKET_COLOR_NAMES: tuple[str, ...] = ("Green", "Yellow", "Red", "Black")


def bin_exec_selector(sel: int) -> tuple[int, int] | None:
    """Return (category_byte, adjusted_index) for default-range selectors."""
    s = sel & 0xFF
    for lo, hi, cat, sub in SELECTOR_BINS:
        if lo <= s <= hi:
            return cat, s - sub
    return None


def arena_gold_reward(color: int, screen: int) -> int:
    c = max(0, min(3, color))
    tier = max(0, min(2, screen))
    return ARENA_GOLD_REWARD[c][tier]


def arena_monster_type(color: int, screen: int, roll_1_to_16: int) -> int:
    c = max(0, min(3, color))
    area = ARENA_AREA_INDEX[screen] if 0 <= screen < 5 else ARENA_AREA_INDEX[0]
    roll = max(1, min(16, roll_1_to_16))
    return ((c * 3 + area) * 16 + roll) & 0xFF


def format_exec_selector_bin(sel: int, map_id: int = -1) -> str:
    """One-line annotation for decode_event.py.

    Selector 0x08 is the Arena Games ticket engine (0x9D76); true
    default-range selectors instead re-enter event_dat_loader (0x92F2) with a
    synthetic category/index (not reverse-engineered yet -- see module
    docstring), so they are annotated as such rather than as arena tickets.
    """
    del map_id  # kept for call-site compatibility; role no longer depends on it
    if (sel & 0xFF) == 0x08:
        return "Arena Games ticket engine (0x08 -> -$7DBE -> 0x9D76)"
    b = bin_exec_selector(sel)
    if not b:
        return ""
    cat, idx = b
    return f"default-range dispatch (event_dat_loader reinvoke): cat=0x{cat:02X} idx={idx}"

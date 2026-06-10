"""Semantic name tables for readable decompile/compile."""

from __future__ import annotations

from pathlib import Path

# Class gate field -> name (doc 37)
CLASS_BY_FIELD = {
    0x00: "Knight",
    0x01: "Paladin",
    0x02: "Archer",
    0x03: "Cleric",
    0x04: "Sorcerer",
    0x05: "Robber",
    0x06: "Ninja",
    0x07: "Barbarian",
}

CLASS_FIELD_BY_NAME = {v.lower(): k for k, v in CLASS_BY_FIELD.items()}

# Explicit OP_0E dispatch cases @ asm 0x160C2 (not default -$7DFA range bins).
# Decompile appends `# label` comments; compile path ignores them.
SELECTOR_HANDLER_LABELS: dict[int, str] = {
    0x01: "open_tavern_food",
    0x02: "open_inn_lodging",
    0x03: "open_temple",
    0x04: "open_training",
    0x05: "open_mages_guild",
    0x06: "open_blacksmith_shop",
    0x07: "open_general_store",
    0x08: "open_special_shop",
    0x0A: "goblet_quest",
    0x0D: "enroll_mages_guild",
    0x64: "portal_travel_100",
    0x7E: "special_126",
    0x7F: "special_127",
    0x80: "special_128",
    0x81: "special_129",
    0x82: "special_130",
    0x83: "special_131",
    0xC9: "quest_handler_201",
    0xCA: "quest_handler_202",
    0xCB: "quest_handler_203",
    0xCC: "quest_handler_204",
    0xCD: "quest_handler_205",
    0xCE: "quest_handler_206",
    0xCF: "quest_handler_207",
    0xE2: "special_226",
    0xFD: "special_253",
}

SELECTOR_NAMES = {
    0x01: ("shop", "tavern"),
    0x02: ("shop", "inn"),
    0x03: ("shop", "temple"),
    0x04: ("shop", "training"),
    0x05: ("shop", "mages_guild"),
    0x06: ("shop", "blacksmith"),
    0x07: ("shop", "general_store"),
    0x08: ("shop", "special_shop"),
    0x0A: ("quest", "goblet"),
    0x0D: ("shop", "enroll_mages"),
    0x64: ("quest", "portal_travel"),
    0xC9: ("quest", "handler_201"),
    0xCA: ("quest", "handler_202"),
    0xCB: ("quest", "handler_203"),
    0xCC: ("quest", "handler_204"),
    0xCD: ("quest", "handler_205"),
    0xCE: ("quest", "handler_206"),
    0xCF: ("quest", "handler_207"),
}

SELECTOR_BY_NAME: dict[tuple[str, str], int] = {
    (cat, name): sel for sel, (cat, name) in SELECTOR_NAMES.items()
}

# OP_0E handlers draw UI text from str.dat / embedded exe — not event.dat strings.
# Hand-author sugar: shop tavern → selector 0x01. Decompile keeps `selector 0xNN`
# plus `# handler` for explicit asm cases in SELECTOR_HANDLER_LABELS only.
SERVICE_UI_NOTES: dict[int, str] = {
    0x01: "dialog from str.dat (tavern menu)",
    0x02: "dialog from embedded exe (inn)",
    0x03: "dialog from str.dat (temple)",
    0x04: "dialog from str.dat + exe (training)",
    0x05: "dialog from str.dat (mages guild spell shop)",
    0x06: "dialog from str.dat (blacksmith)",
    0x07: "dialog from str.dat (general store)",
    0x08: "dialog from str.dat (special shop)",
    0x0A: "dialog from str.dat (goblet quest handler)",
    0x0D: "dialog from str.dat (mages guild enrollment)",
    0x0E: "engine default dispatch (fountain / misc services)",
}

TRANSITION_NAMES = {
    (0x0B, 0x37): "overland_c3",
    (0x11, 0x8F): "cavern_middlegate",
}

# Known apply_party_masked(set=0x75, and, or) patterns — hand-author sugar only (not used on decompile).
QUEST_FLAG_PATTERNS: dict[tuple[int, int, int], tuple[str, str]] = {
    (0x75, 0xFE, 0x01): ("quest_complete", "ok"),
    (0x75, 0xFB, 0x04): ("ancients_code", "ok"),
    (0x75, 0x73, 0x80): ("sorcerer_stasis", "ybmug"),
    (0x75, 0x00, 0x03): ("sorcerer_stasis", "both_freed"),
    (0x75, 0xFD, 0x02): ("sorcerer_stasis", "ybmug_freed"),
}

TRIGGER_COND_BY_BYTE = {
    0x10: "always",
    0x20: "from north",
    0x40: "dir special",
    0x80: "enter",
    0xA0: "facing ns",
    0xC0: "enter special",
    0xF0: "any_direction",
}

TRIGGER_BYTE_BY_COND = {
    "always": 0x10,
    "enter": 0x80,
    "from north": 0x20,
    "dir special": 0x40,
    "any_direction": 0xF0,
    "facing ns": 0xA0,
    "enter special": 0xC0,
}

SAY_VARIANT_BY_OP = {
    0x01: "basic",
    0x02: "block",
    0x03: "",
    0x04: "door",
    0x05: "popup_a",
    0x06: "popup_b",
}

SAY_OP_BY_VARIANT = {
    "basic": 0x01,
    "block": 0x02,
    "": 0x03,
    "door": 0x04,
    "popup_a": 0x05,
    "popup_b": 0x06,
    "say": 0x03,
    "say_door": 0x04,
    "say_block": 0x02,
    "say_popup": 0x05,
}

COND_SET_OPS = {
    0x09,
    0x0A,
    0x16,
    0x17,
    0x1B,
    0x1C,
    0x22,
    0x23,
    0x24,
    0x25,
    0x28,
    0x2D,
    0x30,
    0x32,
}

BRANCH_OPS = {0x10, 0x11, 0x2B}


def load_items_table(root: Path | None = None) -> list[str]:
    root = root or Path(__file__).resolve().parents[2]
    for rel in ("items.dat", "EXTRACTED/items.dat"):
        p = root / rel
        if p.is_file():
            raw = p.read_bytes()
            rec = 0x14
            names: list[str] = []
            for i in range(len(raw) // rec):
                off = i * rec
                names.append(raw[off : off + 0x0C].decode("ascii", errors="replace").rstrip())
            return names
    return []


def load_monsters_table(root: Path | None = None) -> list[str]:
    root = root or Path(__file__).resolve().parents[2]
    for rel in ("monsters.dat", "EXTRACTED/monsters.dat"):
        p = root / rel
        if p.is_file():
            raw = p.read_bytes()
            rec = 26
            names: list[str] = []
            for i in range(min(256, len(raw) // rec)):
                off = i * rec
                names.append(raw[off : off + 0x0C].decode("ascii", errors="replace").rstrip())
            return names
    return []


def item_name(items: list[str], item_id: int) -> str:
    if 0 <= item_id < len(items) and items[item_id]:
        return items[item_id]
    return f"item_{item_id}"


def selector_handler_comment(sel: int) -> str:
    label = SELECTOR_HANDLER_LABELS.get(sel)
    return f"  # {label}" if label else ""


def slugify(text: str, max_len: int = 32) -> str:
    s = text.strip().lower()
    out: list[str] = []
    for ch in s:
        if ch.isalnum():
            out.append(ch)
        elif ch in " \t\n@/":
            if out and out[-1] != "_":
                out.append("_")
    slug = "".join(out).strip("_")
    if not slug:
        return "text"
    return slug[:max_len]


def auto_script_name(event_id: int, preview: str = "") -> str:
    del preview  # mechanical only — no slug from string preview
    return f"event_{event_id:02d}"


def auto_string_name(index: int, text: str) -> str:
    del text  # mechanical only — read actual text from strings: block
    return f"s{index}"

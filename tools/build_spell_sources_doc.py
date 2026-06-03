#!/usr/bin/env python3
"""Generate EXTRACTED/docs/31-spell-sources.md from shop tables, items.dat, and event.dat traces."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from decode_items import load as load_items
from mm2_spells import CAST_NAMES, SPELL_META, decode_record, record_school_flat, spell_at

# spells.dat index (0..95) -> human world-acquisition note.
# Cross-checked vs Might and Magic FAQ §3-2-2 and event.dat (OP_2E / OP_1F / exec_selector).
# Overland tiles use FAQ sector notation: C3 (1,9) = column 1, row 9 (matches AreaNames.h sectors).
WORLD_GRANTS: dict[int, str] = {
    # Sorcerer
    7: "World: Nordon quest — Eagle Eye (event loc 60; overland C2 (10,2); retrieve goblet from cave below)",
    12: "World: Corak's Cave (map 22), tile (11,7) — Lloyd's Beacon behind fake walls (event loc 22)",
    19: (
        "World: Sandsobar (map 4), tile (7,4) — beggar at \"The Beggar's Gift\" "
        "(door signs evt 41/42); event loc 4 evt 43, exec_selector 0x51, 100 gp "
        "(FAQ §3-2-2, §4-3)"
    ),
    27: "World: Overland C1 (map 7), tile (1,8) — Fingers of Death scroll; eat Devil's Food Brownie, face east (event loc 7)",
    36: "World: Overland A2 (map 9), tile (15,11) — Mist Warrior (Dancing Sword; event loc 9, tile (11,15))",
    46: "World: Overland D1 (map 8), tile (5,6) — Star Burst in Dead Zone (event loc 8, tile (6,5); FAQ: Nature's Gate on day 93)",
    47: "World: Gemmaker's Cave (event loc 31, tile (3,3))",
    67: "World: Overland C2 (map 11), tile (11,1) — Walk on Water for 50 gp (event loc 11, tile (1,11))",
    # Cleric
    57: "World: Overland C3 (map 14), tile (1,9) — Nature's Gate after Red Hot Wolf Nipple Chips (event loc 14, tile (9,1); exec_selector 0x59; strings event loc 64)",
    69: "World: Overland A1 (map 5), tile (8,8) — Air Transmutation scroll (event loc 5; OP_2E)",
    74: "World: Overland A1 (map 5), tile (1,14) — Air Encasement after Elemental Plane of Air (event loc 5, tile (14,1); OP_2E)",
    76: "World: Murray's Cave (map 26), tile (1,8) — Frenzy after 15 Crazed Natives (event loc 16; FAQ §3-2-2 C5-3)",
    79: "World: Overland E4 (map 40), tile (8,8) — Earth Transmutation tablet (event loc 40; OP_2E)",
    82: "World: Overland A4 (map 15), tile (1,1) — Water Encasement after Elemental Plane of Water (event loc 15; OP_2E)",
    83: "World: Overland A4 (map 15), tile (8,8) — Water Transmutation scroll (event loc 15; OP_2E)",
    84: "World: Overland E4 (map 40), tile (14,1) — Earth Encasement after Elemental Plane of Earth (event loc 40, tile (1,14); OP_2E)",
    88: "World: Overland E1 (map 33), tile (14,14) — Fire Encasement after Elemental Plane of Fire (event loc 33; OP_2E)",
    89: "World: Overland E1 (map 33), tile (8,8) — Fire Transmutation plaque (event loc 33; OP_2E)",
    92: "World: Druid's Cave (map 27), tile (15,14) — Elder Druid quest vs Horvath (exec_selector 0x67; strings event loc 66)",
    93: "World: Overland C1 (map 7), tile (5,5) — Holy Word carved on tree, face south (event loc 7; FAQ §3-2-2)",
}

# Extra world lines (multi-location spells)
WORLD_GRANTS_EXTRA: dict[int, list[str]] = {}


def spell_label(record_index: int) -> str:
    school, flat = record_school_flat(record_index)
    lv, num, name = spell_at(school, flat)
    return f"{school}{lv}/{num} {name}"


def cast_summary(record_index: int, b0: int, b1: int) -> str:
    rec = decode_record(b0, b1)
    parts = [CAST_NAMES[rec["cast"]]]
    if rec["outdoor"]:
        parts.append("Outdoor")
    if rec["cast"] == "C" and (b0 & 0x20):
        parts.append("not hand-to-hand")
    return "; ".join(parts)


def load_item_spell_map(items_path: Path) -> dict[int, list[str]]:
    """spells.dat index -> item names that teach/cast via use-byte."""
    out: dict[int, list[str]] = {}
    for it in load_items(str(items_path)):
        eff = it.effect_byte
        if eff == 0:
            continue
        if eff < 0x80:
            continue
        if eff <= 0xB0:
            flat = eff - 0x80
            idx = flat - 1
        else:
            flat = eff - 0xB0
            idx = 48 + flat - 1
        out.setdefault(idx, []).append(it.name.strip())
    for idx in out:
        out[idx].sort()
    return out


def bought_lines(record_index: int, shop: dict) -> list[str]:
    school, flat = record_school_flat(record_index)
    lines: list[str] = []
    if school == "S":
        for town, slots in shop["static_by_town"]["mage_guild_spells"]["mage_guild_spells_by_town"].items():
            for slot in slots:
                if slot["spells_dat_index"] == record_index:
                    key = ["A", "B", "C", "D"][slot["slot"]]
                    lines.append(f"Bought: Mage guild {town} (key {key})")
    else:
        for town, slots in shop["static_by_town"]["temple_spells"]["cleric_spells_by_town"].items():
            for slot in slots:
                if slot["spells_dat_index"] == record_index:
                    key = ["D", "E", "F"][slot["slot"]]
                    lines.append(f"Bought: Temple {town} (cleric {key})")
    return lines


def acquisition_parts(record_index: int, shop: dict, item_map: dict[int, list[str]]) -> list[str]:
    parts: list[str] = []
    parts.extend(bought_lines(record_index, shop))
    if record_index in item_map:
        for name in item_map[record_index]:
            parts.append(f"Item: {name}")
    if record_index in WORLD_GRANTS:
        parts.append(WORLD_GRANTS[record_index])
    for extra in WORLD_GRANTS_EXTRA.get(record_index, []):
        if extra not in parts:
            parts.append(extra)
    if not parts:
        school, flat = record_school_flat(record_index)
        cls = "Sorcerer" if school == "S" else "Cleric"
        lv, _, _ = spell_at(school, flat)
        parts.append(f"Learn: {cls} level {lv} (assumed)")
    return parts


def temple_table(shop: dict, town: str) -> list[str]:
    rows = shop["static_by_town"]["temple_spells"]["cleric_spells_by_town"][town]
    lines = ["| Key | Spell | GP |", "|-----|-------|-----|"]
    keys = ["D", "E", "F"]
    for slot in rows:
        idx = slot["spells_dat_index"]
        lines.append(f"| {keys[slot['slot']]} | {spell_label(idx)} | {slot.get('gold_gp', '?')} |")
    return lines


def guild_table(shop: dict, town: str) -> list[str]:
    rows = shop["static_by_town"]["mage_guild_spells"]["mage_guild_spells_by_town"][town]
    lines = ["| Key | Spell | GP |", "|-----|-------|-----|"]
    keys = ["A", "B", "C", "D"]
    for slot in rows:
        idx = slot["spells_dat_index"]
        lines.append(f"| {keys[slot['slot']]} | {spell_label(idx)} | {slot.get('gold_gp', slot.get('gold', ''))} |")
    return lines


def world_appendix_rows() -> list[str]:
    rows: list[tuple[str, str]] = []
    seen: set[tuple[int, str]] = set()
    for idx in sorted(set(WORLD_GRANTS) | set(WORLD_GRANTS_EXTRA)):
        label = spell_label(idx)
        for note in [WORLD_GRANTS.get(idx, "")] + WORLD_GRANTS_EXTRA.get(idx, []):
            if not note:
                continue
            key = (idx, note)
            if key in seen:
                continue
            seen.add(key)
            rows.append((label, note))
    lines = ["| Spell | Acquisition (world) |", "|-------|---------------------|"]
    for label, note in rows:
        lines.append(f"| {label} | {note} |")
    return lines


def item_appendix(item_map: dict[int, list[str]]) -> list[str]:
    lines = ["| Item | Spell |", "|------|-------|"]
    for idx in sorted(item_map):
        for name in item_map[idx]:
            lines.append(f"| {name} | {spell_label(idx)} |")
    return lines


def spell_table(school: str, shop: dict, item_map: dict[int, list[str]]) -> list[str]:
    lines = [
        "| Lv | # | Name | Cast | Acquisition |",
        "|----|---|------|------|-------------|",
    ]
    for i, meta in enumerate(SPELL_META):
        sch, flat = record_school_flat(i)
        if sch != school:
            continue
        lv, num, name = spell_at(sch, flat)
        b0, b1 = 0, 0  # cast line from manual meta flags
        # Re-read spells.dat if present for accurate cast flags
        sp_path = ROOT / "spells.dat"
        if sp_path.is_file():
            data = sp_path.read_bytes()
            b0, b1 = data[i * 2], data[i * 2 + 1]
        cast = cast_summary(i, b0, b1)
        acq = "; ".join(acquisition_parts(i, shop, item_map))
        lines.append(f"| {lv} | {num} | {name} | {cast} | {acq} |")
    return lines


def build_markdown(shop: dict, item_map: dict[int, list[str]]) -> str:
    out: list[str] = []
    out.append("# MM2 spell sources (master index)")
    out.append("")
    out.append("All **96** `spells.dat` records (48 sorcerer + 48 cleric): where each spell")
    out.append("can be obtained in retail data, in plain-language **Acquisition** notes.")
    out.append("")
    out.append("Related docs:")
    out.append("")
    out.append("- [19-spells-and-item-use.md](19-spells-and-item-use.md) — `spells.dat` flags, item use-byte encoding")
    out.append("- [28-town-services.md](28-town-services.md) — temple / mage guild / training handlers")
    out.append("- [18-items-dat-format.md](18-items-dat-format.md) — item effect byte @ `0x0F`")
    out.append("- [07-event-script-opcodes.md](07-event-script-opcodes.md) — `OP_2E` / `OP_1F` party effects, HoS hints")
    out.append("- Machine-readable shops: [`../shop_tables.json`](../shop_tables.json) (`tools/dump_shop_tables.py`)")
    out.append("- Player FAQ cross-check: [`Might and Magic FAQ.txt`](Might%20and%20Magic%20FAQ.txt) §3-2-2 (WHERE TO FIND THEM)")
    out.append("")
    out.append("Overland **sector tiles** use FAQ notation **(column, row)** — e.g. C3 (1,9) north of Middlegate")
    out.append("ferry line. Guild/temple GP below = `decode_gold_encoding()` on data-hunk bytes (`$66CE` guild,")
    out.append("`$66F6` temple) via handlers `0x1D97A` / `0x1DAC6` — matches FAQ §3-2-2 dollar amounts.")
    out.append("")
    out.append("## Acquisition types")
    out.append("")
    out.append("| Type | Meaning |")
    out.append("|------|---------|")
    out.append("| **Learn** | Gained by leveling the matching spellcasting class (spell level on roster); assumed when not listed in static shop/item/world traces below. |")
    out.append("| **Bought** | Temple cleric (**D/E/F**) or mage guild (**A/B/C/D**) — guild requires membership. |")
    out.append("| **Item** | Cast/teach via `items.dat` use-power (`0x0F`); one charge per use, not permanent spellbook unless noted in-game. |")
    out.append("| **World** | Overland return tile, dungeon quest, or `exec_selector` script (`event.dat`); **not** the elemental-plane map screens 41–44 themselves for encase/transmute grants. |")
    out.append("")
    out.append("Cast restrictions (combat / outdoor / dungeon) come from `spells.dat` only — they are")
    out.append("not acquisition paths. **Unknown** would mean no static trace; this doc marks those as")
    out.append("**Learn (assumed)** for the spell's school and level.")
    out.append("")
    out.append("## Temple cleric stock (per town)")
    out.append("")
    out.append("Temple **`OP_0E` `0x03`** sells **three cleric spells per town** only (`A4-$66F6`, handler **`0x1DAC6`**, loop **`cmpi #3`** @ `0x1DF1A`). Menu keys **`D` / `E` / `F`**. Gold = **`decode_gold_encoding(A4-$66F6[town×4+slot])`** (stored in **`A4-$56BE[slot+3]`** for purchase @ **`0x1D872`**). **No sorcerer/mage spells at temple.**")
    out.append("")
    for town in shop["towns"]:
        out.append(f"### {town}")
        out.append("")
        out.extend(temple_table(shop, town))
        out.append("")
    out.append("## Mage guild (per town)")
    out.append("")
    out.append("Guild **`OP_0E` `0x05`** sells **four sorcerer spells per town** (`A4-$66E2[town×4+slot]`, handler **`0x1D97A`**, loop **`cmpi #4`** @ `0x1E64A`). Gold = **`decode_gold_encoding(A4-$66CE[town×4+slot])`** (stored in **`A4-$56BE[slot]`** for purchase @ **`0x1D872`**). Menu keys **`A` / `B` / `C` / `D`** (dispatch sub **`#$41`** @ `0x1E864`). **No cleric spells.**")
    out.append("")
    for town in shop["towns"]:
        out.append(f"### {town}")
        out.append("")
        out.extend(guild_table(shop, town))
        out.append("")
    out.append("## Sorcerer spells")
    out.append("")
    out.extend(spell_table("S", shop, item_map))
    out.append("")
    out.append("## Cleric spells")
    out.append("")
    out.extend(spell_table("C", shop, item_map))
    out.append("")
    out.append("## Items that teach/cast spells")
    out.append("")
    out.extend(item_appendix(item_map))
    out.append("")
    out.append("## World grants traced in `event.dat`")
    out.append("")
    out.extend(world_appendix_rows())
    out.append("")
    out.append("---")
    out.append("")
    out.append("*Generated by `python tools/build_spell_sources_doc.py`. Regenerate after `python tools/dump_shop_tables.py --json EXTRACTED/shop_tables.json`.*")
    out.append("")
    return "\n".join(out)


def main() -> None:
    shop_path = ROOT / "EXTRACTED" / "shop_tables.json"
    items_path = ROOT / "items.dat"
    out_path = ROOT / "EXTRACTED" / "docs" / "31-spell-sources.md"

    if not shop_path.is_file():
        print(f"missing {shop_path}; run dump_shop_tables.py first", file=sys.stderr)
        sys.exit(1)
    shop = json.loads(shop_path.read_text(encoding="utf-8"))
    item_map = load_item_spell_map(items_path) if items_path.is_file() else {}

    text = build_markdown(shop, item_map)
    out_path.write_text(text, encoding="utf-8")
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()

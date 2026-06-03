#!/usr/bin/env python3
"""MM2 symbol table — load, save, harvest, merge."""

from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SYMBOLS = ROOT / "EXTRACTED" / "mm2_symbols.yaml"
DOCS_DIR = ROOT / "EXTRACTED" / "docs"
IRA_ASM = ROOT / "EXTRACTED" / "mm2.asm"

# Code addresses below 0x1000 or above this are usually data / false positives.
CODE_ADDR_MIN = 0x1000
CODE_ADDR_MAX = 0x40000

# Skip doc table rows whose "name" is really a byte flag, roster offset, or opcode row.
_REJECT_NAMES = frozenset(
    {
        "area_id",
        "map_category",
        "tileset_id",
        "env_type",
        "surface_flag",
        "field_09",
        "field_0D",
        "entry_coord",
        "era_gate",
        "pad",
        "label",
        "recall_coord",
        "recall_screen",
        "flags",
        "flags2",
        "lf",
        "so",
        "si",
        "dle",
        "dc1",
        "dc2",
        "dc3",
        "dc4",
        "nak",
        "always",
        "dir_n",
        "dir_special",
        "enter",
        "enter_special",
        "any_dir",
        "combat_only",
        "non_combat_only",
        "outdoor_only",
        "open_tavern_food",  # OP_0E id, not a function — handler is separate
        "open_inn_lodging",
        "open_temple",
        "open_training",
        "open_mages_guild",
        "open_blacksmith_shop",
        "open_general_store",
        "open_special_shop",
        "goblet_quest",
        "enroll_mages_guild",
    }
)

# Harvested names that are doc artifacts (opcode argc labels, IRA gaps, mnemonics).
_REJECT_NAME_PATTERNS = (
    re.compile(r"^\d+$"),
    re.compile(r"^OP_\d", re.I),
    re.compile(r"^var$", re.I),
    re.compile(r"^(bchg|addq|moveq|jsr|bra)$", re.I),
)


def _accept_function_name(name: str) -> bool:
    if name.lower() in _REJECT_NAMES:
        return False
    if name.lower() in ("unknown", "read"):
        return False
    if not re.match(r"^[a-z][a-z0-9_]*$", name, re.I):
        return False
    for pat in _REJECT_NAME_PATTERNS:
        if pat.match(name):
            return False
    return len(name) >= 3


def prune_symbols(data: dict) -> dict:
    """Drop harvested noise from the functions map."""
    funcs = data.get("functions", {})
    data["functions"] = {
        k: v for k, v in funcs.items() if _accept_function_name(v.get("name", ""))
    }
    return data

# Harvested slug → canonical name for combat / unnamed routines.
_COMBAT_SLUGS: dict[str, str] = {
    "round loop": "combat_round_loop",
    "player turn": "combat_player_turn",
    "build command bar + capability flags": "combat_build_command_bar",
    "read valid command key": "combat_read_command_key",
    "print one command-bar entry": "combat_print_command_entry",
    'target selector ("which (a - x)?")': "combat_target_selector",
    "monster turn / ai dispatch": "combat_monster_turn",
    "monster group attack (verb table a4-$6e56)": "combat_monster_group_attack",
    "monster single attack": "combat_monster_single_attack",
    "melee resolution": "combat_melee_resolution",
    'flee ("runs")': "combat_flee",
    "multiply / breed": "combat_multiply_breed",
    "adds friends (reinforcements)": "combat_adds_friends",
    "per-monster reward decode": "combat_reward_decode",
    "victory / end combat": "combat_victory",
    "defeat / retreat": "combat_defeat_retreat",
    "instantiate monster slot / all slots": "combat_instantiate_monsters",
    "monster stat unpacker (doc 16)": "monster_stat_unpacker",
    "spell-effect jump table": "spell_effect_jump_table",
    "front wall": "hood_draw_front_wall",
    "left side": "hood_draw_left_wall",
    "right side": "hood_draw_right_wall",
    "main scheduler": "main_loop_scheduler",
    "(era gate)": "event_era_gate",
    "(read helper)": "event_script_read_byte",
}


def empty_symbols() -> dict:
    return {"functions": {}, "a4_fields": {}, "a4_thunks": {}}


def parse_addr(s: str) -> int | None:
    s = s.strip().lower()
    if s.startswith("0x"):
        try:
            return int(s, 16)
        except ValueError:
            return None
    if s.startswith("$"):
        try:
            return int(s[1:], 16)
        except ValueError:
            return None
    return None


def fmt_addr(addr: int) -> str:
    return f"0x{addr:X}"


def fmt_a4_off(off: int) -> str:
    """Signed A4 displacement as `-0xNNNN` string."""
    if off > 0x7FFF:
        off -= 0x10000
    if off < 0:
        return f"-0x{-off:X}"
    return f"0x{off:X}"


def parse_a4_off(s: str) -> int | None:
    s = s.strip()
    if s.startswith("-0x") or s.startswith("-0X"):
        try:
            return -int(s[3:], 16)
        except ValueError:
            return None
    if s.startswith("0x") or s.startswith("0X"):
        v = int(s, 16)
        return v if v <= 0x7FFF else v - 0x10000
    if s.startswith("$"):
        v = int(s[1:], 16)
        return v if v <= 0x7FFF else v - 0x10000
    return None


def _slugify(text: str) -> str:
    t = text.strip().lower()
    t = re.sub(r"\([^)]*\)", "", t)
    t = re.sub(r"`[^`]*`", "", t)
    t = re.sub(r"[^a-z0-9]+", "_", t).strip("_")
    return t[:64] if t else "unknown"


def _is_code_addr(addr: int) -> bool:
    return CODE_ADDR_MIN <= addr <= CODE_ADDR_MAX


def _entry(name: str, source: str, note: str = "") -> dict:
    e: dict = {"name": name, "source": source}
    if note:
        e["note"] = note.strip()
    return e


def load_symbols(path: Path = DEFAULT_SYMBOLS) -> dict:
    if not path.exists():
        return empty_symbols()
    text = path.read_text(encoding="utf-8")
    if not text.strip():
        return empty_symbols()
    try:
        import yaml  # type: ignore

        data = yaml.safe_load(text)
    except ImportError:
        data = _parse_simple_yaml(text)
    if not isinstance(data, dict):
        return empty_symbols()
    out = empty_symbols()
    for key in out:
        section = data.get(key)
        if isinstance(section, dict):
            out[key] = section
    return out


def save_symbols(data: dict, path: Path = DEFAULT_SYMBOLS) -> None:
    try:
        import yaml  # type: ignore

        path.write_text(
            yaml.safe_dump(data, sort_keys=False, allow_unicode=True, width=100),
            encoding="utf-8",
        )
        return
    except ImportError:
        pass
    path.write_text(_dump_simple_yaml(data), encoding="utf-8")


def _parse_simple_yaml(text: str) -> dict:
    """Minimal YAML subset: top-level sections, `key: value` entries."""
    data = empty_symbols()
    section: str | None = None
    current_key: str | None = None
    for raw in text.splitlines():
        line = raw.rstrip()
        if not line or line.lstrip().startswith("#"):
            continue
        if not line.startswith(" ") and line.endswith(":"):
            sec = line[:-1].strip()
            if sec in data:
                section = sec
                current_key = None
            continue
        if section is None:
            continue
        m = re.match(r"^\s{2}([^:\s]+):\s*$", line)
        if m:
            current_key = m.group(1)
            data[section][current_key] = {}
            continue
        m = re.match(r"^\s{4}(\w+):\s*(.*)$", line)
        if m and current_key:
            val = m.group(2).strip().strip('"').strip("'")
            data[section][current_key][m.group(1)] = val
    return data


def _dump_simple_yaml(data: dict) -> str:
    lines = [
        "# MM2 symbol table — single source of truth for RE names.",
        "# Addresses: code-hunk load addresses (Capstone base 0).",
        "# A4 keys: signed offsets matching mm2_gamestate.h (-0x79B6).",
        "# Regenerate harvest: python tools/harvest_symbols.py --merge",
        "# Apply to asm:       python tools/apply_symbols.py",
        "",
    ]
    for section in ("functions", "a4_fields", "a4_thunks"):
        lines.append(f"{section}:")
        items = data.get(section, {})
        for key in sorted(items, key=lambda k: int(k, 16) if k.startswith(("0x", "-0x")) else k):
            entry = items[key]
            lines.append(f"  {key}:")
            for field in ("name", "note", "source"):
                if field in entry:
                    lines.append(f"    {field}: {entry[field]}")
        lines.append("")
    return "\n".join(lines)


def harvest_from_docs(docs_dir: Path = DOCS_DIR) -> dict:
    out = empty_symbols()
    if not docs_dir.is_dir():
        return out

    pat_addr_name = re.compile(
        r"\|\s*`(0x[0-9A-Fa-f]+)`\s*\|\s*`([a-z][a-z0-9_]*)`", re.I
    )
    pat_name_addr = re.compile(
        r"\|\s*`([a-z][a-z0-9_]*)`\s*\|\s*`(0x[0-9A-Fa-f]+)`", re.I
    )
    pat_addr_desc = re.compile(r"\|\s*`(0x[0-9A-Fa-f]+)`\s*\|\s*([^|`\n]+?)\s*\|")
    pat_backtick_name_addr = re.compile(
        r"`([a-z][a-z0-9_]*)`[^`\n]{0,40}@\s*`?(0x[0-9A-Fa-f]+)`?", re.I
    )
    pat_a4_field = re.compile(r"\|\s*`-\$([0-9A-Fa-f]+)`\s*\|\s*`([a-z][a-z0-9_]*)`", re.I)
    pat_a4_thunk = re.compile(r"`-\$([0-9A-Fa-f]+)`\s*:\s*([^\n|]+)")
    pat_named_routine = re.compile(
        r"`([a-z][a-z0-9_]*)`\s*`(0x[0-9A-Fa-f]+)`", re.I
    )

    for md in sorted(docs_dir.glob("*.md")):
        rel = f"docs/{md.name}"
        text = md.read_text(encoding="utf-8", errors="replace")

        for addr_s, name in pat_addr_name.findall(text):
            addr = parse_addr(addr_s)
            if addr is None or not _is_code_addr(addr):
                continue
            if name.lower() in _REJECT_NAMES:
                continue
            if not _accept_function_name(name):
                continue
            key = fmt_addr(addr)
            out["functions"].setdefault(key, _entry(name, rel))

        for name, addr_s in pat_name_addr.findall(text):
            addr = parse_addr(addr_s)
            if addr is None or not _is_code_addr(addr):
                continue
            if name.lower() in _REJECT_NAMES:
                continue
            if not _accept_function_name(name):
                continue
            key = fmt_addr(addr)
            out["functions"].setdefault(key, _entry(name, rel))

        for name, addr_s in pat_backtick_name_addr.findall(text):
            addr = parse_addr(addr_s)
            if addr is None or not _is_code_addr(addr):
                continue
            if name.lower() in _REJECT_NAMES:
                continue
            if not _accept_function_name(name):
                continue
            key = fmt_addr(addr)
            out["functions"].setdefault(key, _entry(name, rel))

        for addr_s, desc in pat_addr_desc.findall(text):
            addr = parse_addr(addr_s)
            if addr is None or not _is_code_addr(addr):
                continue
            desc_clean = desc.strip().strip("*").strip()
            if not desc_clean or desc_clean.startswith("`"):
                continue
            if desc_clean.startswith("(") and "gate" in desc_clean.lower():
                slug = _COMBAT_SLUGS.get(desc_clean.lower(), _slugify(desc_clean))
            else:
                slug = _COMBAT_SLUGS.get(desc_clean.lower(), _slugify(desc_clean))
            if not _accept_function_name(slug):
                continue
            key = fmt_addr(addr)
            if key not in out["functions"]:
                out["functions"][key] = _entry(slug, rel, desc_clean)

        for off_s, name in pat_a4_field.findall(text):
            off = -int(off_s, 16)
            if name.lower() in _REJECT_NAMES:
                continue
            if not _accept_function_name(name):
                continue
            key = fmt_a4_off(off)
            out["a4_fields"].setdefault(key, _entry(name, rel))

        for off_s, desc in pat_a4_thunk.findall(text):
            off = -int(off_s, 16)
            name = _slugify(desc.split("(")[0])
            if not name or name == "unknown":
                continue
            key = fmt_a4_off(off)
            out["a4_thunks"].setdefault(key, _entry(name, rel, desc.strip()))

    return out


def harvest_ira_func_labels(asm_path: Path = IRA_ASM) -> dict:
    """Import existing FUNC_* labels from the IRA listing."""
    out = empty_symbols()
    if not asm_path.is_file():
        return out
    pat = re.compile(r"^(FUNC_[A-Za-z0-9_]+):\s*\n\s+\S+\s+[^;]*;\s*([0-9a-f]+):", re.M)
    for m in pat.finditer(asm_path.read_text(encoding="utf-8", errors="replace")):
        label, addr_s = m.group(1), m.group(2)
        addr = int(addr_s, 16)
        if not _is_code_addr(addr):
            continue
        name = label[5:]  # strip FUNC_
        # snake_case from PascalCase chunks
        name = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name).lower()
        key = fmt_addr(addr)
        out["functions"].setdefault(
            key, _entry(name, "EXTRACTED/mm2.asm", f"IRA label {label}")
        )
    return out


def merge_symbols(base: dict, *others: dict, prefer: str = "base") -> dict:
    """Merge symbol dicts. `prefer=base` keeps existing entries; `prefer=new` overwrites."""
    out = empty_symbols()
    for section in out:
        out[section] = dict(base.get(section, {}))
    for other in others:
        for section in out:
            for key, entry in other.get(section, {}).items():
                if prefer == "new" or key not in out[section]:
                    out[section][key] = entry
    return out


def lookup_function(symbols: dict, addr: int) -> dict | None:
    return symbols.get("functions", {}).get(fmt_addr(addr))


def lookup_a4_field(symbols: dict, off: int) -> dict | None:
    return symbols.get("a4_fields", {}).get(fmt_a4_off(off))


def lookup_a4_thunk(symbols: dict, disp: int) -> dict | None:
    """disp is the positive magnitude used in `jsr -$NNNN(a4)`."""
    off = -disp if disp > 0 else disp
    return symbols.get("a4_thunks", {}).get(fmt_a4_off(off))


def build_bootstrap() -> dict:
    """Curated seed symbols not reliably caught by harvest regexes."""
    manual = empty_symbols()
    f = manual["functions"]
    src = "tools/mm2_symbols.py (bootstrap)"

    curated = {
        0x248AE: ("startup_main", "Primary init after JMP from 0x20"),
        0x24920: ("set_a4_workspace", "LEA $7FFE,A4"),
        0x24928: ("init_runtime_heap", "MANX arena + AllocMem"),
        0x2900: ("hood_render_3d", "3D view from map page 0"),
        0x4C8E: ("monster_stat_unpacker", "monsters.dat unpack"),
        0x92F2: ("event_dat_loader", "Load event.dat location record"),
        0x1280: ("main_loop_scheduler", "Main scheduler hub"),
        0x133EC: ("combat_use_item_handler", "Item use-effect dispatch"),
        0x160C2: ("op_0e_service_dispatch", "OP_0E town service router"),
        0x172BC: ("event_era_gate", "Era vs attrib+0x0F gate"),
        0x172CA: ("event_script_interpreter", "Event VM dispatch"),
        0x1750C: ("event_script_fetch_next", "Fetch next script byte"),
        0x1754A: ("event_system_init", "Triplet scan + string anchor"),
        0x175E2: ("event_tile_scanner", "Tile event matcher"),
        0x176B6: ("event_queued_dispatch", "Secondary queued-event path"),
        0x2182: ("automap_cell_frame", "Auto-map tile frame helper"),
        0x223A: ("overland_map_view", "16x16 overland minimap draw"),
        0x24C4: ("overland_map_view_loop", "Overland movement loop"),
        0x256E: ("load_screen_context", "Screen/mode materialize"),
        0x26030: ("items_gold_byteswap", "Byte-swap items.dat gold on load"),
        0x29868: ("load_spells_dat", "spells.dat loader path"),
        0x53A0: ("session_interaction_gate", "Darkness / modal gate"),
        0x545E: ("session_start_refresh", "Session restart refresh"),
        0x823C: ("audio_init", "Master audio blob → A4 tables"),
        0x64F8: ("play_song_scripted", "Scripted score playback"),
        0x6798: ("audio_wait_helper", "Score wait/tempo helper"),
        0x7DCC: ("play_death_tones", "Party wipe tone sequence"),
        0x9BCA: ("training_hp_restore", "Training HP path"),
        0x9B48: ("training_mode_select", "Stat vs HP training mode"),
        0x1A132: ("open_tavern_food", "Pub / food / drinks"),
        0x19EC0: ("open_inn_lodging", "Inn rest handler"),
        0x1C54A: ("open_blacksmith_shop", "Blacksmith UI"),
        0x1C898: ("training_stat_apply", "Stat training tables"),
        0x1D208: ("open_temple", "Temple services"),
        0x1D872: ("shop_gold_debit", "Purchase gold debit"),
        0x1D97A: ("guild_spell_price", "Sorcerer spell price decode"),
        0x1DAC6: ("temple_spell_price", "Cleric spell price decode"),
        0x1E870: ("open_mages_guild", "Mage guild entry (alt trace)"),
        0x1F800: ("open_training", "Training menu entry (alt trace)"),
        0x1064C: ("combat_monster_turn", "Monster AI turn"),
        0x106A0: ("combat_monster_ai_dispatch", "Monster AI dispatch"),
        0x10B74: ("combat_reward_decode", "Per-monster rewards"),
        0x11646: ("combat_defeat_retreat", "Party defeat / retreat"),
        0x1175C: ("combat_read_command_key", "Command bar key read"),
        0x11866: ("combat_build_command_bar", "Draw command bar"),
        0x119C2: ("combat_player_turn", "Player combat turn"),
        0x11C2C: ("combat_instantiate_monster_slot", "One monster slot"),
        0x11C82: ("combat_instantiate_all_monsters", "All monster slots"),
        0x12430: ("combat_victory", "Victory + rewards"),
        0x12A22: ("combat_round_loop", "Combat round loop"),
        0x155BE: ("event_script_read_byte", "work_buf[parse_pos++]"),
        0x157FC: ("event_token_skip_helper", "Skip N VM tokens"),
        0x15884: ("event_text_resolve_u8", "String index → pointer"),
        0x171AC: ("event_script_cleanup", "End script / OP_0F"),
        0x17262: ("event_handler_pool_seek", "Skip FF-delimited records"),
        0x2BCC: ("hood_cell_solid_test", "Wall solidity mask test"),
        0x2C1A: ("hood_draw_front_wall", "Front wall blit"),
        0x2C9A: ("hood_draw_left_wall", "Left wall blit"),
        0x2DB4: ("hood_draw_right_wall", "Right wall blit"),
    }
    for addr, (name, note) in curated.items():
        f[fmt_addr(addr)] = _entry(name, src, note)

    th = manual["a4_thunks"]
    for disp, (name, note) in {
        0x83C2: ("frame_input_pump", "Frame/input pump"),
        0x83CE: ("set_update_mode", "Set/update screen mode"),
        0x83E0: ("draw_glyph_tile", "Draw glyph/tile"),
        0x842E: ("read_key", "Read keyboard"),
        0x8440: ("delay_tick", "Delay / tick sleep"),
        0x8392: ("map_render_helper_a", "Map rendering helper"),
        0x8398: ("map_render_helper_b", "Map rendering helper"),
        0x802C: ("engine_init", "Major engine init"),
        0x8140: ("print_string", "Print string"),
        0x8158: ("display_setup_a", "Display pipeline"),
        0x815E: ("display_setup_b", "Display pipeline"),
        0x8146: ("display_setup_c", "Display pipeline"),
        0x7D3A: ("event_tile_scanner_thunk", "Thunk → event_tile_scanner"),
        0x7BB4: ("rng_roll", "RNG helper (1..max)"),
        0x7F20: ("get_party_member_ptr", "Character object by index"),
        0x7C62: ("combat_fanfare", "Combat music fanfare"),
        0x7BDE: ("play_tone", "Tone with duration"),
        0x7BD2: ("spell_ui_tone", "Spell UI tone helper"),
        0x7FD4: ("walk_beep", "Footstep beep"),
    }.items():
        th[fmt_a4_off(-disp)] = _entry(name, "docs/02-runtime-memory-map.md", note)

    return manual


def export_json(symbols: dict, path: Path) -> None:
    path.write_text(json.dumps(symbols, indent=2, sort_keys=True) + "\n", encoding="utf-8")

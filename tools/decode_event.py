#!/usr/bin/env python3
"""Decode event.dat from Might & Magic II (Amiga).

Format (derived from 68k asm at 0x92F2, 0x1754A, 0x175E2):

File layout:
  [header]  71 entries x 6 bytes = 426 bytes
  [data]    71 variable-length location records (contiguous)

Header entry (6 bytes, big-endian):
  uint32  absolute_offset   (from file start)
  uint16  data_length       (clamped to 0x8AC = 2220 max at runtime)

Per-location record:
  1. Tile event table: 3-byte triplets until 00 00 00
       byte 0: tile position = (y << 4) | x  in 16x16 grid
       byte 1: event handler ID (indexes the event script section)
       byte 2: condition flags (AND-masked with context to gate firing)
  2. String offset word (2 bytes, little-endian):
       distance from this word's position to string table start
  3. Event script bytecodes (opcode interpreter, ~51 opcodes)
  4. String table: 0xFF-terminated strings

Relationship to map.dat:
  map.dat = 60 screens x 512 bytes (tile/terrain visual data)
  event.dat = 71 locations x variable (interactions/scripts)
  Both indexed by screen_id; event.dat has extra entries for
  dungeon sublevels and special areas.
"""

import struct
import sys
from pathlib import Path


def read_header(data: bytes) -> list[tuple[int, int]]:
    entries = []
    for i in range(71):
        off = struct.unpack_from(">I", data, i * 6)[0]
        length = struct.unpack_from(">H", data, i * 6 + 4)[0]
        entries.append((off, length))
    return entries


def decode_triplets(blob: bytes) -> tuple[list[tuple[int, int, int]], int, bool]:
    """Parse tile-event triplets. Returns (triplets, end_offset, terminated).

    Confirmed against the engine scanner at 0x1754A: 3-byte groups
    (pos, event, cond) are read from offset 0 and the table ends at the first
    ``00 00 00`` group. ``pos`` here equals the engine's ``terminator+3``.
    ``terminated`` is False when no 00 00 00 group exists (special non
    tile-event records such as the castles / Hall of Spells).
    """
    triplets = []
    pos = 0
    terminated = False
    while pos + 2 < len(blob):
        a, b, c = blob[pos], blob[pos + 1], blob[pos + 2]
        pos += 3
        if a == 0 and b == 0 and c == 0:
            terminated = True
            break
        triplets.append((a, b, c))
    return triplets, pos, terminated


def decode_strings(blob: bytes, str_offset: int) -> list[str]:
    """Extract 0xFF-terminated strings from the string table."""
    strings = []
    pos = str_offset
    while pos < len(blob):
        end = pos
        while end < len(blob) and blob[end] != 0xFF:
            end += 1
        s = blob[pos:end].decode("ascii", errors="replace")
        s = s.replace("@", "\n")  # @ = newline in MM2 strings
        strings.append(s)
        pos = end + 1
    return strings


def looks_like_text_record(seg: bytes) -> bool:
    """Heuristic: a 0xFF-pool record that is plain text, not an opcode script.

    Event ids index a single 0xFF-delimited pool that mixes opcode scripts and
    pure-text records (verified at loc 67). Text records decode to readable
    ASCII; opcode scripts are dominated by control bytes <= 0x32.
    """
    if not seg:
        return False
    printable = sum(1 for b in seg if 0x20 <= b <= 0x7E or b in (0x40,))
    # Require a run of letters to avoid flagging short arg-heavy scripts.
    letters = sum(1 for b in seg if 0x41 <= b <= 0x7A)
    return len(seg) >= 4 and printable / len(seg) >= 0.85 and letters >= 3


def display_string(s: str) -> str:
    """Human-readable label for placeholder/sentinel text entries."""
    if s == "":
        return "<EMPTY>"
    if s == "z":
        return "<SENTINEL_Z>"
    if s == "\n":
        return "<NEWLINE>"
    return s


def decode_location(blob: bytes, loc_id: int) -> dict:
    triplets, pos, terminated = decode_triplets(blob)

    str_rel_lo = blob[pos] if pos < len(blob) else 0
    str_rel_hi = blob[pos + 1] if pos + 1 < len(blob) else 0
    str_rel = (str_rel_hi << 8) | str_rel_lo
    str_table_offset = pos + str_rel
    script_start = pos + 2

    script_end = min(str_table_offset, len(blob))
    script_bytes = blob[script_start:script_end]

    strings = decode_strings(blob, str_table_offset) if str_table_offset < len(blob) else []

    script_len = max(0, str_table_offset - script_start)
    if not terminated:
        kind = "castle_blob"
    elif script_len == 0 and strings:
        kind = "string_bank"
    elif script_len > 500:
        kind = "mixed_pool"
    elif triplets and script_len > 0:
        kind = "standard"
    else:
        kind = "unknown"

    return {
        "location_id": loc_id,
        "record_kind": kind,
        "triplets": triplets,
        "script_offset": script_start,
        "script_length": len(script_bytes),
        "string_table_offset": str_table_offset,
        "strings": strings,
        "terminated": terminated,
    }


FLAG_NAMES = {
    0x10: "DIR_W?",
    0x20: "DIR_S?",
    0x40: "DIR_E?",
    0x80: "DIR_N?",
    0xF0: "ANY_DIR",
    0xC0: "DIR_N?+DIR_E?",
}

OPCODES: dict[int, dict[str, object]] = {
    0x00: {"name": "OP_00_INVALID", "argc": 0, "handler": "0x1748C"},
    0x01: {"name": "OP_01_SHOW_TEXT_BASIC", "argc": 1, "handler": "0x15924"},
    0x02: {"name": "OP_02_SHOW_TEXT_BLOCK", "argc": 1, "handler": "0x15942"},
    0x03: {"name": "OP_03_SHOW_TEXT", "argc": 1, "handler": "0x159CE"},
    0x04: {"name": "OP_04_SHOW_TEXT_ABOVE_DOOR", "argc": 1, "handler": "0x159F4"},
    0x05: {"name": "OP_05_SHOW_TEXT_POPUP_A", "argc": 1, "handler": "0x15A46"},
    0x06: {"name": "OP_06_SHOW_TEXT_POPUP_B", "argc": 1, "handler": "0x15AEE"},
    0x07: {"name": "OP_07_WAIT_SPACE", "argc": 0, "handler": "0x15CE6"},
    0x08: {"name": "OP_08_WAIT_KEY", "argc": 0, "handler": "0x15D26"},
    0x09: {"name": "OP_09_PROMPT_YN_TO_COND", "argc": 0, "handler": "0x15D3C"},
    0x0A: {"name": "OP_0A_PROMPT_YN_B", "argc": 0, "handler": "0x15D9A"},
    0x0B: {"name": "OP_0B_SHOW_SERVICE_WINDOW", "argc": 2, "handler": "0x15DB0"},
    0x0C: {"name": "OP_0C_MAP_TRANSITION", "argc": 2, "handler": "0x15E12"},
    0x0D: {"name": "OP_0D_ENGINE_CALL", "argc": 1, "handler": "0x15EC4"},
    0x0E: {"name": "OP_0E_EXEC_SELECTOR", "argc": 1, "handler": "0x160C2"},
    0x0F: {"name": "OP_0F_END_SCRIPT", "argc": 0, "handler": "0x162A6"},
    0x10: {"name": "OP_10_IF_TRUE_SKIPTOK", "argc": None, "handler": "0x162B8"},
    0x11: {"name": "OP_11_IF_FALSE_SKIPTOK", "argc": None, "handler": "0x162DC"},
    0x12: {"name": "OP_12_ENCOUNTER_SETUP", "argc": 12, "handler": "0x16300"},
    0x13: {"name": "OP_13_ENCOUNTER_SETUP_B", "argc": 10, "handler": "0x16386"},
    0x14: {"name": "OP_14_CLEAR_TILE_EVENT", "argc": 0, "handler": "0x16398"},
    0x15: {"name": "OP_15_APPLY_PARTY", "argc": 3, "handler": "0x16426"},
    0x16: {"name": "OP_16_CHECK_MONSTER_PRESENT", "argc": 2, "handler": "0x16520"},
    0x17: {"name": "OP_17_LOAD_VAR_TO_COND", "argc": 2, "handler": "0x165A4"},
    0x18: {"name": "OP_18_APPLY_PARTY_MASKED", "argc": 4, "handler": "0x165C6"},
    0x19: {"name": "OP_19_ADD_PARTY_ENTITY", "argc": 4, "handler": "0x165D8"},
    0x1A: {"name": "OP_1A_STORE_VAR8", "argc": 2, "handler": "0x166F8"},
    0x1B: {"name": "OP_1B_COND_THRESHOLD", "argc": 1, "handler": "0x16724"},
    0x1C: {"name": "OP_1C_ENGINE_QUERY_TO_COND", "argc": 1, "handler": "0x16742"},
    0x1D: {"name": "OP_1D_ENGINE_CALL_REC7", "argc": 1, "handler": "0x16762"},
    0x1E: {"name": "OP_1E_DELAY", "argc": 1, "handler": "0x16780"},
    0x1F: {"name": "OP_1F_PARTY_EFFECT", "argc": 6, "handler": "0x1690E"},
    0x20: {"name": "OP_20_PARTY_EFFECT_B", "argc": 6, "handler": "0x16A22"},
    0x21: {"name": "OP_21_SET_TILE", "argc": 3, "handler": "0x16A34"},
    0x22: {"name": "OP_22_CHECK_ERA_RANGE", "argc": 2, "handler": "0x16A9E"},
    0x23: {"name": "OP_23_CHECK_DAY_RANGE", "argc": 2, "handler": "0x16ADA"},
    0x24: {"name": "OP_24_CHECK_GOLD_TO_COND", "argc": 2, "handler": "0x16B54"},
    0x25: {"name": "OP_25_CHECK_CODE16_TO_COND", "argc": 2, "handler": "0x16B82"},
    0x26: {"name": "OP_26_SELECT_MEMBER", "argc": 0, "handler": "0x16BC0"},
    0x27: {"name": "OP_27_SELECT_MEMBER_B", "argc": 0, "handler": "0x16BC0"},
    0x28: {"name": "OP_28_CONSUME_ITEM_TO_COND", "argc": 2, "handler": "0x16C86"},
    0x29: {"name": "OP_29_SET_ABORT", "argc": 0, "handler": "0x16D08"},
    0x2A: {"name": "OP_2A_SET_TREASURE", "argc": 14, "handler": "0x16D16"},
    0x2B: {"name": "OP_2B_SKIPTOK", "argc": None, "handler": "0x16D74"},
    0x2C: {"name": "OP_2C_ADJUST_STATE", "argc": 1, "handler": "0x16D98"},
    0x2D: {"name": "OP_2D_CHECK_MEMBER_ATTR", "argc": 2, "handler": "0x16DBA"},
    0x2E: {"name": "OP_2E_SET_PARTY_ATTR", "argc": 2, "handler": "0x16F50"},
    0x2F: {"name": "OP_2F_CLEAR_INPUT_BUF", "argc": 0, "handler": "0x16FEA"},
    0x30: {"name": "OP_30_CHECK_ANSWER", "argc": 10, "handler": "0x17034"},
    0x31: {"name": "OP_31_PARTY_ITERATE_CALL", "argc": 3, "handler": "0x170BC"},
    0x32: {"name": "OP_32_LOAD_COND_FROM_VAR", "argc": 1, "handler": "0x17190"},
}

SELECTOR_NAMES = {
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

TRANSITION_NAMES = {
    (0x0B, 0x37): "overland_area_c3",
    (0x11, 0x8F): "cavern_of_middlegate",
}

# Field observations (user-verified):
# - Atlantium Location 1 shop-sign tiles are text-only retriggerers.
# - Atlantium arena gate is ticket-gated: no-ticket message -> with-ticket immediate combat,
#   ticket consumed, return to same gate tile, retrigger by stepping off/on.
# - There are four ticket items (green/yellow/red/black) and four town-purchased keys.
# - OP_24 appears to be a money predicate (u16 amounts commonly 10/25/50/200/500).
# - OP_25 is a non-gold 16-bit predicate path; values observed are small codes (0/1/2).
# - User verified: bishop key gates are blocked with message when missing key,
#   allowed when present, and key is consumed.


def format_triplet(t: tuple[int, int, int]) -> str:
    pos, evt, flags = t
    y = (pos >> 4) & 0xF
    x = pos & 0xF
    flag_str = FLAG_NAMES.get(flags, f"0x{flags:02X}")
    return f"  tile ({y:2d},{x:2d}) [0x{pos:02X}]  event={evt:3d} (0x{evt:02X})  cond={flag_str}"


def disassemble_script_from(script: bytes, start: int) -> list[str]:
    lines = []
    pos = start
    line_no = 0
    while pos < len(script):
        op_pos = pos
        op = script[pos]
        pos += 1
        if op == 0xFF:
            lines.append(f"    {line_no:02d}: +0x{op_pos:04X}  END")
            break

        spec = OPCODES.get(op)
        if spec is None:
            lines.append(f"    {line_no:02d}: +0x{op_pos:04X}  DB 0x{op:02X}  ; unknown opcode")
            break

        argc = spec["argc"]
        name = str(spec["name"])
        handler = str(spec["handler"])
        if argc is None:
            arg0 = script[pos] if pos < len(script) else 0
            if pos < len(script):
                pos += 1
            lines.append(
                f"    {line_no:02d}: +0x{op_pos:04X}  {name} 0x{arg0:02X}  ; variable-length token skip ({handler})"
            )
            lines.append("    -- decode stop: variable-length token stream follows --")
            break

        argc_i = int(argc)
        args = script[pos : pos + argc_i]
        pos += len(args)
        arg_text = " ".join(f"0x{b:02X}" for b in args)
        if len(args) < argc_i:
            lines.append(
                f"    {line_no:02d}: +0x{op_pos:04X}  {name} {arg_text}  ; truncated args ({handler})"
            )
            break

        if arg_text:
            lines.append(f"    {line_no:02d}: +0x{op_pos:04X}  {name} {arg_text}  ; {handler}")
        else:
            lines.append(f"    {line_no:02d}: +0x{op_pos:04X}  {name}  ; {handler}")
        line_no += 1
    return lines


def find_event_markers(script: bytes) -> list[int]:
    # 0x22 is the key marker checked in sub_17262 before dispatching to sub_172CA.
    return [i for i, b in enumerate(script) if b == 0x22]


def split_script_segments(script: bytes) -> list[bytes]:
    # sub_17262 scans bytes and counts 0xFF separators to locate event script N.
    segments: list[bytes] = []
    start = 0
    for i, b in enumerate(script):
        if b == 0xFF:
            segments.append(script[start:i])
            start = i + 1
    if start <= len(script):
        segments.append(script[start:])
    return segments


def parse_segment_linear(seg: bytes) -> list[tuple[int, list[int]]]:
    out: list[tuple[int, list[int]]] = []
    i = 0
    while i < len(seg):
        op = seg[i]
        i += 1
        spec = OPCODES.get(op)
        if spec is None:
            out.append((op, []))
            break
        argc = spec["argc"]
        if argc is None:
            # Variable-length token stream path; consume selector byte if present.
            if i < len(seg):
                out.append((op, [seg[i]]))
                i += 1
            else:
                out.append((op, []))
            break
        argc_i = int(argc)
        args = list(seg[i : i + argc_i])
        i += len(args)
        out.append((op, args))
        if len(args) < argc_i:
            break
    return out


def parse_segment_linear_nodes(seg: bytes) -> list[dict[str, object]]:
    out: list[dict[str, object]] = []
    i = 0
    while i < len(seg):
        off = i
        op = seg[i]
        i += 1
        spec = OPCODES.get(op)
        if spec is None:
            out.append({"off": off, "op": op, "args": []})
            break
        argc = spec["argc"]
        if argc is None:
            if i < len(seg):
                out.append({"off": off, "op": op, "args": [seg[i]]})
                i += 1
            else:
                out.append({"off": off, "op": op, "args": []})
            break
        argc_i = int(argc)
        args = list(seg[i : i + argc_i])
        i += len(args)
        out.append({"off": off, "op": op, "args": args})
        if len(args) < argc_i:
            break
    return out


def parse_segment_stream_nodes(seg: bytes) -> list[dict[str, object]]:
    """Decode full byte stream order (does not stop at SKIPTOK opcodes)."""
    out: list[dict[str, object]] = []
    i = 0
    while i < len(seg):
        off = i
        op = seg[i]
        i += 1
        spec = OPCODES.get(op)
        if spec is None:
            out.append({"off": off, "op": op, "args": []})
            break
        argc = spec["argc"]
        if argc is None:
            if i < len(seg):
                out.append({"off": off, "op": op, "args": [seg[i]]})
                i += 1
            else:
                out.append({"off": off, "op": op, "args": []})
            continue
        argc_i = int(argc)
        args = list(seg[i : i + argc_i])
        i += len(args)
        out.append({"off": off, "op": op, "args": args})
        if len(args) < argc_i:
            break
    return out


def _advance_one_token(seg: bytes, pos: int) -> int | None:
    """Best-effort token skipper for branch target recovery."""
    if pos >= len(seg):
        return None
    op = seg[pos]
    spec = OPCODES.get(op)
    if spec is None:
        return None
    argc = spec["argc"]
    if argc is None:
        # Variable-length token family (`*_SKIPTOK`) reads one selector byte.
        return pos + 2 if pos + 1 < len(seg) else None
    argc_i = int(argc)
    end = pos + 1 + argc_i
    return end if end <= len(seg) else None


def _skip_n_tokens(seg: bytes, pos: int, n: int) -> int | None:
    cur = pos
    for _ in range(n):
        nxt = _advance_one_token(seg, cur)
        if nxt is None:
            return None
        cur = nxt
    return cur


def parse_segment_path_aware(seg: bytes) -> list[dict[str, object]]:
    """Heuristic control-flow walk that continues through SKIPTOK branches."""
    out: list[dict[str, object]] = []
    queue: list[int] = [0]
    seen_pos: set[int] = set()

    while queue:
        pos = queue.pop()
        while pos < len(seg):
            if pos in seen_pos:
                break
            seen_pos.add(pos)

            op = seg[pos]
            spec = OPCODES.get(op)
            if spec is None:
                break

            argc = spec["argc"]
            if argc is None:
                selector = seg[pos + 1] if pos + 1 < len(seg) else None
                args = [] if selector is None else [selector]
                out.append({"off": pos, "op": op, "args": args})

                # Follow both paths: fallthrough and token-skip targets.
                base = pos + (2 if selector is not None else 1)
                if base < len(seg):
                    queue.append(base)

                # 68k helper at 0x157FC skips variable token streams; exact
                # encoding is still unresolved, so use bounded token-count
                # candidates to expose hidden predicates.
                candidates: set[int] = {1}
                if selector is not None:
                    if 1 <= selector <= 4:
                        candidates.add(selector)
                    lo = selector & 0x0F
                    if 1 <= lo <= 4:
                        candidates.add(lo)

                for n in sorted(candidates):
                    target = _skip_n_tokens(seg, base, n)
                    if target is not None and target < len(seg):
                        queue.append(target)
                break

            argc_i = int(argc)
            args = list(seg[pos + 1 : pos + 1 + argc_i])
            out.append({"off": pos, "op": op, "args": args})
            if len(args) < argc_i:
                break
            pos = pos + 1 + argc_i

    # Stable output: sorted by offset/op/args, deduped.
    dedup: dict[tuple[int, int, tuple[int, ...]], dict[str, object]] = {}
    for row in out:
        key = (int(row["off"]), int(row["op"]), tuple(int(x) for x in row["args"]))
        dedup[key] = row
    return [dedup[k] for k in sorted(dedup.keys())]


def merge_segment_nodes(seg: bytes) -> list[dict[str, object]]:
    parsed_linear = parse_segment_linear_nodes(seg)
    parsed_path = parse_segment_path_aware(seg)
    merged: dict[tuple[int, tuple[int, ...]], dict[str, object]] = {}
    for row in parsed_linear:
        op = int(row["op"])
        args = [int(x) for x in row["args"]]
        off = int(row["off"])
        merged[(off, (op, *args))] = {"off": off, "op": op, "args": args}
    for row in parsed_path:
        key = (int(row["off"]), (int(row["op"]), *(int(x) for x in row["args"])))
        merged[key] = row
    return sorted(merged.values(), key=lambda r: int(r["off"]))


def decode_u16_arg(op: int, args: list[int]) -> int | None:
    if len(args) < 2:
        return None
    # OP_24 uses helper 0x155DA (little-endian); OP_25 reads explicit hi/lo bytes.
    if op == 0x25:
        return ((args[0] << 8) | args[1]) & 0xFFFF
    return (args[0] | (args[1] << 8)) & 0xFFFF


def load_items_table(items_path: Path) -> list[str]:
    rec_size = 0x14
    name_size = 0x0C
    raw = items_path.read_bytes()
    names: list[str] = []
    for i in range(len(raw) // rec_size):
        off = i * rec_size
        name = raw[off : off + name_size].decode("ascii", errors="replace").rstrip()
        names.append(name)
    return names


def try_load_default_items() -> list[str]:
    root = Path(__file__).resolve().parents[1]
    candidates = [root / "items.dat", root / "EXTRACTED" / "items.dat"]
    for p in candidates:
        if p.exists():
            try:
                return load_items_table(p)
            except OSError:
                pass
    return []


def format_decoded_op(op: int, args: list[int], items: list[str]) -> str:
    spec = OPCODES.get(op)
    name = str(spec["name"]) if spec else f"OP_{op:02X}"
    base = f"{name}"
    if args:
        base += " " + " ".join(f"0x{x:02X}" for x in args)

    if op in (0x24, 0x25):
        value = decode_u16_arg(op, args)
        if value is not None:
            base += f"  ; value={value} (0x{value:04X})"
    elif op == 0x28:
        probe = args[0] if len(args) >= 1 else None
        item_id = args[1] if len(args) >= 2 else None
        if item_id is not None:
            item_name = items[item_id] if 0 <= item_id < len(items) else "<unknown>"
            base += f"  ; consume item_id={item_id} ({item_name}), probe={probe}"
    return base


def _fmt_strings_hint(strings: list[str], idx: int) -> str:
    if 0 <= idx < len(strings):
        preview = display_string(strings[idx]).replace("\n", " / ").strip()
        return f'str[{idx}] "{preview[:72]}"'
    return f"str[{idx}]"


def decompile_op(op: int, args: list[int], strings: list[str], items: list[str]) -> str:
    if op == 0x01 and args:
        return f"show_text_basic({_fmt_strings_hint(strings, args[0])})"
    if op == 0x02 and args:
        return f"show_text_block({_fmt_strings_hint(strings, args[0])})"
    if op == 0x04 and args:
        return f"show_text_above_door({_fmt_strings_hint(strings, args[0])})"
    if op == 0x05 and args:
        return f"show_text_popup_style_a({_fmt_strings_hint(strings, args[0])})"
    if op == 0x06 and args:
        return f"show_text_popup_style_b({_fmt_strings_hint(strings, args[0])})"

    if op == 0x03 and args:
        return f"show_text({_fmt_strings_hint(strings, args[0])})"

    if op == 0x07:
        return "wait_for_space()"
    if op == 0x09:
        return "cond = prompt_yes_no()"
    if op == 0x0D and args:
        return f"engine_call(0x{args[0]:02X})"
    if op == 0x0F:
        return "end_script()"
    if op == 0x12 and len(args) >= 12:
        block = args[:10]
        tail = args[10:12]
        return (
            f"encounter_setup(monsters={' '.join(f'{x:02X}' for x in block)}, "
            f"flags={' '.join(f'{x:02X}' for x in tail)})"
        )
    if op == 0x13 and len(args) >= 10:
        return f"encounter_setup_b(data={' '.join(f'{x:02X}' for x in args[:10])})"
    if op == 0x15 and len(args) >= 3:
        return f"apply_party(count=0x{args[0]:02X}, op=0x{args[1]:02X}, val=0x{args[2]:02X})"
    if op == 0x16 and len(args) >= 2:
        return f"cond = check_monster_present(0x{args[0]:02X}, 0x{args[1]:02X})"
    if op == 0x18 and len(args) >= 4:
        return (
            f"apply_party_masked(count=0x{args[0]:02X}, set=0x{args[1]:02X}, "
            f"and=0x{args[2]:02X}, or=0x{args[3]:02X})"
        )
    if op == 0x19 and len(args) >= 4:
        return (
            f"add_party_entity(0x{args[0]:02X}, f3a=0x{args[1]:02X}, "
            f"f40=0x{args[2]:02X}, f46=0x{args[3]:02X})"
        )
    if op == 0x1B and args:
        return f"cond = (cond >= 0x{args[0]:02X})"
    if op == 0x1E and args:
        return f"delay(0x{args[0]:02X})"
    if op == 0x2E and len(args) >= 2:
        return f"set_party_attr(class=0x{args[0]:02X}, bits=0x{args[1]:02X})"
    if op == 0x1F and len(args) >= 6:
        return f"party_effect(sel=0x{args[0]:02X}, {' '.join(f'{x:02X}' for x in args[1:])})"
    if op == 0x20 and len(args) >= 6:
        return f"party_effect_b(sel=0x{args[0]:02X}, {' '.join(f'{x:02X}' for x in args[1:])})"
    if op == 0x21 and len(args) >= 3:
        y, x = (args[0] >> 4) & 0xF, args[0] & 0xF
        return f"set_tile((y={y},x={x}), 0x{args[1]:02X}, 0x{args[2]:02X})"
    if op == 0x23 and len(args) >= 2:
        return f"cond = check_day_of_year(0x{args[0]:02X}, 0x{args[1]:02X})"
    if op == 0x26:
        return "selected = select_party_member()"
    if op == 0x27:
        return "selected = select_party_member(mode=1)"
    if op == 0x2A and len(args) >= 5:
        gold = args[0] | (args[1] << 8) | (args[2] << 16)
        gems = args[3] | (args[4] << 8)
        return f"set_treasure(gold/exp={gold}, gems={gems}, items={args[5:8]})"
    if op == 0x2C and args:
        return f"adjust_state(+0x{args[0]:02X})"
    if op == 0x2D and len(args) >= 2:
        return f"cond = check_member_attr(fields=0x{args[0]:02X}, value=0x{args[1]:02X})"
    if op == 0x08:
        return "wait_key()"
    if op == 0x0A:
        return "cond = prompt_yes_no(mode=1)"
    if op == 0x30:
        # Stored answer is encoded as byte = 0x11A - ascii (0xFA == space pad).
        decoded = "".join(
            chr((0x11A - b) & 0xFF) if 0x20 <= ((0x11A - b) & 0xFF) <= 0x7E else "?"
            for b in args
        ).rstrip()
        return f'cond = check_answer("{decoded}")'
    if op == 0x0E:
        sel = args[0] if args else 0
        label = SELECTOR_NAMES.get(sel)
        if label:
            return f"exec_selector(0x{sel:02X})  # {label}"
        return f"exec_selector(0x{sel:02X})"
    if op == 0x0B and len(args) >= 2:
        return f"set_service_context({_fmt_strings_hint(strings, args[0])}, mode=0x{args[1]:02X})"
    if op == 0x0C and len(args) >= 2:
        name = TRANSITION_NAMES.get((args[0], args[1]))
        if name:
            return f"map_transition(0x{args[0]:02X}, 0x{args[1]:02X})  # {name}"
        return f"map_transition(0x{args[0]:02X}, 0x{args[1]:02X})"
    if op == 0x10:
        n = args[0] if args else 0
        return f"if cond: skip_tokens({n})"
    if op == 0x11:
        n = args[0] if args else 0
        return f"if not cond: skip_tokens({n})"
    if op == 0x14:
        return "clear_current_tile_event_flag()"
    if op == 0x17 and len(args) >= 2:
        return f"cond = load_var8(group=0x{args[0]:02X}, index=0x{args[1]:02X})"
    if op == 0x1A and len(args) >= 2:
        return f"store_var8(group=0x{args[0]:02X}, value=0x{args[1]:02X})"
    if op == 0x22 and len(args) >= 2:
        lo, hi = args[0], args[1]
        return f"cond = (era in [{lo}..{hi}])"
    if op == 0x24:
        value = decode_u16_arg(op, args)
        if value is not None:
            return f"cond = check_gold_at_least({value})"
    if op == 0x25:
        value = decode_u16_arg(op, args)
        if value is not None:
            labels = {0: "flag_clear", 1: "flag_set", 2: "flag_alt"}
            hint = labels.get(value, "")
            suffix = f"  # {hint}" if hint else ""
            return f"cond = check_code16(0x{value:04X}){suffix}"
    if op == 0x28:
        probe = args[0] if len(args) >= 1 else None
        item_id = args[1] if len(args) >= 2 else None
        if item_id is not None:
            item_name = items[item_id] if 0 <= item_id < len(items) else "<unknown>"
            return f'cond = consume_item(item_id={item_id}, name="{item_name}", probe={probe})'
        return "cond = consume_item(<truncated>)"
    if op == 0x29:
        return "set_abort_and_exit()"
    if op == 0x2A:
        return "load_special_block(...)"
    if op == 0x2B:
        n = args[0] if args else 0
        return f"skip_tokens({n})"
    if op == 0x31 and len(args) >= 3:
        return f"for_party(mask=0x{args[0]:02X}): call(0x{args[1]:02X},0x{args[2]:02X})"
    if op == 0x32 and args:
        return f"cond = load_cond_from_var(0x{args[0]:02X})"

    spec = OPCODES.get(op)
    name = str(spec["name"]) if spec else f"OP_{op:02X}"
    if args:
        arg_text = ", ".join(f"0x{x:02X}" for x in args)
        if op in (0x0B, 0x0C) and args[0] < len(strings):
            return f"{name.lower()}({arg_text})  # {_fmt_strings_hint(strings, args[0])}"
        return f"{name.lower()}({arg_text})"
    return f"{name.lower()}()"


def dump_decompiled(data: bytes) -> None:
    """Human-readable pseudo code for each event segment."""
    header = read_header(data)
    items = try_load_default_items()

    print(f"event.dat: {len(data)} bytes")
    print("mode: pseudo decompile")
    print()

    for loc_id, (off, length) in enumerate(header):
        blob = data[off : off + length]
        loc = decode_location(blob, loc_id)
        script = blob[loc["script_offset"] : loc["string_table_offset"]]
        segments = split_script_segments(script)

        by_evt: dict[int, list[tuple[int, int, int]]] = {}
        for pos, evt, flags in loc["triplets"]:
            by_evt.setdefault(evt, []).append((pos, flags, pos))

        print("=" * 78)
        print(f"location {loc_id:02d} {{")
        print(f"  # offset=0x{off:06X}, len={length}, events={len(by_evt)}, strings={len(loc['strings'])}")
        if not loc.get("terminated", True):
            print("  # WARNING: no 00 00 00 triplet terminator found.")
            print("  #   This is a non-standard record (e.g. castle / Hall of Spells)")
            print("  #   not processed by the tile-event scanner at 0x1754A.")
            print("  #   Triplets/events below are unreliable.")

        for evt in sorted(by_evt.keys()):
            triggers = []
            for pos, flags, _ in by_evt[evt]:
                y = (pos >> 4) & 0xF
                x = pos & 0xF
                cond = FLAG_NAMES.get(flags, f"0x{flags:02X}")
                triggers.append(f"({y},{x})/{cond}")
            trig_txt = ", ".join(triggers)

            print(f"  event {evt:02d}  # triggers(y,x): {trig_txt}")
            print("  {")
            if evt >= len(segments):
                print("    <missing script segment>")
                print("  }")
                continue
            seg = segments[evt]
            if not seg:
                print("    pass")
                print("  }")
                continue

            if looks_like_text_record(seg):
                txt = seg.decode("ascii", errors="replace").replace("@", " / ")
                print(f'    text_record: "{txt}"')
                print("        # this record is plain text in the 0xFF pool, not an opcode script")
                print("  }")
                continue

            parsed_nodes = parse_segment_stream_nodes(seg)
            for idx, node in enumerate(parsed_nodes):
                op = int(node["op"])
                args = [int(x) for x in node["args"]]
                pseudo = decompile_op(op, args, loc["strings"], items)
                print(f"    {idx:02d}: {pseudo}")
                if op == 0x00:
                    print("        # invalid opcode boundary; likely alternate token stream follows")
                    break
                if op in (0x10, 0x11, 0x2B):
                    target = None
                    if args:
                        base = int(node["off"]) + 2
                        target = _skip_n_tokens(seg, base, args[0])
                    if target is not None and target < len(seg):
                        target_op = seg[target]
                        spec = OPCODES.get(target_op)
                        if spec is not None:
                            argc = spec["argc"]
                            if argc is not None:
                                argc_i = int(argc)
                                targs = list(seg[target + 1 : target + 1 + argc_i])
                                target_pseudo = decompile_op(target_op, targs, loc["strings"], items)
                                print(f"        # skip target +0x{target:02X}: {target_pseudo}")
                            else:
                                print(f"        # skip target +0x{target:02X}: variable token opcode 0x{target_op:02X}")
                        else:
                            print(f"        # skip target +0x{target:02X}: unknown opcode 0x{target_op:02X}")
                    else:
                        print("        # skip target unresolved (variable token table)")
                    print("        # token-skip flow is variable-length; target is best-effort")
            print("  }")

        if loc["strings"]:
            print("  strings {")
            for i, s in enumerate(loc["strings"]):
                txt = display_string(s).replace("\n", " / ")
                print(f'    {i:02d}: "{txt}"')
            print("  }")

        print("}")
        print()

    print(f"total_locations = {len(header)}")


def dump_events_indexed(data: bytes) -> None:
    """Event-centric decode: trigger map + per-event script disassembly."""
    header = read_header(data)
    items = try_load_default_items()

    print(f"event.dat: {len(data)} bytes")
    print("mode: event-indexed decode")
    print()

    for loc_id, (off, length) in enumerate(header):
        blob = data[off : off + length]
        loc = decode_location(blob, loc_id)
        script = blob[loc["script_offset"] : loc["string_table_offset"]]
        segments = split_script_segments(script)

        by_evt: dict[int, list[tuple[int, int, int, int]]] = {}
        for pos, evt, flags in loc["triplets"]:
            y = (pos >> 4) & 0xF
            x = pos & 0xF
            by_evt.setdefault(evt, []).append((y, x, pos, flags))

        print("=" * 78)
        print(
            f"Location {loc_id:02d}  offset=0x{off:06X} len={length:4d}  "
            f"triplets={len(loc['triplets']):2d} segments={len(segments):2d} strings={len(loc['strings']):2d}"
        )
        print()

        if by_evt:
            print("Triggers by event id (y,x):")
            for evt in sorted(by_evt.keys()):
                trig = by_evt[evt]
                tile_txt = ", ".join(
                    f"({y:02d},{x:02d})/0x{p:02X}/{FLAG_NAMES.get(f, f'0x{f:02X}')}"
                    for y, x, p, f in trig
                )
                seg_len = len(segments[evt]) if evt < len(segments) else -1
                if seg_len >= 0:
                    print(f"  EVT {evt:02d}: tiles={len(trig):2d} seg_len={seg_len:3d}  {tile_txt}")
                else:
                    print(f"  EVT {evt:02d}: tiles={len(trig):2d} seg_len= n/a  {tile_txt}")
            print()

        for evt in sorted(by_evt.keys()):
            print(f"  --- EVT {evt:02d} ---")
            if evt >= len(segments):
                print("    <missing segment>")
                print()
                continue
            seg = segments[evt]
            if not seg:
                print("    <empty segment>")
                print()
                continue

            parsed = parse_segment_linear(seg)
            for idx, (op, args) in enumerate(parsed):
                line = format_decoded_op(op, args, items)
                print(f"    {idx:02d}: {line}")
            print()

        if loc["strings"]:
            print("Strings:")
            for i, s in enumerate(loc["strings"]):
                preview = display_string(s).replace("\n", "\\n")
                print(f"  [{i:02d}] {preview}")
            print()

    print(f"Total locations: {len(header)}")


def dump_consumed_items(data: bytes, items_path: Path, answers: list[str]) -> None:
    items = load_items_table(items_path)
    header = read_header(data)

    rows: list[dict[str, object]] = []
    answer_hits: list[tuple[int, str]] = []
    answer_keys = [a.lower() for a in answers]

    for loc_id, (off, length) in enumerate(header):
        blob = data[off : off + length]
        loc = decode_location(blob, loc_id)
        script = blob[loc["script_offset"] : loc["string_table_offset"]]
        segments = split_script_segments(script)

        for s in loc["strings"]:
            low = s.lower()
            if any(a in low for a in answer_keys):
                answer_hits.append((loc_id, s.replace("\n", " / ")))

        for seg_id, seg in enumerate(segments):
            parsed = parse_segment_linear(seg)
            for off, (op, args) in enumerate(parsed):
                if int(op) != 0x28:
                    continue

                item_id = args[1] if len(args) >= 2 else None
                probe = args[0] if len(args) >= 1 else None
                name = (
                    items[item_id] if item_id is not None and 0 <= item_id < len(items) else "<unknown>"
                )
                rows.append(
                    {
                        "loc": loc_id,
                        "seg": seg_id,
                        "off": off,
                        "probe": probe,
                        "item_id": item_id,
                        "item_name": name,
                        "text": " | ".join(x for x in loc["strings"][:8] if x.strip()),
                    }
                )

    print(f"consume-op hits (OP_28): {len(rows)}")
    uniq: dict[int, int] = {}
    for r in rows:
        item_id = r["item_id"]
        if item_id is None:
            continue
        uniq[item_id] = uniq.get(item_id, 0) + 1

    print("\nUnique consumed item IDs:")
    for item_id in sorted(uniq.keys()):
        name = items[item_id] if 0 <= item_id < len(items) else "<unknown>"
        print(f"  {item_id:3d}  {name:<12}  hits={uniq[item_id]}")

    unresolved = [r for r in rows if r["item_id"] is None]
    if unresolved:
        print("\nUnresolved OP_28 arguments:")
        for r in unresolved:
            print(
                f"  loc {int(r['loc']):02d} seg {int(r['seg']):02d} +0x{int(r['off']):04X} "
                f"probe={r['probe']}"
            )

    print("\nDetailed contexts:")
    for r in rows:
        item_id = r["item_id"]
        id_txt = "None" if item_id is None else f"{item_id:3d}"
        probe = r["probe"]
        probe_txt = "None" if probe is None else f"{probe:3d}"
        print("-" * 72)
        print(
            f"loc {int(r['loc']):02d} seg {int(r['seg']):02d} +0x{int(r['off']):04X} "
            f"probe={probe_txt} item={id_txt} {r['item_name']}"
        )
        print(f"  text: {str(r['text'])[:180].replace(chr(10), ' / ')}")

    if answer_keys:
        print("\nWord-answer string hits:")
        for loc_id, s in answer_hits:
            print(f"  loc {loc_id:02d}: {s[:180]}")


def dump_predicate_usage(data: bytes) -> None:
    header = read_header(data)
    rows: list[dict[str, object]] = []
    for loc_id, (off, length) in enumerate(header):
        blob = data[off : off + length]
        loc = decode_location(blob, loc_id)
        script = blob[loc["script_offset"] : loc["string_table_offset"]]
        segments = split_script_segments(script)
        for seg_id, seg in enumerate(segments):
            parsed = merge_segment_nodes(seg)

            for idx, node in enumerate(parsed):
                op = int(node["op"])
                args = [int(x) for x in node["args"]]
                if op not in (0x24, 0x25):
                    continue
                value = decode_u16_arg(op, args)
                rows.append(
                    {
                        "loc": loc_id,
                        "seg": seg_id,
                        "op": op,
                        "value": value,
                        "prev": parsed[max(0, idx - 4) : idx],
                        "next": parsed[idx + 1 : idx + 3],
                        "strings": loc["strings"][:10],
                    }
                )

    print(f"predicate hits: {len(rows)}")
    freq: dict[tuple[int, int | None], int] = {}
    for row in rows:
        key = (int(row["op"]), row["value"])
        freq[key] = freq.get(key, 0) + 1

    print("\nBy opcode/value:")
    for (op, value), count in sorted(
        freq.items(), key=lambda kv: (kv[0][0], -1 if kv[0][1] is None else kv[0][1])
    ):
        if value is None:
            vtxt = "None"
        else:
            vtxt = f"0x{value:04X} ({value})"
        print(f"  OP_{op:02X} value={vtxt:>12} count={count}")

    print("\nContexts:")
    for row in rows:
        op = int(row["op"])
        value = row["value"]
        vtxt = "None" if value is None else f"0x{value:04X} ({value})"
        print("-" * 72)
        print(f"loc {int(row['loc']):02d} seg {int(row['seg']):02d} OP_{op:02X} value={vtxt}")
        prev = " ".join(
            f"{int(p['op']):02X}({' '.join(f'{int(x):02X}' for x in p['args'])})"
            if p["args"]
            else f"{int(p['op']):02X}"
            for p in row["prev"]
        )
        nxt = " ".join(
            f"{int(p['op']):02X}({' '.join(f'{int(x):02X}' for x in p['args'])})"
            if p["args"]
            else f"{int(p['op']):02X}"
            for p in row["next"]
        )
        print(f"  prev: {prev}")
        print(f"  next: {nxt}")
        s = " | ".join(x for x in row["strings"] if x.strip())
        print(f"  text: {s[:180].replace(chr(10), ' / ')}")


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    if len(sys.argv) >= 2 and sys.argv[1] in ("--predicates", "--consumed", "--events", "--decompile"):
        mode = sys.argv[1]
        if len(sys.argv) >= 3 and not sys.argv[2].startswith("--"):
            path = Path(sys.argv[2])
            rest = sys.argv[3:]
        else:
            path = Path(__file__).resolve().parents[1] / "EXTRACTED" / "event.dat"
            rest = sys.argv[2:]
        data = path.read_bytes()

        if mode == "--predicates":
            dump_predicate_usage(data)
            return
        if mode == "--events":
            dump_events_indexed(data)
            return
        if mode == "--decompile":
            dump_decompiled(data)
            return

        root = Path(__file__).resolve().parents[1]
        items_path = root / "items.dat"
        answers: list[str] = []
        i = 0
        while i < len(rest):
            if rest[i] == "--items" and i + 1 < len(rest):
                items_path = Path(rest[i + 1])
                i += 2
                continue
            if rest[i] == "--answers":
                answers = [x for x in rest[i + 1 :] if not x.startswith("--")]
                break
            i += 1
        dump_consumed_items(data, items_path, answers)
        return

    if len(sys.argv) < 2:
        path = Path(__file__).resolve().parents[1] / "EXTRACTED" / "event.dat"
    else:
        path = Path(sys.argv[1])

    data = path.read_bytes()
    print(f"event.dat: {len(data)} bytes")
    print()

    header = read_header(data)

    loc_filter = None
    if len(sys.argv) >= 3:
        loc_filter = int(sys.argv[2])

    for idx, (off, length) in enumerate(header):
        if loc_filter is not None and idx != loc_filter:
            continue

        blob = data[off : off + length]
        loc = decode_location(blob, idx)

        print(f"{'='*60}")
        print(f"Location {idx:3d}  offset=0x{off:06X}  length={length} bytes")
        print(f"  Tile events: {len(loc['triplets'])} entries")
        print(f"  Script: offset 0x{loc['script_offset']:04X}, {loc['script_length']} bytes")
        print(f"  Strings: offset 0x{loc['string_table_offset']:04X}, {len(loc['strings'])} entries")
        print()

        if loc["triplets"]:
            print("  --- Tile Event Table ---")
            for t in loc["triplets"]:
                print(format_triplet(t))
            print()

        if loc["strings"]:
            print("  --- Strings ---")
            for i, s in enumerate(loc["strings"]):
                preview = display_string(s)[:60].replace("\n", "\\n")
                print(f"  [{i:2d}] {preview}")
            print()

        if loc["script_length"] > 0:
            script = blob[loc["script_offset"] : loc["string_table_offset"]]
            print("  --- Script (raw preview) ---")
            preview = " ".join(f"{b:02X}" for b in script[:64])
            print(f"  {preview}")
            if len(script) > 64:
                print("  ...")
            print()

            markers = find_event_markers(script)
            if markers:
                print(f"  --- Script marker decode (0x22 entries: {len(markers)}) ---")
                for mi, start in enumerate(markers[:12]):
                    print(f"  marker[{mi}] at +0x{start:04X}")
                    for line in disassemble_script_from(script, start):
                        print(line)
                    print()
            else:
                print("  --- Script marker decode ---")
                print("  No 0x22 markers found in script section.")
                print()

            # For targeted reverse-engineering of a location, show event-id segment table.
            if loc_filter is not None:
                segments = split_script_segments(script)
                print(f"  --- Event segment table (0xFF-delimited, count={len(segments)}) ---")
                for seg_id, seg in enumerate(segments):
                    if not seg:
                        continue
                    payload = bytes(seg) + b"\xFF"
                    first = disassemble_script_from(payload, 0)
                    print(
                        f"  EVT {seg_id:02d}: len={len(seg):3d}  bytes="
                        + " ".join(f"{b:02X}" for b in seg[:8])
                    )
                    if first:
                        print(f"    {first[0].strip()}")
                print()

    if loc_filter is None:
        print(f"\nTotal locations: {len(header)}")
        total_triplets = 0
        for idx, (off, length) in enumerate(header):
            blob = data[off : off + length]
            triplets, _, _ = decode_triplets(blob)
            total_triplets += len(triplets)
        print(f"Total tile events: {total_triplets}")


def encode_header(entries: list[tuple[int, int]]) -> bytes:
    out = bytearray()
    for off, length in entries:
        out += struct.pack(">I", off)
        out += struct.pack(">H", length)
    return bytes(out)


def encode_event_dat(records: list[bytes], entries: list[tuple[int, int]] | None = None) -> bytes:
    """Rebuild event.dat from ordered location record blobs.

    If ``entries`` is omitted, offsets are computed contiguously after the
    426-byte header (preserving on-disk layout when record sizes unchanged).
    """
    if entries is None:
        off = len(read_header(b"\x00" * 426)) * 6  # 426
        entries = []
        cursor = off
        for rec in records:
            entries.append((cursor, len(rec)))
            cursor += len(rec)
    header = encode_header(entries)
    body = b"".join(records)
    return header + body


def load_event_records(data: bytes) -> tuple[list[tuple[int, int]], list[bytes]]:
    header = read_header(data)
    records = [data[off : off + length] for off, length in header]
    return header, records


def round_trip_check(data: bytes) -> bool:
    header, records = load_event_records(data)
    return encode_event_dat(records, header) == data


if __name__ == "__main__":
    main()

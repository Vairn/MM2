#pragma once
// event.dat script interpreter opcode metadata.
//
// Truth sources (not EXTRACTED/docs):
//   - ASM dispatcher 0x172CA, jump table 0x17494, handlers 0x15924..0x17190
//   - game EventRuntime::dispatchOp / skipTokens
//   - ROM opcode_len_tbl @ A4-$6CC8 (see eventlang/OpcodeTable + EventVmHelpers)
//
// Opcodes are 0x00..0x32; 0xFF terminates a script record.
// argc < 0 marks the token-skip family (0x10/0x11/0x2B): linear parse consumes
// the selector byte only; CFG lift uses ROM tokenDelta to resolve skip targets.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace mm2 {

constexpr int kEventLocationCount = 71;
constexpr int kEventHeaderSize = kEventLocationCount * 6;   // 426
constexpr int kEventMaxRecord = 0x8AC;                      // runtime length clamp

struct OpcodeInfo {
    const char* name;
    int argc;  // fixed arg byte count, or -1 for variable token-skip ops
};

// Indexed by opcode (0x00..0x32). Matches decode_event.OPCODES.
inline const OpcodeInfo& opcodeInfo(uint8_t op) {
    static const OpcodeInfo kTable[0x33] = {
        {"OP_00_INVALID", 0},                 // 00
        {"OP_01_TEXT", 1},                    // 01
        {"OP_02_TEXT_BLOCK", 1},              // 02
        {"OP_03_TEXT", 1},                    // 03
        {"OP_04_TEXT_DOOR", 1},               // 04
        {"OP_05_TEXT_POPUP_A", 1},            // 05
        {"OP_06_TEXT_POPUP_B", 1},            // 06
        {"OP_07_WAIT_SPACE", 0},              // 07
        {"OP_08_WAIT_KEY", 0},                // 08
        {"OP_09_PROMPT_YN", 0},               // 09
        {"OP_0A_PROMPT_YN_B", 0},             // 0A
        {"OP_0B_SHOW_SERVICE_WINDOW", 2},     // 0B
        {"OP_0C_MAP_TRANSITION", 2},          // 0C
        {"OP_0D_PLAY_SOUND_SEQ", 1},          // 0D  → 0x6FB8 audio (not graphics)
        {"OP_0E_EXEC_SELECTOR", 1},           // 0E
        {"OP_0F_END_SCRIPT", 0},              // 0F
        {"OP_10_IF_TRUE_SKIPTOK", -1},        // 10
        {"OP_11_IF_FALSE_SKIPTOK", -1},       // 11
        {"OP_12_ENCOUNTER_SETUP", 12},        // 12
        {"OP_13_ENCOUNTER_SETUP_B", 10},      // 13
        {"OP_14_CLEAR_TILE_EVENT", 0},        // 14
        {"OP_15_APPLY_PARTY", 3},             // 15
        {"OP_16_SCAN_PARTY_ITEMS", 2},        // 16  item id in arg2; equipped+backpack
        {"OP_17_LOAD_VAR_RAW_TO_COND", 2},    // 17
        {"OP_18_APPLY_PARTY_MASKED", 4},      // 18
        {"OP_19_ADD_PARTY_ENTITY", 4},        // 19
        {"OP_1A_STORE_VAR8", 2},              // 1A
        {"OP_1B_COND_THRESHOLD", 1},          // 1B
        {"OP_1C_ENGINE_QUERY_TO_COND", 1},    // 1C
        {"OP_1D_ENGINE_CALL_REC7", 1},        // 1D
        {"OP_1E_DELAY", 1},                   // 1E
        {"OP_1F_PARTY_EFFECT", 6},            // 1F
        {"OP_20_PARTY_EFFECT_B", 6},          // 20
        {"OP_21_SET_TILE", 3},                // 21
        {"OP_22_CHECK_ERA_RANGE", 2},         // 22
        {"OP_23_CHECK_DAY_RANGE", 2},         // 23
        {"OP_24_PAY_GOLD_TO_COND", 2},        // 24  pool-pay via 0x6ACE
        {"OP_25_PAY_GEMS_TO_COND", 2},        // 25  pool-pay via 0x6B9A (skip-table quirk)
        {"OP_26_SELECT_MEMBER", 0},           // 26
        {"OP_27_SELECT_MEMBER_B", 0},         // 27
        {"OP_28_CONSUME_ITEM_TO_COND", 2},    // 28
        {"OP_29_SET_ABORT", 0},               // 29
        {"OP_2A_SET_TREASURE", 14},           // 2A
        {"OP_2B_SKIPTOK_IF_VICTORY", -1},     // 2B  gated by A4-$77BD
        {"OP_2C_ADD_WORD_COUNTER", 1},        // 2C
        {"OP_2D_CHECK_MEMBER_ATTR", 2},       // 2D
        {"OP_2E_OR_MEMBER_FIELD", 2},         // 2E
        {"OP_2F_READ_ANSWER", 0},             // 2F  fills -$5C50 (not a silent clear)
        {"OP_30_CHECK_ANSWER", 10},           // 30
        {"OP_31_PARTY_ITERATE_CALL", 3},      // 31
        {"OP_32_COUNT_TITLE_NIBBLE", 1},      // 32
    };
    static const OpcodeInfo kUnknown = {"OP_??", 0};
    if (op <= 0x32) return kTable[op];
    return kUnknown;
}

inline bool opcodeKnown(uint8_t op) { return op <= 0x32; }
inline const char* opcodeName(uint8_t op) { return opcodeInfo(op).name; }
inline int opcodeArgc(uint8_t op) { return opcodeInfo(op).argc; }

// String index referenced by a text-display opcode, or -1 if this op does not
// index the event-local string table. Only 01..06 take a u8 string index.
//
// OP_0B is intentionally excluded: ASM (handler 0x15DB0 -> 0x15756) shows it
// loads a signboard `.anm` sprite, not event-local / str.dat text. Its arg0 is
// a per-environment sign-table index (see serviceSignId() below), so it must
// not be wired to a string node.
inline int opcodeStringArg(uint8_t op, const std::vector<uint8_t>& args) {
    switch (op) {
        case 0x01: case 0x02: case 0x03: case 0x04:
        case 0x05: case 0x06:
            return args.empty() ? -1 : args[0];
        default:
            return -1;
    }
}

// ---- OP_0B service-signboard resolver (handler 0x15DB0 -> 0x15756) ---------
//
// OP_0B arg0 is a sign index mapped to a signboard `.anm` id, NOT a string:
//   * arg0 >= 0x80 -> sign id 0x4B                          (0x15760/0x15766)
//   * else sign id = TABLE[env][arg0 - 1]                   (0x1576C subq #1)
//   * env id = area_env_lookup(screen) (0x18AE); env -> one of five 24-byte
//     tables (A4-$6C62/$6C4C/$6C32/$6C1A/$6C02). The OP_0B jump table @ 0x157D2
//     maps env->table in a SCRAMBLED order (NOT sequential by town), verified in
//     mm2.capstone.asm; this mirrors game/src/events/ServiceSignResolver.cpp:
//       env0 -> $6C62(0), env1 -> $6C32(48), env2 -> $6C02(96),
//       env3 -> $6C4C(22), env4 -> $6C1A(72), env5 -> $6C02(96), env6 -> $6C1A(72)
//     [offsets into the packed data block]. Bases are unevenly strided
//     (22/26/24/24), so tables partially overlap in the shared block.
inline int signEnvForScreen(int screen) {
    static const uint8_t lo[] = {0, 5, 17, 33, 41, 45, 55};
    static const uint8_t hi[] = {4, 16, 32, 40, 44, 54, 59};
    static const uint8_t id[] = {0, 3, 1, 6, 4, 5, 2};
    for (int i = 0; i < 7; ++i)
        if (screen >= lo[i] && screen <= hi[i]) return id[i];
    return 7;
}

// Resolve OP_0B arg0 to a signboard `.anm` id for location `loc` (== map screen
// for loc 0..59). Returns -1 when unresolved (overlay banks / out of range).
inline int serviceSignId(int loc, uint8_t strIdx) {
    if (strIdx >= 0x80) return 0x4B;
    if (loc < 0) return -1;
    static const uint8_t kBlock[120] = {
        70, 62, 63, 66, 67, 68, 65,  2, 19, 26, 51, 54, 53, 12, 60, 27, 39,  4, 45, 37, 57, 47, 73, 33,
        42, 43, 17, 14, 69, 34,  9, 26, 24, 52, 53, 21, 25, 28, 44, 49, 11, 31, 55, 36,  1, 61,
        18, 47, 72, 16, 10, 23,  6, 51, 15,  8,  5, 49, 40, 11, 30, 39,  4, 46, 20, 36,  1, 57,
        13, 58, 18, 47, 74, 42,  2, 17, 14, 69, 19, 34,  9, 26, 24, 52, 54,  8, 21, 25,  3, 29,
        44, 50, 27, 39, 61, 48, 71, 59, 33, 19, 35, 10, 24,  6, 75, 51, 15,  7, 60, 56, 29,  5,
        22, 50, 30, 41, 46, 37, 58,  0,
    };
    static const int envOffset[7] = {0, 48, 96, 22, 72, 96, 72};
    const int env = signEnvForScreen(loc);
    if (env < 0 || env > 6) return -1;
    const int i = envOffset[env] + (static_cast<int>(strIdx) - 1);  // 0x1576C subq #1
    if (i < 0 || i >= 120) return -1;
    return kBlock[i];
}

// OP_24 reads a little-endian u16; OP_25 reads explicit hi/lo bytes.
inline int decodeU16Arg(uint8_t op, const std::vector<uint8_t>& args) {
    if (args.size() < 2) return -1;
    if (op == 0x25) return ((args[0] << 8) | args[1]) & 0xFFFF;
    return (args[0] | (args[1] << 8)) & 0xFFFF;
}

// Human-readable pseudo-code for one opcode, ported from decompile_op().
// `strAt` returns the string-table entry for an index (for inline previews),
// `itemAt` resolves an items.dat id to a name. Either may be null. `loc` is the
// event.dat location id (== map screen for 0..59), used to resolve OP_0B sign
// ids; pass -1 when unknown.
inline std::string describeOp(uint8_t op, const std::vector<uint8_t>& args,
                              const std::function<std::string(int)>& strAt,
                              const std::function<std::string(int)>& itemAt,
                              int loc = -1) {
    char buf[256];
    auto strHint = [&](int idx) -> std::string {
        if (strAt) {
            std::string s = strAt(idx);
            if (!s.empty()) {
                if (s.size() > 60) s = s.substr(0, 60) + "...";
                return "str[" + std::to_string(idx) + "] \"" + s + "\"";
            }
        }
        return "str[" + std::to_string(idx) + "]";
    };

    switch (op) {
        case 0x01: if (!args.empty()) return "show_text(" + strHint(args[0]) + ")"; break;
        case 0x02: if (!args.empty()) return "show_text_block(" + strHint(args[0]) + ")"; break;
        case 0x03: if (!args.empty()) return "show_text(" + strHint(args[0]) + ")"; break;
        case 0x04: if (!args.empty()) return "show_text_above_door(" + strHint(args[0]) + ")"; break;
        case 0x05: if (!args.empty()) return "show_text_popup_a(" + strHint(args[0]) + ")"; break;
        case 0x06: if (!args.empty()) return "show_text_popup_b(" + strHint(args[0]) + ")"; break;
        case 0x07: return "wait_for_space()";
        case 0x08: return "wait_key()";
        case 0x09: return "cond = prompt_yes_no()";
        case 0x0A: return "cond = prompt_yes_no(mode=1)";
        case 0x0B:
            // Signboard `.anm` sprite (NOT event-local / str.dat text). arg0 ->
            // sign id via per-env sign table (0x15756); arg1 = placement slot.
            if (args.size() >= 2) {
                int sign = serviceSignId(loc, args[0]);
                if (sign >= 0)
                    snprintf(buf, sizeof(buf),
                             "service_sign(idx=0x%02X -> sign %d [%d.anm], pos=0x%02X)",
                             args[0], sign, sign, args[1]);
                else
                    snprintf(buf, sizeof(buf), "service_sign(idx=0x%02X, pos=0x%02X)",
                             args[0], args[1]);
                return buf;
            }
            break;
        case 0x0C:
            if (args.size() >= 2) {
                snprintf(buf, sizeof(buf), "map_transition(dest=0x%02X, 0x%02X)", args[0], args[1]);
                return buf;
            }
            break;
        case 0x0D: if (!args.empty()) { snprintf(buf, sizeof(buf), "play_sound_seq(#%d)  # -$7E42→0x6FB8 audio; not graphics", args[0]); return buf; } break;
        case 0x0E: if (!args.empty()) {
            static const struct { uint8_t id; const char* name; } kSel[] = {
                {0x01, "inn"}, {0x02, "training"}, {0x03, "tavern"}, {0x04, "temple"},
                {0x05, "mages_guild"}, {0x06, "blacksmith"}, {0x07, "general_store"},
                {0x08, "arena_shop"}, {0x64, "portal"},
            };
            for (const auto& s : kSel) if (s.id == args[0]) {
                snprintf(buf, sizeof(buf), "exec_selector(0x%02X)  # %s", args[0], s.name);
                return buf;
            }
            snprintf(buf, sizeof(buf), "exec_selector(0x%02X)", args[0]); return buf;
        } break;
        case 0x0F: return "end_script()";
        case 0x10: { int n = args.empty() ? 0 : args[0]; snprintf(buf, sizeof(buf), "if cond: skip_tokens(%d)", n); return buf; }
        case 0x11: { int n = args.empty() ? 0 : args[0]; snprintf(buf, sizeof(buf), "if !cond: skip_tokens(%d)", n); return buf; }
        case 0x12: return "encounter_setup(12 bytes)";
        case 0x13: return "encounter_setup_b(10 bytes)";
        case 0x14: return "clear_current_tile_event_flag()";
        case 0x15: if (args.size() >= 3) { snprintf(buf, sizeof(buf), "apply_party(count=0x%02X, 0x%02X, 0x%02X)", args[0], args[1], args[2]); return buf; } break;
        case 0x16: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "cond = scan_party_items(id=0x%02X)  # arg1=0x%02X discarded", args[1], args[0]); return buf; } break;
        case 0x17: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "cond = var8_raw(id=0x%02X)  # raw variable byte, not a bool (2nd byte 0x%02X discarded)", args[0], args[1]); return buf; } break;
        case 0x18: if (args.size() >= 4) { snprintf(buf, sizeof(buf), "apply_party_masked(count=0x%02X, set=0x%02X, and=0x%02X, or=0x%02X)", args[0], args[1], args[2], args[3]); return buf; } break;
        case 0x19: if (args.size() >= 4) { snprintf(buf, sizeof(buf), "add_party_entity(0x%02X, f3a=0x%02X, f40=0x%02X, f46=0x%02X)", args[0], args[1], args[2], args[3]); return buf; } break;
        case 0x1A: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "store_var8(group=0x%02X, value=0x%02X)", args[0], args[1]); return buf; } break;
        case 0x1B: if (!args.empty()) { snprintf(buf, sizeof(buf), "cond = (cond >= 0x%02X)", args[0]); return buf; } break;
        case 0x1C: if (!args.empty()) { snprintf(buf, sizeof(buf), "cond = engine_query(0x%02X)", args[0]); return buf; } break;
        case 0x1D: if (!args.empty()) { snprintf(buf, sizeof(buf), "engine_call_rec7(0x%02X)", args[0]); return buf; } break;
        case 0x1E: if (!args.empty()) { snprintf(buf, sizeof(buf), "delay(0x%02X)", args[0]); return buf; } break;
        case 0x1F: if (args.size() >= 6) { snprintf(buf, sizeof(buf), "party_effect(sel=0x%02X, ...)", args[0]); return buf; } break;
        case 0x20: if (args.size() >= 6) { snprintf(buf, sizeof(buf), "party_effect_b(sel=0x%02X, ...)", args[0]); return buf; } break;
        case 0x21:
            if (args.size() >= 3) {
                snprintf(buf, sizeof(buf), "set_tile((y=%d,x=%d), 0x%02X, 0x%02X)",
                         (args[0] >> 4) & 0xF, args[0] & 0xF, args[1], args[2]);
                return buf;
            }
            break;
        case 0x22: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "cond = (era in [%d..%d])", args[0], args[1]); return buf; } break;
        case 0x23: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "cond = check_day_of_year(0x%02X, 0x%02X)", args[0], args[1]); return buf; } break;
        case 0x24: { int v = decodeU16Arg(op, args); if (v >= 0) { snprintf(buf, sizeof(buf), "cond = pay_gold(%d)  # pool-pay 0x6ACE", v); return buf; } } break;
        case 0x25: { int v = decodeU16Arg(op, args); if (v >= 0) { snprintf(buf, sizeof(buf), "cond = pay_gems(%d)  # pool-pay 0x6B9A", v); return buf; } } break;
        case 0x26: return "selected = select_party_member()";
        case 0x27: return "selected = select_party_member(mode=1)";
        case 0x28:
            if (args.size() >= 2) {
                std::string name = itemAt ? itemAt(args[1]) : std::string();
                snprintf(buf, sizeof(buf), "cond = consume_item(id=%d \"%s\", probe=%d)",
                         args[1], name.c_str(), args[0]);
                return buf;
            }
            break;
        case 0x29: return "set_abort_and_exit()";
        case 0x2A: return "load_special_block(...)";
        case 0x2B: { int n = args.empty() ? 0 : args[0]; snprintf(buf, sizeof(buf), "if combat_victory: skip_tokens(%d)", n); return buf; }
        case 0x2C: if (!args.empty()) { snprintf(buf, sizeof(buf), "word_counter[-$79B8] += %d  # u16 counter, flag redraw", args[0]); return buf; } break;
        case 0x2D: if (args.size() >= 2) {
            // arg1 bit7=race(+0xE)/bit6=sex(+0xC)/else class(+0xF); bit5=any-mode(else all);
            // low nibble=val1; if arg1&0xE0==0 then arg2&0xF=val2.
            const char* field = (args[0] & 0x80) ? "race(+0xE)" : (args[0] & 0x40) ? "sex(+0xC)" : "class(+0xF)";
            const char* mode = (args[0] & 0x20) ? "any" : "all";
            snprintf(buf, sizeof(buf), "cond = match_member_attr(%s %s == 0x%X, arg2=0x%02X)", mode, field, args[0] & 0xF, args[1]);
            return buf;
        } break;
        case 0x2E: if (args.size() >= 2) {
            // OR arg2 into member +(uint8)(arg1-0x6E)+0x51 for classes {4,2} (or {3,1}
            // if arg1>=0x80, with arg1 &= 0x7F).
            const bool hi = (args[0] & 0x80) != 0;
            const int a1 = hi ? (args[0] & 0x7F) : args[0];
            const int fieldOff = (static_cast<uint8_t>(a1 - 0x6E) + 0x51) & 0xFF;
            snprintf(buf, sizeof(buf), "member[+0x%02X] |= 0x%02X  # classes %s", fieldOff, args[1], hi ? "{3,1}" : "{4,2}");
            return buf;
        } break;
        case 0x2F: return "read_answer(buf[-$5C50], max=10)";
        case 0x30: {
            // Stored answer: byte = 0x11A - ascii (0xFA == trailing space).
            std::string decoded;
            for (uint8_t b : args) {
                int c = (0x11A - b) & 0xFF;
                decoded += (c >= 0x20 && c <= 0x7E) ? static_cast<char>(c) : '?';
            }
            while (!decoded.empty() && decoded.back() == ' ') decoded.pop_back();
            return "cond = check_answer(\"" + decoded + "\")";
        }
        case 0x31: if (args.size() >= 3) { snprintf(buf, sizeof(buf), "for_party(mask=0x%02X): call(0x%02X, 0x%02X)", args[0], args[1], args[2]); return buf; } break;
        case 0x32: if (!args.empty()) { snprintf(buf, sizeof(buf), "cond = count_title_nibble(id=0x%02X)  # living members whose record+0x50 nibble == id (0x04 Crusader, 0x08 Heroic, 0x09 druid)", args[0]); return buf; } break;
        default: break;
    }

    // Fallback: name + raw hex args.
    std::string out = opcodeName(op);
    if (!args.empty()) {
        out += " ";
        for (size_t i = 0; i < args.size(); ++i) {
            snprintf(buf, sizeof(buf), "%s0x%02X", i ? " " : "", args[i]);
            out += buf;
        }
    }
    return out;
}

}  // namespace mm2

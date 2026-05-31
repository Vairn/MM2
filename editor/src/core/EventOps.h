#pragma once
// event.dat script interpreter opcode metadata.
//
// Ported from tools/decode_event.py (OPCODES / decompile_op) which was in turn
// derived from the 68k dispatcher at 0x172CA, jump table 0x17494, handler stubs
// 0x172D8..0x17484. Opcodes are 0x00..0x32; 0xFF terminates a script record.
//
// argc < 0 marks the variable-length token-skip family (0x10/0x11/0x2B): the
// handler reads one selector byte then walks a token stream whose exact length
// the static decoder cannot resolve, so we consume just the selector byte.

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
        {"OP_0D_ENGINE_CALL", 1},             // 0D
        {"OP_0E_EXEC_SELECTOR", 1},           // 0E
        {"OP_0F_END_SCRIPT", 0},              // 0F
        {"OP_10_IF_TRUE_SKIPTOK", -1},        // 10
        {"OP_11_IF_FALSE_SKIPTOK", -1},       // 11
        {"OP_12_ENCOUNTER_SETUP", 12},        // 12
        {"OP_13_ENCOUNTER_SETUP_B", 10},      // 13
        {"OP_14_CLEAR_TILE_EVENT", 0},        // 14
        {"OP_15_APPLY_PARTY", 3},             // 15
        {"OP_16_CHECK_MONSTER_PRESENT", 2},   // 16
        {"OP_17_LOAD_VAR_TO_COND", 2},        // 17
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
        {"OP_24_CHECK_GOLD_TO_COND", 2},      // 24
        {"OP_25_CHECK_CODE16_TO_COND", 2},    // 25
        {"OP_26_SELECT_MEMBER", 0},           // 26
        {"OP_27_SELECT_MEMBER_B", 0},         // 27
        {"OP_28_CONSUME_ITEM_TO_COND", 2},    // 28
        {"OP_29_SET_ABORT", 0},               // 29
        {"OP_2A_SET_TREASURE", 14},           // 2A
        {"OP_2B_SKIPTOK", -1},                // 2B
        {"OP_2C_ADJUST_STATE", 1},            // 2C
        {"OP_2D_CHECK_MEMBER_ATTR", 2},       // 2D
        {"OP_2E_SET_PARTY_ATTR", 2},          // 2E
        {"OP_2F_CLEAR_INPUT_BUF", 0},         // 2F
        {"OP_30_CHECK_ANSWER", 10},           // 30
        {"OP_31_PARTY_ITERATE_CALL", 3},      // 31
        {"OP_32_LOAD_COND_FROM_VAR", 1},      // 32
    };
    static const OpcodeInfo kUnknown = {"OP_??", 0};
    if (op <= 0x32) return kTable[op];
    return kUnknown;
}

inline bool opcodeKnown(uint8_t op) { return op <= 0x32; }
inline const char* opcodeName(uint8_t op) { return opcodeInfo(op).name; }
inline int opcodeArgc(uint8_t op) { return opcodeInfo(op).argc; }

// String index referenced by a text-display opcode, or -1 if this op does not
// index the string table. (01..06 take a u8 string index; 0B takes one too.)
inline int opcodeStringArg(uint8_t op, const std::vector<uint8_t>& args) {
    switch (op) {
        case 0x01: case 0x02: case 0x03: case 0x04:
        case 0x05: case 0x06: case 0x0B:
            return args.empty() ? -1 : args[0];
        default:
            return -1;
    }
}

// OP_24 reads a little-endian u16; OP_25 reads explicit hi/lo bytes.
inline int decodeU16Arg(uint8_t op, const std::vector<uint8_t>& args) {
    if (args.size() < 2) return -1;
    if (op == 0x25) return ((args[0] << 8) | args[1]) & 0xFFFF;
    return (args[0] | (args[1] << 8)) & 0xFFFF;
}

// Human-readable pseudo-code for one opcode, ported from decompile_op().
// `strAt` returns the string-table entry for an index (for inline previews),
// `itemAt` resolves an items.dat id to a name. Either may be null.
inline std::string describeOp(uint8_t op, const std::vector<uint8_t>& args,
                              const std::function<std::string(int)>& strAt,
                              const std::function<std::string(int)>& itemAt) {
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
            if (args.size() >= 2) {
                snprintf(buf, sizeof(buf), "open_service_window(%s, mode=0x%02X)",
                         strHint(args[0]).c_str(), args[1]);
                return buf;
            }
            break;
        case 0x0C:
            if (args.size() >= 2) {
                snprintf(buf, sizeof(buf), "map_transition(dest=0x%02X, 0x%02X)", args[0], args[1]);
                return buf;
            }
            break;
        case 0x0D: if (!args.empty()) { snprintf(buf, sizeof(buf), "engine_call(0x%02X)", args[0]); return buf; } break;
        case 0x0E: if (!args.empty()) { snprintf(buf, sizeof(buf), "exec_selector(0x%02X)", args[0]); return buf; } break;
        case 0x0F: return "end_script()";
        case 0x10: { int n = args.empty() ? 0 : args[0]; snprintf(buf, sizeof(buf), "if cond: skip_tokens(%d)", n); return buf; }
        case 0x11: { int n = args.empty() ? 0 : args[0]; snprintf(buf, sizeof(buf), "if !cond: skip_tokens(%d)", n); return buf; }
        case 0x12: return "encounter_setup(12 bytes)";
        case 0x13: return "encounter_setup_b(10 bytes)";
        case 0x14: return "clear_current_tile_event_flag()";
        case 0x15: if (args.size() >= 3) { snprintf(buf, sizeof(buf), "apply_party(count=0x%02X, 0x%02X, 0x%02X)", args[0], args[1], args[2]); return buf; } break;
        case 0x16: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "cond = check_monster_present(0x%02X, 0x%02X)", args[0], args[1]); return buf; } break;
        case 0x17: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "cond = load_var8(group=0x%02X, index=0x%02X)", args[0], args[1]); return buf; } break;
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
        case 0x24: { int v = decodeU16Arg(op, args); if (v >= 0) { snprintf(buf, sizeof(buf), "cond = check_gold_at_least(%d)", v); return buf; } } break;
        case 0x25: { int v = decodeU16Arg(op, args); if (v >= 0) { snprintf(buf, sizeof(buf), "cond = check_code16(0x%04X)", v); return buf; } } break;
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
        case 0x2B: { int n = args.empty() ? 0 : args[0]; snprintf(buf, sizeof(buf), "skip_tokens(%d)", n); return buf; }
        case 0x2C: if (!args.empty()) { snprintf(buf, sizeof(buf), "adjust_state(+0x%02X)", args[0]); return buf; } break;
        case 0x2D: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "cond = check_member_attr(fields=0x%02X, value=0x%02X)", args[0], args[1]); return buf; } break;
        case 0x2E: if (args.size() >= 2) { snprintf(buf, sizeof(buf), "set_party_attr(class=0x%02X, bits=0x%02X)", args[0], args[1]); return buf; } break;
        case 0x2F: return "clear_input_buffer()";
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
        case 0x32: if (!args.empty()) { snprintf(buf, sizeof(buf), "cond = load_cond_from_var(0x%02X)", args[0]); return buf; } break;
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

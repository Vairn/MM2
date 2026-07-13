#include "eventlang/OpcodeTable.h"

namespace mm2::eventlang {

const OpcodeInfo& opcodeInfo(uint8_t op) {
    // argc from handler read counts (ASM / EventRuntime::dispatchOp).
    // Token-skip ops 0x10/0x11/0x2B: argc=-1 (selector only for linear parse).
    static const OpcodeInfo kTable[kOpcodeCount] = {
        {"OP_00_INVALID", 0},              // 00 — ROM skip delta 0
        {"OP_01_TEXT", 1},                 // 01 @ 0x15924
        {"OP_02_TEXT_BLOCK", 1},           // 02 @ 0x15942
        {"OP_03_TEXT", 1},                 // 03 @ 0x159CE
        {"OP_04_TEXT_DOOR", 1},            // 04 @ 0x159F4
        {"OP_05_TEXT_POPUP_A", 1},         // 05 @ 0x15A46
        {"OP_06_TEXT_POPUP_B", 1},         // 06 @ 0x15AEE
        {"OP_07_WAIT_SPACE", 0},           // 07 @ 0x15CE6
        {"OP_08_WAIT_SPACE_SCRIPTED", 0},  // 08 @ 0x15D26 — Space + SCRIPTED_KEY_MODE=$FD
        {"OP_09_PROMPT_YN", 0},            // 09 @ 0x15D3C
        {"OP_0A_PROMPT_YN_B", 0},          // 0A @ 0x15D9A
        {"OP_0B_SHOW_SERVICE_WINDOW", 2},  // 0B @ 0x15DB0 — sign index, NOT string
        {"OP_0C_MAP_TRANSITION", 2},       // 0C @ 0x15E12
        {"OP_0D_PLAY_SOUND_SEQ", 1},       // 0D @ 0x15EC4 → play_sound_seq @ 0x6FB8
        {"OP_0E_EXEC_SELECTOR", 1},        // 0E @ 0x160C2
        {"OP_0F_END_SCRIPT", 0},           // 0F @ 0x162A6
        {"OP_10_IF_TRUE_SKIPTOK", -1},     // 10 @ 0x162B8
        {"OP_11_IF_FALSE_SKIPTOK", -1},    // 11 @ 0x162DC
        {"OP_12_ENCOUNTER_SETUP", 12},     // 12 @ 0x16300
        {"OP_13_ENCOUNTER_SETUP_B", 10},   // 13 @ 0x16386
        {"OP_14_CLEAR_TILE_EVENT", 0},     // 14 @ 0x16398
        {"OP_15_APPLY_PARTY", 3},          // 15 @ 0x16426
        {"OP_16_SCAN_PARTY_ITEMS", 2},     // 16 @ 0x16520 — backpack/equip item id scan
        {"OP_17_LOAD_VAR_RAW_TO_COND", 2}, // 17 @ 0x165A4
        {"OP_18_APPLY_PARTY_MASKED", 4},   // 18 @ 0x165C6
        {"OP_19_GIVE_ITEM", 4},            // 19 @ 0x165D8
        {"OP_1A_STORE_VAR8", 2},           // 1A @ 0x166F8
        {"OP_1B_COND_THRESHOLD", 1},       // 1B @ 0x16724
        {"OP_1C_RNG_ROLL_TO_COND", 1},     // 1C @ 0x16742 — cond = rng(1..arg)
        {"OP_1D_AUDIO_WAIT", 1},           // 1D @ 0x16762
        {"OP_1E_DELAY", 1},                // 1E @ 0x16780
        {"OP_1F_PARTY_EFFECT", 6},         // 1F @ 0x1690E
        {"OP_20_PARTY_EFFECT_B", 6},       // 20 @ 0x16A22
        {"OP_21_SET_TILE", 3},             // 21 @ 0x16A34
        {"OP_22_CHECK_ERA_RANGE", 2},      // 22 @ 0x16A9E
        {"OP_23_CHECK_DAY_RANGE", 2},      // 23 @ 0x16ADA
        {"OP_24_PAY_GOLD_TO_COND", 2},     // 24 @ 0x16B54 — LE u16
        {"OP_25_PAY_GEMS_TO_COND", 2},     // 25 @ 0x16B82 — BE u16; ROM skip delta 2
        {"OP_26_SELECT_MEMBER", 0},        // 26 @ 0x16BC0
        {"OP_27_SELECT_MEMBER_B", 0},      // 27 @ 0x16BC0
        {"OP_28_CONSUME_ITEM_TO_COND", 2}, // 28 @ 0x16C86 — backpack consume
        {"OP_29_SET_ABORT", 0},            // 29 @ 0x16D08
        {"OP_2A_SET_TREASURE", 14},        // 2A @ 0x16D16
        {"OP_2B_SKIPTOK_IF_VICTORY", -1},  // 2B @ 0x16D74
        {"OP_2C_ADD_WORD_COUNTER", 1},     // 2C @ 0x16D98
        {"OP_2D_CHECK_MEMBER_ATTR", 2},    // 2D @ 0x16DBA
        {"OP_2E_OR_MEMBER_FIELD", 2},      // 2E @ 0x16F50
        {"OP_2F_READ_ANSWER", 0},          // 2F @ 0x16FEA
        {"OP_30_CHECK_ANSWER", 10},        // 30 @ 0x17034
        {"OP_31_PARTY_ITERATE_DAMAGE", 3}, // 31 @ 0x170BC
        {"OP_32_COUNT_TITLE_NIBBLE", 1},   // 32 @ 0x17190
    };
    static const OpcodeInfo kUnknown = {"OP_??", 0};
    if (op <= 0x32) return kTable[op];
    return kUnknown;
}

uint8_t tokenDelta(uint8_t op) {
    // Byte-exact copy of EventVmHelpers::kOpTokenDelta (ROM A4-$6CC8).
    static const uint8_t kOpTokenDelta[kOpcodeCount] = {
        0,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  3,  3,  2,  2,  1,  2,  2,  13, 11, 1,  4,
        3,  3,  5,  5,  3,  2,  2,  2,  2,  7,  7,  4,  3,  3,  3,  2,  1,  1,  3,  1,  15, 2,
        2,  3,  3,  1,  11, 4,  2,
    };
    if (op < kOpcodeCount) return kOpTokenDelta[op];
    return 1;
}

int decodeU16Arg(uint8_t op, const uint8_t* args, int argc) {
    if (argc < 2 || !args) return 0;
    if (op == 0x25) return ((args[0] << 8) | args[1]) & 0xFFFF;
    return (args[0] | (args[1] << 8)) & 0xFFFF;
}

}  // namespace mm2::eventlang

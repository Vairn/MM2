#pragma once
// event.dat layout constants + OP_0B signboard resolver.
// Opcode names/argc live in eventlang/OpcodeTable.cpp — do not duplicate them here.

#include <cstdint>

namespace mm2 {

constexpr int kEventLocationCount = 71;
constexpr int kEventHeaderSize = kEventLocationCount * 6;   // 426
constexpr int kEventMaxRecord = 0x8AC;                      // runtime length clamp

// OP_0B arg0 → signboard .anm id (handler 0x15DB0 → 0x15756).
//   arg0 >= 0x80 → 0x4B; else TABLE[env][arg0-1] (0x1576C subq #1).
//   env from area_env_lookup (0x18AE); jump table 0x157D2 is not town order.
//   Packed offsets: env0=0, 1=48, 2=96, 3=22, 4=72, 5=96, 6=72.
inline int signEnvForScreen(int screen) {
    static const uint8_t lo[] = {0, 5, 17, 33, 41, 45, 55};
    static const uint8_t hi[] = {4, 16, 32, 40, 44, 54, 59};
    static const uint8_t id[] = {0, 3, 1, 6, 4, 5, 2};
    for (int i = 0; i < 7; ++i)
        if (screen >= lo[i] && screen <= hi[i]) return id[i];
    return 7;
}

// OP_0B arg0 → signboard .anm id for map screen `loc` (0..59). -1 if unknown.
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
    const int i = envOffset[env] + (static_cast<int>(strIdx) - 1);
    if (i < 0 || i >= 120) return -1;
    return kBlock[i];
}

}  // namespace mm2

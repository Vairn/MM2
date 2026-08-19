#pragma once
// Auto-map tile mapping (@0x2182, called by overland_map_view @0x223A).
// Branches on -$79E2:
//   != 0 (outdoor/surface): frame = visual & 0x1F (outb.32 terrain id)
//   == 0 (interior):        frame = kCartoTile[visual >> 2] (A4-$762E / data 0x9D0)
// Tilesets (A4-$7A1A / env dispatcher @0x1880): outdoor → outb.32; town/cavern/castle → townb.32.
// Elemental planes 41..44: visual & 0x1F (outb). Wall-edge overlay and party arrows are not applied here.

#include <cstdint>

namespace mm2 {

// 64-entry table indexed by (visual_byte >> 2). Data hunk offset 0x9D0.
inline constexpr uint8_t kCartoTile[64] = {
    0x00, 0x05, 0x06, 0x05, 0x03, 0x0B, 0x0D, 0x0B,
    0x04, 0x0C, 0x0E, 0x0C, 0x03, 0x0B, 0x0D, 0x0B,
    0x01, 0x0F, 0x11, 0x0F, 0x07, 0x13, 0x16, 0x13,
    0x09, 0x15, 0x19, 0x15, 0x07, 0x13, 0x16, 0x13,
    0x02, 0x10, 0x12, 0x10, 0x08, 0x14, 0x18, 0x14,
    0x0A, 0x17, 0x1A, 0x17, 0x08, 0x14, 0x18, 0x14,
    0x01, 0x0F, 0x11, 0x0F, 0x07, 0x13, 0x16, 0x13,
    0x09, 0x15, 0x19, 0x15, 0x07, 0x13, 0x16, 0x13,
};

// Wall-edge overlay tiles, indexed by (visual & 3) - 1. Data hunk offset 0xA30.
inline constexpr uint8_t kCartoEdge[3] = {0x1B, 0x1C, 0x1B};

constexpr int kElementalPlaneFirst = 41;
constexpr int kElementalPlaneLast = 44;

inline bool isElementalPlane(int screen) {
    return screen >= kElementalPlaneFirst && screen <= kElementalPlaneLast;
}

// Runtime outdoor view: surface sectors (-$79E2) or elemental planes 41..44.
inline bool isOutdoorArea(int screen, bool surfaceNonZero) {
    return isElementalPlane(screen) || surfaceNonZero;
}

inline bool cartoUsesOutb(int screen, bool outdoorSurface) {
    return isOutdoorArea(screen, outdoorSurface);
}

// Cartography frame index. `outdoorSurface` = -$79E2 != 0 or plane screens.
inline int cartoFrame(int screen, uint8_t visual, bool outdoorSurface) {
    if (isOutdoorArea(screen, outdoorSurface)) return visual & 0x1F;
    return kCartoTile[(visual >> 2) & 0x3F];
}

}  // namespace mm2

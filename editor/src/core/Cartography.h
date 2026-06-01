#pragma once
// Auto-map ("cartography") tile mapping, reverse-engineered from the MM2 ASM.
//
// The map-cell -> tileset-frame helper (@0x2182 in mm2.capstone.asm, called by
// the 16x16 overland_map_view @0x223A) branches on the view-mode flag -$79E2:
//
//   * -$79E2 != 0 (outdoor/surface view, set to 1 on entering a surface area):
//         frame = visual_byte & 0x1F           (raw terrain id; @0x222A)
//     The outdoor map bytes ARE terrain tile ids (the high bits 0x20/0x40/0x80
//     are passability/event flags); the low 5 bits index outb.32 directly.
//
//   * -$79E2 == 0 (interior dungeon auto-map):
//         frame = kCartoTile[ visual_byte >> 2 ]   (wall-bit decode; @0x21D4)
//     where kCartoTile is a 64-byte table in the DATA hunk (A4-$762E/data 0x9D0).
//
// Tileset (field A4-$7A1A, loaded by the env dispatcher @0x1880):
//     outdoor / surface   -> outb.32   (filename table entry 25)
//     town/cavern/castle   -> townb.32  (entries 12/16/20, all townb.32)
// Both tilesets are 36 frames of 14x11 px.
//
// Elemental planes (screens 41..44): uniform map bytes 0x28/0x25/0x27/0x26
// encode outb.32 terrain ids 8/5/7/6 (air / fire-lava / earth / water). The
// interior branch @0x21EA also has townb overrides (8/4/4/5) when -$79E2==0;
// editor + wiki use outb + (visual & 0x1F) to match the authored terrain art.
//
// Extra ASM behaviour folded in here:
//   - Wall-edge overlay uses (visual & 3) -> kCartoEdge (frames 27/28/27); the
//     overlay is alpha-composited in-engine and is not applied here.
//   - Direction-arrow frames (party marker) are 0x20..0x23 (N/S/E/W).
//   - Unexplored cells map to 0xFF and are skipped; an editor shows everything.

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

// True when auto-map should blit from outb.32 (surface sectors + elemental planes).
inline bool cartoUsesOutb(int screen, bool outdoorSurface) {
    if (screen >= 41 && screen <= 44) return true;
    return outdoorSurface;
}

// Cartography frame index for a map cell on a given screen (0..59).
// `outdoorSurface` = engine outdoor branch (-$79E2 != 0) or plane screens.
inline int cartoFrame(int screen, uint8_t visual, bool outdoorSurface) {
    if (screen >= 41 && screen <= 44) return visual & 0x1F;
    if (outdoorSurface) return visual & 0x1F;
    return kCartoTile[(visual >> 2) & 0x3F];
}

}  // namespace mm2

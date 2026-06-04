#pragma once

#include "mm2/CppStdCompat.h"

// MM2 Amiga font-8 UI chrome (control glyphs 0x00–0x1F + format chars).
// See EXTRACTED/docs/07-event-script-opcodes.md § "MM2 8×8 Text Format Characters".
namespace mm2::gfx::font_glyphs {

// Box outline @ JSR -$809E / event OP_06 popup B @ 0x15AEE.
constexpr uint8_t kTopBar = 0x0E;
constexpr uint8_t kBottomBar = 0x0F;
constexpr uint8_t kTopLeft = 0x10;
constexpr uint8_t kTopRight = 0x11;
constexpr uint8_t kBottomLeft = 0x12;
constexpr uint8_t kBottomRight = 0x13;
constexpr uint8_t kLeft = 0x14;
constexpr uint8_t kRight = 0x15;

// Full-width horizontal rule (OP_06 rewrites '-' → this before draw).
constexpr uint8_t kHorizRule = 0x7B;  // '{'
// Solid fill tile (Popup B interior / borders).
constexpr uint8_t kSolidBlock = 0x7E;  // '~'

}  // namespace mm2::gfx::font_glyphs

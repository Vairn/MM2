#pragma once

#include "mm2/CppStdCompat.h"

// MM2 Amiga font-8 UI chrome (control glyphs 0x00–0x1F + format chars).
// See EXTRACTED/docs/07-event-script-opcodes.md § "MM2 8×8 Text Format Characters".
namespace mm2::gfx::font_glyphs {

// Box outline glyphs for event OP_06 outdoor signpost @ 0x15AEE (pen A4-$7A50).
// Character UI red frame uses the same glyph codes via JSR -$809E (pen $17) — not OP_06.
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

// read_key_with_spinner @ 0x3D8C — A4-$77BC 8-byte glyph table (data @ 0x842).
// Cycle: | \ — / | \ — /  (ctrl 0x1C / 0x1D / 0x05 / 0x1F).
constexpr uint8_t kSpinnerGlyphs[] = {0x1C, 0x1D, 0x05, 0x1F, 0x1C, 0x1D, 0x05, 0x1F};
constexpr int kSpinnerGlyphCount = 8;
/* Delay arg #$32 → -$7BC0 (0x22B4A) clamps/divides by #$14 → 2 wait units. */
constexpr int kSpinnerStepTicks = 2;

}  // namespace mm2::gfx::font_glyphs

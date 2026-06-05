#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MM2_FONT8X8_GLYPHS 128
#define MM2_FONT8X8_ROWS 8

/** ROM image; on Amiga use mm2_font8x8_live() after mm2_amiga_font_init(). */
extern const uint8_t mm2_font8x8[128][8];

/** Active glyph table (chip copy on Amiga, else mm2_font8x8). */
const uint8_t *mm2_font8x8_live(void);

#if defined(AMIGA)
/** Copy mm2_font8x8 into chip RAM (call once after memCreate). */
void mm2_amiga_font_init(void);
#endif

#ifdef __cplusplus
}
#endif

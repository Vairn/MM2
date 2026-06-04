#pragma once

#include "mm2/CppStdCompat.h"

namespace mm2::gfx {

class ScreenCompositor {
public:
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 200;  // NTSC display height (Amiga MM2 runs 320x200)
    static constexpr int kStride = kWidth * 4;
    static constexpr int kCellW = 8;
    static constexpr int kCellH = 8;

    ScreenCompositor();
    ~ScreenCompositor();

    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255);
    void clearRect(int x, int y, int w, int h, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255);
    void fillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    void blitRgba(const uint8_t *src, int src_w, int src_h, int dst_x, int dst_y, bool skip_transparent = true,
                  uint8_t global_alpha = 255);

    /** Draw one font-8 glyph by codepoint (0..127). */
    void drawGlyph(int x, int y, uint8_t codepoint, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
                   uint8_t a = 255);

    void drawText(int x, int y, const char *text, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255);
    void drawTextShadow(int x, int y, const char *text, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);

    /** Red console outline in text cells @ ASM JSR -$809E (font glyphs 0x0E..0x15). */
    void drawConsoleBox(int row, int col, int w_cells, int h_cells, uint8_t r = 255, uint8_t g = 0, uint8_t b = 0,
                        uint8_t a = 255);

    /** Hollow rectangle in pixels (legacy fallback). */
    void drawBoxBorder(int x, int y, int w, int h, uint8_t r = 255, uint8_t g = 0, uint8_t b = 0,
                       uint8_t a = 255);

    const uint8_t *pixels() const { return rgba_; }
    int width() const { return kWidth; }
    int height() const { return kHeight; }

private:
    void drawChar(int x, int y, char ch, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void putPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    /* On Amiga the ACE simpleBuffer owns the bitplanes — no RGBA buffer needed.
     * On PC we heap-allocate so the 250 KB doesn't land on the stack. */
    uint8_t *rgba_ = nullptr;
};

}  // namespace mm2::gfx

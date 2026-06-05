#include "mm2/gfx/ScreenCompositor.h"

#include "mm2/gfx/Mm2FontGlyphs.h"
#include "mm2/gfx/mm2_font8x8.h"

#include "mm2/CppStdCompat.h"
#include "mm2/Config.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#endif

#if !MM2_HOST_AMIGA
#include <cstdlib>
#endif

namespace mm2::gfx {

ScreenCompositor::ScreenCompositor()
{
#if !MM2_HOST_AMIGA
    rgba_ = static_cast<uint8_t *>(std::malloc(kWidth * kHeight * 4));
    clear();
#endif
    /* On Amiga rgba_ stays nullptr — ACE owns the bitplane buffers. */
}

ScreenCompositor::~ScreenCompositor()
{
#if !MM2_HOST_AMIGA
    std::free(rgba_);
    rgba_ = nullptr;
#endif
}

void ScreenCompositor::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
#if MM2_HOST_AMIGA
    if (!rgba_) {
        if (r == 0 && g == 0 && b == 0 && a >= 255) {
            mm2_amiga_clear_screen();
        } else {
            mm2_amiga_fill_rect_rgb(0, 0, kWidth, kHeight, r, g, b, a);
        }
        return;
    }
#endif
    if (!rgba_) {
        return;
    }
    for (int i = 0; i < kWidth * kHeight; ++i) {
        rgba_[i * 4 + 0] = r;
        rgba_[i * 4 + 1] = g;
        rgba_[i * 4 + 2] = b;
        rgba_[i * 4 + 3] = a;
    }
}

void ScreenCompositor::putPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
#if MM2_HOST_AMIGA
    mm2_amiga_fill_rect_rgb((UWORD)x, (UWORD)y, 1, 1, r, g, b, a);
    return;
#else
    if (!rgba_ || x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return;
    }
    const int i = (y * kWidth + x) * 4;
    if (a == 255 || rgba_[i + 3] == 0) {
        rgba_[i + 0] = r;
        rgba_[i + 1] = g;
        rgba_[i + 2] = b;
        rgba_[i + 3] = a;
        return;
    }
    const uint32_t src_a = a;
    const uint32_t dst_a = rgba_[i + 3];
    const uint32_t inv_src = 255u - src_a;
    const uint32_t out_a = src_a + (dst_a * inv_src + 127u) / 255u;
    if (out_a == 0u) {
        return;
    }
    rgba_[i + 0] = static_cast<uint8_t>((r * src_a + rgba_[i + 0] * dst_a * inv_src / 255u + out_a / 2u) / out_a);
    rgba_[i + 1] = static_cast<uint8_t>((g * src_a + rgba_[i + 1] * dst_a * inv_src / 255u + out_a / 2u) / out_a);
    rgba_[i + 2] = static_cast<uint8_t>((b * src_a + rgba_[i + 2] * dst_a * inv_src / 255u + out_a / 2u) / out_a);
    rgba_[i + 3] = static_cast<uint8_t>(out_a);
#endif
}

void ScreenCompositor::clearRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
#if MM2_HOST_AMIGA
    mm2_amiga_fill_rect_rgb((UWORD)x, (UWORD)y, (UWORD)w, (UWORD)h, r, g, b, a);
#else
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            putPixel(px, py, r, g, b, a);
        }
    }
#endif
}

void ScreenCompositor::fillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    clearRect(x, y, w, h, r, g, b, a);
}

void ScreenCompositor::blitRgba(const uint8_t *src, int src_w, int src_h, int dst_x, int dst_y,
                                bool skip_transparent, uint8_t global_alpha)
{
    if (!src || src_w <= 0 || src_h <= 0) {
        return;
    }
    for (int y = 0; y < src_h; ++y) {
        const int oy = dst_y + y;
        if (oy < 0 || oy >= kHeight) {
            continue;
        }
        for (int x = 0; x < src_w; ++x) {
            const int ox = dst_x + x;
            if (ox < 0 || ox >= kWidth) {
                continue;
            }
            const uint8_t *sp = &src[(static_cast<size_t>(y) * src_w + x) * 4];
            if (skip_transparent && sp[3] == 0) {
                continue;
            }
            const uint8_t a =
                global_alpha == 255 ? sp[3] : static_cast<uint8_t>((sp[3] * global_alpha) / 255);
            putPixel(ox, oy, sp[0], sp[1], sp[2], a);
        }
    }
}

void ScreenCompositor::drawGlyph(int x, int y, uint8_t codepoint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (codepoint >= MM2_FONT8X8_GLYPHS) {
        return;
    }
#if MM2_HOST_AMIGA
    mm2_amiga_draw_glyph8((UWORD)x, (UWORD)y, codepoint, r, g, b, a);
    return;
#else
    // Mm2Font8x8_data.inc: LSB column order (bit 0 = leftmost pixel).
    const uint8_t *glyph = mm2_font8x8_live()[codepoint];
    for (int row = 0; row < MM2_FONT8X8_ROWS; ++row) {
        const uint8_t bits = glyph[row];
        for (int col = 0; col < 8; ++col) {
            if ((bits >> col) & 1u) {
                putPixel(x + col, y + row, r, g, b, a);
            }
        }
    }
#endif
}

void ScreenCompositor::drawChar(int x, int y, char ch, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    const unsigned uch = static_cast<unsigned char>(ch);
    if (uch < 32u || uch > 127u) {
        return;
    }
    drawGlyph(x, y, static_cast<uint8_t>(uch), r, g, b, a);
}

void ScreenCompositor::drawText(int x, int y, const char *text, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!text) {
        return;
    }
    int cx = x;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n') {
            cx = x;
            y += 8;
            continue;
        }
        drawChar(cx, y, *p, r, g, b, a);
        cx += 8;
    }
}

void ScreenCompositor::drawTextShadow(int x, int y, const char *text, uint8_t r, uint8_t g, uint8_t b)
{
    drawText(x + 1, y + 1, text, 0, 0, 0, 255);
    drawText(x, y, text, r, g, b, 255);
}

void ScreenCompositor::drawConsoleBox(int row, int col, int w_cells, int h_cells, uint8_t r, uint8_t g, uint8_t b,
                                      uint8_t a)
{
    using namespace font_glyphs;
    if (w_cells < 2 || h_cells < 2) {
        return;
    }
    const int x0 = col * kCellW;
    const int y0 = row * kCellH;

    drawGlyph(x0, y0, kTopLeft, r, g, b, a);
    for (int c = 1; c < w_cells - 1; ++c) {
        drawGlyph(x0 + c * kCellW, y0, kTopBar, r, g, b, a);
    }
    drawGlyph(x0 + (w_cells - 1) * kCellW, y0, kTopRight, r, g, b, a);

    for (int rr = 1; rr < h_cells - 1; ++rr) {
        const int y = y0 + rr * kCellH;
        drawGlyph(x0, y, kLeft, r, g, b, a);
        drawGlyph(x0 + (w_cells - 1) * kCellW, y, kRight, r, g, b, a);
    }

    const int y_bot = y0 + (h_cells - 1) * kCellH;
    drawGlyph(x0, y_bot, kBottomLeft, r, g, b, a);
    for (int c = 1; c < w_cells - 1; ++c) {
        drawGlyph(x0 + c * kCellW, y_bot, kBottomBar, r, g, b, a);
    }
    drawGlyph(x0 + (w_cells - 1) * kCellW, y_bot, kBottomRight, r, g, b, a);
}

void ScreenCompositor::drawBoxBorder(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (w < 2 || h < 2) {
        return;
    }
    for (int px = x; px < x + w; ++px) {
        putPixel(px, y, r, g, b, a);
        putPixel(px, y + h - 1, r, g, b, a);
    }
    for (int py = y; py < y + h; ++py) {
        putPixel(x, py, r, g, b, a);
        putPixel(x + w - 1, py, r, g, b, a);
    }
}

}  // namespace mm2::gfx

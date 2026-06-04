// Offline golden check: render known strings with ScreenCompositor and write PPM.
// Run: game/build/font_golden.exe && inspect game/build/font_golden.ppm
#include "mm2/gfx/ScreenCompositor.h"

#include <cstdio>
#include <cstring>

namespace {

bool writePpm(const char *path, const mm2::gfx::ScreenCompositor &c)
{
    FILE *fp = std::fopen(path, "wb");
    if (!fp) {
        return false;
    }
    const int w = c.width();
    const int h = c.height();
    std::fprintf(fp, "P6\n%d %d\n255\n", w, h);
    const uint8_t *px = c.pixels();
    for (int i = 0; i < w * h; ++i) {
        const uint8_t out[3] = {px[i * 4 + 0], px[i * 4 + 1], px[i * 4 + 2]};
        if (std::fwrite(out, 1, 3, fp) != 3) {
            std::fclose(fp);
            return false;
        }
    }
    std::fclose(fp);
    return true;
}

}  // namespace

int main()
{
    mm2::gfx::ScreenCompositor c;
    c.clear(0, 0, 0, 255);
    c.drawTextShadow(12, 16, "and", 255, 220, 90);
    c.drawTextShadow(12, 32, "C - Create Character", 230, 230, 230);
    c.drawTextShadow(12, 48, "Gates To Another World!", 255, 220, 90);
    c.drawConsoleBox(2, 2, 20, 10, 255, 0, 0, 255);
    if (!writePpm("font_golden.ppm", c)) {
        std::fprintf(stderr, "failed to write font_golden.ppm\n");
        return 1;
    }
    std::fprintf(stdout, "wrote font_golden.ppm\n");
    return 0;
}

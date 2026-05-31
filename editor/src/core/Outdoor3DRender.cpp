#include "core/Outdoor3DRender.h"

#include <algorithm>
#include <cstdio>

namespace mm2 {

namespace {

std::string joinPath(const std::string& dir, const char* name) {
    if (dir.empty()) return name;
    if (dir.back() == '/' || dir.back() == '\\') return dir + name;
    return dir + "/" + name;
}

}  // namespace

OutdoorSheetCache::OutdoorSheetCache(std::string dataDir) : dataDir_(std::move(dataDir)) {}

GfxImage& OutdoorSheetCache::loadSheet(const char* name) {
    const std::string key = name;
    auto it = sheets_.find(key);
    if (it != sheets_.end()) return it->second;

    GfxImage img;
    gfxLoad(joinPath(dataDir_, name), false, img);
    auto inserted = sheets_.emplace(key, std::move(img));
    return inserted.first->second;
}

const GfxFrame* OutdoorSheetCache::frame(const char* sheetName, int frameIndex) {
    GfxImage& img = loadSheet(sheetName);
    if (!img.ok || frameIndex < 0 || frameIndex >= static_cast<int>(img.frames.size()))
        return nullptr;
    return &img.frames[static_cast<size_t>(frameIndex)];
}

const GfxFrame* OutdoorSheetCache::biomeFrame(OutdoorBiome biome, int frameIndex) {
    return frame(outdoorBiomeFilename(biome), frameIndex);
}

const GfxFrame* OutdoorSheetCache::horizonFrame(OutdoorHorizonSheet sheet, int frameIndex) {
    return frame(outdoorHorizonFilename(sheet), frameIndex);
}

void outdoorBlitFrame(OutdoorCanvas& canvas, const GfxFrame& sprite, int x, int y) {
    for (int sy = 0; sy < sprite.height; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= canvas.height) continue;
        for (int sx = 0; sx < sprite.width; ++sx) {
            const int dx = x + sx;
            if (dx < 0 || dx >= canvas.width) continue;
            const uint8_t* src = &sprite.rgba[static_cast<size_t>((sy * sprite.width + sx) * 4)];
            if (src[3] == 0) continue;
            uint8_t* dst =
                &canvas.rgba[static_cast<size_t>((dy * canvas.width + dx) * 4)];
            if (src[3] >= 250) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
            } else {
                const float a = src[3] / 255.f;
                const float ia = 1.f - a;
                dst[0] = static_cast<uint8_t>(src[0] * a + dst[0] * ia);
                dst[1] = static_cast<uint8_t>(src[1] * a + dst[1] * ia);
                dst[2] = static_cast<uint8_t>(src[2] * a + dst[2] * ia);
                dst[3] = 255;
            }
        }
    }
}

bool renderOutdoorView(const OutdoorScene& scene, OutdoorSheetCache& cache, OutdoorCanvas& out,
                       const char* floorSheet, const char* skySheet) {
    out = OutdoorCanvas{};
    std::fill(out.rgba.begin(), out.rgba.end(), 0);

    if (const GfxFrame* floor = cache.frame(floorSheet, 0))
        outdoorBlitFrame(out, *floor, kView3DOriginX, kView3DFloorY);
    if (const GfxFrame* sky = cache.frame(skySheet, 0))
        outdoorBlitFrame(out, *sky, kView3DOriginX, kView3DSkyY);

    for (const OutdoorSpriteBlit& b : scene.decor) {
        if (const GfxFrame* spr = cache.biomeFrame(b.biome, b.frame))
            outdoorBlitFrame(out, *spr, b.x, b.y);
    }
    for (const OutdoorSpriteBlit& b : scene.horizon) {
        if (const GfxFrame* spr = cache.horizonFrame(b.horizon, b.frame))
            outdoorBlitFrame(out, *spr, b.x, b.y);
    }
    return true;
}

bool writeOutdoorCanvasPpm(const OutdoorCanvas& canvas, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    std::fprintf(fp, "P6\n%d %d\n255\n", canvas.width, canvas.height);
    for (int i = 0; i < canvas.width * canvas.height; ++i) {
        const uint8_t* px = &canvas.rgba[static_cast<size_t>(i * 4)];
        std::fputc(px[0], fp);
        std::fputc(px[1], fp);
        std::fputc(px[2], fp);
    }
    std::fclose(fp);
    return true;
}

}  // namespace mm2

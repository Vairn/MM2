#pragma once

#include <string>
#include <unordered_map>

#include "core/Gfx.h"
#include "core/Outdoor3D.h"

namespace mm2 {

constexpr int kOutdoorViewW = kView3DViewportW;
constexpr int kOutdoorViewH = 120;

struct OutdoorCanvas {
    int width = kOutdoorViewW;
    int height = kOutdoorViewH;
    std::vector<uint8_t> rgba;  // width*height*4, premultiplied-friendly over blend

    OutdoorCanvas() { rgba.assign(static_cast<size_t>(width * height * 4), 0); }
};

class OutdoorSheetCache {
public:
    explicit OutdoorSheetCache(std::string dataDir);

    const GfxFrame* frame(const char* sheetName, int frameIndex);
    const GfxFrame* biomeFrame(OutdoorBiome biome, int frameIndex);
    const GfxFrame* horizonFrame(OutdoorHorizonSheet sheet, int frameIndex);

private:
    std::string dataDir_;
    std::unordered_map<std::string, GfxImage> sheets_;

    GfxImage& loadSheet(const char* name);
};

void outdoorBlitFrame(OutdoorCanvas& canvas, const GfxFrame& sprite, int x, int y);

bool renderOutdoorView(const OutdoorScene& scene, OutdoorSheetCache& cache, OutdoorCanvas& out,
                       const char* floorSheet = "outf.32", const char* skySheet = "sky.32");

bool writeOutdoorCanvasPpm(const OutdoorCanvas& canvas, const std::string& path);

}  // namespace mm2

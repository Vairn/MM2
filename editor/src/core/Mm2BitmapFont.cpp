#include "core/Mm2BitmapFont.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "stb_image.h"

namespace mm2 {
namespace {

constexpr int kGlyphColumns = 16;
constexpr int kGlyphCount = 128;
std::string g_fontDebugStatus = "MM2 font: not initialized";

struct GlyphRect {
    int codepoint = 0;
    ImFontAtlasRectId rectId = ImFontAtlasRectId_Invalid;
};

std::filesystem::path findPngPath(int fontPx) {
    const std::string filename = "mm2_" + std::to_string(fontPx) + ".png";
    const std::string fallback = "mm2_16.png";
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("editor/assets/fonts") / filename,
        std::filesystem::path("assets/fonts") / filename,
        std::filesystem::path("../assets/fonts") / filename,
        std::filesystem::path("../editor/assets/fonts") / filename,
        std::filesystem::path("../../editor/assets/fonts") / filename,
        std::filesystem::path("editor/assets/fonts") / fallback,
        std::filesystem::path("assets/fonts") / fallback,
        std::filesystem::path("../assets/fonts") / fallback,
        std::filesystem::path("../editor/assets/fonts") / fallback,
        std::filesystem::path("../../editor/assets/fonts") / fallback,
    };
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) return p;
    }
    return {};
}

bool loadRgbaPng(const std::filesystem::path& path, int* w, int* h, std::vector<uint8_t>* rgba) {
    int n = 0;
    stbi_uc* p = stbi_load(path.string().c_str(), w, h, &n, 4);
    if (!p || !w || !h || *w <= 0 || *h <= 0) return false;
    const size_t sz = static_cast<size_t>(*w) * static_cast<size_t>(*h) * 4u;
    rgba->assign(p, p + sz);
    stbi_image_free(p);
    return true;
}

bool blitCellToAtlas(const std::vector<uint8_t>& src, int srcW, int srcH, int srcX, int srcY,
                     uint8_t* dst, int dstW, int dstH, int dstX, int dstY, int copyW, int copyH) {
    if (!dst) return false;
    if (srcX < 0 || srcY < 0 || dstX < 0 || dstY < 0) return false;
    if (srcX + copyW > srcW || srcY + copyH > srcH) return false;
    if (dstX + copyW > dstW || dstY + copyH > dstH) return false;
    for (int y = 0; y < copyH; ++y) {
        for (int x = 0; x < copyW; ++x) {
            const size_t so = static_cast<size_t>(((srcY + y) * srcW + (srcX + x)) * 4);
            const size_t doff = static_cast<size_t>(((dstY + y) * dstW + (dstX + x)) * 4);
            dst[doff + 0] = src[so + 0];
            dst[doff + 1] = src[so + 1];
            dst[doff + 2] = src[so + 2];
            dst[doff + 3] = src[so + 3];
        }
    }
    return true;
}

}  // namespace

ImFont* installMm2BitmapFont(ImGuiIO& io, float sizePixels) {
    const int requestedPx = std::max(1, static_cast<int>(std::lround(sizePixels)));
    const std::filesystem::path pngPath = findPngPath(requestedPx);
    if (pngPath.empty()) {
        std::fprintf(stderr, "[mm2-font] failed: no png found for requested size %d\n", requestedPx);
        g_fontDebugStatus = "MM2 font: FAILED (png not found)";
        return nullptr;
    }
    std::fprintf(stderr, "[mm2-font] using png: %s\n", pngPath.string().c_str());
    g_fontDebugStatus = "MM2 font: loading " + pngPath.string();

    int srcW = 0;
    int srcH = 0;
    std::vector<uint8_t> srcRgba;
    if (!loadRgbaPng(pngPath, &srcW, &srcH, &srcRgba)) {
        std::fprintf(stderr, "[mm2-font] failed: stbi_load failed\n");
        g_fontDebugStatus = "MM2 font: FAILED (stbi_load)";
        return nullptr;
    }
    if (srcW <= 0 || srcH <= 0 || srcW % kGlyphColumns != 0) {
        std::fprintf(stderr, "[mm2-font] failed: invalid atlas size %dx%d\n", srcW, srcH);
        g_fontDebugStatus = "MM2 font: FAILED (invalid atlas size)";
        return nullptr;
    }

    const int cellW = srcW / kGlyphColumns;
    if (cellW <= 0) {
        std::fprintf(stderr, "[mm2-font] failed: invalid cell width %d\n", cellW);
        g_fontDebugStatus = "MM2 font: FAILED (invalid cell width)";
        return nullptr;
    }
    const int rows = srcH / cellW;
    if (rows <= 0 || rows * kGlyphColumns < kGlyphCount) {
        std::fprintf(stderr, "[mm2-font] failed: invalid rows=%d for glyph count\n", rows);
        g_fontDebugStatus = "MM2 font: FAILED (invalid row count)";
        return nullptr;
    }
    const int cellH = srcH / rows;
    if (cellH <= 0) {
        std::fprintf(stderr, "[mm2-font] failed: invalid cell height %d\n", cellH);
        g_fontDebugStatus = "MM2 font: FAILED (invalid cell height)";
        return nullptr;
    }

    ImFontAtlas* atlas = io.Fonts;
    static const ImWchar kDummyRange[] = {0xE000, 0xE000, 0};
    ImFontConfig cfg;
    cfg.SizePixels = static_cast<float>(cellH);
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    cfg.GlyphRanges = kDummyRange;
    cfg.Name[0] = '\0';

    ImFont* font = atlas->AddFontDefault(&cfg);
    if (!font) {
        std::fprintf(stderr, "[mm2-font] failed: AddFontDefault returned null\n");
        g_fontDebugStatus = "MM2 font: FAILED (AddFontDefault)";
        return nullptr;
    }

    std::vector<GlyphRect> rects;
    rects.reserve(kGlyphCount);
    const float scaledSize = static_cast<float>(requestedPx) *
                             std::max(1.0f, io.DisplayFramebufferScale.y);
    for (int cp = 0; cp < kGlyphCount; ++cp) {
        ImFontAtlasRectId rectId = atlas->AddCustomRectFontGlyphForSize(
            font, static_cast<float>(cellH), static_cast<ImWchar>(cp), cellW, cellH,
            static_cast<float>(cellW));
        if (rectId == ImFontAtlasRectId_Invalid) {
            // Compatibility fallback for builds where per-size API doesn't populate.
            rectId = atlas->AddCustomRectFontGlyph(
                font, static_cast<ImWchar>(cp), cellW, cellH, static_cast<float>(cellW));
        }
        if (rectId == ImFontAtlasRectId_Invalid && std::fabs(scaledSize - static_cast<float>(cellH)) > 0.5f) {
            // Add glyphs for a likely DPI-scaled baked size.
            rectId = atlas->AddCustomRectFontGlyphForSize(
                font, scaledSize, static_cast<ImWchar>(cp), cellW, cellH, static_cast<float>(cellW));
        }
        if (rectId == ImFontAtlasRectId_Invalid) continue;
        rects.push_back({cp, rectId});
    }

    // OpenGL backend in this workspace expects RGBA32 texture format.
    atlas->TexDesiredFormat = ImTextureFormat_RGBA32;
    unsigned char* pixels = nullptr;
    int texW = 0;
    int texH = 0;
    int bytesPerPixel = 0;
    atlas->GetTexDataAsRGBA32(&pixels, &texW, &texH, &bytesPerPixel);
    if (bytesPerPixel != 4) {
        std::fprintf(stderr, "[mm2-font] failed: bytesPerPixel=%d (expected 4)\n", bytesPerPixel);
        g_fontDebugStatus = "MM2 font: FAILED (atlas bpp != 4)";
        return nullptr;
    }
    if (!pixels || texW <= 0 || texH <= 0) {
        std::fprintf(stderr, "[mm2-font] failed: atlas pixel buffer unavailable\n");
        g_fontDebugStatus = "MM2 font: FAILED (atlas pixels unavailable)";
        return nullptr;
    }

    for (size_t i = 0; i < rects.size(); ++i) {
        const int cp = rects[i].codepoint;
        const int sx = (cp % kGlyphColumns) * cellW;
        const int sy = (cp / kGlyphColumns) * cellH;
        ImFontAtlasRect rect;
        if (!atlas->GetCustomRect(rects[i].rectId, &rect)) continue;
        const int copyW = std::min<int>(cellW, rect.w);
        const int copyH = std::min<int>(cellH, rect.h);
        blitCellToAtlas(srcRgba, srcW, srcH, sx, sy, pixels, texW, texH, rect.x, rect.y, copyW, copyH);
    }

    io.FontDefault = font;
    std::fprintf(stderr, "[mm2-font] loaded: atlas=%dx%d cell=%dx%d glyphs=%d\n",
                 srcW, srcH, cellW, cellH, static_cast<int>(rects.size()));
    g_fontDebugStatus = "MM2 font: LOADED (" + pngPath.string() + ")";
    return font;
}

const char* mm2BitmapFontDebugStatus() {
    return g_fontDebugStatus.c_str();
}

}  // namespace mm2


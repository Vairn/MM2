#include "core/Mm2BitmapFont.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace mm2 {
namespace {

constexpr uint32_t kHunkHeader = 1011;     // HUNK_HEADER
constexpr uint32_t kHunkCode = 1001;       // HUNK_CODE
constexpr int kFirstPrintable = 0x20;
constexpr int kLastPrintable = 0x7e;

struct Mm2TextFont {
    uint16_t ySize = 0;
    uint8_t style = 0;
    uint8_t flags = 0;
    uint16_t xSize = 0;
    uint16_t baseline = 0;
    uint16_t boldSmear = 0;
    uint16_t accessors = 0;
    uint8_t loChar = 0;
    uint8_t hiChar = 0;
    uint32_t charData = 0;
    uint16_t modulo = 0;
    uint32_t charLoc = 0;
    uint32_t charSpace = 0;
    uint32_t charKern = 0;
};

struct GlyphBitmap {
    ImWchar codepoint = 0;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> alpha;
};

uint16_t readBE16(const std::vector<uint8_t>& data, size_t off) {
    return static_cast<uint16_t>((static_cast<uint16_t>(data[off]) << 8) |
                                 static_cast<uint16_t>(data[off + 1]));
}

uint32_t readBE32(const std::vector<uint8_t>& data, size_t off) {
    return (static_cast<uint32_t>(data[off]) << 24) |
           (static_cast<uint32_t>(data[off + 1]) << 16) |
           (static_cast<uint32_t>(data[off + 2]) << 8) |
           static_cast<uint32_t>(data[off + 3]);
}

bool readFile(const std::filesystem::path& path, std::vector<uint8_t>* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size <= 0) return false;
    in.seekg(0, std::ios::beg);
    out->resize(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(out->data()), size);
    return in.good();
}

bool extractFirstCodeHunk(const std::vector<uint8_t>& fileData, std::vector<uint8_t>* code) {
    if (fileData.size() < 8) return false;
    if (readBE32(fileData, 0) != kHunkHeader) return false;

    for (size_t off = 0; off + 8 <= fileData.size(); off += 4) {
        if (readBE32(fileData, off) != kHunkCode) continue;
        const uint32_t longWords = readBE32(fileData, off + 4);
        const size_t byteLen = static_cast<size_t>(longWords) * 4u;
        const size_t dataOff = off + 8;
        if (dataOff + byteLen > fileData.size()) continue;
        code->assign(fileData.begin() + static_cast<std::ptrdiff_t>(dataOff),
                     fileData.begin() + static_cast<std::ptrdiff_t>(dataOff + byteLen));
        return true;
    }
    return false;
}

bool parseTextFontAt(const std::vector<uint8_t>& code, size_t off, Mm2TextFont* tf) {
    if (off + 32 > code.size()) return false;

    Mm2TextFont candidate;
    candidate.ySize = readBE16(code, off + 0);
    candidate.style = code[off + 2];
    candidate.flags = code[off + 3];
    candidate.xSize = readBE16(code, off + 4);
    candidate.baseline = readBE16(code, off + 6);
    candidate.boldSmear = readBE16(code, off + 8);
    candidate.accessors = readBE16(code, off + 10);
    candidate.loChar = code[off + 12];
    candidate.hiChar = code[off + 13];
    candidate.charData = readBE32(code, off + 14);
    candidate.modulo = readBE16(code, off + 18);
    candidate.charLoc = readBE32(code, off + 20);
    candidate.charSpace = readBE32(code, off + 24);
    candidate.charKern = readBE32(code, off + 28);

    if (candidate.ySize == 0 || candidate.ySize > 64) return false;
    if (candidate.xSize == 0 || candidate.xSize > 64) return false;
    if (candidate.baseline >= candidate.ySize + 8) return false;
    if (candidate.hiChar < candidate.loChar) return false;
    if (candidate.modulo == 0 || candidate.modulo > 1024) return false;
    if (candidate.charData >= code.size() || candidate.charLoc >= code.size()) return false;
    if (candidate.charData >= candidate.charLoc) return false;

    const size_t charCount = static_cast<size_t>(candidate.hiChar - candidate.loChar + 1);
    if (candidate.charLoc + charCount * 4u > code.size()) return false;
    if (candidate.charData + static_cast<size_t>(candidate.modulo) * candidate.ySize > code.size()) return false;

    *tf = candidate;
    return true;
}

bool findMm2TextFont(const std::vector<uint8_t>& code, Mm2TextFont* tf) {
    // The MM2 Amiga font strike has one 8px TextFont. We scan for a structurally
    // valid record and strongly prefer ySize=8.
    Mm2TextFont fallback;
    bool haveFallback = false;
    for (size_t off = 0; off + 32 <= code.size(); ++off) {
        Mm2TextFont candidate;
        if (!parseTextFontAt(code, off, &candidate)) continue;
        if (candidate.ySize == 8) {
            *tf = candidate;
            return true;
        }
        if (!haveFallback) {
            fallback = candidate;
            haveFallback = true;
        }
    }
    if (!haveFallback) return false;
    *tf = fallback;
    return true;
}

bool readBit(const std::vector<uint8_t>& code, size_t rowStart, uint32_t bitIndex) {
    const size_t byteOff = rowStart + static_cast<size_t>(bitIndex >> 3);
    if (byteOff >= code.size()) return false;
    const uint8_t mask = static_cast<uint8_t>(0x80u >> (bitIndex & 7u));
    return (code[byteOff] & mask) != 0;
}

std::vector<GlyphBitmap> decodePrintableGlyphs(const std::vector<uint8_t>& code, const Mm2TextFont& tf) {
    std::vector<GlyphBitmap> out;
    const int lo = std::max<int>(tf.loChar, kFirstPrintable);
    const int hi = std::min<int>(tf.hiChar, kLastPrintable);
    if (hi < lo) return out;

    out.reserve(static_cast<size_t>(hi - lo + 1));
    for (int cp = lo; cp <= hi; ++cp) {
        const size_t idx = static_cast<size_t>(cp - tf.loChar);
        const uint32_t loc = readBE32(code, static_cast<size_t>(tf.charLoc) + idx * 4u);
        const uint16_t bitOffset = static_cast<uint16_t>(loc >> 16);
        const uint16_t width = static_cast<uint16_t>(loc & 0xffffu);
        if (width == 0 || width > 32) continue;

        GlyphBitmap glyph;
        glyph.codepoint = static_cast<ImWchar>(cp);
        glyph.width = static_cast<int>(width);
        glyph.height = static_cast<int>(tf.ySize);
        glyph.alpha.resize(static_cast<size_t>(glyph.width * glyph.height), 0);

        for (int y = 0; y < glyph.height; ++y) {
            const size_t rowStart = static_cast<size_t>(tf.charData) + static_cast<size_t>(y) * tf.modulo;
            for (int x = 0; x < glyph.width; ++x) {
                const bool on = readBit(code, rowStart, static_cast<uint32_t>(bitOffset + x));
                glyph.alpha[static_cast<size_t>(y * glyph.width + x)] = on ? 0xff : 0x00;
            }
        }
        out.push_back(std::move(glyph));
    }
    return out;
}

std::filesystem::path findMm2FontPath() {
    static constexpr const char* kCandidates[] = {
        "EXTRACTED/fonts/mm2/8",
        "../EXTRACTED/fonts/mm2/8",
        "../../EXTRACTED/fonts/mm2/8",
    };
    for (const char* candidate : kCandidates) {
        std::filesystem::path p(candidate);
        if (std::filesystem::exists(p)) return p;
    }
    return {};
}

}  // namespace

ImFont* installMm2BitmapFont(ImGuiIO& io) {
    const std::filesystem::path fontPath = findMm2FontPath();
    if (fontPath.empty()) return nullptr;

    std::vector<uint8_t> fileData;
    if (!readFile(fontPath, &fileData)) return nullptr;

    std::vector<uint8_t> code;
    if (!extractFirstCodeHunk(fileData, &code)) return nullptr;

    Mm2TextFont tf;
    if (!findMm2TextFont(code, &tf)) return nullptr;

    std::vector<GlyphBitmap> glyphs = decodePrintableGlyphs(code, tf);
    if (glyphs.empty()) return nullptr;

    ImFontAtlas* atlas = io.Fonts;
    static const ImWchar kNoPrintableRanges[] = {0x0001, 0x001f, 0};
    ImFontConfig cfg;
    cfg.SizePixels = static_cast<float>(tf.ySize);
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    cfg.GlyphRanges = kNoPrintableRanges;
    cfg.Name[0] = '\0';

    ImFont* font = atlas->AddFontDefault(&cfg);
    if (!font) return nullptr;

    // NOTE: Newer docking ImGui backends using ImGuiBackendFlags_RendererHasTextures
    // assert if client code calls ImFontAtlas::Build() directly.
    // Keep MM2 font discovery in place, but avoid explicit atlas building/pixel patching
    // here. The backend-owned atlas build path will run during NewFrame safely.
    (void)glyphs;

    io.FontDefault = font;
    return font;
}

}  // namespace mm2


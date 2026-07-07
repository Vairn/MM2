#include "core/PcGfx.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <tuple>

namespace mm2 {

namespace fs = std::filesystem;

namespace {

constexpr int kLzwMask[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1FF, 0x3FF, 0x7FF, 0xFFF};

// IBM CGA 320x200x4 BIOS palettes. CGA.DRV drv_init (asm 0x2A8) issues INT 10h
// AH=0Bh BX=0x0101 -> palette 1 (black/cyan/magenta/white).
constexpr uint8_t kCgaPalette0[4][3] = {{0, 0, 0}, {0, 170, 0}, {170, 0, 0}, {170, 85, 0}};
constexpr uint8_t kCgaPalette1[4][3] = {{0, 0, 0}, {85, 255, 255}, {255, 85, 255}, {255, 255, 255}};
constexpr uint8_t kEgaRgb[16][3] = {
    {0, 0, 0},     {0, 0, 170},   {0, 170, 0},     {0, 170, 170},
    {170, 0, 0},   {170, 0, 170}, {170, 85, 0},    {170, 170, 170},
    {85, 85, 85},  {85, 85, 255}, {85, 255, 85},   {85, 255, 255},
    {255, 85, 85}, {255, 85, 255}, {255, 255, 85}, {255, 255, 255},
};

// EGA.DRV combat-sprite low-nibble -> colour translation (xlat @0x121B).
// Low nibble 5 is the transparent marker, handled before this lookup.
constexpr int kEgaMonsterXlat[16] = {0, 1, 2, 9, 6, 8, 10, 3, 4, 5, 7, 11, 12, 13, 14, 15};

}  // namespace

void pcCgaPaletteColor(int idx, int cgaPalette, uint8_t out[3]) {
    const auto& pal = (cgaPalette == 0) ? kCgaPalette0 : kCgaPalette1;
    idx &= 3;
    out[0] = pal[idx][0];
    out[1] = pal[idx][1];
    out[2] = pal[idx][2];
}

void pcEgaPaletteColor(int idx, uint8_t out[3]) {
    idx &= 0xF;
    out[0] = kEgaRgb[idx][0];
    out[1] = kEgaRgb[idx][1];
    out[2] = kEgaRgb[idx][2];
}

namespace {
void paletteColor(int idx, int bpp, int cgaPalette, uint8_t out[3]) {
    if (bpp == 2) pcCgaPaletteColor(idx, cgaPalette, out);
    else pcEgaPaletteColor(idx, out);
}
}  // namespace

// --- LZW ---------------------------------------------------------------------

Bytes pcLzwDecompress(const Bytes& source, size_t destSize) {
    std::vector<int> dictRoot(4096, 0);
    std::vector<int> dictCode(4096, 0);
    size_t bitsRead = 0;
    Bytes out;
    out.reserve(destSize);

    auto readCode = [&](int width) -> int {
        size_t base = bitsRead / 8;
        if (base >= source.size()) return -1;
        uint32_t b0 = source[base];
        uint32_t b1 = (base + 1 < source.size()) ? source[base + 1] : 0;
        uint32_t b2 = (base + 2 < source.size()) ? source[base + 2] : 0;
        uint32_t word = b0 | (b1 << 8) | (b2 << 16);
        word >>= (bitsRead % 8);
        int code = static_cast<int>(word & static_cast<uint32_t>(kLzwMask[width]));
        bitsRead += static_cast<size_t>(width);
        return code;
    };

    // Values are always byte-range (0..255): the walk stops once code<=0xFF.
    auto expand = [&](int code) -> Bytes {
        Bytes stack;
        while (code > 0xFF) {
            stack.push_back(static_cast<uint8_t>(dictRoot[code]));
            code = dictCode[code];
        }
        stack.push_back(static_cast<uint8_t>(code));
        std::reverse(stack.begin(), stack.end());
        return stack;
    };

    int codeWidth = 9;
    int dictLimit = 0x200;
    int dictNext = 0x102;
    int prev = -1;
    bool havePrev = false;

    while (out.size() < destSize) {
        int code = readCode(codeWidth);
        if (code < 0) break;
        if (code == 0x100) {
            codeWidth = 9;
            dictLimit = 0x200;
            dictNext = 0x102;
            code = readCode(codeWidth);
            if (code < 0) break;
            out.push_back(static_cast<uint8_t>(code & 0xFF));
            prev = code;
            havePrev = true;
            continue;
        }
        if (code == 0x101) break;
        if (!havePrev) break;

        Bytes chars;
        int first;
        // ASM 0x2C38-0x2C40: dictionary link-walk ends at the ROOT char, i.e.
        // the *first* character of the string (expand()[0]).
        if (code < dictNext) {
            chars = expand(code);
            first = chars[0];
        } else {
            chars = expand(prev);
            first = chars[0];
            chars.push_back(static_cast<uint8_t>(first));
        }
        out.insert(out.end(), chars.begin(), chars.end());

        if (dictNext < 4096) {
            dictRoot[dictNext] = first;
            dictCode[dictNext] = prev;
            ++dictNext;
            if (dictNext >= dictLimit && codeWidth < 12) {
                ++codeWidth;
                dictLimit <<= 1;
            }
        }
        prev = code;
        havePrev = true;
    }

    if (out.size() > destSize) out.resize(destSize);
    return out;
}

// --- Wall / sprite sheets ----------------------------------------------------

namespace {

bool validFrameDims(int width, int height, int bpp) {
    if (width < 4 || height < 1) return false;
    if (bpp == 2) return width <= 320 && height <= 200;
    return width <= 640 && height <= 200;
}

size_t frameEnd(const std::vector<size_t>& offsets, int index, size_t decLen) {
    size_t start = offsets[static_cast<size_t>(index)];
    for (size_t i = static_cast<size_t>(index) + 1; i < offsets.size(); ++i) {
        if (offsets[i] > start) return offsets[i];
    }
    return decLen;
}

size_t computedFrameEnd(const Bytes& dec, size_t off, int bpp, size_t endLimit) {
    if (off + 4 > endLimit || off + 4 > dec.size()) return 0;
    int width = readU16LE(dec.data() + off);
    int height = readU16LE(dec.data() + off + 2);
    if (!validFrameDims(width, height, bpp)) return 0;
    int rb = pcRowBytes(width, bpp);
    size_t pixEnd = off + 4 + static_cast<size_t>(rb) * static_cast<size_t>(height);
    if (pixEnd > endLimit || pixEnd > dec.size()) return 0;
    return pixEnd;
}

bool isGroupedU16Table(const Bytes& dec, const std::vector<size_t>& offsets, int bpp) {
    if (offsets.size() < 4) return false;
    int pairs = 0;
    int matches = 0;
    for (size_t i = 0; i + 1 < offsets.size(); i += 2) {
        size_t start = offsets[i];
        if (start < 2 || start + 4 > dec.size()) continue;
        int width = readU16LE(dec.data() + start);
        int height = readU16LE(dec.data() + start + 2);
        if (!validFrameDims(width, height, bpp)) continue;
        size_t endLimit = frameEnd(offsets, static_cast<int>(i), dec.size());
        size_t expected = computedFrameEnd(dec, start, bpp, endLimit);
        if (expected <= start) continue;
        ++pairs;
        if (offsets[i + 1] == expected) ++matches;
    }
    return pairs >= 2 && matches >= std::max(2, pairs * 2 / 3);
}

std::vector<PcWallFrame> extractGroupedU16Frames(const Bytes& dec, const std::vector<size_t>& offsets, int bpp) {
    std::vector<PcWallFrame> frames;
    int logical = 0;
    for (size_t i = 0; i < offsets.size(); i += 2) {
        size_t off = offsets[i];
        if (off < 2 || off >= dec.size()) continue;
        size_t end = (i + 1 < offsets.size()) ? offsets[i + 1] : dec.size();
        if (end <= off) end = dec.size();
        if (off + 4 > end) continue;
        int width = readU16LE(dec.data() + off);
        int height = readU16LE(dec.data() + off + 2);
        if (!validFrameDims(width, height, bpp)) continue;
        int rb = pcRowBytes(width, bpp);
        size_t pixLen = static_cast<size_t>(rb) * static_cast<size_t>(height);
        size_t pixOff = off + 4;
        if (pixOff + pixLen > end) continue;

        PcWallFrame fr;
        fr.index = logical;
        fr.tableSlot = static_cast<int>(i);
        fr.offset = off;
        fr.width = width;
        fr.height = height;
        fr.pixels.assign(dec.begin() + static_cast<std::ptrdiff_t>(pixOff),
                         dec.begin() + static_cast<std::ptrdiff_t>(pixOff + pixLen));
        frames.push_back(std::move(fr));
        ++logical;
    }
    return frames;
}

std::vector<PcWallFrame> extractWallFrames(const Bytes& dec, const std::vector<size_t>& offsets, int bpp) {
    if (isGroupedU16Table(dec, offsets, bpp)) return extractGroupedU16Frames(dec, offsets, bpp);

    std::vector<PcWallFrame> frames;
    int logical = 0;
    for (int i = 0; i < static_cast<int>(offsets.size()); ++i) {
        size_t off = offsets[static_cast<size_t>(i)];
        if (off < 2 || off >= dec.size()) continue;
        size_t end = frameEnd(offsets, i, dec.size());
        if (off + 4 > end) continue;
        int width = readU16LE(dec.data() + off);
        int height = readU16LE(dec.data() + off + 2);
        if (!validFrameDims(width, height, bpp)) continue;
        int rb = pcRowBytes(width, bpp);
        size_t pixLen = static_cast<size_t>(rb) * static_cast<size_t>(height);
        size_t pixOff = off + 4;
        if (pixOff + pixLen > end) continue;

        PcWallFrame fr;
        fr.index = logical;
        fr.tableSlot = i;
        fr.offset = off;
        fr.width = width;
        fr.height = height;
        fr.pixels.assign(dec.begin() + static_cast<std::ptrdiff_t>(pixOff),
                         dec.begin() + static_cast<std::ptrdiff_t>(pixOff + pixLen));
        frames.push_back(std::move(fr));
        ++logical;
    }
    return frames;
}

std::vector<PcWallFrame> extractPackedU32Frames(const Bytes& dec, int frameCount, int bpp) {
    std::vector<PcWallFrame> frames;
    for (int i = 0; i < frameCount; ++i) {
        size_t pos = 2 + static_cast<size_t>(i) * 4;
        if (pos + 4 > dec.size()) break;
        uint32_t u32 = readU32LE(dec.data() + pos);
        size_t start = u32 & 0xFFFFu;
        size_t end = u32 >> 16;
        if (start < 2 || start >= dec.size()) continue;
        if (end <= start) end = dec.size();
        if (start + 4 > end) continue;
        int width = readU16LE(dec.data() + start);
        int height = readU16LE(dec.data() + start + 2);
        if (!validFrameDims(width, height, bpp)) continue;
        int rb = pcRowBytes(width, bpp);
        size_t pixLen = static_cast<size_t>(rb) * static_cast<size_t>(height);
        size_t pixOff = start + 4;
        if (pixOff + pixLen > end) continue;

        PcWallFrame fr;
        fr.index = static_cast<int>(frames.size());
        fr.tableSlot = i;
        fr.offset = start;
        fr.width = width;
        fr.height = height;
        fr.pixels.assign(dec.begin() + static_cast<std::ptrdiff_t>(pixOff),
                         dec.begin() + static_cast<std::ptrdiff_t>(pixOff + pixLen));
        frames.push_back(std::move(fr));
    }
    return frames;
}

std::pair<int, int> packedU32EndMatches(const Bytes& dec, int frameCount, int bpp) {
    int matches = 0;
    int valid = 0;
    for (int i = 0; i < frameCount; ++i) {
        size_t pos = 2 + static_cast<size_t>(i) * 4;
        if (pos + 4 > dec.size()) break;
        uint32_t u32 = readU32LE(dec.data() + pos);
        size_t start = u32 & 0xFFFFu;
        size_t end = u32 >> 16;
        if (start < 2 || start + 4 > dec.size()) continue;
        int width = readU16LE(dec.data() + start);
        int height = readU16LE(dec.data() + start + 2);
        if (!validFrameDims(width, height, bpp)) continue;
        int rb = pcRowBytes(width, bpp);
        size_t expected = start + 4 + static_cast<size_t>(rb) * static_cast<size_t>(height);
        if (expected > dec.size()) continue;
        ++valid;
        if (end == expected || (end == 0 && expected <= dec.size())) ++matches;
    }
    return {matches, valid};
}

std::vector<size_t> offsetTableU32(const Bytes& dec, int frameCount) {
    std::vector<size_t> out;
    size_t need = 2 + static_cast<size_t>(frameCount) * 4;
    if (need > dec.size()) return out;
    out.reserve(static_cast<size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i) out.push_back(readU32LE(dec.data() + 2 + i * 4));
    return out;
}

std::vector<size_t> offsetTableU16(const Bytes& dec, int frameCount) {
    std::vector<size_t> out;
    size_t need = 2 + static_cast<size_t>(frameCount) * 2;
    if (need > dec.size()) return out;
    out.reserve(static_cast<size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i) out.push_back(readU16LE(dec.data() + 2 + i * 2));
    return out;
}

// Auto-score packed u32 vs plain u32 vs u16. Prefer more decodable frames.
void pickOffsetTable(const Bytes& dec, int frameCount, int bpp, bool& outIsU32, bool& outPackedU32,
                     std::vector<size_t>& outOffsets) {
    outIsU32 = false;
    outPackedU32 = false;
    outOffsets.clear();
    if (frameCount == 0) return;
    std::vector<size_t> u16 = offsetTableU16(dec, frameCount);
    std::vector<size_t> u32 = offsetTableU32(dec, frameCount);

    struct Candidate {
        int score;
        int bonus;
        bool isU32;
        bool packed;
        std::vector<size_t> table;
    };
    std::vector<Candidate> scored;
    if (!u32.empty()) {
        int plain = static_cast<int>(extractWallFrames(dec, u32, bpp).size());
        scored.push_back({plain, plain == frameCount ? 1 : 0, true, false, u32});
        auto packedFrames = extractPackedU32Frames(dec, frameCount, bpp);
        if (!packedFrames.empty()) {
            auto [endM, endV] = packedU32EndMatches(dec, frameCount, bpp);
            int endBonus = (endV > 0 && endM >= std::max(2, endV * 2 / 3)) ? 1 : 0;
            scored.push_back({static_cast<int>(packedFrames.size()), endBonus, true, true, u32});
        }
    }
    if (!u16.empty()) {
        int n16 = static_cast<int>(extractWallFrames(dec, u16, bpp).size());
        scored.push_back({n16, 0, false, false, u16});
    }
    if (scored.empty()) {
        outOffsets = u16;
        return;
    }
    std::sort(scored.begin(), scored.end(), [](const Candidate& a, const Candidate& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.bonus != b.bonus) return a.bonus > b.bonus;
        return a.isU32 && !a.packed;
    });
    const Candidate& best = scored.front();
    outIsU32 = best.isU32;
    outPackedU32 = best.packed;
    outOffsets = best.table;
}

}  // namespace

PcWallSheet pcParseWallSheet(const Bytes& raw, const std::string& ext) {
    PcWallSheet sheet;
    sheet.bpp = pcBppForExt(ext);
    if (raw.size() < 8) {
        sheet.error = "too small for wall sheet header";
        return sheet;
    }
    sheet.uncompressedSize = readU32LE(raw.data());
    if (sheet.uncompressedSize < 4) {
        sheet.error = "invalid uncompressed_size";
        return sheet;
    }
    sheet.compressedBytes = raw.size() - 4;
    Bytes src(raw.begin() + 4, raw.end());
    sheet.decompressed = pcLzwDecompress(src, sheet.uncompressedSize);
    if (sheet.decompressed.size() < sheet.uncompressedSize) {
        sheet.error = "LZW decompress truncated";
        return sheet;
    }
    sheet.tableSlotCount = sheet.decompressed[0] & 0x3F;
    if (sheet.tableSlotCount == 0) {
        sheet.error = "invalid frame_count";
        return sheet;
    }
    std::vector<size_t> offsets;
    pickOffsetTable(sheet.decompressed, sheet.tableSlotCount, sheet.bpp, sheet.offsetsAreU32, sheet.packedU32,
                   offsets);
    sheet.groupedU16 = !sheet.offsetsAreU32 && !sheet.packedU32 &&
                       isGroupedU16Table(sheet.decompressed, offsets, sheet.bpp);
    if (sheet.packedU32) {
        sheet.frames = extractPackedU32Frames(sheet.decompressed, sheet.tableSlotCount, sheet.bpp);
    } else {
        sheet.frames = extractWallFrames(sheet.decompressed, offsets, sheet.bpp);
    }
    if (sheet.frames.empty()) {
        sheet.error = "no decodable frames";
        return sheet;
    }
    sheet.frameCount = static_cast<int>(sheet.frames.size());
    sheet.ok = true;
    return sheet;
}

bool pcLoadWallSheet(const std::string& path, PcWallSheet& out) {
    Bytes raw;
    if (!readFile(path, raw)) {
        out.clear();
        out.error = "read failed";
        return false;
    }
    std::string ext = fs::path(path).extension().string();
    out = pcParseWallSheet(raw, ext);
    out.path = path;
    return out.ok;
}

void pcDecodeWallFrameRGBA(const PcWallFrame& frame, int bpp, int cgaPalette, std::vector<uint8_t>& rgba) {
    const int w = frame.width;
    const int h = frame.height;
    rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    const int rb = pcRowBytes(w, bpp);
    uint8_t color[3];
    for (int y = 0; y < h; ++y) {
        size_t rowOff = static_cast<size_t>(y) * rb;
        int x = 0;
        if (bpp == 2) {
            for (int bi = 0; bi < rb && x < w; ++bi) {
                uint8_t b = frame.pixels[rowOff + static_cast<size_t>(bi)];
                for (int shift : {6, 4, 2, 0}) {
                    if (x >= w) break;
                    int idx = (b >> shift) & 3;
                    pcCgaPaletteColor(idx, cgaPalette, color);
                    uint8_t* o = &rgba[(static_cast<size_t>(y) * w + x) * 4];
                    o[0] = color[0];
                    o[1] = color[1];
                    o[2] = color[2];
                    o[3] = 255;
                    ++x;
                }
            }
        } else {
            for (int bi = 0; bi < rb && x < w; ++bi) {
                uint8_t b = frame.pixels[rowOff + static_cast<size_t>(bi)];
                for (int v : {(b >> 4) & 0xF, b & 0xF}) {
                    if (x >= w) break;
                    pcEgaPaletteColor(v, color);
                    uint8_t* o = &rgba[(static_cast<size_t>(y) * w + x) * 4];
                    o[0] = color[0];
                    o[1] = color[1];
                    o[2] = color[2];
                    o[3] = 255;
                    ++x;
                }
            }
        }
    }
}

// --- Monster combat atlas ----------------------------------------------------

namespace {

std::optional<Bytes> decompressMonsterBlob(const Bytes& raw, size_t off) {
    if (off + 8 > raw.size()) return std::nullopt;
    uint32_t decSize = readU32LE(raw.data() + off);
    if (decSize < 4 || decSize > 512 * 1024) return std::nullopt;
    Bytes src(raw.begin() + static_cast<std::ptrdiff_t>(off) + 4, raw.end());
    Bytes dec = pcLzwDecompress(src, decSize);
    if (dec.size() < decSize) return std::nullopt;
    return dec;
}

size_t monsterTableEnd(const Bytes& raw) {
    if (raw.size() < 4) return 0;
    return readU32LE(raw.data());
}

size_t monsterSeekFromHeader(const Bytes& raw, int entry, size_t headerBytes) {
    if (entry < 0 || static_cast<size_t>(entry) * 4 + 4 > headerBytes || headerBytes > raw.size()) return 0;
    return readU32LE(raw.data() + entry * 4);
}

bool monsterBlobValid(const Bytes& raw, size_t fileOff, size_t tableEnd) {
    if (fileOff < tableEnd) return false;
    auto dec = decompressMonsterBlob(raw, fileOff);
    return dec.has_value() && ((*dec)[0] & 0x3F) > 0;
}

std::optional<std::tuple<int, int, int, int>> parseMonsterFrameHeader(const Bytes& dec, size_t frameOff,
                                                                        size_t frameEndOff) {
    // Returns (x, y, width, height); caller slices the stream separately.
    if (frameOff + 4 > dec.size() || frameEndOff > dec.size() || frameEndOff <= frameOff + 4) return std::nullopt;
    int x = dec[frameOff];
    int y = dec[frameOff + 1];
    int width = dec[frameOff + 2];
    int height = dec[frameOff + 3];
    if (width < 1 || height < 1 || width > 128 || height > 128) return std::nullopt;
    return std::make_tuple(x, y, width, height);
}

std::vector<PcMonsterFrame> parseMonsterPictureFrames(const Bytes& dec) {
    std::vector<PcMonsterFrame> frames;
    if (dec.empty()) return frames;
    int frameCount = dec[0] & 0x3F;
    size_t tableNeed = 2 + static_cast<size_t>(frameCount) * 2;
    if (frameCount == 0 || tableNeed > dec.size()) return frames;

    std::vector<size_t> inner(static_cast<size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i) inner[static_cast<size_t>(i)] = readU16LE(dec.data() + 2 + i * 2);

    std::vector<size_t> ordered;
    for (size_t o : inner) {
        if (o > 0 && o <= dec.size()) ordered.push_back(o);
    }
    std::sort(ordered.begin(), ordered.end());

    for (int fi = 0; fi < frameCount; ++fi) {
        size_t innerOff = inner[static_cast<size_t>(fi)];
        if (innerOff == 0 || innerOff >= dec.size()) continue;
        size_t frameEndOff = dec.size();
        for (size_t o : ordered) {
            if (o > innerOff) {
                frameEndOff = o;
                break;
            }
        }
        auto hdr = parseMonsterFrameHeader(dec, innerOff, frameEndOff);
        if (!hdr) continue;
        auto [x, y, width, height] = *hdr;
        PcMonsterFrame fr;
        fr.frameIndex = fi;
        fr.x = x;
        fr.y = y;
        fr.width = width;
        fr.height = height;
        fr.stream.assign(dec.begin() + static_cast<std::ptrdiff_t>(innerOff) + 4,
                         dec.begin() + static_cast<std::ptrdiff_t>(frameEndOff));
        frames.push_back(std::move(fr));
    }
    return frames;
}

std::vector<std::vector<std::pair<int, int>>> parseMonsterScripts(const Bytes& dec, int frameCount) {
    std::vector<std::vector<std::pair<int, int>>> scripts;
    size_t tableEnd = 2 + static_cast<size_t>(frameCount) * 2;
    if (tableEnd > dec.size()) return scripts;
    std::vector<size_t> inner(static_cast<size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i) inner[static_cast<size_t>(i)] = readU16LE(dec.data() + 2 + i * 2);

    size_t first = tableEnd;
    bool haveFirst = false;
    for (size_t o : inner) {
        if (o > 0 && (!haveFirst || o < first)) {
            first = o;
            haveFirst = true;
        }
    }
    if (!haveFirst) first = tableEnd;
    if (first > dec.size()) first = dec.size();
    if (tableEnd > first) return scripts;

    std::vector<std::pair<int, int>> current;
    size_t i = tableEnd;
    while (i < first) {
        uint8_t b = dec[i];
        if (b == 0xFF) {
            if (i + 1 < first && dec[i + 1] == 0xFF) {
                if (!current.empty()) scripts.push_back(current);
                break;
            }
            if (!current.empty()) {
                scripts.push_back(current);
                current.clear();
            }
            ++i;
            continue;
        }
        if (i + 1 < first) {
            current.emplace_back(dec[i], dec[i + 1]);
            i += 2;
        } else {
            ++i;
        }
    }
    if (!current.empty()) scripts.push_back(current);
    return scripts;
}

}  // namespace

bool pcLoadMonstersAtlas(const std::string& path, PcMonstersAtlas& out) {
    out.clear();
    Bytes raw;
    if (!readFile(path, raw)) {
        out.error = "read failed";
        return false;
    }
    if (raw.size() < 4) {
        out.error = "too small";
        return false;
    }
    out.path = path;
    out.bpp = pcBppForExt(fs::path(path).extension().string());
    out.raw = std::move(raw);
    out.headerBytes = monsterTableEnd(out.raw);
    if (out.headerBytes < 4 || out.headerBytes > out.raw.size()) {
        out.error = "invalid header table size";
        out.headerBytes = 0;
        return false;
    }
    out.ok = true;
    return true;
}

size_t pcMonsterBlobOffsetForId(const PcMonstersAtlas& atlas, int pictureId) {
    if (!atlas.ok) return 0;
    size_t fo = monsterSeekFromHeader(atlas.raw, pictureId - 1, atlas.headerBytes);
    if (fo != 0 && monsterBlobValid(atlas.raw, fo, atlas.headerBytes)) return fo;
    return 0;
}

std::vector<int> pcMonsterAvailablePictureIds(const PcMonstersAtlas& atlas) {
    std::vector<int> ids;
    if (!atlas.ok) return ids;
    int entryCount = static_cast<int>(atlas.headerBytes / 4);
    for (int id = 1; id <= entryCount; ++id) {
        if (pcMonsterBlobOffsetForId(atlas, id) != 0) ids.push_back(id);
    }
    return ids;
}

std::optional<PcMonsterPicture> pcMonsterPictureForId(const PcMonstersAtlas& atlas, int pictureId) {
    size_t fo = pcMonsterBlobOffsetForId(atlas, pictureId);
    if (fo == 0) return std::nullopt;
    auto dec = decompressMonsterBlob(atlas.raw, fo);
    if (!dec) return std::nullopt;
    int frameCount = (*dec)[0] & 0x3F;
    if (frameCount == 0) return std::nullopt;

    PcMonsterPicture pic;
    pic.pictureId = pictureId;
    pic.blobOffset = fo;
    pic.flags = (*dec)[1];
    pic.bpp = atlas.bpp;
    pic.frames = parseMonsterPictureFrames(*dec);
    pic.scripts = parseMonsterScripts(*dec, frameCount);
    pic.ok = !pic.frames.empty();
    return pic.ok ? std::optional<PcMonsterPicture>(std::move(pic)) : std::nullopt;
}

std::vector<int> pcDecodeMonsterSpriteIndices(int width, int height, const Bytes& stream, int bpp) {
    std::vector<int> grid(static_cast<size_t>(width) * height, -1);
    int row = -1;
    int remaining = 0;
    int col = 0;
    bool done = false;
    size_t i = 0;
    while (i < stream.size() && !done) {
        uint8_t b = stream[i++];
        int count = (b >> 4) & 0xF;
        int low = b & 0xF;
        bool transparent;
        int color;
        if (bpp == 2) {
            transparent = low >= 4;
            color = low & 3;
        } else {
            transparent = low == 5;
            color = kEgaMonsterXlat[low];
        }
        for (int r = 0; r <= count; ++r) {
            if (remaining == 0) {
                ++row;
                if (row >= height) {
                    done = true;
                    break;
                }
                col = 0;
                remaining = width;
            }
            if (!transparent && col < width) grid[static_cast<size_t>(row) * width + col] = color;
            ++col;
            --remaining;
        }
    }
    return grid;
}

void pcDecodeMonsterFrameRGBA(const PcMonsterFrame& frame, int bpp, int cgaPalette, std::vector<uint8_t>& rgba) {
    std::vector<int> grid = pcDecodeMonsterSpriteIndices(frame.width, frame.height, frame.stream, bpp);
    rgba.assign(static_cast<size_t>(frame.width) * frame.height * 4, 0);
    uint8_t color[3];
    for (int r = 0; r < frame.height; ++r) {
        for (int c = 0; c < frame.width; ++c) {
            int v = grid[static_cast<size_t>(r) * frame.width + c];
            uint8_t* o = &rgba[(static_cast<size_t>(r) * frame.width + c) * 4];
            if (v < 0) {
                o[0] = o[1] = o[2] = o[3] = 0;
                continue;
            }
            paletteColor(v, bpp, cgaPalette, color);
            o[0] = color[0];
            o[1] = color[1];
            o[2] = color[2];
            o[3] = 255;
        }
    }
}

namespace {

void clearMonsterRect(std::vector<int>& canvas, int x, int y, int w, int h) {
    for (int r = 0; r < h; ++r) {
        int yy = y + r;
        if (yy < 0 || yy >= kPcCombatCanvasH) continue;
        for (int c = 0; c < w; ++c) {
            int xx = x + c;
            if (xx < 0 || xx >= kPcCombatCanvasW) continue;
            canvas[static_cast<size_t>(yy) * kPcCombatCanvasW + xx] = -1;
        }
    }
}

void blitMonsterGrid(std::vector<int>& canvas, const std::vector<int>& grid, int w, int h, int ox, int oy) {
    for (int r = 0; r < h; ++r) {
        int yy = oy + r;
        if (yy < 0 || yy >= kPcCombatCanvasH) continue;
        for (int c = 0; c < w; ++c) {
            int v = grid[static_cast<size_t>(r) * w + c];
            if (v < 0) continue;
            int xx = ox + c;
            if (xx < 0 || xx >= kPcCombatCanvasW) continue;
            canvas[static_cast<size_t>(yy) * kPcCombatCanvasW + xx] = v;
        }
    }
}

void canvasToRgba(const std::vector<int>& canvas, int bpp, int cgaPalette, std::vector<uint8_t>& rgba) {
    rgba.assign(canvas.size() * 4, 0);
    uint8_t color[3];
    for (size_t i = 0; i < canvas.size(); ++i) {
        int v = canvas[i];
        uint8_t* o = &rgba[i * 4];
        if (v < 0) {
            o[0] = o[1] = o[2] = o[3] = 0;
            continue;
        }
        paletteColor(v, bpp, cgaPalette, color);
        o[0] = color[0];
        o[1] = color[1];
        o[2] = color[2];
        o[3] = 255;
    }
}

}  // namespace

void pcCompositeMonsterFrame(const PcMonsterPicture& pic, int frameIdx, int cgaPalette, std::vector<uint8_t>& rgba) {
    std::vector<int> canvas(static_cast<size_t>(kPcCombatCanvasW) * kPcCombatCanvasH, -1);
    if (pic.frames.empty()) {
        canvasToRgba(canvas, pic.bpp, cgaPalette, rgba);
        return;
    }
    std::map<int, const PcMonsterFrame*> byIdx;
    for (const auto& fr : pic.frames) byIdx[fr.frameIndex] = &fr;

    std::vector<int> layers = {0};
    if (frameIdx != 0 && byIdx.count(frameIdx)) layers.push_back(frameIdx);

    for (int idx : layers) {
        auto it = byIdx.find(idx);
        if (it == byIdx.end()) continue;
        const PcMonsterFrame& fr = *it->second;
        if (idx != 0) clearMonsterRect(canvas, fr.x, fr.y, fr.width, fr.height);
        std::vector<int> grid = pcDecodeMonsterSpriteIndices(fr.width, fr.height, fr.stream, pic.bpp);
        blitMonsterGrid(canvas, grid, fr.width, fr.height, fr.x, fr.y);
    }
    canvasToRgba(canvas, pic.bpp, cgaPalette, rgba);
}

bool pcMonsterHasSequencePlayback(const PcMonsterPicture& pic) {
    for (const auto& seq : pic.scripts) {
        if (seq.size() >= 1) return true;
    }
    return false;
}

int pcMonsterSequenceFrameAt(const PcMonsterPicture& pic, int seqIndex, int step) {
    if (seqIndex < 0 || seqIndex >= static_cast<int>(pic.scripts.size())) return -1;
    const auto& seq = pic.scripts[static_cast<size_t>(seqIndex)];
    if (seq.empty()) return -1;
    if (step < 0) step = 0;
    if (step >= static_cast<int>(seq.size())) step = static_cast<int>(seq.size()) - 1;
    return seq[static_cast<size_t>(step)].first;
}

float pcMonsterSequenceStepDurationSec(const PcMonsterPicture& pic, int seqIndex, int step, float speed) {
    if (seqIndex < 0 || seqIndex >= static_cast<int>(pic.scripts.size()) || speed <= 0.0f) return 0.10f;
    const auto& seq = pic.scripts[static_cast<size_t>(seqIndex)];
    if (seq.empty()) return 0.10f;
    if (step < 0) step = 0;
    if (step >= static_cast<int>(seq.size())) step = static_cast<int>(seq.size()) - 1;
    int delayTicks = seq[static_cast<size_t>(step)].second;
    float base = static_cast<float>(delayTicks > 0 ? delayTicks : 1) / 60.0f;
    return base / speed;
}

// --- Asset directory discovery -----------------------------------------------

std::string pcFindAssetsDir(const std::string& baseDir) {
    std::error_code ec;
    if (baseDir.empty() || !fs::is_directory(baseDir, ec)) return "";

    auto hasExt4 = [&](const std::string& dir) -> bool {
        std::error_code lec;
        if (!fs::is_directory(dir, lec)) return false;
        for (auto& e : fs::directory_iterator(dir, lec)) {
            if (!e.is_regular_file()) continue;
            std::string ext = e.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".4" || ext == ".16") return true;
        }
        return false;
    };

    for (const char* sub : {"", "PC", "pc", "DOS", "dos"}) {
        std::string cand = (sub[0] == '\0') ? baseDir : (baseDir + "/" + sub);
        if (hasExt4(cand)) return cand;
    }
    return "";
}

}  // namespace mm2

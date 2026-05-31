#pragma once
// Decoder for the MM2 planar image-chunk format shared by:
//   - .32 tileset/sprite files (image chunk begins at offset 0)
//   - .anm "TV" animations    (image chunk located after the prelude/sequence
//                               via an FF 00 marker)
//
// Image chunk layout (big-endian words), see EXTRACTED/docs/07-anm-tv-format.md
// and tools/decode_anm.py:
//   u16 frame_count
//   u16 depth_or_mode      (observed 3; plane count is fixed at 5)
//   frame_count * { u16 width; u16 height; u16 flags }
//   32 * u16 palette        (Amiga 0x0RGB, 4 bits per channel)
//   nibble-RLE plane stream  (5 concatenated bitplanes per frame)
//
// Pixel codec: command byte; if hi nibble is 0x0 or 0xF emit that nibble
// (low_nibble+1) times, else emit the two literal nibbles. Nibbles pack MSB
// first into bytes. Each frame decodes to 5 * rassize(w,h) bytes.

#include <cstdint>
#include <string>
#include <vector>

#include "core/ByteIO.h"

namespace mm2 {

constexpr int kGfxPlanes = 5;
constexpr int kGfxPaletteColors = 32;

inline int gfxRassize(int w, int h) {
    return h * ((((w) + 15) >> 3) & 0xFFFE);
}

struct GfxFrame {
    int width = 0;
    int height = 0;
    int flags = 0;
    std::vector<uint8_t> rgba;  // width*height*4, RGBA8
};

struct GfxImage {
    bool ok = false;
    std::string error;
    int frameCount = 0;
    int depth = 0;
    size_t chunkOffset = 0;
    uint8_t palette[kGfxPaletteColors][4] = {};  // RGBA
    std::vector<GfxFrame> frames;
};

// Decode an image chunk. When isAnm is true the FF 00 marker is located first.
GfxImage gfxDecode(const Bytes& bytes, bool isAnm);
bool gfxLoad(const std::string& path, bool isAnm, GfxImage& out);

}  // namespace mm2

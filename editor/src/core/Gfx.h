#pragma once
// Planar image-chunk decoder for .32 tilesets (chunk at offset 0) and .anm
// TV animations (chunk after prelude/sequence, located via FF 00).
// globe.32 / disk.32 share the .32 extension but are XOR blobs, not images
// (tryDecryptXor32 is a leftover heuristic).
// Layout (big-endian words):
//   u16 frame_count
//   u16 depth_or_mode      (observed 3; plane count is fixed at 5)
//   frame_count * { u16 width; u16 height; u16 flags }
//   32 * u16 palette        (Amiga 0x0RGB)
//   nibble-RLE plane stream (5 concatenated bitplanes per frame)
//
// Pixel codec: command byte; hi nibble 0x0 or 0xF repeats that nibble
// (low_nibble+1) times, else two literal nibbles. MSB first.

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

struct GfxAnimPreludeEntry {
    int xOffset = 0;
    int yOffset = 0;
    int width = 0;
    int height = 0;
    bool used = false;
};

struct GfxImage {
    bool ok = false;
    std::string error;
    int frameCount = 0;
    int depth = 0;
    size_t chunkOffset = 0;
    // .anm sequence header bytes at 0x30..0x32 (unused for .32).
    int seqHeaderA = 0;
    int seqHeaderB = 0;
    int seqHeaderC = 0;
    std::vector<GfxAnimPreludeEntry> preludeEntries;  // fixed 11 slots for .anm
    std::vector<std::vector<uint8_t>> sequences;      // raw bytes per sequence
    uint8_t palette[kGfxPaletteColors][4] = {};  // RGBA
    std::vector<GfxFrame> frames;

    void clear() {
        ok = false;
        error.clear();
        frameCount = 0;
        depth = 0;
        chunkOffset = 0;
        seqHeaderA = seqHeaderB = seqHeaderC = 0;
        preludeEntries.clear();
        sequences.clear();
        frames.clear();
        for (auto& c : palette) c[0] = c[1] = c[2] = c[3] = 0;
    }
};

// Decode an image chunk. When isAnm is true the FF 00 marker is located first.
// ``palette_override`` (optional) replaces RGB pens before rasterize; alpha is kept.
GfxImage gfxDecode(const Bytes& bytes, bool isAnm, const uint8_t (*palette_override)[4] = nullptr);
bool gfxLoad(const std::string& path, bool isAnm, GfxImage& out,
             const uint8_t (*palette_override)[4] = nullptr);

// .32 encoder (inverse of gfxDecode for chunk-at-offset-0 sheets).
struct GfxEncodeFrame {
    int width = 0;
    int height = 0;
    int flags = 0;
    std::vector<uint8_t> indices;  // width*height, values 0..31
};

// Encode header + frame table + 32-colour palette + nibble-RLE planes.
Bytes gfxEncode32(const std::vector<GfxEncodeFrame>& frames,
                  const uint8_t palette[kGfxPaletteColors][4], int depth);

// Convenience: re-index a decoded GfxImage (rgba frames + palette) and encode.
// Pixels with alpha 0 map to index 0; others map to the nearest palette colour.
Bytes gfxEncode32FromImage(const GfxImage& img);
bool gfxSave32(const std::string& path, const GfxImage& img);

// --- .anm TV-prelude composition ---
// frame 0 = full base sprite; frame N = patch at prelude[N-1] over frame 0
// (rect cleared first). Sequence indices refer to composed states.

struct GfxAnmCanvas {
    int minX = 0;
    int minY = 0;
    int width = 0;
    int height = 0;
    bool valid = false;
};

enum class AnmPlayMode {
    Flipbook,  // composed frames 0..N-1
    Sequence,  // game (frame,delay) stream
};

GfxAnmCanvas gfxAnmCompositeCanvas(const GfxImage& img);
// Writes width*height*4 RGBA into `rgba` (resized as needed). Returns false if unsupported.
bool gfxAnmCompositeFrame(const GfxImage& img, int frameIdx, std::vector<uint8_t>& rgba,
                          const GfxAnmCanvas* canvas = nullptr);

bool gfxAnmHasSequencePlayback(const GfxImage& img);
int gfxAnmSequenceFrameAt(const GfxImage& img, int seqIndex, int step);
float gfxAnmSequenceStepDurationSec(const GfxImage& img, int seqIndex, int step, float speed);

}  // namespace mm2

#pragma once
// Decoder for the MM2 PC DOS (GOG) CGA (.4) / EGA (.16) graphics formats.
// Ported from tools/decode_pc_gfx.py (the source of truth for this codec) —
// see EXTRACTED/docs/54-pc-dos-graphics-formats.md for the format spec.
//
// Two families of files share the same LZW container:
//   - Wall / sprite sheets (THROW.4, TOWN.16, SKY.4, ...): a flat table of
//     complete frames, no compositing.
//   - MONSTERS.4 / MONSTERS.16 combat atlas: one LZW blob per monster
//     "picture id" (1-based, same id as Amiga NN.anm), each blob holding a
//     base frame (0) plus delta patch frames composited onto a 96x96 canvas,
//     driven by (frame,delay) animation scripts.
//
// Not Amiga: see core/Gfx.h for the .32/.anm planar formats.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/ByteIO.h"

namespace mm2 {

// bpp for the two on-disk pixel formats: CGA .4 = 2bpp packed, EGA .16 = 4bpp
// linear (NOT EGA bitplanes despite the extension).
inline int pcBppForExt(const std::string& ext) {
    std::string lower = ext;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower == ".4" ? 2 : 4;
}

inline int pcRowBytes(int width, int bpp) {
    return bpp == 2 ? (width + 3) / 4 : (width + 1) / 2;
}

inline bool pcIsMonstersFile(const std::string& fileName) {
    std::string upper = fileName;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return upper.rfind("MONSTERS", 0) == 0;
}

// MM2.EXE's custom LZW decompressor (asm 0x2A42): 9->12 bit codes, early
// dictionary growth, clear code 0x100, stop code 0x101. Root character for
// new dictionary entries is expand(code)[0] (first char), not the last.
Bytes pcLzwDecompress(const Bytes& source, size_t destSize);

// --- Wall / sprite sheets (.4 / .16) ----------------------------------------

struct PcWallFrame {
    int index = 0;       // logical frame index (0..N-1)
    int tableSlot = -1;  // offset-table slot (-1 = same as index)
    size_t offset = 0;   // frame header offset within the decompressed blob
    int width = 0;
    int height = 0;
    Bytes pixels;  // row-major packed pixels (CGA 2bpp MSB-first / EGA 4bpp linear)
};

struct PcWallSheet {
    bool ok = false;
    std::string error;
    std::string path;
    int bpp = 2;  // 2 = CGA (.4), 4 = EGA (.16)
    uint32_t uncompressedSize = 0;
    int frameCount = 0;       // logical decodable frame count
    int tableSlotCount = 0;   // dec[0] & 0x3F (offset table length)
    bool groupedU16 = false;  // interleaved start/end u16 table (legacy view)
    bool packedU32 = false;   // u32 (end<<16)|start pairs (MASTER, CASTLE, …)
    bool offsetsAreU32 = false;
    size_t compressedBytes = 0;
    std::vector<PcWallFrame> frames;
    Bytes decompressed;

    void clear() {
        ok = false;
        error.clear();
        path.clear();
        bpp = 2;
        uncompressedSize = 0;
        frameCount = 0;
        tableSlotCount = 0;
        groupedU16 = false;
        packedU32 = false;
        offsetsAreU32 = false;
        compressedBytes = 0;
        frames.clear();
        decompressed.clear();
    }
};

// `ext` selects CGA (".4") vs EGA (".16") pixel unpacking.
PcWallSheet pcParseWallSheet(const Bytes& raw, const std::string& ext);
bool pcLoadWallSheet(const std::string& path, PcWallSheet& out);

// Decode one frame's packed pixels to opaque RGBA8 (width*height*4).
// cgaPalette: 0 = green/red/brown (BIOS palette 0), 1 = cyan/magenta/white
// (BIOS palette 1 — MM2's CGA.DRV default). Ignored for EGA.
void pcDecodeWallFrameRGBA(const PcWallFrame& frame, int bpp, int cgaPalette, std::vector<uint8_t>& rgba);

// --- Monster combat atlas (MONSTERS.4 / MONSTERS.16) ------------------------

// Off-screen combat sprite canvas size (CGA.DRV 0xA17 / EGA.DRV equivalent).
constexpr int kPcCombatCanvasW = 96;
constexpr int kPcCombatCanvasH = 96;

struct PcMonsterFrame {
    int frameIndex = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    Bytes stream;  // nibble-RLE token stream (frame 0 = base, N>0 = delta patch)
};

struct PcMonsterPicture {
    bool ok = false;
    int pictureId = 0;
    size_t blobOffset = 0;
    int flags = 0;
    int bpp = 2;
    std::vector<PcMonsterFrame> frames;
    // Animation scripts: sequences of (frame_index, delay_ticks) steps.
    std::vector<std::vector<std::pair<int, int>>> scripts;
};

struct PcMonstersAtlas {
    bool ok = false;
    std::string error;
    std::string path;
    int bpp = 2;
    Bytes raw;
    size_t headerBytes = 0;  // header table size in bytes == first blob's file offset

    void clear() {
        ok = false;
        error.clear();
        path.clear();
        bpp = 2;
        raw.clear();
        headerBytes = 0;
    }
};

bool pcLoadMonstersAtlas(const std::string& path, PcMonstersAtlas& out);

// monsters.dat picture id (1-based) -> header table entry (0-based) -> blob
// file offset. Returns 0 if this atlas has no blob for that id.
size_t pcMonsterBlobOffsetForId(const PcMonstersAtlas& atlas, int pictureId);

// List every picture id (1..headerBytes/4) that resolves to a valid blob.
std::vector<int> pcMonsterAvailablePictureIds(const PcMonstersAtlas& atlas);

// Decode the full picture (frames + animation scripts) for a picture id.
std::optional<PcMonsterPicture> pcMonsterPictureForId(const PcMonstersAtlas& atlas, int pictureId);

// Decode one frame's nibble-RLE stream (CGA.DRV 0xAF2 / EGA.DRV 0x1422) to a
// width*height index grid; -1 marks a transparent pixel. EGA indices are
// already translated via the driver's xlat table.
std::vector<int> pcDecodeMonsterSpriteIndices(int width, int height, const Bytes& stream, int bpp);

// Decode a single frame's stream directly to RGBA8 (width*height*4), no
// canvas compositing — matches Python render_monster_frame_rgba.
void pcDecodeMonsterFrameRGBA(const PcMonsterFrame& frame, int bpp, int cgaPalette, std::vector<uint8_t>& rgba);

// Composite combat states onto the 96x96 canvas: frame 0 (+ frame N with its
// rect cleared first for N>0), producing kPcCombatCanvasW*H*4 RGBA8.
void pcCompositeMonsterFrame(const PcMonsterPicture& pic, int frameIdx, int cgaPalette,
                              std::vector<uint8_t>& rgba);

bool pcMonsterHasSequencePlayback(const PcMonsterPicture& pic);
int pcMonsterSequenceFrameAt(const PcMonsterPicture& pic, int seqIndex, int step);
float pcMonsterSequenceStepDurationSec(const PcMonsterPicture& pic, int seqIndex, int step, float speed);

// --- Palettes ----------------------------------------------------------------

void pcCgaPaletteColor(int idx, int cgaPalette, uint8_t out[3]);
void pcEgaPaletteColor(int idx, uint8_t out[3]);

// --- Asset directory discovery ----------------------------------------------

// Looks for PC .4/.16 assets in `baseDir` itself or common sibling folder
// names ("PC", "pc", "DOS"). Returns the found directory, or "" if none.
std::string pcFindAssetsDir(const std::string& baseDir);

}  // namespace mm2

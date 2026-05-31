#pragma once
// attrib.dat - 60 screens * 64 bytes = 3840 bytes (partially decoded).
//   Confirmed offsets (aloc = area * 64). See
//   EXTRACTED/docs/12-attrib-dat-format.md.
//     +0x00  area id (== record index)
//     +0x01  map category (1 town/surface, 2 cavern, 3 dungeon, 4 castle)
//     +0x02  tileset/graphics set
//     +0x03  environment type (town/cavern/castle/outside)
//     +0x04  if >0: outside-area flag (value = terrain class), name at +0x15
//     +0x05..+0x08  world adjacency: 4 neighbour screen ids (opposite pairs
//                   (0x05,0x07) and (0x06,0x08)); == area id when interior.
//                   Verified fully symmetric across all 60 records.
//     +0x15  outside area label byte / interior complex-id hi byte
//     +0x15..+0x16  complex id (big-endian word, shared town<->cavern)
//     +0x17  level/floor (interior)
//     +0x20..+0x3F  roof flags (32 bytes * 8 bits = 256 roof bits)
//   Remaining bytes still unresolved; preserved raw.

#include <array>
#include <string>

#include "core/ByteIO.h"

namespace mm2 {

constexpr int kAttribScreens = 60;
constexpr int kAttribRecordSize = 64;
constexpr int kAttribFileSize = kAttribScreens * kAttribRecordSize;

namespace attrib_off {
constexpr int kAreaId = 0x00;
constexpr int kMapCategory = 0x01;
constexpr int kTileset = 0x02;
constexpr int kEnvType = 0x03;
constexpr int kOutsideFlag = 0x04;
constexpr int kNeighbor0 = 0x05;  // slots 0..3; (0,2) and (1,3) are opposite
constexpr int kEntryCoord = 0x0E;       // packed (Y<<4)|X party entry pos
constexpr int kEraGate = 0x0F;          // compared vs current era index
constexpr int kOutsideLabel = 0x15;
constexpr int kComplexId = 0x15;        // big-endian word (interior)
constexpr int kTransitionCoord = 0x16;  // packed (Y<<4)|X dest pos
constexpr int kLevel = 0x17;
constexpr int kTransitionScreen = 0x18;
constexpr int kFlags = 0x1A;            // btst bitfield (bits 0,3,4,5,6,7)
constexpr int kRoofBits = 0x20;   // 32 bytes
constexpr int kRoofBytes = 32;
}  // namespace attrib_off

struct AttribScreen {
    std::array<uint8_t, kAttribRecordSize> raw{};

    uint8_t areaId() const { return raw[attrib_off::kAreaId]; }
    uint8_t mapCategory() const { return raw[attrib_off::kMapCategory]; }
    uint8_t tileset() const { return raw[attrib_off::kTileset]; }
    uint8_t envType() const { return raw[attrib_off::kEnvType]; }
    uint8_t outsideFlag() const { return raw[attrib_off::kOutsideFlag]; }
    bool isOutside() const { return raw[attrib_off::kOutsideFlag] != 0; }
    uint8_t outsideLabel() const { return raw[attrib_off::kOutsideLabel]; }
    uint8_t neighbor(int slot) const {
        return raw[attrib_off::kNeighbor0 + (slot & 3)];
    }
    uint16_t complexId() const {
        return static_cast<uint16_t>((raw[attrib_off::kComplexId] << 8) |
                                     raw[attrib_off::kComplexId + 1]);
    }
    uint8_t level() const { return raw[attrib_off::kLevel]; }
    uint8_t eraGate() const { return raw[attrib_off::kEraGate]; }
    uint8_t transitionScreen() const { return raw[attrib_off::kTransitionScreen]; }
    uint8_t flags() const { return raw[attrib_off::kFlags]; }
    int entryX() const { return raw[attrib_off::kEntryCoord] & 0x0F; }
    int entryY() const { return (raw[attrib_off::kEntryCoord] >> 4) & 0x0F; }
    int transitionX() const { return raw[attrib_off::kTransitionCoord] & 0x0F; }
    int transitionY() const { return (raw[attrib_off::kTransitionCoord] >> 4) & 0x0F; }
    bool roofBit(int tile) const {
        int byte = attrib_off::kRoofBits + (tile >> 3);
        return (raw[byte] >> (tile & 7)) & 1;
    }
};

// Roof grid cell (col x, row y, y=0 at top) -> on-disk roof bit index.
// ASM-confirmed: tile t => byte (0x20 + t>>3), bit (t & 7), t = y*16 + x.
// Plain linear, no reversal (docs/12-attrib-dat-format.md).
inline int attribRoofTileAt(int x, int y) { return y * 16 + x; }

struct AttribFile {
    std::array<AttribScreen, kAttribScreens> screens{};

    bool decode(const Bytes& bytes);
    Bytes encode() const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace mm2

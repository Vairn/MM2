#pragma once
// map.dat - 60 screens * 512 bytes = 30720 bytes.
//   Per screen: +0x000 256 visual tile bytes, +0x100 256 collision bytes.
//   Each page is a 16x16 grid. Loaded flat into runtime at A4-$EEF4.

#include <array>
#include <string>

#include "core/ByteIO.h"

namespace mm2 {

constexpr int kMapScreens = 60;
constexpr int kMapScreenSize = 512;
constexpr int kMapPageSize = 256;  // 16x16
constexpr int kMapGridDim = 16;
constexpr int kMapFileSize = kMapScreens * kMapScreenSize;

// Visual page (page 0): four 2-bit wall fields per cell — walls only, no event bit.
//   bits 0-1 North   2-3 East   4-5 South   6-7 West
//   0 open   1 wall   2 wall+torch   3 door
enum class VisualWall : uint8_t { Open = 0, Wall = 1, Torch = 2, Door = 3 };

inline int visualWallN(uint8_t cell) { return cell & 3; }
inline int visualWallE(uint8_t cell) { return (cell >> 2) & 3; }
inline int visualWallS(uint8_t cell) { return (cell >> 4) & 3; }
inline int visualWallW(uint8_t cell) { return (cell >> 6) & 3; }

// Collision-page (page 1) bit layout. Each cell packs four directional fields,
// each = (dark<<1)|wall: the low bit is a wall, the high bit marks DARKNESS
// (entering a dark square drains one party light factor -- hence the two Cleric
// light spells: Light +1, Lasting Light +20). West's high bit is reused as a
// top-bit EVENT marker instead (verified against event.dat: every tile event
// triplet lands on a cell with 0x80 set; see docs/15).
//   bit0 0x01 North wall   bit1 0x02 North dark
//   bit2 0x04 East  wall   bit3 0x08 East  dark
//   bit4 0x10 South wall   bit5 0x20 South dark
//   bit6 0x40 West  wall   bit7 0x80 EVENT flag (West's dark slot reused)
constexpr uint8_t kCollisionEventFlag = 0x80;
// Wall bits only; passability first gate @ 0x9424 AND #$55.
constexpr uint8_t kCollisionPassWallMask = 0x55;
// Wall+dark bits with event stripped — cartography / display, not movement.
constexpr uint8_t kCollisionWallMask = 0x7F;

struct MapScreen {
    std::array<uint8_t, kMapPageSize> visual{};
    std::array<uint8_t, kMapPageSize> collision{};

    uint8_t visualAt(int x, int y) const { return visual[y * kMapGridDim + x]; }
    uint8_t collisionAt(int x, int y) const { return collision[y * kMapGridDim + x]; }
    bool hasEventAt(int x, int y) const {
        return (collision[y * kMapGridDim + x] & kCollisionEventFlag) != 0;
    }
};

inline bool collisionHasEvent(uint8_t cell) { return (cell & kCollisionEventFlag) != 0; }

struct MapFile {
    std::array<MapScreen, kMapScreens> screens{};

    bool decode(const Bytes& bytes);
    Bytes encode() const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace mm2

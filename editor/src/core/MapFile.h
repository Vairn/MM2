#pragma once
// map.dat: 60 screens * 512 bytes = 30720.
//   Per screen: +0x000 256 visual, +0x100 256 collision; 16x16. Runtime A4-$EEF4.

#include <array>
#include <string>

#include "core/ByteIO.h"

namespace mm2 {

constexpr int kMapScreens = 60;
constexpr int kMapScreenSize = 512;
constexpr int kMapPageSize = 256;  // 16x16
constexpr int kMapGridDim = 16;
constexpr int kMapFileSize = kMapScreens * kMapScreenSize;

// Visual page: four 2-bit wall fields, no event bit.
//   bits 0-1 N  2-3 E  4-5 S  6-7 W
//   0 open  1 wall  2 door  3 wall+torch
enum class VisualWall : uint8_t { Open = 0, Wall = 1, Door = 2, Torch = 3 };

inline int visualWallN(uint8_t cell) { return cell & 3; }
inline int visualWallE(uint8_t cell) { return (cell >> 2) & 3; }
inline int visualWallS(uint8_t cell) { return (cell >> 4) & 3; }
inline int visualWallW(uint8_t cell) { return (cell >> 6) & 3; }

// Collision page: each dir = (dark<<1)|wall. West high bit is the EVENT flag
// (0x80) instead of dark. Passability gate @ 0x9424 AND #$55.
//   bit0 0x01 N wall   bit1 0x02 N dark
//   bit2 0x04 E wall   bit3 0x08 E dark
//   bit4 0x10 S wall   bit5 0x20 S dark
//   bit6 0x40 W wall   bit7 0x80 EVENT
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

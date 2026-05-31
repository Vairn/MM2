#pragma once
// Endian-aware read/write helpers for MM2 data files.
//
// MM2 is an Amiga (68000) game. Multibyte fields in .dat files are generally
// little-endian on disk (matches Blitz3D ReadShort/ReadInt and on-disk bytes).
// Default to readU16LE / writeU16LE unless a format doc or ASM trace says otherwise.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mm2 {

using Bytes = std::vector<uint8_t>;

inline uint16_t readU16BE(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
inline uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
inline uint32_t readU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}
inline uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline void writeU16BE(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v & 0xFF);
}
inline void writeU16LE(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>(v >> 8);
}
inline void writeU32BE(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}
inline void writeU32LE(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

inline uint8_t nibbleHi(uint8_t packed) { return (packed >> 4) & 0x0F; }
inline uint8_t nibbleLo(uint8_t packed) { return packed & 0x0F; }
inline uint8_t packNibbles(uint8_t hi, uint8_t lo) {
    return static_cast<uint8_t>(((hi & 0x0F) << 4) | (lo & 0x0F));
}

// Read entire file into a byte vector. Returns false on failure.
bool readFile(const std::string& path, Bytes& out);
// Write byte vector to file. Returns false on failure.
bool writeFile(const std::string& path, const Bytes& data);

}  // namespace mm2

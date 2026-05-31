#pragma once
// str.dat - text pool (7808 bytes). Byte transform:
//   decoded = (encoded + 0x1C) & 0xFF ; encoded byte 0x01 = newline.
//   See tools/mm2_codec.py and EXTRACTED/docs/11-str-decoded.txt.

#include <string>

#include "core/ByteIO.h"

namespace mm2 {

constexpr uint8_t kStrShift = 0x1C;
constexpr uint8_t kStrNewlineEncoded = 0x01;

inline uint8_t strDecodeByte(uint8_t enc) {
    return (enc == kStrNewlineEncoded) ? static_cast<uint8_t>('\n')
                                       : static_cast<uint8_t>((enc + kStrShift) & 0xFF);
}
inline uint8_t strEncodeByte(uint8_t dec) {
    return (dec == static_cast<uint8_t>('\n')) ? kStrNewlineEncoded
                                               : static_cast<uint8_t>((dec - kStrShift) & 0xFF);
}

struct StrFile {
    std::string text;       // decoded, human-readable
    size_t rawSize = 0;     // original byte count

    bool decode(const Bytes& bytes);
    Bytes encode() const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace mm2

#include "core/MapFile.h"

#include <cstring>

#include "core/PcDatLzw.h"

namespace mm2 {

bool MapFile::decode(const Bytes& bytes) {
    if (bytes.size() < static_cast<size_t>(kMapFileSize)) return false;
    for (int i = 0; i < kMapScreens; ++i) {
        const uint8_t* p = bytes.data() + i * kMapScreenSize;
        std::memcpy(screens[i].visual.data(), p, kMapPageSize);
        std::memcpy(screens[i].collision.data(), p + kMapPageSize, kMapPageSize);
    }
    return true;
}

Bytes MapFile::encode() const {
    Bytes out(kMapFileSize, 0);
    for (int i = 0; i < kMapScreens; ++i) {
        uint8_t* p = out.data() + i * kMapScreenSize;
        std::memcpy(p, screens[i].visual.data(), kMapPageSize);
        std::memcpy(p + kMapPageSize, screens[i].collision.data(), kMapPageSize);
    }
    return out;
}

bool MapFile::load(const std::string& path) {
    Bytes b;
    if (!pcDatReadFlexible(path, b)) return false;
    // GOG MAP.DAT stores each of the 60 screens as its own LZW blob behind a
    // u16 offset table; reassembling them reproduces the flat Amiga map.dat
    // byte-identical. No-op on already-plain files.
    b = pcDatDecompressMap(b);
    return decode(b);
}

bool MapFile::save(const std::string& path) const {
    return writeFile(path, encode());
}

}  // namespace mm2

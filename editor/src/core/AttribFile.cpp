#include "core/AttribFile.h"

#include <cstring>

#include "core/PcDatLzw.h"

namespace mm2 {

bool AttribFile::decode(const Bytes& bytes) {
    if (bytes.size() < static_cast<size_t>(kAttribFileSize)) return false;
    for (int i = 0; i < kAttribScreens; ++i) {
        std::memcpy(screens[i].raw.data(), bytes.data() + i * kAttribRecordSize, kAttribRecordSize);
    }
    return true;
}

Bytes AttribFile::encode() const {
    Bytes out(kAttribFileSize, 0);
    for (int i = 0; i < kAttribScreens; ++i) {
        std::memcpy(out.data() + i * kAttribRecordSize, screens[i].raw.data(), kAttribRecordSize);
    }
    return out;
}

bool AttribFile::load(const std::string& path) {
    Bytes b;
    if (!pcDatReadFlexible(path, b)) return false;
    // GOG ATTRIB.DAT is LZW-compressed (whole-file container); decompresses
    // byte-identical to the Amiga attrib.dat. No-op on already-plain files.
    b = pcDatDecompressFlat(b);
    return decode(b);
}

bool AttribFile::save(const std::string& path) const {
    return writeFile(path, encode());
}

}  // namespace mm2

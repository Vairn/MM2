#include "core/SpellsFile.h"

#include <cstring>

namespace mm2 {

bool SpellsFile::decode(const Bytes& bytes) {
    if (bytes.size() < static_cast<size_t>(kSpellsFileSize)) return false;
    std::memcpy(raw.data(), bytes.data(), kSpellsFileSize);
    for (int i = 0; i < kSpellsRecordCount; ++i) {
        records[i].b0 = raw[i * kSpellRecordSize];
        records[i].b1 = raw[i * kSpellRecordSize + 1];
    }
    return true;
}

Bytes SpellsFile::encode() const {
    Bytes out(raw.begin(), raw.end());
    for (int i = 0; i < kSpellsRecordCount; ++i) {
        out[i * kSpellRecordSize] = records[i].b0;
        out[i * kSpellRecordSize + 1] = records[i].b1;
    }
    return out;
}

bool SpellsFile::load(const std::string& path) {
    Bytes b;
    if (!readFile(path, b)) return false;
    return decode(b);
}

bool SpellsFile::save(const std::string& path) const {
    return writeFile(path, encode());
}

}  // namespace mm2

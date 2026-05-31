#include "core/ItemsFile.h"

#include <cstring>

namespace mm2 {

std::string ItemRecord::nameStr() const {
    std::string s(name, name + kItemNameSize);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

void ItemRecord::setName(const std::string& s) {
    std::memset(name, ' ', kItemNameSize);
    for (int i = 0; i < kItemNameSize && i < static_cast<int>(s.size()); ++i) name[i] = s[i];
}

bool ItemsFile::decode(const Bytes& bytes) {
    if (bytes.size() < static_cast<size_t>(kItemsFileSize)) return false;
    for (int i = 0; i < kItemsCount; ++i) {
        const uint8_t* p = bytes.data() + i * kItemRecordSize;
        ItemRecord& r = records[i];
        std::memcpy(r.name, p, kItemNameSize);
        r.separator = p[0x0C];
        r.forbiddenClassMask = p[0x0D];
        r.bonusType = nibbleHi(p[0x0E]);
        r.bonusAmount = nibbleLo(p[0x0E]);
        r.effectType = nibbleHi(p[0x0F]);
        r.effectAmount = nibbleLo(p[0x0F]);
        r.damage = p[0x10];
        r.pad = p[0x11];
        r.gold = readU16LE(p + 0x12);
    }
    return true;
}

Bytes ItemsFile::encode() const {
    Bytes out(kItemsFileSize, 0);
    for (int i = 0; i < kItemsCount; ++i) {
        uint8_t* p = out.data() + i * kItemRecordSize;
        const ItemRecord& r = records[i];
        std::memcpy(p, r.name, kItemNameSize);
        p[0x0C] = r.separator;
        p[0x0D] = r.forbiddenClassMask;
        p[0x0E] = packNibbles(r.bonusType, r.bonusAmount);
        p[0x0F] = packNibbles(r.effectType, r.effectAmount);
        p[0x10] = r.damage;
        p[0x11] = r.pad;
        writeU16LE(p + 0x12, r.gold);
    }
    return out;
}

bool ItemsFile::load(const std::string& path) {
    Bytes b;
    if (!readFile(path, b)) return false;
    return decode(b);
}

bool ItemsFile::save(const std::string& path) const {
    return writeFile(path, encode());
}

}  // namespace mm2

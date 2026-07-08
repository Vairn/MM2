#include "core/RosterFile.h"

#include <cstring>

namespace mm2 {

std::string RosterRecord::nameStr() const {
    std::string s;
    for (int i = 0; i < kRosterNameSize; ++i) {
        char c = static_cast<char>(raw[roster_off::kName + i]);
        if (c == 0) break;
        s.push_back(c);
    }
    while (!s.empty() && (s.back() == ' ')) s.pop_back();
    return s;
}

void RosterRecord::setName(const std::string& s) {
    for (int i = 0; i < kRosterNameSize; ++i) {
        raw[roster_off::kName + i] =
            (i < static_cast<int>(s.size())) ? static_cast<uint8_t>(s[i]) : 0;
    }
}

// On-disk layout is Structure-of-Arrays: id run, then charges run, then flags
// run (each kRosterItemSlots long). `base` points at the id run (kEquipped /
// kBackpack); the charges/flags runs follow at +6 / +12.
RosterItemSlot RosterRecord::slot(int base, int idx) const {
    const uint8_t* p = raw.data() + base + idx;
    return RosterItemSlot{p[0], p[roster_off::kItemChargesRun], p[2 * roster_off::kItemChargesRun]};
}

void RosterRecord::setSlot(int base, int idx, const RosterItemSlot& s) {
    uint8_t* p = raw.data() + base + idx;
    p[0] = s.itemId;
    p[roster_off::kItemChargesRun] = s.charges;
    p[2 * roster_off::kItemChargesRun] = s.flags;
}

bool RosterRecord::isEmpty() const {
    for (int i = 0; i < kRosterNameSize; ++i) {
        uint8_t b = raw[roster_off::kName + i];
        if (b != 0 && b != ' ') return false;
    }
    return true;
}

bool normalizeRosterDiskImage(Bytes& bytes) {
    const size_t n = bytes.size();
    if (n == static_cast<size_t>(kRosterFileSize)) return true;
    if (n == static_cast<size_t>(kRosterPcFileSize)) {
        bytes.resize(static_cast<size_t>(kRosterFileSize), 0);
        return true;
    }
    return false;
}

bool RosterFile::decode(const Bytes& bytes) {
    if (bytes.size() != static_cast<size_t>(kRosterFileSize)) return false;
    for (int i = 0; i < kRosterCount; ++i) {
        std::memcpy(records[i].raw.data(), bytes.data() + i * kRosterRecordSize, kRosterRecordSize);
    }
    return true;
}

Bytes RosterFile::encode() const {
    Bytes out(kRosterFileSize, 0);
    for (int i = 0; i < kRosterCount; ++i) {
        std::memcpy(out.data() + i * kRosterRecordSize, records[i].raw.data(), kRosterRecordSize);
    }
    return out;
}

bool RosterFile::load(const std::string& path) {
    Bytes b;
    if (!readFile(path, b)) return false;
    if (!normalizeRosterDiskImage(b)) return false;
    return decode(b);
}

bool RosterFile::save(const std::string& path) const {
    return writeFile(path, encode());
}

}  // namespace mm2

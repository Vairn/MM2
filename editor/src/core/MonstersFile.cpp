#include "core/MonstersFile.h"

#include <cstring>

#include "core/PcDatLzw.h"

namespace mm2 {

namespace {
constexpr uint16_t kMonHpXpMul[4] = {1, 10, 100, 1000};
}

// Effect names, index 0..31, transcribed from b3d adesc.log. Used by both the
// single-target (Sabil) and party-wide (Pabil) attack effect fields.
const char* const kAbilityNames[kAbilityCount] = {
    "nothing",                 // 0
    "loose gold",              // 1
    "loose gems",              // 2
    "Poison",                  // 3
    "Disease",                 // 4
    "Sleep",                   // 5
    "Curse",                   // 6
    "Silence",                 // 7
    "Paralyse",                // 8
    "Collapse",                // 9
    "Kills",                   // 10
    "Stones",                  // 11
    "Eradicates",              // 12
    "Steals an Item",          // 13
    "Steals ALL Items",        // 14
    "Steals food",             // 15
    "Steals all Food",         // 16
    "loose all Gold",          // 17
    "Loose all Gems",          // 18
    "Loose all Valuables",     // 19
    "Ages (non perm)",         // 20
    "Ages (PERM)",             // 21
    "loose 1 level",           // 22
    "Halves all Stats",        // 23
    "gives you Personality",   // 24
    "loose experience levels", // 25
    "halves experience level", // 26
    "loose Experience",        // 27
    "Items Scrambled",         // 28
    "Spell points loose all",  // 29
    "Assassinates",            // 30
    "Random Effect",           // 31
};

const char* abilityName(uint8_t index) {
    return index < kAbilityCount ? kAbilityNames[index] : "?";
}

// Group/party attack verbs, index 0..31. Transcribed verbatim from the verb
// table the combat dispatcher prints at asm 0x10002 (strings at 0xFB98..0xFD1E,
// indexed directly by the Pabil low-5 value). Index 29 = "frenzies".
const char* const kPartyVerbNames[kPartyVerbCount] = {
    "sprays poison",       // 0
    "sprays acid",         // 1
    "casts a curse",       // 2
    "breathes fire",       // 3
    "breathes lightning",  // 4
    "breathes cold",       // 5
    "breathes energy",     // 6
    "breathes gas",        // 7
    "breathes acid",       // 8
    "explodes",            // 9
    "gazes",               // 10
    "drains magic",        // 11
    "drains spell level",  // 12
    "vaporizes valuables", // 13
    "juggles party",       // 14
    "energy blast",        // 15
    "sleep",               // 16
    "lightning bolts",     // 17
    "fireballs",           // 18
    "fingers of death",    // 19
    "disintegrate",        // 20
    "super shock",         // 21
    "dancing sword",       // 22
    "incinerate",          // 23
    "invokes power",       // 24
    "implosion",           // 25
    "inferno",             // 26
    "pain",                // 27
    "silence",             // 28
    "frenzies",            // 29
    "paralyze",            // 30
    "swarms",              // 31
};

const char* partyVerbName(uint8_t index) {
    return index < kPartyVerbCount ? kPartyVerbNames[index] : "?";
}

std::string MonsterRecord::nameStr() const {
    std::string s;
    for (int i = 0; i < kMonsterNameSize; ++i) {
        uint8_t b = raw[i];
        char c = static_cast<char>((b - 128) & 0xFF);
        if (c == 0) break;
        s.push_back(c);
    }
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

void MonsterRecord::setName(const std::string& s) {
    for (int i = 0; i < kMonsterNameSize; ++i) {
        char c = (i < static_cast<int>(s.size())) ? s[i] : ' ';
        raw[i] = static_cast<uint8_t>((static_cast<uint8_t>(c) + 128) & 0xFF);
    }
}

uint32_t MonsterRecord::hpValue() const {
    // ASM unpacker 0x4C8E:
    // hp = ((code & 0x3F) + 1) * hpmul[(code >> 6) & 3]
    const uint8_t c = hpCode();
    const uint32_t base = static_cast<uint32_t>(c & 0x3F) + 1u;
    return base * static_cast<uint32_t>(kMonHpXpMul[(c >> 6) & 0x03]);
}

uint32_t MonsterRecord::xpValue() const {
    // ASM unpacker 0x4C8E:
    // xp = ((code & 0x1F) + 1) * xpmul[(code & 0x60) >> 5], then *1000 if bit7.
    const uint8_t c = xpCode();
    const uint32_t base = static_cast<uint32_t>(c & 0x1F) + 1u;
    uint32_t xp = base * static_cast<uint32_t>(kMonHpXpMul[(c >> 5) & 0x03]);
    if ((c & 0x80) != 0) xp *= 1000u;
    return xp;
}

bool MonstersFile::decode(const Bytes& bytes) {
    if (bytes.size() < static_cast<size_t>(kMonstersFileSize)) return false;
    for (int i = 0; i < kMonstersCount; ++i) {
        std::memcpy(records[i].raw.data(), bytes.data() + i * kMonsterRecordSize, kMonsterRecordSize);
    }
    return true;
}

Bytes MonstersFile::encode() const {
    Bytes out(kMonstersFileSize, 0);
    for (int i = 0; i < kMonstersCount; ++i) {
        std::memcpy(out.data() + i * kMonsterRecordSize, records[i].raw.data(), kMonsterRecordSize);
    }
    return out;
}

bool MonstersFile::load(const std::string& path) {
    Bytes b;
    if (!pcDatReadFlexible(path, b)) return false;
    // GOG MONSTERS.DAT is LZW-compressed (whole-file container); decompresses
    // byte-identical to the Amiga monsters.dat. No-op on already-plain files.
    b = pcDatDecompressFlat(b);
    return decode(b);
}

bool MonstersFile::save(const std::string& path) const {
    return writeFile(path, encode());
}

}  // namespace mm2

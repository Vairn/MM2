#pragma once
// monsters.dat: 256 * 26 = 6656 bytes (accessor 0x99C8, index * 0x1A).
// Names: each byte = char + 128. Unknown bytes survive round-trip.
//
//   0x00-0x0D name
//   0x0E      hp code
//   0x0F      xp code
//   0x10      treasure/reward pack (gold/gems/item/XP; decode @ 0x10b74)
//   0x11      Pabil (group attack: low 5 = verb index into kPartyVerbNames,
//                    bits 5-7 = use-chance tier)
//   0x12      Sabil (single-target: low 5 = kAbilityNames; bit7 undead,
//                    bit6 archer, bit5 misc)
//   0x13      Oabil (low nibble+1, ×10 if bit4 = "adds friends";
//                    bits 5-6 flee tier; bit7 multiplies)
//   0x14      speed
//   0x15      picture → "NN.anm" (picture & 0x7F); 0x80 is a size/placement flag
//   0x16      AC
//   0x17      damage
//   0x18      speed2
//   0x19      magic resistance

#include <array>
#include <string>

#include "core/ByteIO.h"

namespace mm2 {

constexpr int kMonstersCount = 256;
constexpr int kMonsterRecordSize = 0x1A;
constexpr int kMonsterNameSize = 14;
constexpr int kMonstersFileSize = kMonstersCount * kMonsterRecordSize;

// Sabil (0x12) low 5 bits → victim-message table (~0xFA1A).
constexpr int kAbilityCount = 32;
extern const char* const kAbilityNames[kAbilityCount];
const char* abilityName(uint8_t index);

// Pabil (0x11) low 5 bits → combat verb table @ 0x10002 (strings 0xFB98+).
constexpr int kPartyVerbCount = 32;
extern const char* const kPartyVerbNames[kPartyVerbCount];
const char* partyVerbName(uint8_t index);

struct MonsterRecord {
    std::array<uint8_t, kMonsterRecordSize> raw{};

    // Names stored as (char + 128) per byte.
    std::string nameStr() const;
    void setName(const std::string& s);

    uint8_t& byteAt(int off) { return raw[off]; }
    uint8_t hpCode() const { return raw[0x0E]; }
    uint8_t xpCode() const { return raw[0x0F]; }
    uint32_t hpValue() const;
    uint32_t xpValue() const;
    uint8_t treasureCode() const { return raw[0x10]; }
    uint8_t itemDropLevel() const { return raw[0x10] & 0x03; }      // bits 0..1
    bool dropsGems() const { return (raw[0x10] & 0x04) != 0; }      // bit 2
    uint8_t goldTier() const { return (raw[0x10] >> 3) & 0x03; }    // bits 3..4
    uint8_t pabil() const { return raw[0x11]; }
    uint8_t sabil() const { return raw[0x12]; }
    uint8_t oabil() const { return raw[0x13]; }
    uint8_t speed() const { return raw[0x14]; }

    // Sabil (0x12) / Pabil (0x11) / Oabil (0x13) — bits preserved on write.
    uint8_t singleEffect() const { return raw[0x12] & 0x1F; }
    bool isUndead() const { return (raw[0x12] & 0x80) != 0; }
    bool isArcher() const { return (raw[0x12] & 0x40) != 0; }
    bool sabilMisc() const { return (raw[0x12] & 0x20) != 0; }  // bit5 misc (unmapped)
    void setSingleEffect(uint8_t e) { raw[0x12] = (raw[0x12] & ~0x1F) | (e & 0x1F); }
    void setUndead(bool v) { raw[0x12] = v ? (raw[0x12] | 0x80) : (raw[0x12] & ~0x80); }
    void setArcher(bool v) { raw[0x12] = v ? (raw[0x12] | 0x40) : (raw[0x12] & ~0x40); }
    void setSabilMisc(bool v) { raw[0x12] = v ? (raw[0x12] | 0x20) : (raw[0x12] & ~0x20); }

    // Pabil low 5 = kPartyVerbNames; bits 5-7 = use-chance tier (not flee).
    uint8_t partyVerb() const { return raw[0x11] & 0x1F; }
    uint8_t partyChance() const { return (raw[0x11] >> 5) & 0x07; }
    void setPartyVerb(uint8_t e) { raw[0x11] = (raw[0x11] & ~0x1F) | (e & 0x1F); }
    void setPartyChance(uint8_t c) { raw[0x11] = (raw[0x11] & ~0xE0) | ((c & 0x07) << 5); }

    // Oabil: low nibble+1 = "adds friends" (×10 if bit4); bits 5-6 flee; bit7 multiplies.
    uint8_t addFriends() const {
        uint8_t n = (raw[0x13] & 0x0F) + 1;
        return (raw[0x13] & 0x10) ? static_cast<uint8_t>(n * 10) : n;
    }
    uint8_t friendCount() const { return (raw[0x13] & 0x0F) + 1; }  // low nibble (1..16)
    bool friendCountX10() const { return (raw[0x13] & 0x10) != 0; } // bit4 scales x10
    uint8_t fleeTier() const { return (raw[0x13] >> 5) & 0x03; }
    bool multiplies() const { return (raw[0x13] & 0x80) != 0; }
    void setFriendCount(uint8_t n) {
        uint8_t v = n > 0 ? n : 1;
        v = v > 16 ? 16 : v;
        raw[0x13] = static_cast<uint8_t>((raw[0x13] & ~0x0F) | ((v - 1) & 0x0F));
    }
    void setFriendCountX10(bool v) { raw[0x13] = v ? (raw[0x13] | 0x10) : (raw[0x13] & ~0x10); }
    void setMultiplies(bool v) { raw[0x13] = v ? (raw[0x13] | 0x80) : (raw[0x13] & ~0x80); }

    uint8_t picture() const { return raw[0x15]; }
    uint8_t ac() const { return raw[0x16]; }
    uint8_t damage() const { return raw[0x17]; }
    uint8_t magicResist() const { return raw[0x19]; }
};

struct MonstersFile {
    std::array<MonsterRecord, kMonstersCount> records{};

    bool decode(const Bytes& bytes);
    Bytes encode() const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace mm2

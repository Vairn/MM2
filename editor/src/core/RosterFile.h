#pragma once
// roster.dat - layout confirmed (64 records * 130 bytes (0x82) = 8320 bytes).
//   Multibyte fields are little-endian on disk.
//   See EXTRACTED/docs/06-roster-format.md and decomp/mm2_roster_codec.h.
//
// We keep the full 130-byte record as raw bytes and expose typed accessors
// for the confirmed fields, so unknown spans round-trip losslessly.

#include <array>
#include <string>

#include "core/ByteIO.h"

namespace mm2 {

constexpr int kRosterCount = 64;
constexpr int kRosterCharacterCount = 48;  // Ends at "Mr. Wizard" (slot 47).
constexpr int kRosterGlobalStart = kRosterCharacterCount;
constexpr int kRosterGlobalCount = kRosterCount - kRosterGlobalStart;
constexpr int kRosterRecordSize = 0x82;
constexpr int kRosterNameSize = 11;
constexpr int kRosterFileSize = kRosterCount * kRosterRecordSize;
constexpr int kRosterItemSlots = 6;

struct RosterItemSlot {
    uint8_t itemId = 0;
    uint8_t bonus = 0;
    uint8_t flags = 0;
};

// Field offsets within a 130-byte record.
namespace roster_off {
constexpr int kName = 0x00;          // 11 bytes
constexpr int kTownFlags = 0x0B;     // low7 town, bit7 in-party
constexpr int kSex = 0x0C;
constexpr int kAlignCur = 0x0D;
constexpr int kRace = 0x0E;
constexpr int kClass = 0x0F;
constexpr int kMight = 0x10;
constexpr int kIntellect = 0x11;
constexpr int kPersonality = 0x12;
constexpr int kSpeed = 0x13;
constexpr int kAccuracy = 0x14;
constexpr int kLuck = 0x15;
constexpr int kThievery = 0x16;
constexpr int kAge = 0x21;
constexpr int kArmorClass = 0x24;
constexpr int kFood = 0x25;
constexpr int kCondition = 0x26;
constexpr int kEnduranceCur = 0x27;
constexpr int kEquipped = 0x28;      // 6 slots * 3 bytes
constexpr int kBackpack = 0x3A;      // 6 slots * 3 bytes
constexpr int kSpells = 0x4C;        // 12 bytes bitmask
constexpr int kSpMax = 0x58;         // u16 LE
constexpr int kSpCur = 0x5A;
constexpr int kGems = 0x5C;
constexpr int kHpMax = 0x5E;
constexpr int kHpAux = 0x60;
constexpr int kExperience = 0x62;    // u32 LE
constexpr int kGold = 0x66;          // u32 LE
constexpr int kAlignBase = 0x6A;
constexpr int kMightBase = 0x6B;
constexpr int kIntellectBase = 0x6C;
constexpr int kPersonalityBase = 0x6D;
constexpr int kSpeedBase = 0x6E;
constexpr int kAccuracyBase = 0x6F;
constexpr int kLuckBase = 0x70;
constexpr int kLevel = 0x71;
constexpr int kSpellLevel = 0x72;
constexpr int kEnduranceBase = 0x73;
constexpr int kHpCur = 0x74;         // u16 LE
constexpr int kTempScoreWord = 0x76; // u16 LE
constexpr int kScriptWorkFlag = 0x78;
constexpr int kClassQuestGuildMask = 0x79;  // bit7 = class '+'; low bits AND A4-$66A9[town]
constexpr int kTailPadding7A = 0x7A;        // 10 bytes, mostly padding
}  // namespace roster_off

struct RosterRecord {
    std::array<uint8_t, kRosterRecordSize> raw{};

    std::string nameStr() const;
    void setName(const std::string& s);

    uint8_t u8(int off) const { return raw[off]; }
    void setU8(int off, uint8_t v) { raw[off] = v; }
    uint16_t u16(int off) const { return readU16LE(raw.data() + off); }
    void setU16(int off, uint16_t v) { writeU16LE(raw.data() + off, v); }
    uint32_t u32(int off) const { return readU32LE(raw.data() + off); }
    void setU32(int off, uint32_t v) { writeU32LE(raw.data() + off, v); }

    RosterItemSlot slot(int base, int idx) const;
    void setSlot(int base, int idx, const RosterItemSlot& s);

    bool isEmpty() const;  // name blank/zero
};

struct RosterFile {
    std::array<RosterRecord, kRosterCount> records{};

    bool decode(const Bytes& bytes);
    Bytes encode() const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace mm2

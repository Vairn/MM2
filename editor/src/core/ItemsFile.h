#pragma once
// items.dat: 256 records * 20 bytes (0x14) = 5120.
//
//   0x00-0x0B name (12 ASCII, space padded)
//   0x0C      separator (editor writes 0)
//   0x0D      forbidden-class bitmask (set bit = class cannot use)
//   0x0E      bonus  (hi nibble = type, lo = amount)
//   0x0F      effect (flat spell index / boost; see decodeItemEffect)
//   0x10      damage
//   0x11      pad (editor writes 0)
//   0x12-0x13 gold (u16 LE on disk; game byte-swaps on load, asm 0x26030)

#include <array>
#include <string>

#include "core/ByteIO.h"

namespace mm2 {

constexpr int kItemsCount = 256;
constexpr int kItemRecordSize = 0x14;
constexpr int kItemNameSize = 12;
constexpr int kItemsFileSize = kItemsCount * kItemRecordSize;

// Byte 0x0D restriction mask: set bit = class cannot use. Bit order K P A C S R N B
// (Knight 0x80 .. Barbarian 0x01). Usable when `(mask & bit) == 0`.
enum ItemClassBit : uint8_t {
    ITEM_CLASS_KNIGHT    = 0x80,
    ITEM_CLASS_PALADIN   = 0x40,
    ITEM_CLASS_ARCHER    = 0x20,
    ITEM_CLASS_CLERIC    = 0x10,
    ITEM_CLASS_SORCERER  = 0x08,
    ITEM_CLASS_ROBBER    = 0x04,
    ITEM_CLASS_NINJA     = 0x02,
    ITEM_CLASS_BARBARIAN = 0x01,
};

// Bonus category (hi nibble of 0x0E); lo nibble = magnitude (0 = none).
constexpr int kItemBonusTypeCount = 16;
constexpr const char* kItemBonusTypeNames[kItemBonusTypeCount] = {
    "Might",         // 0  (amount 0 = no bonus)
    "Intellect",     // 1
    "Personality",   // 2
    "Speed",         // 3
    "Accuracy",      // 4
    "Luck",          // 5
    "Magic resist",  // 6
    "Fire resist",   // 7
    "Elec resist",   // 8
    "Cold resist",   // 9
    "Energy resist", // 10
    "Sleep resist",  // 11
    "Max HP",        // 12
    "Acid resist",   // 13
    "Thievery",      // 14
    "Armor Class",   // 15
};

// Effect byte 0x0F is a flat index, not (type,amount) like the bonus byte:
//   0x00          no use power
//   0x01..0x7F    non-spell stat boost (hi nibble = kind, lo = amount):
//                 0 MaxHP, 1 Might, 2 Speed, 3 Accuracy, 5 Level, 6 SpLvl
//   0x81..0xB0    Sorcerer spell, index = byte - 0x80
//   0xB1..0xE0    Cleric   spell, index = byte - 0xB0
// Flat index walks the spell list by level: L1..L9 = 7,7,6,6,5,5,4,4,4.

struct ItemRecord {
    char name[kItemNameSize] = {};
    uint8_t separator = 0;
    uint8_t forbiddenClassMask = 0;  // set bit = class cannot use (0x0D)
    uint8_t bonusType = 0;     // hi nibble of 0x0E
    uint8_t bonusAmount = 0;   // lo nibble of 0x0E
    uint8_t effectType = 0;    // hi nibble of 0x0F
    uint8_t effectAmount = 0;  // lo nibble of 0x0F
    uint8_t damage = 0;
    uint8_t pad = 0;
    uint16_t gold = 0;         // little-endian on disk (shop price in gp)

    std::string nameStr() const;
    void setName(const std::string& s);

    uint8_t bonusByte() const {
        return static_cast<uint8_t>(((bonusType & 0xF) << 4) | (bonusAmount & 0xF));
    }
    uint8_t effectByte() const {
        return static_cast<uint8_t>(((effectType & 0xF) << 4) | (effectAmount & 0xF));
    }

    // Usable when the class bit is clear in the restriction mask.
    bool canBeUsedBy(ItemClassBit cls) const { return (forbiddenClassMask & cls) == 0; }
    void setUsableBy(ItemClassBit cls, bool allowed) {
        if (allowed) forbiddenClassMask &= static_cast<uint8_t>(~cls);
        else forbiddenClassMask |= cls;
    }
};

struct ItemsFile {
    std::array<ItemRecord, kItemsCount> records{};

    bool decode(const Bytes& bytes);
    Bytes encode() const;

    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace mm2

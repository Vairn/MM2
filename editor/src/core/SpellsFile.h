#pragma once
// spells.dat: 96 spells × 2 bytes. Records 0..47 Sorcerer, 48..95 Cleric.
//
//   byte0:
//     0x80  non-combat-only
//     0x40  combat-only (neither bit => anytime)
//     0x20  set with 0x10 on some special-cost non-combat spells
//     0x10  special/computed gem cost (low nibble is not the real gem count)
//     0x0F  gem cost (0..15) when not special
//   byte1:
//     0x80    outdoor-only
//     bits 6-4 per-level SP multiplier ("X/L") when low nibble is 0
//     0x0F    flat SP cost; 0 => per caster level
//
// File is 256 bytes; first 192 are records, trailing 64 preserved on round-trip.

#include <array>
#include <cstdint>
#include <string>

#include "core/ByteIO.h"
#include "core/Spells.h"

namespace mm2 {

constexpr int kSpellsRecordCount = kSpellTotal;  // 96
constexpr int kSpellRecordSize = 2;
constexpr int kSpellsFileSize = 256;
constexpr int kSpellsDataSize = kSpellsRecordCount * kSpellRecordSize;  // 192

struct SpellRecord {
    uint8_t b0 = 0;
    uint8_t b1 = 0;

    int gems() const { return b0 & 0x0F; }
    SpellCast cast() const {
        if (b0 & 0x40) return SpellCast::Combat;
        if (b0 & 0x80) return SpellCast::NonCombat;
        return SpellCast::Anytime;
    }
    bool specialCost() const { return (b0 & 0x10) != 0; }
    bool outdoor() const { return (b1 & 0x80) != 0; }
    bool perLevel() const { return (b1 & 0x0F) == 0; }
    int sp() const { return perLevel() ? ((b1 >> 4) & 0x07) : (b1 & 0x0F); }

    void setGems(int g) { b0 = static_cast<uint8_t>((b0 & 0xF0) | (g & 0x0F)); }
    void setCast(SpellCast c) {
        b0 &= ~0xC0;
        if (c == SpellCast::Combat) b0 |= 0x40;
        else if (c == SpellCast::NonCombat) b0 |= 0x80;
    }
    void setSpecialCost(bool v) { b0 = v ? (b0 | 0x10) : (b0 & ~0x10); }
    void setOutdoor(bool v) { b1 = v ? (b1 | 0x80) : (b1 & ~0x80); }
    // Set a flat SP cost (1..15, perLevel=false) or per-level multiplier
    // (perLevel=true, stored in bits 6-4 with a 0 low nibble). Preserves the
    // outdoor flag (bit 0x80).
    void setCost(int sp, bool perLevel) {
        bool out = outdoor();
        if (perLevel) b1 = static_cast<uint8_t>((sp & 0x07) << 4);
        else b1 = static_cast<uint8_t>((b1 & 0x70) | (sp & 0x0F));
        setOutdoor(out);
    }
};

struct SpellsFile {
    std::array<SpellRecord, kSpellsRecordCount> records{};
    // Full 256-byte file; trailing 64 bytes after the records are preserved.
    std::array<uint8_t, kSpellsFileSize> raw{};

    // Reference (manual) data for record i (Sorcerer 0..47, Cleric 48..95).
    static const SpellInfo& info(int recordIndex) { return kSpells[recordIndex]; }

    bool decode(const Bytes& bytes);
    Bytes encode() const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace mm2

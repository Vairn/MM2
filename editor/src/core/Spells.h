#pragma once
// Spell tables + item "use power" decoding.
//
// Two schools, 9 levels, same counts: L1..L9 = 7,7,6,6,5,5,4,4,4 (48 each).
// Order within a level matches in-game / items.dat effect bytes.
//
// spells.dat 2-byte record (see SpellsFile.h):
//   byte0: 0x80 non-combat-only, 0x40 combat-only (neither = anytime),
//          0x10 special/computed cost, low nibble = gem cost.
//   byte1: 0x80 outdoor-only, bits 6-4 = per-level SP multiplier,
//          low nibble = flat SP (0 => per caster level).
//
// items.dat 0x0F is a flat index:
//   0x00        none
//   0x01..0x7F  non-spell boost (hi nibble = kind, lo = amount)
//   0x81..0xB0  Sorcerer, index = byte - 0x80
//   0xB1..0xE0  Cleric,   index = byte - 0xB0

#include <cstdint>
#include <string>

namespace mm2 {

enum class SpellSchool { Sorcerer, Cleric };

constexpr int kSpellLevels = 9;
constexpr int kSpellsPerSchool = 48;
constexpr int kSpellsPerLevel[kSpellLevels] = {7, 7, 6, 6, 5, 5, 4, 4, 4};

// When/where a spell may be cast (manual "TYPE" field).
enum class SpellCast { Anytime, Combat, NonCombat };

// One spell's reference data (from the manual).
struct SpellInfo {
    SpellSchool school;
    int level;          // 1..9
    int number;         // 1-based within the level
    const char* name;
    int sp;             // spell-point cost (per-level multiplier when perLevel)
    bool perLevel;      // true => cost is sp * caster level ("X/L")
    int gems;           // gem cost (may exceed 15; on disk such costs are flagged)
    SpellCast cast;     // when it can be cast
    const char* where;  // "", "Outdoor", "Indoor", "Dungeon", "not hand-to-hand"
    const char* object; // who/what is affected
    const char* desc;   // short description
};

// Full reference table: 96 entries, Sorcerer 0..47 then Cleric 48..95.
constexpr int kSpellTotal = 2 * kSpellsPerSchool;
extern const SpellInfo kSpells[kSpellTotal];

// Reference data for a school + flat 1-based index (1..48), or nullptr.
const SpellInfo* spellInfo(SpellSchool school, int flat);

// Spell name by school + flat 1-based index (1..48), or nullptr if out of range.
const char* spellName(SpellSchool school, int flat);

// Spell name by school + level (1..9) + number (1-based), or nullptr.
const char* spellName(SpellSchool school, int level, int number);

// Convert a flat 1-based index to (level, number). Returns false if out of range.
bool spellLevelNumber(int flat, int& level, int& number);

// "1 SP", "3/L", "2 SP + 1 Gem", "5 SP + 5 Gems", "10 SP + 50 Gems".
std::string formatSpellCost(const SpellInfo& s);

// Decoded item effect byte (items.dat 0x0F).
struct ItemEffect {
    enum class Kind { None, Boost, Spell } kind = Kind::None;
    // Boost: non-spell stat boost (kind == Boost).
    const char* boostName = nullptr;  // "Might", "Speed", ...
    int amount = 0;
    // Spell: cast on use (kind == Spell).
    SpellSchool school = SpellSchool::Sorcerer;
    int level = 0;
    int number = 0;
    const char* spell = nullptr;  // may be nullptr if index is out of range
};

ItemEffect decodeItemEffect(uint8_t effectByte);

// Human-readable description, e.g. "S4/3 Fire Ball", "C2/4 Pain",
// "Might +15", or "none".
std::string describeItemEffect(uint8_t effectByte);

}  // namespace mm2

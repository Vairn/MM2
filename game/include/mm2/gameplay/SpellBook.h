#pragma once
// Spell-book data + known-spell decode for the in-game character sheet.
//
// 100% ASM-faithful to EXTRACTED/mm2.capstone.annotated.asm:
//
//  * Known-spell storage (roster record): the spell-book is a 6-byte bitfield at
//    record offset $51..$56 (48 bits = one school of 48 spells). This is NOT the
//    full $4C..$57 span the format doc loosely labels "spell book", and $50 is a
//    separate class/title nibble (event VM OP_32). Evidence:
//      - Grid renderer expand_spellbook_bits @ 0x659a reads `$51(base+i)` for
//        i = 0..5 (6 bytes) and tests each bit j (0..7) -> spell index i*8 + j.
//      - Guild buy-spell @ 0x1d8d4 sets spell N: byte `$51 + (N>>3)`, bit
//        mask[N&7] (mask table A4-$66b1).
//      - Event OP_2E learn @ 0x16f50 writes `$51(base + idx)` for a class match.
//    Bit order is LSB-first (mask[i] = 1<<i): the same A4-$66b1 table builds the
//    5-town donation field 0x01..0x10 -> 0x1F @ 0x1d7b8.
//
//  * School by class (OP_2E @ 0x16f64/0x16f7c, and $7B36): Sorcerer spells are
//    learned by class 4 (Sorcerer) and 2 (Archer); Cleric spells by class 3
//    (Cleric) and 1 (Paladin). All other classes have no spell book.
//
//  * Spell metadata (names, level grouping, SP / gem cost): transcribed from the
//    manual Appendix B via editor/src/core/Spells.cpp `kSpells[]` and
//    EXTRACTED/docs/19-spells-and-item-use.md. Per-level counts 7,7,6,6,5,5,4,4,4
//    (grid uses cumulative offsets A4-$8C22 + counts A4-$8C2B). Flat order matches
//    spells.dat and the items.dat use-effect index. No names/costs are invented.

#include <cstdint>

#include "mm2_roster_codec.h"

namespace mm2::gameplay {

enum class SpellSchool : uint8_t { None, Sorcerer, Cleric };

// SP cost: when per_level is true the runtime cost is "sp per caster level"
// (printed "sp/L"); otherwise it is a flat sp cost. gems = gem cost (0 = none).
// Values are the manual-transcribed costs (special computed-cost spells keep the
// manual's hard-coded gem figure, e.g. Duplication 100, Star Burst 20).
struct SpellMeta {
    const char *name;
    uint8_t level;   // 1..9
    uint8_t number;  // 1..n within the level
    uint8_t sp;
    bool per_level;
    uint8_t gems;
};

inline constexpr int kSpellsPerSchool = 48;
inline constexpr int kSpellLevels = 9;
inline constexpr uint8_t kSpellsPerLevel[kSpellLevels] = {7, 7, 6, 6, 5, 5, 4, 4, 4};

/** Cumulative flat-index base for level 1..9 (A4-$73DE / -$8C22). */
inline constexpr uint8_t kSpellLevelBase[kSpellLevels] = {0, 7, 14, 20, 26, 31, 36, 40, 44};

/** Flat school index 0..47 from 1-based level/number, or -1 if out of range. */
inline int spellFlatFromLevelNumber(int level, int number)
{
    if (level < 1 || level > kSpellLevels || number < 1) {
        return -1;
    }
    const int max_n = kSpellsPerLevel[level - 1];
    if (number > max_n) {
        return -1;
    }
    return static_cast<int>(kSpellLevelBase[level - 1]) + (number - 1);
}

/** Retail dispatch code: sorcerer/archer keep 0..47; cleric/paladin add $30 (0x79EE). */
inline int spellDispatchCode(SpellSchool school, int flat0)
{
    if (flat0 < 0 || flat0 >= kSpellsPerSchool) {
        return -1;
    }
    return (school == SpellSchool::Cleric) ? (flat0 + 0x30) : flat0;
}

// Record offset of the spell-book bitfield ($51) relative to the codec's
// `spells[]` array (which starts at record $4C). spell N -> byte index + N/8.
inline constexpr int kSpellBookByteBase = 0x51 - 0x4C;  // = 5

inline constexpr SpellMeta kSorcererSpells[kSpellsPerSchool] = {
    // Level 1
    {"Awaken", 1, 1, 1, false, 0},
    {"Detect Magic", 1, 2, 1, false, 0},
    {"Energy Blast", 1, 3, 1, true, 1},
    {"Flame Arrow", 1, 4, 1, false, 0},
    {"Light", 1, 5, 1, false, 0},
    {"Location", 1, 6, 1, false, 0},
    {"Sleep", 1, 7, 1, false, 0},
    // Level 2
    {"Eagle Eye", 2, 1, 2, true, 0},
    {"Electric Arrow", 2, 2, 2, false, 0},
    {"Identify Monster", 2, 3, 2, false, 1},
    {"Jump", 2, 4, 2, false, 0},
    {"Levitate", 2, 5, 2, false, 0},
    {"Lloyd's Beacon", 2, 6, 2, false, 1},
    {"Protection from Magic", 2, 7, 1, true, 1},
    // Level 3
    {"Acid Stream", 3, 1, 1, true, 2},
    {"Fly", 3, 2, 3, false, 0},
    {"Invisibility", 3, 3, 3, false, 0},
    {"Lightning Bolt", 3, 4, 1, true, 2},
    {"Web", 3, 5, 3, false, 2},
    {"Wizard Eye", 3, 6, 3, true, 2},
    // Level 4
    {"Cold Beam", 4, 1, 1, true, 3},
    {"Feeble Mind", 4, 2, 4, false, 3},
    {"Fire Ball", 4, 3, 1, true, 3},
    {"Guard Dog", 4, 4, 4, false, 0},
    {"Shield", 4, 5, 4, false, 0},
    {"Time Distortion", 4, 6, 4, false, 3},
    // Level 5
    {"Disrupt", 5, 1, 5, false, 5},
    {"Fingers of Death", 5, 2, 5, false, 5},
    {"Sand Storm", 5, 3, 2, true, 5},
    {"Shelter", 5, 4, 5, false, 0},
    {"Teleport", 5, 5, 5, false, 0},
    // Level 6
    {"Disintegration", 6, 1, 6, false, 6},
    {"Entrapment", 6, 2, 6, false, 6},
    {"Fantastic Freeze", 6, 3, 2, true, 6},
    {"Recharge Item", 6, 4, 6, false, 6},
    {"Super Shock", 6, 5, 2, true, 6},
    // Level 7
    {"Dancing Sword", 7, 1, 3, true, 7},
    {"Duplication", 7, 2, 7, false, 100},
    {"Etherealize", 7, 3, 7, false, 7},
    {"Prismatic Light", 7, 4, 7, false, 7},
    // Level 8
    {"Incinerate", 8, 1, 3, true, 8},
    {"Mega Volts", 8, 2, 3, true, 8},
    {"Meteor Shower", 8, 3, 8, false, 8},
    {"Power Shield", 8, 4, 8, false, 8},
    // Level 9
    {"Implosion", 9, 1, 10, false, 10},
    {"Inferno", 9, 2, 3, true, 10},
    {"Star Burst", 9, 3, 10, false, 20},
    {"Enchant Item", 9, 4, 50, false, 50},
};

inline constexpr SpellMeta kClericSpells[kSpellsPerSchool] = {
    // Level 1
    {"Apparition", 1, 1, 1, false, 0},
    {"Awaken", 1, 2, 1, false, 0},
    {"Bless", 1, 3, 1, false, 0},
    {"First Aid", 1, 4, 1, false, 0},
    {"Light", 1, 5, 1, false, 0},
    {"Power Cure", 1, 6, 1, true, 1},
    {"Turn Undead", 1, 7, 1, false, 0},
    // Level 2
    {"Cure Wounds", 2, 1, 2, false, 0},
    {"Heroism", 2, 2, 2, false, 1},
    {"Nature's Gate", 2, 3, 2, false, 0},
    {"Pain", 2, 4, 2, false, 0},
    {"Protection From Elements", 2, 5, 2, false, 1},
    {"Silence", 2, 6, 2, false, 0},
    {"Weaken", 2, 7, 2, false, 1},
    // Level 3
    {"Cold Ray", 3, 1, 3, false, 2},
    {"Create Food", 3, 2, 3, false, 2},
    {"Cure Poison", 3, 3, 3, false, 0},
    {"Immobilize", 3, 4, 3, false, 0},
    {"Lasting Light", 3, 5, 3, false, 0},
    {"Walk on Water", 3, 6, 3, false, 2},
    // Level 4
    {"Acid Spray", 4, 1, 4, false, 3},
    {"Air Transmutation", 4, 2, 4, false, 3},
    {"Cure Disease", 4, 3, 4, false, 0},
    {"Restore Alignment", 4, 4, 4, false, 3},
    {"Surface", 4, 5, 4, false, 0},
    {"Holy Bonus", 4, 6, 4, false, 3},
    // Level 5
    {"Air Encasement", 5, 1, 5, false, 5},
    {"Deadly Swarm", 5, 2, 5, false, 5},
    {"Frenzy", 5, 3, 5, false, 5},
    {"Paralyze", 5, 4, 5, false, 5},
    {"Remove Condition", 5, 5, 5, false, 5},
    // Level 6
    {"Earth Transmutation", 6, 1, 6, false, 6},
    {"Rejuvenate", 6, 2, 6, false, 6},
    {"Stone to Flesh", 6, 3, 6, false, 6},
    {"Water Encasement", 6, 4, 6, false, 6},
    {"Water Transmutation", 6, 5, 6, false, 6},
    // Level 7
    {"Earth Encasement", 7, 1, 7, false, 7},
    {"Fiery Flail", 7, 2, 7, false, 7},
    {"Moon Ray", 7, 3, 7, false, 7},
    {"Raise Dead", 7, 4, 7, false, 7},
    // Level 8
    {"Fire Encasement", 8, 1, 8, false, 8},
    {"Fire Transmutation", 8, 2, 8, false, 8},
    {"Mass Distortion", 8, 3, 8, false, 8},
    {"Town Portal", 8, 4, 8, false, 8},
    // Level 9
    {"Divine Intervention", 9, 1, 10, false, 20},
    {"Holy Word", 9, 2, 10, false, 10},
    {"Resurrection", 9, 3, 10, false, 10},
    {"Uncurse Item", 9, 4, 10, false, 50},
};

// School a class casts from (ASM OP_2E class matches + $7B36). Returns None for
// non-casters (Knight, Robber, Ninja, Barbarian).
inline SpellSchool spellSchoolForClass(uint8_t class_id)
{
    switch (class_id) {
    case 2:  // Archer
    case 4:  // Sorcerer
        return SpellSchool::Sorcerer;
    case 1:  // Paladin
    case 3:  // Cleric
        return SpellSchool::Cleric;
    default:
        return SpellSchool::None;
    }
}

inline const SpellMeta *schoolSpellTable(SpellSchool school)
{
    if (school == SpellSchool::Sorcerer) {
        return kSorcererSpells;
    }
    if (school == SpellSchool::Cleric) {
        return kClericSpells;
    }
    return nullptr;
}

inline const char *schoolName(SpellSchool school)
{
    switch (school) {
    case SpellSchool::Sorcerer:
        return "Sorcerer";
    case SpellSchool::Cleric:
        return "Cleric";
    default:
        return "None";
    }
}

// One-letter school tag used in spell labels ("S3/5 Web", "C1/4 First Aid").
inline char schoolTag(SpellSchool school)
{
    return (school == SpellSchool::Sorcerer) ? 'S' : (school == SpellSchool::Cleric) ? 'C' : '?';
}

// Known-spell test for spell index n (0..47) within the character's school.
// spell N known <=> rec.spells[5 + (N>>3)] & (1 << (N&7)). See header notes.
inline bool spellKnownInBook(const Mm2RosterRecord &rec, int n)
{
    if (n < 0 || n >= kSpellsPerSchool) {
        return false;
    }
    const int byte_index = kSpellBookByteBase + (n >> 3);
    if (byte_index < 0 || byte_index >= MM2_ROSTER_SPELL_BYTES) {
        return false;
    }
    return (rec.spells[byte_index] & (1u << (n & 7))) != 0u;
}

// Mark spell index n (0..47) as known. Mirrors the guild buy-spell write
// ($1d8d4): byte $51+(n>>3) |= 1<<(n&7). Provided for tests/tools (does NOT
// modify the roster codec).
inline void spellLearnInBook(Mm2RosterRecord &rec, int n)
{
    if (n < 0 || n >= kSpellsPerSchool) {
        return;
    }
    const int byte_index = kSpellBookByteBase + (n >> 3);
    if (byte_index < 0 || byte_index >= MM2_ROSTER_SPELL_BYTES) {
        return;
    }
    rec.spells[byte_index] = static_cast<uint8_t>(rec.spells[byte_index] | (1u << (n & 7)));
}

inline int knownSpellCount(const Mm2RosterRecord &rec, SpellSchool school)
{
    if (school == SpellSchool::None) {
        return 0;
    }
    int count = 0;
    for (int n = 0; n < kSpellsPerSchool; ++n) {
        if (spellKnownInBook(rec, n)) {
            ++count;
        }
    }
    return count;
}

}  // namespace mm2::gameplay

#include "core/Spells.h"

#include <cstdio>

namespace mm2 {

// Reference data transcribed from the MM2 "Gates to Another World" manual,
// Appendix B (pp. 40-58). Order = manual spell numbering (alphabetical within
// each level), which also matches the spells.dat record order and the
// items.dat flat-effect index. SP/gem/cast values cross-validate the on-disk
// 2-byte records: gems 96/96, cast 96/96, outdoor 12/12, SP 95/96 (only
// Meteor Shower differs - its base SP is computed per-monster in game code).
#define SO SpellSchool::Sorcerer
#define CL SpellSchool::Cleric
#define ANY SpellCast::Anytime
#define COM SpellCast::Combat
#define NON SpellCast::NonCombat
const SpellInfo kSpells[kSpellTotal] = {
    // ---- SORCERER ----
    // Level 1
    {SO, 1, 1, "Awaken", 1, false, 0, ANY, "", "All sleeping party members",
     "Awakens all sleeping party members, cancelling the sleep condition."},
    {SO, 1, 2, "Detect Magic", 1, false, 0, NON, "", "Items in backpack",
     "Reveals magical items in the caster's backpack and their charges."},
    {SO, 1, 3, "Energy Blast", 1, true, 1, COM, "", "1 monster",
     "Zaps a monster with pure energy: 1-6 damage per level of caster."},
    {SO, 1, 4, "Flame Arrow", 1, false, 0, COM, "", "1 monster",
     "Fires a burning shaft for 2-8 fire damage unless immune."},
    {SO, 1, 5, "Light", 1, false, 0, NON, "", "Entire party",
     "Gives the party 1 light factor to light a darkened square."},
    {SO, 1, 6, "Location", 1, false, 0, NON, "", "Entire party",
     "Shows the current 16x16 area mapped and the party's location."},
    {SO, 1, 7, "Sleep", 1, false, 0, COM, "", "4 monsters +1/L",
     "Sends monsters into deep sleep until damaged or overcome."},
    // Level 2
    {SO, 2, 1, "Eagle Eye", 2, true, 0, NON, "Outdoor", "5 steps/L",
     "An overhead 5x5 view of the outdoor terrain and party location."},
    {SO, 2, 2, "Electric Arrow", 2, false, 0, COM, "", "1 monster",
     "Electrocutes a monster for 4-16 damage unless immune."},
    {SO, 2, 3, "Identify Monster", 2, false, 1, COM, "", "1 monster",
     "Informs the caster of the current condition of one monster."},
    {SO, 2, 4, "Jump", 2, false, 0, NON, "", "Entire party",
     "Moves the party 2 squares forward if not magically obstructed."},
    {SO, 2, 5, "Levitate", 2, false, 0, NON, "", "Entire party",
     "Raises characters above ground level, protecting them for 1 day."},
    {SO, 2, 6, "Lloyd's Beacon", 2, false, 1, NON, "Dungeon", "Entire party",
     "Leaves a beacon you may instantly return to next cast."},
    {SO, 2, 7, "Protection from Magic", 1, true, 1, ANY, "", "Entire party",
     "Increases all characters' resistance to magic for 1 day."},
    // Level 3
    {SO, 3, 1, "Acid Stream", 1, true, 2, COM, "", "1 monster",
     "Sprays burning acid: 2-8 damage per level of caster unless immune."},
    {SO, 3, 2, "Fly", 3, false, 0, NON, "Outdoor", "Entire party",
     "Grants flight, moving the party to any other outdoor area."},
    {SO, 3, 3, "Invisibility", 3, false, 0, COM, "", "Entire party",
     "Cloaks all characters, greatly decreasing monsters' chance to hit."},
    {SO, 3, 4, "Lightning Bolt", 1, true, 2, COM, "", "4 monsters",
     "A gigantic bolt inflicting 1-6 damage per level of caster."},
    {SO, 3, 5, "Web", 3, false, 2, COM, "not hand-to-hand", "4 monsters +1/L",
     "Wraps monsters in a web, preventing them from fighting."},
    {SO, 3, 6, "Wizard Eye", 3, true, 2, NON, "Indoor", "5 steps/L",
     "A 5x5 overhead view of the party's location in any maze."},
    // Level 4
    {SO, 4, 1, "Cold Beam", 1, true, 3, COM, "", "1 monster",
     "A beam of intense cold: 6 damage per level of caster unless immune."},
    {SO, 4, 2, "Feeble Mind", 4, false, 3, COM, "", "5 monsters",
     "Erases monsters' brains, removing their abilities until overcome."},
    {SO, 4, 3, "Fire Ball", 1, true, 3, COM, "not hand-to-hand", "6 monsters",
     "A deadly ball of fire: 1-6 damage per level of caster."},
    {SO, 4, 4, "Guard Dog", 4, false, 0, NON, "", "Entire party",
     "Places a supernatural guard preventing surprise attacks for 1 day."},
    {SO, 4, 5, "Shield", 4, false, 0, COM, "", "Entire party",
     "An invisible shield protecting from most missile weapons."},
    {SO, 4, 6, "Time Distortion", 4, false, 3, COM, "", "Entire party",
     "Warps time, letting the party retreat safely from most battles."},
    // Level 5
    {SO, 5, 1, "Disrupt", 5, false, 5, COM, "not hand-to-hand", "1 monster",
     "Disrupts the target's molecular bonds for 100 points of damage."},
    {SO, 5, 2, "Fingers of Death", 5, false, 5, COM, "", "3 monsters (not undead)",
     "Channels dead sorcerers' power, killing the monsters pointed at."},
    {SO, 5, 3, "Sand Storm", 2, true, 5, COM, "Outdoor", "10 monsters",
     "A violent sand storm: 1-8 damage per level of caster."},
    {SO, 5, 4, "Shelter", 5, false, 0, NON, "", "Entire party",
     "Provides 1 day's rest free of the danger of encounter."},
    {SO, 5, 5, "Teleport", 5, false, 0, NON, "", "Entire party",
     "Instantly moves the party up to 9 squares in any direction."},
    // Level 6
    {SO, 6, 1, "Disintegration", 6, false, 6, COM, "", "3 monsters",
     "Inflicts 50 damage while disintegrating parts or all of the target."},
    {SO, 6, 2, "Entrapment", 6, false, 6, COM, "", "All",
     "Surrounds the battle with an energy field, preventing escape."},
    {SO, 6, 3, "Fantastic Freeze", 2, true, 6, COM, "not hand-to-hand", "3 monsters",
     "A beam of cold crystallizing 3 monsters: 10 damage per level."},
    {SO, 6, 4, "Recharge Item", 6, false, 6, NON, "", "Spell caster",
     "Restores 1-6 charges to a backpack item; may fail and destroy it."},
    {SO, 6, 5, "Super Shock", 2, true, 6, COM, "", "1 monster",
     "An intense beam of electricity: 20 damage per level of caster."},
    // Level 7
    {SO, 7, 1, "Dancing Sword", 3, true, 7, COM, "", "10 monsters",
     "A magical sword: 1-12 damage per level of caster."},
    {SO, 7, 2, "Duplication", 7, false, 100, NON, "", "Spell caster",
     "Duplicates one backpack item; small chance to destroy the original."},
    {SO, 7, 3, "Etherealize", 7, false, 7, NON, "", "Entire party",
     "Lets all characters move 1 square forward through any barrier."},
    {SO, 7, 4, "Prismatic Light", 7, false, 7, COM, "", "10 monsters",
     "A powerful but erratic spell with completely unpredictable effects."},
    // Level 8
    {SO, 8, 1, "Incinerate", 3, true, 8, COM, "", "1 monster",
     "Engulfs a monster in fire: 20-40 damage per level of caster."},
    {SO, 8, 2, "Mega Volts", 3, true, 8, COM, "", "10 monsters",
     "A chain of electricity: 4-16 damage per level of caster."},
    {SO, 8, 3, "Meteor Shower", 8, false, 8, COM, "Outdoor", "All (SP-limited)",
     "Buries all monsters under meteors: 5-50 damage each. +1 SP/monster."},
    {SO, 8, 4, "Power Shield", 8, false, 8, COM, "", "Entire party",
     "Halves the damage inflicted on all characters for the combat."},
    // Level 9
    {SO, 9, 1, "Implosion", 10, false, 10, COM, "", "1 monster",
     "Creates a hole in space, sucking the target into nothingness."},
    {SO, 9, 2, "Inferno", 3, true, 10, COM, "", "10 monsters",
     "Unleashes the sun's heat: 1-20 damage per level of caster."},
    {SO, 9, 3, "Star Burst", 10, false, 20, COM, "Outdoor", "All (SP-limited)",
     "Showers monsters with star pieces: 20-200 damage. +1 SP/monster."},
    {SO, 9, 4, "Enchant Item", 50, false, 50, NON, "", "Spell caster",
     "Raises an item's '+' by 1. Costs 50 SP per plus of the item."},

    // ---- CLERIC ----
    // Level 1
    {CL, 1, 1, "Apparition", 1, false, 0, COM, "", "10 monsters",
     "A frightening apparition that reduces monsters' chance to hit."},
    {CL, 1, 2, "Awaken", 1, false, 0, ANY, "", "All sleeping party members",
     "Awakens all sleeping party members, cancelling the sleep condition."},
    {CL, 1, 3, "Bless", 1, false, 0, COM, "", "Entire party",
     "Increases the accuracy of all characters for the combat."},
    {CL, 1, 4, "First Aid", 1, false, 0, ANY, "", "1 character",
     "Heals minor wounds, restoring 8 Hit Points to a character."},
    {CL, 1, 5, "Light", 1, false, 0, NON, "", "Entire party",
     "Gives the party 1 light factor to light a dark area."},
    {CL, 1, 6, "Power Cure", 1, true, 1, ANY, "", "1 character",
     "Restores health and 1-10 Hit Points per experience level of caster."},
    {CL, 1, 7, "Turn Undead", 1, false, 0, COM, "", "All undead monsters",
     "Destroys some or all undead, depending on caster vs monster level."},
    // Level 2
    {CL, 2, 1, "Cure Wounds", 2, false, 0, ANY, "", "1 character",
     "Cures serious wounds, restoring 15 Hit Points to the character."},
    {CL, 2, 2, "Heroism", 2, false, 1, COM, "", "1 character",
     "Temporarily elevates a character 6 experience levels for combat."},
    {CL, 2, 3, "Nature's Gate", 2, false, 0, NON, "Outdoor", "Entire party",
     "Opens a portal between two locations in Cron (varies with time)."},
    {CL, 2, 4, "Pain", 2, false, 0, COM, "", "1 monster (not undead)",
     "Cripples a monster with 2-16 damage unless immune to pain."},
    {CL, 2, 5, "Protection From Elements", 2, false, 1, ANY, "", "Entire party",
     "Raises resistance to fear, cold, fire, poison, acid, electricity 1 day."},
    {CL, 2, 6, "Silence", 2, false, 0, COM, "", "4 monsters +1/L",
     "Prevents monsters from casting spells until they overcome it."},
    {CL, 2, 7, "Weaken", 2, false, 1, COM, "", "10 monsters",
     "Halves affected monsters' physical damage until overcome."},
    // Level 3
    {CL, 3, 1, "Cold Ray", 3, false, 2, COM, "not hand-to-hand", "5 monsters",
     "A ray of intense cold for 25 points of damage to each monster."},
    {CL, 3, 2, "Create Food", 3, false, 2, NON, "", "Spell caster",
     "Adds 8 food units to the caster's supply for distribution."},
    {CL, 3, 3, "Cure Poison", 3, false, 0, ANY, "", "1 character",
     "Flushes poison out, removing the poisoned condition."},
    {CL, 3, 4, "Immobilize", 3, false, 0, COM, "", "5 monsters",
     "Immobilizes any monster affected."},
    {CL, 3, 5, "Lasting Light", 3, false, 0, NON, "", "Entire party",
     "Bestows 20 light factors on the party for dispelling darkness."},
    {CL, 3, 6, "Walk on Water", 3, false, 2, NON, "Outdoor", "Entire party",
     "Creates a floating sand dune to walk on. Lasts 1 day."},
    // Level 4
    {CL, 4, 1, "Acid Spray", 4, false, 3, COM, "not hand-to-hand", "3 monsters",
     "A corrosive stream of acid: 6-60 damage unless immune."},
    {CL, 4, 2, "Air Transmutation", 4, false, 3, NON, "Outdoor", "Entire party",
     "Transforms the party into air to explore the plane of air."},
    {CL, 4, 3, "Cure Disease", 4, false, 0, ANY, "", "1 character",
     "Restores full health to a sick character, removing disease."},
    {CL, 4, 4, "Restore Alignment", 4, false, 3, NON, "", "1 character",
     "Restores a character's original alignment after it has shifted."},
    {CL, 4, 5, "Surface", 4, false, 0, NON, "", "Entire party",
     "Instantly transports the party from underground to the surface."},
    {CL, 4, 6, "Holy Bonus", 4, false, 3, COM, "", "Entire party",
     "Increases party damage by 1 point per 2 levels of the caster."},
    // Level 5
    {CL, 5, 1, "Air Encasement", 5, false, 5, COM, "", "1 monster",
     "Encases a monster in air: 10 damage/round, separated from battle."},
    {CL, 5, 2, "Deadly Swarm", 5, false, 5, COM, "", "10 monsters",
     "A swarm of killer insects: 4-40 damage against each monster."},
    {CL, 5, 3, "Frenzy", 5, false, 5, COM, "", "1 character (once each)",
     "Sends a party member into a frenzy to attack all monsters."},
    {CL, 5, 4, "Paralyze", 5, false, 5, COM, "", "10 monsters",
     "Attempts to immobilize all monsters and prevent them fighting."},
    {CL, 5, 5, "Remove Condition", 5, false, 5, ANY, "", "1 character",
     "Releases a character from all conditions except dead/stoned/eradicated."},
    // Level 6
    {CL, 6, 1, "Earth Transmutation", 6, false, 6, NON, "Outdoor", "Entire party",
     "Transforms the party into earth to explore the plane of earth."},
    {CL, 6, 2, "Rejuvenate", 6, false, 6, NON, "", "1 character",
     "Trims 1-10 years off a character's age; some risk of the opposite."},
    {CL, 6, 3, "Stone to Flesh", 6, false, 6, ANY, "", "1 character",
     "Re-animates a character turned to stone, removing the stoned condition."},
    {CL, 6, 4, "Water Encasement", 6, false, 6, COM, "", "1 monster",
     "Encases a monster in water: 20 damage/round, separated from battle."},
    {CL, 6, 5, "Water Transmutation", 6, false, 6, NON, "Outdoor", "Entire party",
     "Transforms the party into water to explore the plane of water."},
    // Level 7
    {CL, 7, 1, "Earth Encasement", 7, false, 7, COM, "", "1 monster",
     "Encases a monster in earth: 40 damage/round, separated from battle."},
    {CL, 7, 2, "Fiery Flail", 7, false, 7, COM, "", "1 monster",
     "A huge flail of fire striking a single opponent for 100-400 damage."},
    {CL, 7, 3, "Moon Ray", 7, false, 7, COM, "Outdoor", "10 monsters",
     "Bestows 10-100 HP on each character and removes 10-100 from monsters."},
    {CL, 7, 4, "Raise Dead", 7, false, 7, ANY, "", "1 character",
     "Brings a character back to life; moderate failure chance, ages 1 year."},
    // Level 8
    {CL, 8, 1, "Fire Encasement", 8, false, 8, COM, "", "1 monster",
     "Encases a monster in fire: 80 damage/round, separated from battle."},
    {CL, 8, 2, "Fire Transmutation", 8, false, 8, NON, "Outdoor", "Entire party",
     "Transforms the party into fire to explore the plane of fire."},
    {CL, 8, 3, "Mass Distortion", 8, false, 8, COM, "", "2 monsters",
     "Increases monsters' weight, making them fall and lose half their HP."},
    {CL, 8, 4, "Town Portal", 8, false, 8, NON, "", "Entire party",
     "Opens a temporary portal and moves the party to any town."},
    // Level 9
    {CL, 9, 1, "Divine Intervention", 10, false, 20, COM, "", "Entire party",
     "Restores all HP and removes all conditions except eradicated; ages 5 yr."},
    {CL, 9, 2, "Holy Word", 10, false, 10, COM, "", "All",
     "A word of devastating power destroying all undead. Ages caster 1 year."},
    {CL, 9, 3, "Resurrection", 10, false, 10, NON, "", "1 character",
     "Removes the eradicated condition; ages 5 years, -1 endurance, may fail."},
    {CL, 9, 4, "Uncurse Item", 10, false, 50, NON, "", "Spell caster",
     "Attempts to remove the curse from an item in the caster's backpack."},
};
#undef SO
#undef CL
#undef ANY
#undef COM
#undef NON

namespace {
// Non-spell stat-boost kinds for effect bytes 0x01..0x7F (hi nibble).
const char* const kEffectBoostKinds[7] = {
    "Max HP", "Might", "Speed", "Accuracy", "?", "Level", "Spell Level"};

int schoolBase(SpellSchool school) {
    return (school == SpellSchool::Sorcerer) ? 0 : kSpellsPerSchool;
}
}  // namespace

const SpellInfo* spellInfo(SpellSchool school, int flat) {
    if (flat < 1 || flat > kSpellsPerSchool) return nullptr;
    return &kSpells[schoolBase(school) + flat - 1];
}

const char* spellName(SpellSchool school, int flat) {
    const SpellInfo* s = spellInfo(school, flat);
    return s ? s->name : nullptr;
}

bool spellLevelNumber(int flat, int& level, int& number) {
    if (flat < 1 || flat > kSpellsPerSchool) return false;
    int rem = flat - 1;
    for (int lv = 0; lv < kSpellLevels; ++lv) {
        if (rem < kSpellsPerLevel[lv]) {
            level = lv + 1;
            number = rem + 1;
            return true;
        }
        rem -= kSpellsPerLevel[lv];
    }
    return false;
}

const char* spellName(SpellSchool school, int level, int number) {
    if (level < 1 || level > kSpellLevels || number < 1) return nullptr;
    int flat = 0;
    for (int lv = 0; lv < level - 1; ++lv) flat += kSpellsPerLevel[lv];
    if (number > kSpellsPerLevel[level - 1]) return nullptr;
    return spellName(school, flat + number);
}

std::string formatSpellCost(const SpellInfo& s) {
    char buf[48];
    if (s.perLevel)
        std::snprintf(buf, sizeof(buf), "%d/L", s.sp);
    else
        std::snprintf(buf, sizeof(buf), "%d SP", s.sp);
    std::string out = buf;
    if (s.gems > 0) {
        std::snprintf(buf, sizeof(buf), " + %d Gem%s", s.gems, s.gems == 1 ? "" : "s");
        out += buf;
    }
    return out;
}

ItemEffect decodeItemEffect(uint8_t b) {
    ItemEffect e;
    if (b == 0) {
        e.kind = ItemEffect::Kind::None;
        return e;
    }
    if (b < 0x80) {
        e.kind = ItemEffect::Kind::Boost;
        int kind = b >> 4;
        e.boostName = (kind < 7) ? kEffectBoostKinds[kind] : "unknown";
        e.amount = b & 0x0F;
        return e;
    }
    e.kind = ItemEffect::Kind::Spell;
    int flat;
    if (b <= 0xB0) {
        e.school = SpellSchool::Sorcerer;
        flat = b - 0x80;
    } else {
        e.school = SpellSchool::Cleric;
        flat = b - 0xB0;
    }
    spellLevelNumber(flat, e.level, e.number);
    e.spell = spellName(e.school, flat);
    return e;
}

std::string describeItemEffect(uint8_t b) {
    ItemEffect e = decodeItemEffect(b);
    char buf[64];
    switch (e.kind) {
        case ItemEffect::Kind::None:
            return "none";
        case ItemEffect::Kind::Boost:
            std::snprintf(buf, sizeof(buf), "%s +%d", e.boostName, e.amount);
            return buf;
        case ItemEffect::Kind::Spell: {
            char school = (e.school == SpellSchool::Sorcerer) ? 'S' : 'C';
            if (e.spell)
                std::snprintf(buf, sizeof(buf), "%c%d/%d %s", school, e.level,
                              e.number, e.spell);
            else
                std::snprintf(buf, sizeof(buf), "%c#%d (?)", school, (b & 0x7F));
            return buf;
        }
    }
    return "none";
}

}  // namespace mm2

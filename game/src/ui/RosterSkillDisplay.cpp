#include "mm2/ui/RosterSkillDisplay.h"

#include "mm2/gameplay/RosterSkills.h"
#include "mm2/CppStdCompat.h"

namespace mm2::ui {

namespace {

/* A4-$8720 title/skill strings used by LAB_38EA fields $14/$15. */
const char *kSkillNames[] = {
    kRosterEmptySkillSlot,
    "Arms Master",
    "Athlete",
    "Cartographer",
    "Crusader",
    "Diplomat",
    "Gambler",
    "Gladiator",
    "Hero/Heroine",
    "Linguist",
    "Merchant",
    "Mountaineer",
    "Navigator",
    "Pathfinder",
    "PickPocket",
    "Soldier",
};

struct HirelingMeta {
    const char *skills; /* space-separated FAQ abbreviations, or nullptr */
};

// FAQ §3-4-1: hireling preset skills (A–X). Daily fee is roster +$66.
const HirelingMeta kHirelingSkillMeta[] = {
    {nullptr}, {nullptr}, {nullptr},
    {"Cru"},   {"Car"},   {"Lin"},
    {"Ath"},   {"Arm"},   {"Cru"},
    {"Mou Pat"}, {"Gam Pic"}, {"Dip Nav"},
    {"Lin Mer"}, {"Cru Sol"}, {"Arm Pat"},
    {"Dip Her"}, {"Cru Mer"}, {"Gla Sol"},
    {"Ath Gam"}, {"Arm Gla"}, {"Arm Mou"},
    {"Dip Gam"}, {"Gam Nav"}, {"Lin Mer"},
};

const char *skillNameFromId(uint8_t id)
{
    if (id <= 15) {
        return kSkillNames[id];
    }
    return nullptr;
}

const char *skillAbbrevToName(const char *abbrev)
{
    if (!abbrev || !abbrev[0]) {
        return nullptr;
    }
    if (std::strncmp(abbrev, "Arm", 3) == 0) {
        return "Arms Master";
    }
    if (std::strncmp(abbrev, "Ath", 3) == 0) {
        return "Athlete";
    }
    if (std::strncmp(abbrev, "Car", 3) == 0) {
        return "Cartographer";
    }
    if (std::strncmp(abbrev, "Cru", 3) == 0) {
        return "Crusader";
    }
    if (std::strncmp(abbrev, "Dip", 3) == 0) {
        return "Diplomat";
    }
    if (std::strncmp(abbrev, "Gam", 3) == 0) {
        return "Gambler";
    }
    if (std::strncmp(abbrev, "Gla", 3) == 0) {
        return "Gladiator";
    }
    if (std::strncmp(abbrev, "Her", 3) == 0) {
        return "Hero/Heroine";
    }
    if (std::strncmp(abbrev, "Lin", 3) == 0) {
        return "Linguist";
    }
    if (std::strncmp(abbrev, "Mer", 3) == 0) {
        return "Merchant";
    }
    if (std::strncmp(abbrev, "Mou", 3) == 0) {
        return "Mountaineer";
    }
    if (std::strncmp(abbrev, "Nav", 3) == 0) {
        return "Navigator";
    }
    if (std::strncmp(abbrev, "Pat", 3) == 0) {
        return "Pathfinder";
    }
    if (std::strncmp(abbrev, "Pic", 3) == 0) {
        return "PickPocket";
    }
    if (std::strncmp(abbrev, "Sol", 3) == 0) {
        return "Soldier";
    }
    return nullptr;
}

bool isRosterSkillTemplate(const Mm2RosterRecord &rec)
{
    if (gameplay::rosterSkillPackedByte(rec) != 0) {
        return false;
    }
    const uint8_t a = rec.secondary_skills[0];
    const uint8_t b = rec.secondary_skills[1];
    const uint8_t c = rec.secondary_skills[2];
    if (a != b || b != c) {
        return false;
    }
    return a == 0x05 || a == 0x0A;
}

}  // namespace

const char *rosterSheetSkillName(uint8_t id)
{
    const char *name = skillNameFromId(id);
    return name ? name : kRosterEmptySkillSlot;
}

uint8_t rosterDisplayThievery(const Mm2RosterRecord &rec, const Mm2ItemsFile *items)
{
    /* Character sheet @ $3C42 reads live +$1E. Remake keeps create/training in
     * +$1E and adds equipped type-14 here so pre-worn gear is counted. */
    return gameplay::rosterLiveThievery(rec, items);
}

int collectRosterSkillNames(const Mm2RosterRecord &rec, const char **names, int max_names)
{
    if (isRosterSkillTemplate(rec)) {
        return 0;
    }
    int count = 0;
    const uint8_t packed = gameplay::rosterSkillPackedByte(rec);
    const uint8_t ids[2] = {static_cast<uint8_t>(packed & 0x0F),
                            static_cast<uint8_t>((packed >> 4) & 0x0F)};
    for (uint8_t id : ids) {
        if (id == 0) {
            continue;
        }
        const char *name = skillNameFromId(id);
        if (!name) {
            continue;
        }
        bool dup = false;
        for (int j = 0; j < count; ++j) {
            if (std::strcmp(names[j], name) == 0) {
                dup = true;
                break;
            }
        }
        if (!dup && count < max_names) {
            names[count++] = name;
        }
    }
    return count;
}

int collectHirelingSkillNames(int hireling_index, const char **names, int max_names)
{
    if (hireling_index < 0 ||
        hireling_index >= static_cast<int>(sizeof(kHirelingSkillMeta) / sizeof(kHirelingSkillMeta[0]))) {
        return 0;
    }
    const char *skills = kHirelingSkillMeta[hireling_index].skills;
    if (!skills || !skills[0]) {
        return 0;
    }
    int count = 0;
    const char *p = skills;
    while (*p && count < max_names) {
        while (*p == ' ') {
            ++p;
        }
        if (!*p) {
            break;
        }
        const char *start = p;
        while (*p && *p != ' ') {
            ++p;
        }
        char abbrev[8];
        const std::size_t len = static_cast<std::size_t>(p - start);
        if (len >= sizeof(abbrev)) {
            continue;
        }
        ::memcpy(abbrev, start, len);
        abbrev[len] = '\0';
        const char *name = skillAbbrevToName(abbrev);
        if (name) {
            names[count++] = name;
        }
    }
    return count;
}

}  // namespace mm2::ui

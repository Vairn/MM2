// Offline checks for remake developer menu grant helpers.

#include "mm2/gameplay/DevMenuGrants.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

#include <cstdio>
#include <cstring>

namespace {

bool expect(bool cond, const char *msg, int &fails)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++fails;
        return false;
    }
    return true;
}

void setupParty(Mm2RosterFile &roster, Mm2PartyLaunch &launch)
{
    std::memset(&roster, 0, sizeof(roster));
    std::memset(&launch, 0, sizeof(launch));
    launch.party_count = 2;
    launch.roster_slots[0] = 0;
    launch.roster_slots[1] = 1;
    for (int i = 2; i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        launch.roster_slots[i] = -1;
    }
    roster.records[0].class_id = 0; /* Knight */
    roster.records[0].experience = 100;
    roster.records[0].gold = 50;
    roster.records[0].gems = 10;
    roster.records[0].hp_aux = 20;
    roster.records[0].hp_max = 5;
    roster.records[0].hp_current = 0;
    roster.records[0].condition = 0x40;
    roster.records[0].might_base = 10;
    roster.records[0].might_current = 10;
    roster.records[0].food = 1;
    roster.records[0].sp_max = 0;
    roster.records[0].sp_current = 0;

    roster.records[1].class_id = 4; /* Sorcerer */
    roster.records[1].experience = 0;
    roster.records[1].gold = 0;
    roster.records[1].gems = 65000;
    roster.records[1].hp_aux = 12;
    roster.records[1].hp_max = 12;
    roster.records[1].hp_current = 3;
    roster.records[1].condition = 0x81;
    roster.records[1].spell_level = 1;
    roster.records[1].sp_max = 8;
    roster.records[1].sp_current = 0;
    roster.records[1].might_base = 250;
    roster.records[1].might_current = 250;
}

}  // namespace

int main()
{
    int fails = 0;
    Mm2RosterFile roster{};
    Mm2PartyLaunch launch{};
    setupParty(roster, launch);

    expect(mm2::gameplay::grantAmountPresetCount() >= 6, "grant presets exist", fails);
    expect(mm2::gameplay::grantAmountPreset(5) == 2000000u, "preset idx5 = 2M", fails);
    expect(mm2::gameplay::statBoostPreset(3) == 20u, "stat preset +20", fails);

    expect(mm2::gameplay::grantXpAll(roster, launch, 2000000u) == 2, "xp grant count", fails);
    expect(roster.records[0].experience == 2000100u, "xp member0", fails);
    expect(roster.records[1].experience == 2000000u, "xp member1", fails);

    expect(mm2::gameplay::grantGoldAll(roster, launch, 1000000u) == 2, "gold grant count", fails);
    expect(roster.records[0].gold == 1000050u, "gold member0", fails);

    /* Hireling gold is daily fee — Add gold must skip roster index >= 0x18. */
    launch.party_count = 3;
    launch.roster_slots[2] = 24;
    roster.records[24].gold = 100; /* hireling daily fee */
    expect(mm2::gameplay::grantGoldAll(roster, launch, 5000u) == 2, "gold skips hireling", fails);
    expect(roster.records[24].gold == 100u, "hireling fee unchanged", fails);
    expect(roster.records[0].gold == 1005050u, "PC still gets gold with hireling in party", fails);
    launch.party_count = 2;
    launch.roster_slots[2] = -1;

    expect(mm2::gameplay::grantGemsAll(roster, launch, 1000u) == 2, "gems grant count", fails);
    expect(roster.records[0].gems == 1010u, "gems member0", fails);
    expect(roster.records[1].gems == 65535u, "gems saturate at 65535", fails);

    expect(mm2::gameplay::boostStatsAll(roster, launch, 20u) == 2, "stat boost count", fails);
    expect(roster.records[0].might_base == 10 && roster.records[0].might_current == 30,
           "temp +20 current only (base unchanged)", fails);
    expect(roster.records[1].might_base == 250 && roster.records[1].might_current == 255,
           "temp stat saturates at 255; base untouched", fails);

    expect(mm2::gameplay::healReviveAll(roster, launch) == 2, "heal count", fails);
    expect(roster.records[0].condition == 0 && roster.records[0].hp_current == 20 &&
               roster.records[0].hp_max == 20,
           "heal restores knight", fails);
    expect(roster.records[1].condition == 0 && roster.records[1].sp_current == 8,
           "heal clears dead sorcerer SP", fails);

    expect(mm2::gameplay::fillFoodAll(roster, launch) == 2, "food count", fails);
    expect(roster.records[0].food == 40, "food filled", fails);

    expect(mm2::gameplay::maxSpellsAll(roster, launch) == 1, "max spells only casters", fails);
    expect(roster.records[1].spell_level == 10, "sorcerer spell level 10", fails);
    expect(mm2::gameplay::spellKnownInBook(roster.records[1], 0) &&
               mm2::gameplay::spellKnownInBook(roster.records[1], 47),
           "full spell book learned", fails);
    expect(mm2::gameplay::kSorcererSpells[2].level == 1 && mm2::gameplay::kSorcererSpells[2].number == 3 &&
               std::strcmp(mm2::gameplay::kSorcererSpells[2].name, "Energy Blast") == 0,
           "spell list 1-3 is Energy Blast", fails);
    expect(mm2::gameplay::knownSpellCount(roster.records[1], mm2::gameplay::SpellSchool::Sorcerer) ==
               mm2::gameplay::kSpellsPerSchool,
           "max spells fills school list", fails);

    int x = -1, y = -1;
    mm2::gameplay::decodeTripletPos(0x47, &x, &y);
    expect(x == 7 && y == 4, "triplet pos (y<<4)|x decode", fails);

    expect(mm2::gameplay::clampDevMonsterCount(0) == 1, "count min 1", fails);
    expect(mm2::gameplay::clampDevMonsterCount(99) == MM2_GS_MONSTER_SLOT_COUNT,
           "count max slots", fails);
    expect(mm2::gameplay::clampDevMonsterType(-3) == 0, "type min 0", fails);
    expect(mm2::gameplay::clampDevMonsterType(300) == 255, "type max 255", fails);
    expect(mm2::gameplay::clampDevScreenId(-1) == 0, "screen min 0", fails);
    expect(mm2::gameplay::clampDevScreenId(99) == 59, "screen max 59", fails);

    expect(mm2::gameplay::clampDevBattleDifficulty(-1) == 0, "diff min Easy", fails);
    expect(mm2::gameplay::clampDevBattleDifficulty(9) == 3, "diff max Deadly", fails);
    expect(std::strcmp(mm2::gameplay::devBattleDifficultyName(2), "Hard") == 0, "Hard name", fails);

    uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};
    mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
    mm2::gameplay::seedDevRandomEncounter(gs);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE) == 0, "random mode 0", fails);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT) == 0, "random live 0", fails);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS) == 0, "random slot0 empty", fails);

    mm2::gameplay::seedDevFixedEncounter(gs, 7, 3);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE) == 0x80, "fixed mode 0x80", fails);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT) == 3, "fixed live 3", fails);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + 0) == 7 &&
               mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + 1) == 7 &&
               mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + 2) == 7 &&
               mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + 3) == 0,
           "fixed fills 3 slots", fails);

    uint8_t attrib_raw[64]{};
    attrib_raw[mm2::gameplay::kDevAttribOffGroupGate] = 4;
    attrib_raw[mm2::gameplay::kDevAttribOffMaxMon] = 3;
    attrib_raw[mm2::gameplay::kDevAttribOffMinMon] = 1;
    mm2_gs_set_u8(gs.a4(), MM2_GS_DISPOSITION, 2);
    mm2::gameplay::DevEncounterDiffPatch patch{};
    mm2::gameplay::applyDevRandomDifficulty(gs, attrib_raw, 1, &patch);
    expect(!patch.applied && mm2_gs_u8(gs.a4(), MM2_GS_DISPOSITION) == 2,
           "Normal difficulty is a no-op", fails);
    expect(attrib_raw[mm2::gameplay::kDevAttribOffMaxMon] == 3, "Normal leaves attrib max", fails);

    mm2::gameplay::applyDevRandomDifficulty(gs, attrib_raw, 0, &patch);
    expect(patch.applied && mm2_gs_u8(gs.a4(), MM2_GS_DISPOSITION) == 0, "Easy sets disp 0", fails);
    expect(attrib_raw[mm2::gameplay::kDevAttribOffMaxMon] == 2 &&
               attrib_raw[mm2::gameplay::kDevAttribOffMinMon] == 1 &&
               attrib_raw[mm2::gameplay::kDevAttribOffGroupGate] == 3,
           "Easy caps picker tiers", fails);
    mm2::gameplay::restoreDevRandomDifficulty(gs, attrib_raw, patch);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_DISPOSITION) == 2, "restore disposition", fails);
    expect(attrib_raw[mm2::gameplay::kDevAttribOffMaxMon] == 3 &&
               attrib_raw[mm2::gameplay::kDevAttribOffGroupGate] == 4,
           "restore attrib tuning", fails);

    mm2::gameplay::applyDevRandomDifficulty(gs, attrib_raw, 3, &patch);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_DISPOSITION) == 3, "Deadly sets disp 3", fails);
    expect(attrib_raw[mm2::gameplay::kDevAttribOffMinMon] == 8 &&
               attrib_raw[mm2::gameplay::kDevAttribOffMaxMon] == 14 &&
               attrib_raw[mm2::gameplay::kDevAttribOffGroupGate] == 11,
           "Deadly opens high tiers", fails);
    mm2::gameplay::restoreDevRandomDifficulty(gs, attrib_raw, patch);

    /* Saturating XP near UINT32_MAX */
    roster.records[0].experience = 0xFFFFFFF0u;
    mm2::gameplay::grantXpAll(roster, launch, 100u);
    expect(roster.records[0].experience == 0xFFFFFFFFu, "xp saturates UINT32_MAX", fails);

    if (fails == 0) {
        std::printf("dev_menu_test: OK\n");
        return 0;
    }
    std::fprintf(stderr, "dev_menu_test: %d failure(s)\n", fails);
    return 1;
}

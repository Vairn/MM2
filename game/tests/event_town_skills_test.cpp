// Skill-buy smoke tests for all five towns (event.dat + OP_0E overlay selectors).
// Usage: event_town_skills_test <data_dir>

#include <cstdio>
#include <cstring>

#include "mm2/GameState.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/gameplay/RosterSkills.h"
#include "mm2/platform/Platform.h"
#include "mm2/world/MapWorld.h"

#include "mm2_gamestate.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace {

bool expect(bool ok, const char *msg, int &fails)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++fails;
    }
    return ok;
}

void setupMember(Mm2RosterFile &roster, int idx, uint32_t gold)
{
    Mm2RosterRecord &rec = roster.records[idx];
    std::memset(&rec, 0, sizeof(rec));
    mm2_roster_set_name(&rec, "Tester");
    rec.level = 5;
    rec.gold = gold;
    rec.condition = 0;
    rec.class_id = 0;
}

struct SkillCase {
    int map_id;
    int x;
    int y;
    char facing;
    uint8_t expect_skill_id;
    const char *prompt_snippet;
};

bool runOverlayBuy(mm2::events::EventRuntime &runtime, mm2::GameStateView &gs,
                   mm2::world::MapWorld &world, Mm2RosterFile &roster, Mm2PartyLaunch &launch,
                   const SkillCase &c, int &fails)
{
    setupMember(roster, 0, 50000);
    launch.party_count = 1;
    launch.roster_slots[0] = 0;
    runtime.bindParty(&roster, &launch);

    world.enterScreen(static_cast<uint8_t>(c.map_id));
    gs.setScreenId(static_cast<uint8_t>(c.map_id));
    runtime.enterLocation(c.map_id, gs, world);
    gs.setCoordX(static_cast<uint8_t>(c.x));
    gs.setCoordY(static_cast<uint8_t>(c.y));
    gs.setFacingKey(c.facing);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);

    char tag[128];
    std::snprintf(tag, sizeof(tag), "%s: scan fires", c.prompt_snippet);
    if (!expect(runtime.scanAndRun(gs, world), tag, fails)) {
        return false;
    }
    std::snprintf(tag, sizeof(tag), "%s: intro text", c.prompt_snippet);
    expect(runtime.textView().containsText(c.prompt_snippet), tag, fails);

    mm2::platform::KeyState yes{};
    yes.last_ascii = 'Y';
    runtime.continueInput(gs, world, yes);
    yes.last_ascii = '1';
    expect(!runtime.continueInput(gs, world, yes), "skill buy completes", fails);
    expect(mm2::gameplay::rosterHasSkillId(roster.records[0], c.expect_skill_id),
           "skill id at roster+0x50", fails);
    expect(roster.records[0].gold < 50000u, "skill buy deducted gold via OP_24", fails);
    runtime.bindParty(nullptr, nullptr);
    return true;
}

bool runInlineBuy(mm2::events::EventRuntime &runtime, mm2::GameStateView &gs,
                  mm2::world::MapWorld &world, Mm2RosterFile &roster, Mm2PartyLaunch &launch,
                  const SkillCase &c, int &fails)
{
    return runOverlayBuy(runtime, gs, world, roster, launch, c, fails);
}

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc >= 2) ? argv[1] : "..";
    int fails = 0;

    mm2::events::EventRuntime runtime;
    if (!runtime.load(data_dir)) {
        std::fprintf(stderr, "FAIL: load event.dat\n");
        return 1;
    }

    mm2::world::MapWorld world;
    if (!world.load(data_dir)) {
        std::fprintf(stderr, "FAIL: load map\n");
        return 1;
    }

    uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
    mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
    gs.initCalendarDefaults();

    Mm2RosterFile roster{};
    Mm2PartyLaunch launch{};

    /* Inline event.dat scripts (OP_15/18 on selector 0x6D -> roster+0x50). */
    runInlineBuy(runtime, gs, world, roster, launch,
                 SkillCase{0, 0, 15, 'W', 3, "Otto adds"}, fails);
    runInlineBuy(runtime, gs, world, roster, launch,
                 SkillCase{0, 3, 12, 'E', 11, "Sir Edmund"}, fails);
    runInlineBuy(runtime, gs, world, roster, launch,
                 SkillCase{0, 1, 9, 'W', 13, "forest travel"}, fails);
    runInlineBuy(runtime, gs, world, roster, launch,
                 SkillCase{1, 6, 2, 'S', 2, "Olympic manner"}, fails);
    runInlineBuy(runtime, gs, world, roster, launch,
                 SkillCase{1, 8, 4, 'N', 9, "Odysseus"}, fails);
    runInlineBuy(runtime, gs, world, roster, launch,
                 SkillCase{2, 2, 14, 'W', 4, "crusade"}, fails);
    runInlineBuy(runtime, gs, world, roster, launch,
                 SkillCase{2, 5, 13, 'N', 10, "haggle"}, fails);

    /* OP_0E overlay skill vendors — real loc 62/63 bytecode (not EventSkillBuy). */
    runOverlayBuy(runtime, gs, world, roster, launch,
                  SkillCase{2, 8, 13, 'S', 12, "sextant"}, fails);
    runOverlayBuy(runtime, gs, world, roster, launch,
                  SkillCase{3, 0, 3, 'N', 15, "Sgt. Stephen"}, fails);
    runOverlayBuy(runtime, gs, world, roster, launch,
                  SkillCase{3, 3, 9, 'S', 7, "Spartacus"}, fails);
    runOverlayBuy(runtime, gs, world, roster, launch,
                  SkillCase{3, 15, 3, 'N', 1, "ancient secrets"}, fails);
    runOverlayBuy(runtime, gs, world, roster, launch,
                  SkillCase{4, 3, 4, 'E', 5, "diplomacy"}, fails);
    /* Sandsobar Sly (0,5)/W: class-gated Robber/Ninja — set class before buy. */
    {
        setupMember(roster, 0, 50000);
        roster.records[0].class_id = 5; /* Robber */
        launch.party_count = 1;
        launch.roster_slots[0] = 0;
        runtime.bindParty(&roster, &launch);
        world.enterScreen(4);
        gs.setScreenId(4);
        runtime.enterLocation(4, gs, world);
        gs.setCoordX(0);
        gs.setCoordY(5);
        gs.setFacingKey('W');
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
        expect(runtime.scanAndRun(gs, world), "pickpocket: scan fires", fails);
        expect(runtime.textView().containsText("pickpocket"), "pickpocket: intro text", fails);
        mm2::platform::KeyState yes{};
        yes.last_ascii = 'Y';
        runtime.continueInput(gs, world, yes);
        yes.last_ascii = '1';
        expect(!runtime.continueInput(gs, world, yes), "pickpocket completes", fails);
        expect(mm2::gameplay::rosterHasSkillId(roster.records[0], 14),
               "pickpocket skill id 14", fails);
        expect(roster.records[0].gold < 50000u, "pickpocket deducted gold", fails);
        runtime.bindParty(nullptr, nullptr);
    }
    runOverlayBuy(runtime, gs, world, roster, launch,
                  SkillCase{4, 3, 0, 'E', 6, "Lucky Spade"}, fails);

    /* Atlantium Hero: inline y/n + member select, then OP_0E 0x52 → loc 63 qid 7.
     * Bytecode ORs skill nibble 0x08 (not FAQ id 7). */
    {
        setupMember(roster, 0, 50000);
        launch.party_count = 1;
        launch.roster_slots[0] = 0;
        runtime.bindParty(&roster, &launch);
        world.enterScreen(1);
        gs.setScreenId(1);
        runtime.enterLocation(1, gs, world);
        gs.setCoordX(9);
        gs.setCoordY(2);
        gs.setFacingKey('S');
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
        expect(runtime.scanAndRun(gs, world), "Atlantium Hero evt 29 fires", fails);
        expect(runtime.textView().containsText("Hero"), "Hero intro", fails);
        mm2::platform::KeyState yes{};
        yes.last_ascii = 'Y';
        runtime.continueInput(gs, world, yes);
        yes.last_ascii = '1';
        expect(!runtime.continueInput(gs, world, yes), "Hero skill completes", fails);
        expect(mm2::gameplay::rosterHasSkillId(roster.records[0], 8),
               "Hero skill id 8 at roster+0x50", fails);
        runtime.bindParty(nullptr, nullptr);
    }

    if (fails == 0) {
        std::printf("OK: event_town_skills_test (15 skills)\n");
        return 0;
    }
    return 1;
}

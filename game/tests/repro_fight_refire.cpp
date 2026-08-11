// Reproduction: after winning a fixed triplet OP_12 fight, does the post-combat
// re-scan re-fire the same fight? Bug report: "got out of combat and all events
// are now combat" / "monsters keep spawning at the same spot after I won".
//
// Usage: repro_fight_refire <data_dir>
#include <cstdio>
#include <cstring>

#include "mm2/GameState.h"
#include "mm2/combat/CombatSession.h"
#include "mm2/events/EventCombatEncounter.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2/world/MapWorld.h"

#include "mm2_map_codec.h"
#include "mm2_monsters_codec.h"
#include "mm2_party_launch.h"
#include "mm2_gamestate.h"

static int fails = 0;

static bool expect(bool cond, const char *msg)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++fails;
        return false;
    }
    std::printf("ok: %s\n", msg);
    return true;
}

static void setupMember(Mm2RosterFile &r, int idx, int lvl, int gold)
{
    Mm2RosterRecord &rec = r.records[idx];
    rec.hp_current = 200;
    rec.condition = 0;
    rec.gold = static_cast<uint32_t>(gold);
    rec.experience = static_cast<uint32_t>(lvl * 100);
}

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : "../..";

    /* Synthetic monsters DB: every type is 1 HP / 0 speed so one party hit wins. */
    Mm2MonstersFile monsters{};
    for (int i = 0; i < MM2_MONSTER_RECORD_COUNT; ++i) {
        monsters.records[i].fields[MM2_MON_OFF_HP - MM2_MONSTER_NAME_SIZE] = 0x00;
        monsters.records[i].fields[MM2_MON_OFF_SPEED - MM2_MONSTER_NAME_SIZE] = 0x00;
    }

    mm2::world::MapWorld world;
    if (!expect(world.load(data_dir) && world.enterScreen(17), "load map + screen 17")) {
        return 1;
    }

    uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
    mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
    gs.initCalendarDefaults();

    mm2::events::EventRuntime runtime;
    if (!expect(runtime.load(data_dir), "event.dat loads")) {
        return 1;
    }

    Mm2RosterFile roster{};
    setupMember(roster, 0, 5, 100);
    roster.records[0].might_current = 250;
    roster.records[0].speed_current = 250;
    roster.records[0].hp_current = 100000;
    roster.records[0].hp_max = 100000;
    roster.records[0].condition = 0;
    Mm2PartyLaunch launch{};
    launch.party_count = 1;
    launch.roster_slots[0] = 0;
    mm2::gameplay::Rng rng(42);
    mm2::combat::CombatSession combat;
    combat.bindParty(&roster, &launch);
    combat.bindMonsters(&monsters);
    combat.bindRng(&rng);
    runtime.bindParty(&roster, &launch);
    runtime.bindCombat(&combat);

    if (!expect(runtime.enterLocation(17, gs, world), "enterLocation(17)")) {
        return 1;
    }
    gs.setScreenId(17);
    gs.setCoordX(2);
    gs.setCoordY(1);
    gs.setFacingKey('N');

    /* --- Scan 1: fire the fight --- */
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    const bool fired1 = runtime.scanAndRun(gs, world);
    expect(fired1, "scan 1 fires the triplet fight");
    expect(combat.active(), "scan 1 starts combat");
    const uint8_t coll_before = mm2_map_collision_at(&world.mapFileMut().screens[world.currentScreen()], 1, 2);
    std::printf("  collision (1,2) after scan1 = 0x%02X\n", coll_before);
    std::printf("  TILE_RT_FLAGS=%02X TILE_VISITED[34]=%02X latch=%d\n",
                (int)mm2_gs_u8(gs.a4(), MM2_GS_TILE_RT_FLAGS),
                (int)mm2_gs_u8(gs.a4(), MM2_GS_TILE_VISITED + 34),
                (int)mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH));

    /* --- Simulate victory: latched by CombatSession.enter->finishVictory.
     * Drive to a win via the standard attack/ack sequence. The monster is 1 HP
     * so a single party Attack should end it; drive with keys + ack. */
    mm2::platform::KeyState keys{};
    int guard = 0;
    while (combat.active() && guard < 300) {
        keys = mm2::platform::KeyState{};
        switch (combat.state()) {
        case mm2::combat::CombatState::AwaitingSurpriseDismiss:
        case mm2::combat::CombatState::AwaitingPartyOptions:
            keys.last_ascii = 'A';
            break;
        case mm2::combat::CombatState::AwaitingCommand:
            keys.last_ascii = 'A';
            break;
        case mm2::combat::CombatState::AwaitingAttackTarget:
            keys.last_ascii = 'A';
            break;
        case mm2::combat::CombatState::AwaitingVictoryDismiss:
            keys.space = true;
            break;
        default:
            keys.space = true; /* ack any pending message queue */
            break;
        }
        const bool ended = combat.tick(gs, world, keys);
        if (ended) {
            break;
        }
        ++guard;
    }
    std::printf("  combat active after drive = %d, outcome=%d\n", (int)combat.active(),
                (int)combat.lastOutcome());
    if (combat.lastOutcome() != mm2::combat::CombatOutcome::Victory) {
        std::fprintf(stderr, "NOTE: could not force victory (outcome=%d) - cannot verify re-fire\n",
                     (int)combat.lastOutcome());
    }
    expect(combat.lastOutcome() == mm2::combat::CombatOutcome::Victory,
           "drive to victory outcome");

    /* --- Re-scan (post-combat latch). Do NOT manually set the victory latch:
     * it must still be set from finishVictory via the real combat->tick path. --- */
    std::printf("  victory latch after drive = %d\n",
                (int)mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH));
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    const bool fired2 = runtime.scanAndRun(gs, world);
    std::printf("  re-scan fired=%d combat_active=%d\n", (int)fired2, (int)combat.active());
    expect(!combat.active(), "re-scan must NOT re-start combat after victory (no re-fire)");

    /* --- Revisit-after-move: the victory latch is cleared on the next step
     * (Movement.cpp latchExploreEventsAfterMove). If the persistent collision
     * event bit on the fight tile is NOT cleared (OP_14 gap), returning to the
     * tile re-matches the triplet and re-fires. This models "monsters keep
     * spawning at the same spot after I won". --- */
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 0); /* next move away+back */
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    const bool fired3 = runtime.scanAndRun(gs, world);
    std::printf("  revisit re-scan fired=%d combat_active=%d\n",
                (int)fired3, (int)combat.active());
    expect(!combat.active(), "revisit after win must NOT re-fire the fight");

    std::printf(fails == 0 ? "\nPASS: repro_fight_refire\n" : "\nFAILS: %d\n", fails);
    return fails == 0 ? 0 : 1;
}

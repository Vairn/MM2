// Movement tests: Middlegate spawn (7,3,N) collision + turns; screen 2 dark tile.
//
// Passability uses the 0x9424 first gate: collision cell AND facing bundle
// hi (A4-$55D8) AND $55 (wall bits only) — darkness does not block.
//
// Usage: movement_middlegate_test <data_dir>

#include <cstdio>

#include "mm2/GameState.h"
#include "mm2/gameplay/Movement.h"
#include "mm2/world/MapWorld.h"

#include "mm2_party_launch.h"
#include "mm2/gamestate.h"
#include "mm2_gamestate.h"

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

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : "../..";
    int fails = 0;

    mm2::world::MapWorld world;
    if (!world.load(data_dir) || !world.enterScreen(0)) {
        std::fprintf(stderr, "FAIL: load map/attrib\n");
        return 1;
    }

    uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
    mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
    gs.initCalendarDefaults();

    Mm2PartyLaunch launch{};
    mm2_party_launch_build(&launch, 1, nullptr, 0);
    mm2_party_launch_apply(gs.a4(), &launch);

    expect(gs.coordX() == 7 && gs.coordY() == 3 && gs.facingKey() == 'N', "spawn (7,3,N)", fails);

    /* Turn right: N -> E, no position change (0x5838 CW). */
    const mm2::gameplay::MoveResult t1 = mm2::gameplay::turn(world, gs, true);
    expect(t1.turned && !t1.moved && gs.facingKey() == 'E', "turn right N->E", fails);
    expect(gs.coordX() == 7 && gs.coordY() == 3, "turn does not move", fails);

    /* Forward while facing S: 0x9424 first gate blocks at (7,3). */
    gs.setFacingKey('S');
    const mm2::gameplay::MoveResult s_block = mm2::gameplay::step(world, gs, true);
    expect(s_block.blocked && !s_block.moved, "step S from (7,3) blocked", fails);
    expect(gs.coordX() == 7 && gs.coordY() == 3, "blocked step unchanged", fails);

    /* Forward N: first gate open -> (7,4). */
    gs.setFacingKey('N');
    const mm2::gameplay::MoveResult s_n = mm2::gameplay::step(world, gs, true);
    expect(s_n.moved && !s_n.blocked, "step N from (7,3) succeeds", fails);
    expect(gs.coordX() == 7 && gs.coordY() == 4, "after step N at (7,4)", fails);

    /* Step west from (7,4): facing W bundle 0x03, cell 0x91 -> blocked. */
    gs.setFacingKey('W');
    const mm2::gameplay::MoveResult w_block = mm2::gameplay::step(world, gs, true);
    expect(w_block.blocked && gs.coordX() == 7 && gs.coordY() == 4, "step W from (7,4) blocked", fails);

    /* asm 0x574E: victory latch clears on successful step before tile events run. */
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 1);
    const mm2::gameplay::MoveResult s_n2 = mm2::gameplay::step(world, gs, true);
    expect(s_n2.moved && gs.coordY() == 5, "step N from (7,4) for latch clear", fails);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 0,
           "COMBAT_VICTORY_LATCH cleared on successful step", fails);

    /* Screen 2 (dungeon): cell (1,0)=0x66 has S-dark in E-facing bundle 0x30.
     * 0x9424 AND #$55 must not treat darkness as a wall; step E -> (2,0). */
    expect(world.enterScreen(2), "enter screen 2", fails);
    gs.setScreenId(2);
    gs.setCoordX(1);
    gs.setCoordY(0);
    gs.setFacingKey('E');
    gs.setLightFactor(5);
    const mm2::gameplay::MoveResult dark_step = mm2::gameplay::step(world, gs, true);
    expect(dark_step.moved && !dark_step.blocked, "dark bits do not block step E", fails);
    expect(gs.coordX() == 2 && gs.coordY() == 0, "after dark-passable step at (2,0)", fails);
    expect(gs.lightFactor() == 4, "dark destination drains light @ 0x69DC", fails);

    if (fails == 0) {
        std::printf("OK: movement_middlegate_test (13 checks)\n");
        return 0;
    }
    return 1;
}

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
#include "mm2_map_codec.h"

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

/* Door-lock facing rotation regression: the ASM reads the collision byte against
 * the -$55D8 bundle mask (N=0xC0 -> wall bit 0x40 = file W), the SAME rotation
 * mm2_map_facing_shift uses on the visual page. mm2_map_facing_wall_bit must be
 * kept in lock-step with mm2_map_facing_mask_hi & 0x55 (bash @ 0x9B88, clear_lock
 * @ 0x4B06). A cell with ONLY its file-W wall bit (0x40) set:
 *   - facing N must read it as a locked door
 *   - facing E/S/W must NOT read it as locked  */
void testDoorLockRotation(int &fails)
{
    Mm2MapScreen s{};
    Mm2MapScreen *sp = &s;
    /* x=3,y=5 collision cell = 0x40 (file W wall only). */
    s.collision[static_cast<size_t>(5 * MM2_MAP_GRID_DIM + 3)] = MM2_MAP_COLL_W_WALL;

    expect(mm2_map_door_locked_at(sp, 3, 5, 'N') != 0, "file-W wall bit reads locked facing N", fails);
    expect(mm2_map_door_locked_at(sp, 3, 5, 'E') == 0, "not locked facing E", fails);
    expect(mm2_map_door_locked_at(sp, 3, 5, 'S') == 0, "not locked facing S", fails);
    expect(mm2_map_door_locked_at(sp, 3, 5, 'W') == 0, "not locked facing W", fails);

    /* clear clears only the facing's rotated bit (N->file W). */
    mm2_map_clear_door_lock(sp, 3, 5, 'N');
    expect(mm2_map_collision_at(sp, 3, 5) == 0, "clear door lock N clears file-W bit", fails);

    /* Symmetric: a file-N wall bit (0x01) reads locked only facing W. */
    s.collision[static_cast<size_t>(5 * MM2_MAP_GRID_DIM + 3)] = MM2_MAP_COLL_N_WALL;
    expect(mm2_map_door_locked_at(sp, 3, 5, 'W') != 0, "file-N wall bit reads locked facing W", fails);
    expect(mm2_map_door_locked_at(sp, 3, 5, 'N') == 0, "not locked facing N (file-N bit)", fails);

    /* Full bundle masking: byte 0x44 = file W (bit6) + file E (bit2) walls. */
    s.collision[static_cast<size_t>(5 * MM2_MAP_GRID_DIM + 3)] = 0x44;
    expect(mm2_map_door_locked_at(sp, 3, 5, 'N') != 0, "0x44, locked facing N (file W bit set)", fails);
    expect(mm2_map_door_locked_at(sp, 3, 5, 'E') == 0, "0x44, not locked facing E (file S unset)", fails);
    expect(mm2_map_door_locked_at(sp, 3, 5, 'S') != 0, "0x44, locked facing S (file E bit set)", fails);
    expect(mm2_map_door_locked_at(sp, 3, 5, 'W') == 0, "0x44, not locked facing W (file N unset)", fails);
}

/* Outdoor 0x9424 skill/water override: screen 14 (surf $AA → env $0A). */
void testOutdoorTerrainSkills(mm2::world::MapWorld &world, mm2::GameStateView &gs, int &fails)
{
    expect(world.enterScreen(14), "enter outdoor screen 14", fails);
    gs.setScreenId(14);

    Mm2RosterFile roster{};
    Mm2PartyLaunch launch{};
    launch.party_count = 2;
    launch.roster_slots[0] = 0;
    launch.roster_slots[1] = 1;
    roster.records[0].condition = 0;
    roster.records[1].condition = 0;
    /* Non-empty name so rosterRecord() does not skip the slots. */
    roster.records[0].name[0] = 'A';
    roster.records[1].name[0] = 'B';

    auto pack_skills = [&](int slot0_lo, int slot0_hi, int slot1_lo, int slot1_hi) {
        reinterpret_cast<uint8_t *>(&roster.records[0])[0x50] =
            static_cast<uint8_t>((slot0_hi << 4) | (slot0_lo & 0x0F));
        reinterpret_cast<uint8_t *>(&roster.records[1])[0x50] =
            static_cast<uint8_t>((slot1_hi << 4) | (slot1_lo & 0x0F));
    };

    /* Mountain at (2,1): stand (2,0) facing N. Class 1 needs Mountaineering×2. */
    pack_skills(0, 0, 0, 0);
    gs.setCoordX(2);
    gs.setCoordY(0);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_WALK_WATER_FLAG, 0);
    mm2::gameplay::MoveResult m0 = mm2::gameplay::step(world, gs, true, &roster, &launch);
    expect(m0.blocked && m0.obstruction == mm2::gameplay::ObstructionMsg::Impassable,
           "mountain without skills → Impassable!", fails);
    expect(gs.coordX() == 2 && gs.coordY() == 0, "mountain block keeps coords", fails);

    pack_skills(0x0B, 0, 0x0B, 0); /* two Mountaineering nibbles across party */
    m0 = mm2::gameplay::step(world, gs, true, &roster, &launch);
    expect(m0.moved && !m0.blocked && gs.coordX() == 2 && gs.coordY() == 1,
           "two Mountaineering allow mountain step", fails);

    /* Forest at (4,0): stand (3,0) facing E. Class 3 needs Pathfinder×2. */
    pack_skills(0, 0, 0, 0);
    gs.setCoordX(3);
    gs.setCoordY(0);
    gs.setFacingKey('E');
    mm2::gameplay::MoveResult m1 = mm2::gameplay::step(world, gs, true, &roster, &launch);
    expect(m1.blocked && m1.obstruction == mm2::gameplay::ObstructionMsg::Impassable,
           "forest without skills → Impassable!", fails);

    pack_skills(0x0D, 0, 0x0D, 0);
    m1 = mm2::gameplay::step(world, gs, true, &roster, &launch);
    expect(m1.moved && !m1.blocked && gs.coordX() == 4 && gs.coordY() == 0,
           "two Pathfinder allow forest step", fails);

    /* Water at (6,8): stand (6,7) facing N. Env $0A → need Walk on Water. */
    pack_skills(0, 0, 0, 0);
    gs.setCoordX(6);
    gs.setCoordY(7);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_WALK_WATER_FLAG, 0);
    mm2::gameplay::MoveResult m2 = mm2::gameplay::step(world, gs, true, &roster, &launch);
    expect(m2.blocked && m2.obstruction == mm2::gameplay::ObstructionMsg::CantSwim,
           "water without Walk on Water → Can't swim!", fails);

    mm2_gs_set_u8(gs.a4(), MM2_GS_WALK_WATER_FLAG, 1);
    m2 = mm2::gameplay::step(world, gs, true, &roster, &launch);
    expect(m2.moved && !m2.blocked && gs.coordX() == 6 && gs.coordY() == 8,
           "Walk on Water allows water step", fails);
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

    /* asm 0x574E/0x5748 tail: after a successful step the game loop runs the
     * encounter check and then latchExploreEventsAfterMove, which clears the
     * victory latch before tile events scan (doc 43 loop order). */
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 1);
    const mm2::gameplay::MoveResult s_n2 = mm2::gameplay::step(world, gs, true);
    expect(s_n2.moved && gs.coordY() == 5, "step N from (7,4) for latch clear", fails);
    mm2::gameplay::latchExploreEventsAfterMove(gs);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 0,
           "COMBAT_VICTORY_LATCH cleared by post-move latch", fails);

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

    testDoorLockRotation(fails);
    testOutdoorTerrainSkills(world, gs, fails);

    if (fails == 0) {
        std::printf("OK: movement_middlegate_test (outdoor skills + water)\n");
        return 0;
    }
    return 1;
}

#include "mm2/gameplay/Movement.h"

#include "mm2_map_codec.h"

namespace mm2::gameplay {

namespace {

constexpr int32_t kGsFacingBundleHi = -0x55D8; /* A4-$AA28 */
constexpr int32_t kGsLightFactor = -0x79AB;
constexpr int32_t kGsPositionChanged = -0x4F4E; /* word; loop tail -> event latch */

char flipFacingForBackStep(char key)
{
    switch (key) {
    case 'N': return 'S';
    case 'S': return 'N';
    case 'E': return 'W';
    case 'W': return 'E';
    default: return key;
    }
}

char turnFacing(char key, bool right_cw)
{
    static const char kCw[] = {'N', 'E', 'S', 'W'};
    int idx = 0;
    for (int i = 0; i < 4; ++i) {
        if (kCw[i] == key) {
            idx = i;
            break;
        }
    }
    if (right_cw) {
        idx = (idx + 1) & 3;
    } else {
        idx = (idx + 3) & 3;
    }
    return kCw[idx];
}

void latchEventOnTurn(GameStateView &gs)
{
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
}

void latchEventAfterStep(GameStateView &gs)
{
    mm2_gs_set_u16(gs.a4(), kGsPositionChanged, 1);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
}

uint8_t collisionAt(const world::MapWorld &world, int x, int y)
{
    return world.collisionAt(x, y);
}

bool passabilityBlocked(const world::MapWorld &world, int x, int y, char facing_key)
{
    return mm2_map_passability_blocked(collisionAt(world, x, y), facing_key) != 0;
}

/* world_edge_resolve @ 0x1D0A — neighbour byte from materialized attrib 0x05..0x08. */
bool resolveScreenEdge(world::MapWorld &world, GameStateView &gs, int *x, int *y)
{
    int nx = *x;
    int ny = *y;
    int neighbor_slot = -1;

    if (nx == MM2_MAP_GRID_DIM) {
        neighbor_slot = 1; /* E */
        nx &= 0x0F;
    } else if (nx == -1 || nx == 0xFF) {
        neighbor_slot = 3; /* W */
        nx &= 0x0F;
    }

    if (ny == MM2_MAP_GRID_DIM) {
        neighbor_slot = 0; /* N */
        ny &= 0x0F;
    } else if (ny == -1 || ny == 0xFF) {
        neighbor_slot = 2; /* S */
        ny &= 0x0F;
    }

    if (neighbor_slot < 0) {
        return false;
    }

    const int new_screen = world.neighborScreen(neighbor_slot);
    if (new_screen < 0 || new_screen > 0xFE) {
        /* GAP: 0x1D92 → 0x19D6 when neighbour byte is $FF — blocked at map edge. */
        return false;
    }

    if (!world.enterScreen(new_screen)) {
        return false;
    }

    gs.setScreenId(static_cast<uint8_t>(new_screen));
    gs.setCoordX(static_cast<uint8_t>(nx));
    gs.setCoordY(static_cast<uint8_t>(ny));
    *x = nx;
    *y = ny;
    return true;
}

}  // namespace

void applyStepTimeTick(GameStateView &gs, uint8_t collision_cell_at_dest)
{
    /* time_tick @ 0x69DC(n=1): subday += n; dark tile drains light when -$79AB > 0. */
    uint16_t subday = mm2_gs_u16(gs.a4(), MM2_GS_TIME_SUBDAY);
    subday = static_cast<uint16_t>(subday + 1);
    mm2_gs_set_u16(gs.a4(), MM2_GS_TIME_SUBDAY, subday);

    if (mm2_map_collision_is_dark(collision_cell_at_dest)) {
        const uint8_t light = mm2_gs_u8(gs.a4(), kGsLightFactor);
        if (light > 0) {
            mm2_gs_set_u8(gs.a4(), kGsLightFactor, static_cast<uint8_t>(light - 1));
            /* GAP: Protect panel redraw (-$7EAE @ 0x5E28) deferred. */
        }
    }

    /* GAP: subday >= 0x100 day rollover @ 0x6A06 not implemented yet. */
}

MoveResult turn(world::MapWorld &world, GameStateView &gs, bool right_cw)
{
    (void)world;
    MoveResult r{};
    if (!gs.valid()) {
        return r;
    }

    const char next = turnFacing(gs.facingKey(), right_cw);
    gs.setFacingKey(next);
    latchEventOnTurn(gs);
    r.acted = true;
    r.turned = true;
    return r;
}

MoveResult step(world::MapWorld &world, GameStateView &gs, bool forward)
{
    MoveResult r{};
    if (!gs.valid()) {
        return r;
    }

    char step_facing = gs.facingKey();
    char saved_facing = step_facing;
    if (!forward) {
        step_facing = flipFacingForBackStep(step_facing);
    }

    const int sx = static_cast<int>(gs.coordX());
    const int sy = static_cast<int>(gs.coordY());

    if (passabilityBlocked(world, sx, sy, step_facing)) {
        r.blocked = true;
        return r;
    }

    int8_t dx = 0;
    int8_t dy = 0;
    mm2_map_facing_delta(step_facing, &dx, &dy);
    int tx = sx + dx;
    int ty = sy + dy;

    bool screen_changed = false;
    if (tx < 0 || ty < 0 || tx > MM2_MAP_GRID_DIM || ty > MM2_MAP_GRID_DIM ||
        tx == MM2_MAP_GRID_DIM || ty == MM2_MAP_GRID_DIM) {
        const int before_screen = world.currentScreen();
        if (!resolveScreenEdge(world, gs, &tx, &ty)) {
            r.blocked = true;
            return r;
        }
        screen_changed = world.currentScreen() != before_screen;
        tx = gs.coordX();
        ty = gs.coordY();
    } else {
        gs.setCoordX(static_cast<uint8_t>(tx));
        gs.setCoordY(static_cast<uint8_t>(ty));
    }

    if (!forward) {
        gs.setFacingKey(saved_facing);
    }

    const uint8_t dest_cell = collisionAt(world, tx, ty);
    applyStepTimeTick(gs, dest_cell);
    latchEventAfterStep(gs);

    r.acted = true;
    r.moved = true;
    r.screen_changed = screen_changed;
    return r;
}

}  // namespace mm2::gameplay

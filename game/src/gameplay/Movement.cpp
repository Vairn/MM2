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

/* Day rollover @ 0x6A06: once subday reaches one full day (0x100), advance the
 * per-era calendar and fold subday back into [0,0x100). Shared by the per-step
 * tick (n=1) and the multi-tick advance (Rest n=0x55). Arithmetic mirrors the
 * ASM exactly. */
void applyDayRollover(GameStateView &gs)
{
    uint8_t *a4 = gs.a4();
    uint16_t subday = mm2_gs_u16(a4, MM2_GS_TIME_SUBDAY);
    /* 006a0e: cmpi.w #$100,-$79b4; blt -> nothing to do. */
    if (subday < 0x100) {
        return;
    }

    /* 006a18: era index (raw word, asl.l #1 -> word stride). era is held to
     * 0..9 at every write site, so the array access stays in bounds. */
    const int era = static_cast<int>(gs.era());
    const int32_t day_off = MM2_GS_DAY + era * 2;
    const int32_t year_off = MM2_GS_YEAR + era * 2;

    /* 006a24: ++day[era]; 006a28: keep the just-incremented value. */
    const uint16_t day = static_cast<uint16_t>(mm2_gs_u16(a4, day_off) + 1);
    mm2_gs_set_u16(a4, day_off, day);

    /* 006a2e: subday = subday mod 0x100 (divs.w #$100 + swap = remainder).
     * For the positive [0x100,0x1FF] range that a single step (n=1) or a Rest
     * (n=0x55, prior subday <= 0xFF) produces, this is the low byte; matches the
     * signed-divide remainder. */
    subday = static_cast<uint16_t>(subday % 0x100);
    mm2_gs_set_u16(a4, MM2_GS_TIME_SUBDAY, subday);

    /* DEFER: per-party-member aging @ 0x6988 (jsr $6988): increments each
     * roster member's age-day byte (record +$22), rolling to ++age-year
     * (+$21) at >=0xB5. Needs roster-record access (owned elsewhere). */

    /* 006a42/4a/52: period-flag clears at day 60 / 120 / 180. */
    if (day == 60 || day == 120 || day == 180) {
        mm2_gs_set_u8(a4, MM2_GS_PERIOD_FLAG_A, 0); /* -$798c */
        mm2_gs_set_u8(a4, MM2_GS_PERIOD_FLAG_B, 0); /* -$798d */
    }

    /* 006a6e: day[era] > 180 -> new year (day resets to 1, year++ capped 999). */
    if (day > 180) {
        mm2_gs_set_u16(a4, day_off, 1);
        const uint16_t year = mm2_gs_u16(a4, year_off);
        if (year != 0x3E7) { /* 006a94: cap at 999, no further increment */
            mm2_gs_set_u16(a4, year_off, static_cast<uint16_t>(year + 1));
        }
        mm2_gs_set_u8(a4, MM2_GS_GUARDIAN_CAVE, 0); /* 006aac: clr -$7996 */
    }

    /* DEFER: date-display redraw thunks @ 0x6abe (jsr -$7c3e / jsr $62c8),
     * guarded by n==1 && -$79e5==0 — pure UI repaint, no state change. */
}

}  // namespace

void applyStepTimeTick(GameStateView &gs, uint8_t collision_cell_at_dest)
{
    /* time_tick @ 0x69DC(n=1): subday += n; dark tile drains light when -$79AB > 0. */
    uint16_t subday = mm2_gs_u16(gs.a4(), MM2_GS_TIME_SUBDAY);
    subday = static_cast<uint16_t>(subday + 1);
    mm2_gs_set_u16(gs.a4(), MM2_GS_TIME_SUBDAY, subday);

    /* 0069e8: light drain only fires for n==1 (a step), not the multi-tick Rest. */
    if (mm2_map_collision_is_dark(collision_cell_at_dest)) {
        const uint8_t light = mm2_gs_u8(gs.a4(), kGsLightFactor);
        if (light > 0) {
            mm2_gs_set_u8(gs.a4(), kGsLightFactor, static_cast<uint8_t>(light - 1));
            /* GAP: Protect panel redraw (-$7EAE @ 0x5E28) deferred. */
        }
    }

    applyDayRollover(gs);
}

void advanceTimeTick(GameStateView &gs, uint16_t n)
{
    /* time_tick @ 0x69DC(n!=1): subday += n then day rollover. The light-drain
     * (0069e8) and date-redraw (006abe) branches are gated on n==1, so a Rest
     * advance (n=0x55 @ 0x19CEC) skips both — it only moves the clock. */
    uint16_t subday = mm2_gs_u16(gs.a4(), MM2_GS_TIME_SUBDAY);
    subday = static_cast<uint16_t>(subday + n);
    mm2_gs_set_u16(gs.a4(), MM2_GS_TIME_SUBDAY, subday);
    applyDayRollover(gs);
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

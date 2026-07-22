#include "mm2/gameplay/Movement.h"

#include "mm2/events/EventVmHelpers.h"
#include "mm2/platform/Audio.h"
#include "mm2_attrib_codec.h"
#include "mm2_map_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

#include <cstddef>

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
    /* movement_turn @ 0x5802 also clears A4-$77BD when the turn updates coords. */
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 0);
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

/* Party aging @ 0x6988 (called from day rollover @ 0x6A3E):
 * for each party slot: ++record+$22; if >= $B5 → ++$21 and $22=1. */
void agePartyOnDayRollover(Mm2RosterFile *roster, const Mm2PartyLaunch *launch)
{
    if (!roster || !launch) {
        return;
    }
    for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch->roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        auto *raw = reinterpret_cast<uint8_t *>(&roster->records[idx]);
        raw[0x22] = static_cast<uint8_t>(raw[0x22] + 1);
        if (raw[0x22] >= 0xB5) {
            raw[0x21] = static_cast<uint8_t>(raw[0x21] + 1);
            raw[0x22] = 1;
        }
    }
}

/* Day rollover @ 0x6A06: once subday reaches one full day (0x100), advance the
 * per-era calendar and fold subday back into [0,0x100). Shared by the per-step
 * tick (n=1) and the multi-tick advance (Rest n=0x55). Arithmetic mirrors the
 * ASM exactly. */
void applyDayRollover(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch)
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

    /* 0x6A3E: jsr 0x6988 — age every party member. */
    agePartyOnDayRollover(roster, launch);

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

void applyStepTimeTick(GameStateView &gs, uint8_t collision_cell_at_dest, Mm2RosterFile *roster,
                       const Mm2PartyLaunch *launch)
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

    applyDayRollover(gs, roster, launch);
}

void advanceTimeTick(GameStateView &gs, uint16_t n, Mm2RosterFile *roster,
                     const Mm2PartyLaunch *launch)
{
    /* time_tick @ 0x69DC(n!=1): subday += n then day rollover. The light-drain
     * (0069e8) and date-redraw (006abe) branches are gated on n==1, so a Rest
     * advance (n=0x55 @ 0x19CEC) skips both — it only moves the clock. */
    uint16_t subday = mm2_gs_u16(gs.a4(), MM2_GS_TIME_SUBDAY);
    subday = static_cast<uint16_t>(subday + n);
    mm2_gs_set_u16(gs.a4(), MM2_GS_TIME_SUBDAY, subday);
    applyDayRollover(gs, roster, launch);
}

void materializeScreenAttrib(GameStateView &gs, const world::MapWorld &world)
{
    /* 0x923E: copy attrib.dat[screen]*64 → A4-$561A. */
    if (!gs.valid() || !world.loaded()) {
        return;
    }
    const Mm2AttribRecord &rec = world.attrib();
    uint8_t *a4 = gs.a4();
    for (int i = 0; i < MM2_ATTRIB_RECORD_SIZE; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_ATTRIB_BUF + i, rec.raw[i]);
    }
}

void syncCurrentCellFlags(GameStateView &gs, const world::MapWorld &world)
{
    /* 0x1B1C: collision[(y<<4)|x] → -$55D6 (current-cell byte). */
    if (!gs.valid() || !world.loaded()) {
        return;
    }
    const uint8_t cell = world.collisionAt(static_cast<int>(gs.coordX()), static_cast<int>(gs.coordY()));
    mm2_gs_set_u8(gs.a4(), MM2_GS_TILE_RT_FLAGS, cell);
}

void sessionInteractionGate(GameStateView &gs)
{
    /* Darkness leaf of session_interaction_gate @ 0x53C0..0x53E8. */
    if (!gs.valid()) {
        return;
    }
    uint8_t *a4 = gs.a4();
    mm2_gs_set_u8(a4, MM2_GS_CANT_SEE_FLAG, 0); /* 0x53C0 */
    if (mm2_gs_u8(a4, MM2_GS_LIGHT_FACTOR) != 0) { /* 0x53C4: light suppresses */
        return;
    }
    /* 0x53CA: -$5600 (attrib flags 0x1A) >= $80 → can't-see. */
    if (mm2_gs_u8(a4, MM2_GS_ATTRIB_FLAGS) >= 0x80) {
        mm2_gs_set_u8(a4, MM2_GS_CANT_SEE_FLAG, 1); /* 0x53D6 */
        return;
    }
    /* 0x53DC: btst #5,-$55D6 (S-dark on current collision cell). */
    if ((mm2_gs_u8(a4, MM2_GS_TILE_RT_FLAGS) & 0x20) != 0) {
        mm2_gs_set_u8(a4, MM2_GS_CANT_SEE_FLAG, 1); /* 0x53E4 */
    }
}

uint8_t restSpellBonusFactor(uint8_t attr)
{
    /* 0x4442: walk A4-$7486 thresholds; start bonus=$FD (−3 signed), addq per miss.
     * Return value is the unsigned byte used at 0x19C74 before addq #3. */
    static const uint8_t kThresh[] = {4,  6,  9,  13, 15, 17, 19, 22, 26, 30, 45,
                                      60, 75, 90, 105, 120, 135, 150, 175, 200, 225, 250, 255};
    uint8_t bonus = 0xFD; /* −3 */
    for (size_t i = 0; i < sizeof(kThresh); ++i) {
        if (attr <= kThresh[i]) {
            break;
        }
        ++bonus;
    }
    return bonus;
}

void syncRosterWorkingLevelFields(Mm2RosterRecord &rec)
{
    rec.unknown_1a_20[6] = rec.level; /* +$20 ← +$71 */
    rec.unknown_22 = static_cast<uint16_t>((rec.unknown_22 & 0x00FFu) |
                                           (static_cast<uint16_t>(rec.spell_level) << 8)); /* +$23 ← +$72 */
}

void recomputeRestSpellPoints(Mm2RosterRecord &rec)
{
    /* 0x19C30: if +$23==0 → skip; else INT for Sorc/Archer, PER otherwise. */
    const uint8_t caster_flag = static_cast<uint8_t>((rec.unknown_22 >> 8) & 0xFF); /* +$23 */
    if (caster_flag == 0) {
        return;
    }
    uint8_t attr = rec.personality_current; /* +$12 */
    if (rec.class_id == 4 || rec.class_id == 2) {
        attr = rec.intelligence_current; /* +$11 */
    }
    uint8_t bonus = restSpellBonusFactor(attr);
    if (bonus >= 0xF2) {
        bonus = 0;
    }
    const uint8_t mul = rec.unknown_1a_20[6]; /* +$20 */
    const uint16_t sp = static_cast<uint16_t>((static_cast<uint16_t>(bonus) + 3u) * mul);
    rec.sp_current = sp;
    rec.sp_max = sp; /* 0x19CD2 */
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
    /* movement_turn @ 0x5838: JSR -$7E42(A4) with id 0 (walk beep). */
    audio::playSoundSeq(0, gs.soundsEnabled(), gs.walkBeepEnabled());
    r.acted = true;
    r.turned = true;
    return r;
}

MoveResult step(world::MapWorld &world, GameStateView &gs, bool forward, Mm2RosterFile *roster,
                const Mm2PartyLaunch *launch)
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
    applyStepTimeTick(gs, dest_cell, roster, launch);
    events::eventVmTickSpellEyeOnStep(gs.a4(), world.isOutdoor());
    /* Hood refresh @ 0x1B1C latches the destination collision into -$55D6. */
    syncCurrentCellFlags(gs, world);

    r.acted = true;
    r.moved = true;
    r.screen_changed = screen_changed;
    /* movement_step @ 0x5758 / 0x580c: JSR -$7E42(A4) id 0 after coords change. */
    audio::playSoundSeq(0, gs.soundsEnabled(), gs.walkBeepEnabled());
    return r;
}

void latchExploreEventsAfterMove(GameStateView &gs)
{
    mm2_gs_set_u16(gs.a4(), MM2_GS_ENCOUNTER_REDRAW, 1);
    mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
}

}  // namespace mm2::gameplay

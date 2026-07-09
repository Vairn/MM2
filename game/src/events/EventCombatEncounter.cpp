#include "mm2/events/EventCombatEncounter.h"

#include "mm2/CppStdCompat.h"

namespace mm2::events {

void eventRunFixedEncounter(GameStateView &gs, EventTextView &text, EventVmWait &wait,
                            const uint8_t *monster_block, int block_len, bool variant_b,
                            combat::CombatSession *combat, const world::MapWorld *world)
{
    (void)text;
    (void)wait;

    uint8_t *a4 = gs.a4();
    if (!a4 || !monster_block || block_len < MM2_GS_MONSTER_SLOT_COUNT) {
        return;
    }

    /* 0x16304 / 0x16314: mode = 0x80 (OP_12 fixed) or 0 (OP_13 seeded-random). */
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, variant_b ? 0x00 : 0x80);
    /* 0x1630A: clr.w -$4F4E (combat/redraw flag). */
    mm2_gs_set_u16(a4, MM2_GS_ENCOUNTER_REDRAW, 0);

    /* 0x1631C-0x16336: read 10 monster-type ids into A4-$11DE[0..9]. */
    for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, monster_block[i]);
    }

    if (!variant_b) {
        /* 0x16344-0x16350: OP_12 reads 2 tail bytes — overflow_type + live_count. */
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, monster_block[10]);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, monster_block[11]);
    } else {
        /* 0x16356-0x1635A: OP_13 clears both tail fields. */
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 0);
    }

    /* 0x1635E: jsr -$7EDE (combat engine). */
    if (combat && world) {
        combat->enter(gs, *world);
    }
    /* 0x16362: abort flag set so the event interpreter yields to combat.
     * 0x16368-0x1637C: post-combat pending-event latch (-$7F1A → A4-$7952) is
     * driven by GameSession once combat->active() goes false.
     * Port: only abort when a fight actually armed (empty picker → no yield). */
    if (combat && combat->active()) {
        mm2_gs_set_u8(a4, MM2_GS_SCRIPT_ABORT, 1);
    }
}

void eventRunTileAmbientEncounter(GameStateView &gs, combat::CombatSession *combat,
                                  const world::MapWorld *world)
{
    uint8_t *a4 = gs.a4();
    if (!a4) {
        return;
    }

    /* 0x176F8-0x17718: zero battle slots + encounter mode before -$7EDE. */
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0);
    mm2_gs_set_u16(a4, MM2_GS_ENCOUNTER_REDRAW, 0);
    for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);
    }
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 0);

    if (combat && world) {
        combat->enter(gs, *world);
    }
    if (combat && combat->active()) {
        mm2_gs_set_u8(a4, MM2_GS_SCRIPT_ABORT, 1);
    }
}

}  // namespace mm2::events

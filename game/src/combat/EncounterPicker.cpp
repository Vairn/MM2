#include "mm2/combat/EncounterPicker.h"



#include "mm2_encounter.h"



namespace mm2::combat {



namespace {



/* Data-hunk 0x103E (A4-$6FC0), byte-verified against EXTRACTED/ghidra/mm2_data_00.bin. */

constexpr uint8_t kDispositionMod[4] = {0, 1, 2, 5};



uint8_t clampU8(uint8_t v, uint8_t lo, uint8_t hi)

{

    if (v > hi) v = hi;

    if (v < lo) v = lo;

    return v;

}



/* Visual/collision low-5 terrain id -> encounter class @ A4-$720A (data-hunk

 * off 0xDF4, asm 0x9524). Class 4 is the only step-fight terrain (0x10A2). */

constexpr uint8_t kTileClassLookup[32] = {

    0, 1, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0,

};



uint8_t tileEncounterClass(const world::MapWorld &world, int x, int y)

{

    const int tile_idx = (y << 4) | (x & 0x0F);

    const uint8_t visual = world.visualPage()[tile_idx];

    return kTileClassLookup[visual & 0x1F];

}



/* A4-$7660 @ 0x16EA: surface low nibble with remaps for $9/$A/$B/$C. Town-gate
 * screens (map_category 1, surf $AA) share nibble $A with wilderness gfx but
 * use env type $04 — must not hit the wilderness step path @ 0x10DE. */
uint8_t encounterRuntimeEnvId(const Mm2AttribRecord &rec)

{

    if (rec.raw[MM2_ATTRIB_OFF_MAP_CATEGORY] == 1) {

        return rec.raw[MM2_ATTRIB_OFF_ENV_TYPE];

    }

    return mm2_attrib_runtime_env_id(&rec);

}



}  // namespace



void encounterSyncScreenContext(GameStateView &gs, const world::MapWorld &world)

{

    uint8_t *a4 = gs.a4();

    if (!a4 || !world.loaded()) {

        return;

    }

    const Mm2AttribRecord &rec = world.attrib();

    mm2_gs_set_u8(a4, MM2_GS_VIEW_MODE, mm2_attrib_is_outdoor(&rec) ? 1 : 0);

    mm2_gs_set_u8(a4, MM2_GS_RUNTIME_ENV, encounterRuntimeEnvId(rec));

}



void encounterInitXpBudget(GameStateView &gs, const Mm2RosterFile &roster,

                            const Mm2PartyLaunch &launch)

{

    uint8_t *a4 = gs.a4();

    if (!a4) {

        return;

    }



    uint32_t total_hp = 0;

    uint8_t max_half_level = 0;

    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {

        const int idx = launch.roster_slots[i];

        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {

            continue;

        }

        const Mm2RosterRecord &rec = roster.records[idx];

        total_hp += rec.hp_current;

        const uint8_t half_level = static_cast<uint8_t>(rec.level >> 1);

        if (half_level > max_half_level) {

            max_half_level = half_level;

        }

    }



    /* 0x11EB2-0x11EBC: long divide by 8 (thunk -$7B4E -> 0x24D9A). */

    uint32_t budget = total_hp / 8;

    const uint8_t disposition = gs.disposition();

    if (disposition == 0) {

        budget /= 4; /* total /32 */

    } else if (disposition == 1) {

        budget /= 2; /* total /16 */

    } else if (disposition == 3) {

        budget *= 2; /* total /4 (thunk -$7B54 -> 0x24C74, long multiply) */

    }

    /* disposition == 2: unchanged, total /8 (default). */



    mm2_gs_set_u32(a4, MM2_GS_PARTY_XP_BUDGET, budget);

    mm2_gs_set_u8(a4, MM2_GS_PICKER_TIER_MOD, max_half_level);

}



uint8_t encounterRecomputeLiveCount(GameStateView &gs)

{

    uint8_t *a4 = gs.a4();

    if (!a4) {

        return 0;

    }

    Mm2EncounterSetup setup{};

    for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {

        setup.monster_slot[i] = mm2_gs_u8(a4, MM2_GS_MONSTER_SLOTS + i);

    }

    setup.overflow_type = mm2_gs_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE);

    setup.live_count = mm2_gs_u8(a4, MM2_GS_MONSTER_COUNT);

    const uint8_t live = mm2_encounter_live_count(&setup);

    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, live);

    return live;

}



void encounterPickerBudgetCheck(GameStateView &gs, const Mm2AttribRecord &attrib)

{

    uint8_t *a4 = gs.a4();

    if (!a4) {

        return;

    }

    mm2_gs_set_u8(a4, MM2_GS_PICKER_DONE, 0);



    const uint8_t live = mm2_gs_u8(a4, MM2_GS_MONSTER_COUNT);

    const uint8_t scan = live > 10 ? 10 : live;

    uint32_t total = 0;

    for (int i = 0; i < scan; ++i) {

        const uint8_t type = mm2_gs_u8(a4, MM2_GS_MONSTER_SLOTS + i);

        total += static_cast<uint32_t>(type >> 4) + 1;

    }

    if (live > 10) {

        const uint8_t overflow_type = mm2_gs_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE);

        const uint32_t overflow_cost = static_cast<uint32_t>(overflow_type >> 4) + 1;

        total += overflow_cost * static_cast<uint32_t>(live - 10);

    }



    if (total >= mm2_gs_u32(a4, MM2_GS_PARTY_XP_BUDGET)) {

        mm2_gs_set_u8(a4, MM2_GS_PICKER_DONE, 1);

        return;

    }



    Mm2EncounterTuning tuning{};

    mm2_encounter_tuning_from_attrib(attrib.raw, &tuning);

    if (live >= tuning.group_size_gate) {

        mm2_gs_set_u8(a4, MM2_GS_PICKER_DONE, 1);

    }

}



void encounterAddsFriends(GameStateView &gs, const Mm2AttribRecord &attrib, gameplay::Rng &rng,

                          FriendCountLookup friend_count_lookup, const void *ctx)

{

    uint8_t *a4 = gs.a4();

    if (!a4) {

        return;

    }



    Mm2EncounterTuning tuning{};

    mm2_encounter_tuning_from_attrib(attrib.raw, &tuning);



    /* Step 1: roll a tier-pick ceiling (0x11F0E-0x11F76). */

    const uint8_t disposition = static_cast<uint8_t>(gs.disposition() & 3);

    const uint8_t tier_mod = mm2_gs_u8(a4, MM2_GS_PICKER_TIER_MOD);

    int ceiling = kDispositionMod[disposition] + tier_mod + 1;

    const int roll100 = rng.range(1, 100);

    int bonus;

    if (roll100 < 0x3D) bonus = 0;

    else if (roll100 < 0x51) bonus = 1;

    else if (roll100 < 0x60) bonus = 2;

    else bonus = 3;

    ceiling += bonus;

    if (ceiling > 13) {

        ceiling = 14; /* 0x11F82: replaced with 14, not clamped to 13. */

    }



    /* Step 2: pick the tier, clamped to [attrib min, attrib max] (0x11F94-0x11FBE). */

    int tier_pick = rng.range(1, ceiling);

    tier_pick = clampU8(static_cast<uint8_t>(tier_pick), tuning.min_monsters, tuning.max_monsters);

    const int tier_index = tier_pick - 1;



    /* Step 3: pick a specific type within the tier (0x11FC2-0x11FFC). */

    const int roll16 = rng.range(1, 16);

    const uint8_t type_id = static_cast<uint8_t>(tier_index * 16 + roll16 - 1);



    const uint8_t saved_slot0 = mm2_gs_u8(a4, MM2_GS_MONSTER_SLOTS);

    mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS, type_id);

    const uint8_t friend_field = friend_count_lookup ? friend_count_lookup(type_id, ctx) : 1;

    mm2_gs_set_u8(a4, MM2_GS_MONSTER_FRIEND_COUNT, friend_field);

    mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS, saved_slot0);



    /* Step 4: pick a copy count — NOT re-clamped by attrib (0x012002-0x012014). */

    const uint8_t friend_count = mm2_gs_u8(a4, MM2_GS_MONSTER_FRIEND_COUNT);

    int remaining = rng.range(1, friend_count > 0 ? friend_count : 1);



    /* Step 5: fill consecutive slots up to 11, else fold into overflow (0x012018-0x01206E). */

    uint8_t live = mm2_gs_u8(a4, MM2_GS_MONSTER_COUNT);

    while (live < MM2_GS_MONSTER_BATTLE_SLOTS && remaining > 0) {

        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + live, type_id);

        ++live;

        --remaining;

    }

    if (remaining > 0) {

        if (remaining > 0xF0) {

            remaining = 0xF0;

        }

        int total = live + remaining;

        if (total > 0xFA) {

            total = 0xFA;

        }

        live = static_cast<uint8_t>(total);

    }

    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, live);

}



void encounterRunRandomPicker(GameStateView &gs, const Mm2AttribRecord &attrib, gameplay::Rng &rng,

                               FriendCountLookup friend_count_lookup, const void *ctx)

{

    encounterPickerBudgetCheck(gs, attrib);

    while (!mm2_gs_u8(gs.a4(), MM2_GS_PICKER_DONE)) {

        encounterAddsFriends(gs, attrib, rng, friend_count_lookup, ctx);

        encounterPickerBudgetCheck(gs, attrib);

    }

}



bool encounterTryStepRandom(GameStateView &gs, const world::MapWorld &world, gameplay::Rng &rng)

{

    if (!gs.valid() || !world.loaded()) {

        return false;

    }



    /* Doc 35: random step fights are for overland/dungeon play, not town interiors. */
    if (mm2_attrib_env_type(&world.attrib()) == MM2_ENV_TOWN) {

        return false;

    }



    const int x = static_cast<int>(gs.coordX());

    const int y = static_cast<int>(gs.coordY());

    if (x < 0 || y < 0 || x >= MM2_MAP_GRID_DIM || y >= MM2_MAP_GRID_DIM) {

        return false;

    }



    uint8_t *a4 = gs.a4();



    /* 0x1096: skip when the current tile byte (A4-$55D6) has bit 7 set. */

    const uint8_t collision = world.collisionAt(x, y);

    if ((collision & MM2_MAP_COLL_EVENT) != 0) {

        return false;

    }



    /* 0x10A2: step-rate roll from attrib byte 0x09 (A4-$5611). */

    uint8_t trigger = 0;

    const uint8_t rate = world.attrib().raw[0x09];

    if (rate > 0 && rng.range(1, rate) == 1) {

        trigger = 1;

    }



    /* 0x10BE-0x10CA: -$4F4E (position-changed / redraw) or -$77BD victory latch. */

    if (mm2_gs_u16(a4, MM2_GS_ENCOUNTER_REDRAW) != 0) {

        trigger = 0;

    } else if (mm2_gs_u8(a4, MM2_GS_COMBAT_VICTORY_LATCH) != 0) {

        trigger = 0;

    }



    int mode2_roll5 = 0;



    /* 0x10CE: path-1 when trigger == 1. */

    if (trigger == 1) {

        const uint8_t view = mm2_gs_u8(a4, MM2_GS_VIEW_MODE);

        if (view == 1) {

            const uint8_t runtime_env = mm2_gs_u8(a4, MM2_GS_RUNTIME_ENV);

            if (runtime_env == 0x0A) {

                /* Wilderness @ 0x10DE: tile class 4 only; rng(1,5) seeds mode-2 group. */

                if (tileEncounterClass(world, x, y) != 4) {

                    trigger = 0;

                } else {

                    mode2_roll5 = rng.range(1, 5);

                    trigger = 2;

                }

            }

            /* Non-wilderness outdoor (env != $0A): trigger stays 1, no tile-class gate. */

        }

        /* view != 1: trigger stays 1 (ASM branches to 0x1132 without clearing). */

    }



    /* 0x1138 / 0x11AA: path-2 when trigger == 0 is not random combat (teleport UI). */

    if (trigger == 0) {

        return false;

    }



    /* 0x113e-0x11a2: seed combat block and enter via -$7EDE. */

    for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {

        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);

    }

    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);

    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 0);



    if (trigger == 2) {

        /* 0x1166-0x1198: mode $02 — type (roll5 + $E1), count (6 - roll5). */

        const uint8_t type_id = static_cast<uint8_t>(0xE1 + mode2_roll5);

        const int fill_count = 6 - mode2_roll5;

        for (int i = 0; i < fill_count && i < MM2_GS_MONSTER_BATTLE_SLOTS; ++i) {

            mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, type_id);

        }

        mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, static_cast<uint8_t>(fill_count));

        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 2);

    } else {

        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0);

    }



    return true;

}



}  // namespace mm2::combat



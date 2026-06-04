#include "mm2_party_launch.h"

#if defined(MM2_CODEC_AMIGA) || defined(MM2_HOST_AMIGA)
#include "mm2_codec_platform.h"
#else
#include <string.h>
#endif

/*
 * Verified from EXTRACTED/ghidra/mm2_data_00.bin (lea -$7670/$766b/$7666, a4)).
 * Indexed by town filter 0..4 (UI keys '1'..'5' minus 1).
 */
static const uint8_t kTownSpawnX[5] = {7, 9, 7, 7, 3};
static const uint8_t kTownSpawnY[5] = {3, 13, 11, 0, 10};
static const char kTownFacingKey[5] = {'N', 'N', 'E', 'N', 'W'};

void mm2_town_inn_spawn_lookup(uint8_t town_filter_1_to_5, uint8_t *area_id, uint8_t *coord_x,
                               uint8_t *coord_y, char *facing_key)
{
    uint8_t idx = 0;
    if (town_filter_1_to_5 >= 1 && town_filter_1_to_5 <= 5) {
        idx = (uint8_t)(town_filter_1_to_5 - 1);
    }
    if (area_id) {
        *area_id = idx;
    }
    if (coord_x) {
        *coord_x = kTownSpawnX[idx];
    }
    if (coord_y) {
        *coord_y = kTownSpawnY[idx];
    }
    if (facing_key) {
        *facing_key = kTownFacingKey[idx];
    }
}

void mm2_party_launch_build(Mm2PartyLaunch *out, uint8_t town_filter_1_to_5,
                            const int *member_roster_indices, int party_count)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->town_filter = town_filter_1_to_5;
    mm2_town_inn_spawn_lookup(town_filter_1_to_5, &out->area_id, &out->coord_x, &out->coord_y,
                              &out->facing_key);

    if (party_count < 0) {
        party_count = 0;
    }
    if (party_count > MM2_PARTY_LAUNCH_SLOTS) {
        party_count = MM2_PARTY_LAUNCH_SLOTS;
    }
    out->party_count = party_count;

    for (int i = 0; i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        out->roster_slots[i] = -1;
    }
    if (member_roster_indices) {
        for (int i = 0; i < party_count; ++i) {
            out->roster_slots[i] = (int16_t)member_roster_indices[i];
        }
    }
}

/* map_facing_from_key @ 0x5636 — facing_index + tile bundle seed. */
static void apply_facing_key(uint8_t *a4, char facing_key)
{
    uint8_t bundle_hi = 0;
    uint8_t facing_idx = 0;
    switch (facing_key) {
    case 'N':
        bundle_hi = 0xC0;
        facing_idx = 6;
        break;
    case 'E':
        bundle_hi = 0x30;
        facing_idx = 4;
        break;
    case 'S':
        bundle_hi = 0x0C;
        facing_idx = 2;
        break;
    case 'W':
        bundle_hi = 0x03;
        facing_idx = 0;
        break;
    default:
        break;
    }
    mm2_gs_set_u8(a4, MM2_GS_LAST_MOVE_KEY, (uint8_t)facing_key);
    mm2_gs_set_u8(a4, -0x55D8, bundle_hi);              /* A4-$AA28 tile bundle hi */
    mm2_gs_set_u8(a4, MM2_GS_FACING_INDEX, facing_idx); /* A4-$AA29 */
}

void mm2_party_launch_apply(uint8_t *a4_base, const Mm2PartyLaunch *launch)
{
    if (!a4_base || !launch) {
        return;
    }

    /* char_create_party_assemble exit @ 0x0E2C */
    mm2_gs_set_u8(a4_base, -0x79AC, launch->area_id); /* A4-$8654 town/area latch */
    mm2_gs_set_u8(a4_base, MM2_GS_SCREEN_MODE_ID, launch->area_id);
    mm2_gs_set_u8(a4_base, MM2_GS_COORD_B, launch->coord_x);
    mm2_gs_set_u8(a4_base, MM2_GS_COORD_A, launch->coord_y);
    apply_facing_key(a4_base, launch->facing_key);

    /* Party roster index table A4-$8696 (8 × int16, 0xFFFF = empty). */
    for (int i = 0; i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int16_t slot = launch->roster_slots[i];
        const uint16_t word = (slot < 0) ? 0xFFFFu : (uint16_t)slot;
        mm2_gs_set_u16(a4_base, MM2_GS_ROSTER_INDEX_TBL + i * 2, word);
    }

    mm2_gs_set_u16(a4_base, -0x795A, (uint16_t)launch->party_count); /* A4-$86A6 */
    mm2_gs_set_u8(a4_base, MM2_GS_NEW_GAME_FLAG, 1);
}

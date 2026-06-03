#ifndef MM2_PARTY_LAUNCH_H
#define MM2_PARTY_LAUNCH_H

#include <stdint.h>

#include "mm2_gamestate.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Title "Goto Town" party confirm @ char_create_party_assemble (0x0826).
 *
 * On key Z (0x5A) with a non-empty party, ASM @ 0x0E2C–0x0E62:
 *   - copies town filter A4-$860E → A4-$8654
 *   - indexes spawn tables at A4-$8990 / $8995 / $899A (file +0x98E..)
 *   - sets A4-$860F (coord_b/X), $8610 (coord_a/Y), $864F (facing key)
 *   - JSR map_facing_from_key @ 0x5636
 * then returns to caller; game_world_boot @ 0x0F1C loads the map via -$7FDA.
 *
 * Town filter in the UI uses roster home-town bytes 1..5; table index is 0..4.
 * Map screen id equals table index (areas 0..4 = Middlegate..Sandsobar).
 */

#define MM2_PARTY_LAUNCH_SLOTS MM2_GS_PARTY_SIZE

typedef struct Mm2PartyLaunch {
    uint8_t town_filter; /* 1..5 (roster $0B low 7 bits) */
    uint8_t area_id;     /* map.dat screen 0..4 */
    uint8_t coord_x;     /* A4-$860F (coord_b) */
    uint8_t coord_y;     /* A4-$8610 (coord_a) */
    char facing_key;     /* A4-$864F: 'N'/'E'/'S'/'W' */
    int party_count;
    int16_t roster_slots[MM2_PARTY_LAUNCH_SLOTS]; /* unused → -1 (0xFFFF) */
} Mm2PartyLaunch;

void mm2_town_inn_spawn_lookup(uint8_t town_filter_1_to_5, uint8_t *area_id, uint8_t *coord_x,
                               uint8_t *coord_y, char *facing_key);

void mm2_party_launch_build(Mm2PartyLaunch *out, uint8_t town_filter_1_to_5,
                            const int *member_roster_indices, int party_count);

/* Materialize launch into the A4 workspace (roster index tbl + map coords). */
void mm2_party_launch_apply(uint8_t *a4_base, const Mm2PartyLaunch *launch);

#ifdef __cplusplus
}
#endif

#endif /* MM2_PARTY_LAUNCH_H */

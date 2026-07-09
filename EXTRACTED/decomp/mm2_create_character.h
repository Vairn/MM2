#ifndef MM2_CREATE_CHARACTER_H
#define MM2_CREATE_CHARACTER_H

#include <stdint.h>

#include "mm2_roster_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MM2_CREATE_STAT_COUNT = 7,
    MM2_CREATE_CLASS_COUNT = 8,
    MM2_CREATE_RACE_COUNT = 5,
    MM2_CREATE_ALIGN_COUNT = 3,
    MM2_CREATE_SEX_COUNT = 2,
    MM2_CREATE_NAME_MAX = MM2_ROSTER_NAME_SIZE
};

/** Rolled / modified stat block (Mgt, Int, Per, End, Spd, Acc, Luck). */
typedef struct Mm2CreateStats {
    uint8_t might;
    uint8_t intelligence;
    uint8_t personality;
    uint8_t endurance;
    uint8_t speed;
    uint8_t accuracy;
    uint8_t luck;
} Mm2CreateStats;

/** Working state for the multi-step create flow. */
typedef struct Mm2PendingCharacter {
    Mm2CreateStats rolled;
    Mm2CreateStats modified;
    int8_t class_id;    /* 0..7, or -1 if not chosen */
    int8_t race;        /* 0..4, or -1 */
    int8_t alignment;   /* 0 good, 1 neutral, 2 evil; or -1 */
    int8_t sex;         /* 0 male, 1 female; or -1 */
    char name[MM2_CREATE_NAME_MAX + 1];
} Mm2PendingCharacter;

uint32_t mm2_create_rng_next(uint32_t *state);

void mm2_create_pending_init(Mm2PendingCharacter *pending);
void mm2_create_roll_stats(Mm2CreateStats *out, uint32_t *rng_state);
int mm2_create_class_eligible(int class_id, const Mm2CreateStats *rolled);
void mm2_create_apply_race(int race, const Mm2CreateStats *rolled, Mm2CreateStats *modified);
void mm2_create_swap_stats(Mm2CreateStats *stats, int index_a, int index_b);

int mm2_create_endurance_hp_bonus(uint8_t endurance);
int mm2_create_speed_ac_bonus(uint8_t speed);
int mm2_create_primary_sp_per_level(int class_id, const Mm2CreateStats *modified);
int mm2_create_class_hp_base(int class_id);
uint8_t mm2_create_starting_thievery(int class_id);

void mm2_create_build_record(const Mm2PendingCharacter *pending, Mm2RosterRecord *out);

/** When spell_level > 0 but sp_max is 0 (stale roster.dat), derive max/current SP
 *  from class + stats per 0x19C30 / $7B36: sp_per_level * spell_level. */
void mm2_roster_refresh_spell_points(Mm2RosterRecord *rec);

#ifdef __cplusplus
}
#endif

#endif

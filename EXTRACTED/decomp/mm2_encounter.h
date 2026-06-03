#ifndef MM2_ENCOUNTER_H
#define MM2_ENCOUNTER_H

#include <stdint.h>
#include <stddef.h>

/* Per-screen random-encounter tuning from attrib.dat bytes 0x09-0x0D.
 * Loaded into A4-$561A on screen enter; reads appear as -$5611..-$560D. */
typedef struct Mm2EncounterTuning {
    uint8_t rate_denom;        /* 0x09: RNG(1, N)==1 @ 0x10A2 */
    uint8_t group_size_gate;   /* 0x0A: cmp vs live count @ 0x12124 */
    uint8_t max_monsters;      /* 0x0B: cap in picker @ 0x11FA2 */
    uint8_t min_monsters;      /* 0x0C: floor in picker @ 0x11FB2 */
    uint8_t retreat_difficulty;/* 0x0D: run roll @ 0x116CA / 0x13148 */
} Mm2EncounterTuning;

enum { MM2_ENCOUNTER_AREA_COUNT = 60 };

/* Embedded exe globals (mm2_data_00.bin via off = 0x7FFE - a4_disp). */
typedef struct Mm2EncounterGlobals {
    uint8_t arena_area_index[MM2_ENCOUNTER_AREA_COUNT]; /* A4-$718A @ 0x9E96 */
    uint8_t disposition_mod[4];                         /* A4-$6FC0 @ 0x11F14 */
    uint32_t party_xp_budget;                           /* A4-$6FCA @ 0x12110 */
} Mm2EncounterGlobals;

/* Extract tuning from one 64-byte attrib record (does not copy raw record). */
void mm2_encounter_tuning_from_attrib(const uint8_t rec[64], Mm2EncounterTuning *out);

/* Load embedded globals from the data hunk blob (8604 bytes). */
int mm2_encounter_globals_load(const uint8_t *data, size_t len, Mm2EncounterGlobals *out);

#endif

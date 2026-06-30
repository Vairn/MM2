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

/* ---- OP_12 / OP_13 fixed-encounter setup (event-VM handler @ 0x16300) ----
 *
 * Inline script-byte layout following the opcode:
 *   OP_12 (fixed):         10 monster ids, then overflow_type, then live_count  (12 bytes)
 *   OP_13 (seeded-random): 10 monster ids only                                  (10 bytes)
 *
 * The handler seeds A4-$11DE[0..9] (slots), A4-$11D4 (overflow_type), A4-$796B
 * (mode), A4-$77BE (live_count). See docs/07 §"OP_12/OP_13 encounter setup".
 */
enum { MM2_ENCOUNTER_SLOT_COUNT = 10 };
enum { MM2_ENCOUNTER_MODE_FIXED = 0x80, MM2_ENCOUNTER_MODE_RANDOM = 0x00 };

typedef struct Mm2EncounterSetup {
    uint8_t monster_slot[MM2_ENCOUNTER_SLOT_COUNT]; /* A4-$11DE visible monster types */
    uint8_t overflow_type;  /* A4-$11D4 type for monsters beyond 10 slots / breed seed; flag */
    uint8_t mode;           /* A4-$796B 0x80=fixed, 0x00=seeded-random */
    uint8_t live_count;     /* A4-$77BE live monster count (seed; recomputed @ 0x12CE0) */
} Mm2EncounterSetup;

/* Decode the inline OP_12/OP_13 byte block (is_op12 selects 12- vs 10-byte form).
 * Returns the number of script bytes consumed (12 for OP_12, 10 for OP_13). */
int mm2_encounter_setup_decode(const uint8_t *script, int is_op12, Mm2EncounterSetup *out);

/* Encode an Mm2EncounterSetup back to inline script bytes. `out` must hold >=12.
 * Returns the number of bytes written (12 for fixed mode, 10 otherwise). */
int mm2_encounter_setup_encode(const Mm2EncounterSetup *in, uint8_t *out);

/* Final on-screen monster count per the combat-setup recompute @ 0x12CE0. */
uint8_t mm2_encounter_live_count(const Mm2EncounterSetup *in);

#endif

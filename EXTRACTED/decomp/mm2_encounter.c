#include "mm2_encounter.h"

enum {
    MM2_A4_ANCHOR = 0x7FFE,
    MM2_A4_ENCOUNTER_AREA_INDEX = 0x718A,
    MM2_A4_DISPOSITION_MOD = 0x6FC0,
    MM2_A4_PARTY_XP_BUDGET = 0x6FCA,
};

static const uint8_t *a4_ptr(const uint8_t *data, size_t len, unsigned disp)
{
    size_t off = MM2_A4_ANCHOR - disp;
    if (off + 1 > len)
        return NULL;
    return data + off;
}

void mm2_encounter_tuning_from_attrib(const uint8_t rec[64], Mm2EncounterTuning *out)
{
    out->rate_denom = rec[0x09];
    out->group_size_gate = rec[0x0A];
    out->max_monsters = rec[0x0B];
    out->min_monsters = rec[0x0C];
    out->retreat_difficulty = rec[0x0D];
}

int mm2_encounter_globals_load(const uint8_t *data, size_t len, Mm2EncounterGlobals *out)
{
    const uint8_t *area = a4_ptr(data, len, MM2_A4_ENCOUNTER_AREA_INDEX);
    const uint8_t *disp = a4_ptr(data, len, MM2_A4_DISPOSITION_MOD);
    const uint8_t *xp = a4_ptr(data, len, MM2_A4_PARTY_XP_BUDGET);
    size_t i;

    if (!area || !disp || !xp || len < MM2_A4_ANCHOR - MM2_A4_PARTY_XP_BUDGET + 4)
        return -1;

    for (i = 0; i < MM2_ENCOUNTER_AREA_COUNT; ++i)
        out->arena_area_index[i] = area[i];

    for (i = 0; i < 4; ++i)
        out->disposition_mod[i] = disp[i];

    out->party_xp_budget =
        ((uint32_t)xp[0] << 24) | ((uint32_t)xp[1] << 16) |
        ((uint32_t)xp[2] << 8) | (uint32_t)xp[3];
    return 0;
}

int mm2_encounter_setup_decode(const uint8_t *script, int is_op12, Mm2EncounterSetup *out)
{
    int i;
    for (i = 0; i < MM2_ENCOUNTER_SLOT_COUNT; ++i)
        out->monster_slot[i] = script[i];
    if (is_op12) {
        out->mode = MM2_ENCOUNTER_MODE_FIXED;
        out->overflow_type = script[10];
        out->live_count = script[11];
        return 12;
    }
    out->mode = MM2_ENCOUNTER_MODE_RANDOM;
    out->overflow_type = 0;
    out->live_count = 0;
    return 10;
}

int mm2_encounter_setup_encode(const Mm2EncounterSetup *in, uint8_t *out)
{
    int i;
    for (i = 0; i < MM2_ENCOUNTER_SLOT_COUNT; ++i)
        out[i] = in->monster_slot[i];
    if (in->mode == MM2_ENCOUNTER_MODE_FIXED) {
        out[10] = in->overflow_type;
        out[11] = in->live_count;
        return 12;
    }
    return 10;
}

uint8_t mm2_encounter_live_count(const Mm2EncounterSetup *in)
{
    /* Combat-setup recompute @ 0x12CE0: j = #non-zero slots; if overflow_type
     * != 0, j += live_count. (j is then clamped to <=10 visible @ 0x12D34.) */
    int j = 0;
    int i;
    for (i = 0; i < MM2_ENCOUNTER_SLOT_COUNT; ++i) {
        if (in->monster_slot[i] != 0)
            ++j;
    }
    if (in->overflow_type != 0)
        j += in->live_count;
    if (j > 255)
        j = 255;
    return (uint8_t)j;
}

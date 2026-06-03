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

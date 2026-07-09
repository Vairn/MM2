#include "mm2_create_character.h"

#include <string.h>

enum {
    MM2_CREATE_D20 = 20,
    MM2_CREATE_STAT_MIN = 3,
    MM2_CREATE_STAT_MAX = 21,
    MM2_CREATE_START_GOLD = 200,
    MM2_CREATE_START_FOOD = 10,
    MM2_CREATE_START_AGE = 18,
    MM2_CREATE_START_TOWN = 1
};

uint32_t mm2_create_rng_next(uint32_t *state)
{
    if (!state) {
        return 0;
    }
    /* Low word = roll counter (A4-$795a increments per RNG draw in the original).
     * High word = mixed seed. Not a byte-perfect -$7BB4 trace yet, but uniform
     * 1..n draws for create stat rolls. */
    uint16_t counter = (uint16_t)(*state & 0xFFFFu);
    uint16_t mix = (uint16_t)(*state >> 16);
    counter++;
    mix = (uint16_t)(mix * 214013u + 2531011u);
    *state = ((uint32_t)mix << 16) | counter;
    return *state;
}

static unsigned mm2_rng_range_1_to_n(uint32_t *rng_state, unsigned n)
{
    if (!rng_state || n == 0) {
        return 1;
    }
    const uint32_t r = mm2_create_rng_next(rng_state);
    return (unsigned)((r >> 16) % n) + 1u;
}

static uint8_t roll_create_stat(uint32_t *rng_state)
{
    /* Manual + ASM $01C10C: JSR -$7B54 with d1=20 (1d20). Ratings are 3..21
     * (Appendix A / p.7), i.e. d20 + 2 with 22 clamped to 21. */
    const unsigned d20 = mm2_rng_range_1_to_n(rng_state, MM2_CREATE_D20);
    unsigned rating = d20 + 2u;
    if (rating > MM2_CREATE_STAT_MAX) {
        rating = MM2_CREATE_STAT_MAX;
    }
    return (uint8_t)rating;
}

static int bonus_tier(uint8_t stat)
{
    if (stat <= 12) {
        return 0;
    }
    if (stat <= 14) {
        return 1;
    }
    if (stat <= 16) {
        return 2;
    }
    if (stat <= 18) {
        return 3;
    }
    if (stat <= 21) {
        return 4;
    }
    if (stat <= 25) {
        return 5;
    }
    if (stat <= 29) {
        return 6;
    }
    if (stat <= 44) {
        return 7;
    }
    if (stat <= 59) {
        return 8;
    }
    if (stat <= 74) {
        return 9;
    }
    if (stat <= 89) {
        return 10;
    }
    if (stat <= 104) {
        return 11;
    }
    if (stat <= 119) {
        return 12;
    }
    if (stat <= 134) {
        return 13;
    }
    if (stat <= 149) {
        return 14;
    }
    if (stat <= 174) {
        return 15;
    }
    if (stat <= 199) {
        return 16;
    }
    if (stat <= 224) {
        return 17;
    }
    if (stat <= 249) {
        return 18;
    }
    return 18;
}

static int sp_per_level_table(int tier)
{
    static const int kTable[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
    if (tier < 0) {
        tier = 0;
    }
    if (tier >= (int)(sizeof(kTable) / sizeof(kTable[0]))) {
        tier = (int)(sizeof(kTable) / sizeof(kTable[0])) - 1;
    }
    return kTable[tier];
}

static uint8_t *stat_ptr(Mm2CreateStats *stats, int index)
{
    switch (index) {
    case 0:
        return &stats->might;
    case 1:
        return &stats->intelligence;
    case 2:
        return &stats->personality;
    case 3:
        return &stats->endurance;
    case 4:
        return &stats->speed;
    case 5:
        return &stats->accuracy;
    case 6:
        return &stats->luck;
    default:
        return NULL;
    }
}

static uint8_t stat_value(const Mm2CreateStats *stats, int index)
{
    const uint8_t *p = stat_ptr((Mm2CreateStats *)stats, index);
    return p ? *p : 0;
}

void mm2_create_pending_init(Mm2PendingCharacter *pending)
{
    if (!pending) {
        return;
    }
    memset(pending, 0, sizeof(*pending));
    pending->class_id = -1;
    pending->race = -1;
    pending->alignment = -1;
    pending->sex = -1;
    pending->name[0] = '\0';
}

void mm2_create_roll_stats(Mm2CreateStats *out, uint32_t *rng_state)
{
    int i;
    if (!out || !rng_state) {
        return;
    }
    for (i = 0; i < MM2_CREATE_STAT_COUNT; i++) {
        uint8_t *p = stat_ptr(out, i);
        if (p) {
            *p = roll_create_stat(rng_state);
        }
    }
}

int mm2_create_class_eligible(int class_id, const Mm2CreateStats *rolled)
{
    if (!rolled || class_id < 0 || class_id >= MM2_CREATE_CLASS_COUNT) {
        return 0;
    }
    switch (class_id) {
    case 0:
        return rolled->might >= 12;
    case 1:
        return rolled->personality >= 13;
    case 2:
        return rolled->intelligence >= 13;
    case 3:
        return rolled->personality >= 13;
    case 4:
        return rolled->intelligence >= 13;
    case 5:
        return rolled->luck >= 13;
    case 6:
        return rolled->speed >= 13 && rolled->accuracy >= 13;
    case 7:
        return rolled->endurance >= 15;
    default:
        return 0;
    }
}

void mm2_create_apply_race(int race, const Mm2CreateStats *rolled, Mm2CreateStats *modified)
{
    if (!rolled || !modified) {
        return;
    }
    *modified = *rolled;
    switch (race) {
    case 1: /* Elf */
        if (modified->might > 0) {
            modified->might--;
        }
        modified->intelligence++;
        if (modified->endurance > 0) {
            modified->endurance--;
        }
        modified->accuracy++;
        break;
    case 2: /* Dwarf */
        if (modified->intelligence > 0) {
            modified->intelligence--;
        }
        modified->endurance++;
        if (modified->speed > 0) {
            modified->speed--;
        }
        modified->luck++;
        break;
    case 3: /* Gnome */
        if (modified->speed > 0) {
            modified->speed--;
        }
        if (modified->accuracy > 0) {
            modified->accuracy--;
        }
        modified->luck += 2;
        break;
    case 4: /* Half-Orc */
        modified->might++;
        if (modified->intelligence > 0) {
            modified->intelligence--;
        }
        if (modified->personality > 0) {
            modified->personality--;
        }
        modified->endurance++;
        if (modified->luck > 0) {
            modified->luck--;
        }
        break;
    default:
        break;
    }
}

void mm2_create_swap_stats(Mm2CreateStats *stats, int index_a, int index_b)
{
    uint8_t *a;
    uint8_t *b;
    uint8_t tmp;
    if (!stats || index_a < 0 || index_b < 0 || index_a >= MM2_CREATE_STAT_COUNT
        || index_b >= MM2_CREATE_STAT_COUNT || index_a == index_b) {
        return;
    }
    a = stat_ptr(stats, index_a);
    b = stat_ptr(stats, index_b);
    if (!a || !b) {
        return;
    }
    tmp = *a;
    *a = *b;
    *b = tmp;
}

int mm2_create_endurance_hp_bonus(uint8_t endurance)
{
    return bonus_tier(endurance);
}

int mm2_create_speed_ac_bonus(uint8_t speed)
{
    return bonus_tier(speed);
}

int mm2_create_primary_sp_per_level(int class_id, const Mm2CreateStats *modified)
{
    uint8_t attr = 0;
    if (!modified) {
        return 0;
    }
    switch (class_id) {
    case 3:
    case 1:
        attr = modified->personality;
        break;
    case 4:
    case 2:
        attr = modified->intelligence;
        break;
    default:
        return 0;
    }
    return sp_per_level_table(bonus_tier(attr));
}

int mm2_create_class_hp_base(int class_id)
{
    static const int kHp[] = {12, 10, 10, 8, 6, 8, 10, 15};
    if (class_id < 0 || class_id >= MM2_CREATE_CLASS_COUNT) {
        return 8;
    }
    return kHp[class_id];
}

uint8_t mm2_create_starting_thievery(int class_id)
{
    if (class_id == 5) {
        return 30;
    }
    if (class_id == 6) {
        return 10;
    }
    return 0;
}

void mm2_create_build_record(const Mm2PendingCharacter *pending, Mm2RosterRecord *out)
{
    int hp_base;
    int hp_bonus;
    int hp_max;
    int sp_max;
    int ac;

    if (!pending || !out) {
        return;
    }
    if (pending->class_id < 0 || pending->race < 0 || pending->alignment < 0 || pending->sex < 0) {
        return;
    }

    memset(out, 0, sizeof(*out));
    mm2_roster_set_name(out, pending->name);

    out->town_flags = (uint8_t)(MM2_CREATE_START_TOWN & 0x7F);
    out->sex = (uint8_t)pending->sex;
    out->alignment_current = (uint8_t)pending->alignment;
    out->alignment_base = (uint8_t)pending->alignment;
    out->race = (uint8_t)pending->race;
    out->class_id = (uint8_t)pending->class_id;

    out->might_current = pending->modified.might;
    out->intelligence_current = pending->modified.intelligence;
    out->personality_current = pending->modified.personality;
    out->endurance_current = pending->modified.endurance;
    out->speed_current = pending->modified.speed;
    out->accuracy_current = pending->modified.accuracy;
    out->luck_current = pending->modified.luck;

    out->might_base = pending->modified.might;
    out->intelligence_base = pending->modified.intelligence;
    out->personality_base = pending->modified.personality;
    out->endurance_base = pending->modified.endurance;
    out->speed_base = pending->modified.speed;
    out->accuracy_base = pending->modified.accuracy;
    out->luck_base = pending->modified.luck;

    out->thievery_percent = mm2_create_starting_thievery(pending->class_id);
    out->age = MM2_CREATE_START_AGE;
    out->food = MM2_CREATE_START_FOOD;
    out->condition = 0;
    out->level = 1;
    out->spell_level = 0;
    out->experience = 0;
    out->gold = MM2_CREATE_START_GOLD;
    out->gems = 0;

    hp_base = mm2_create_class_hp_base(pending->class_id);
    hp_bonus = mm2_create_endurance_hp_bonus(pending->modified.endurance);
    hp_max = hp_base + hp_bonus;
    out->hp_max = (uint16_t)hp_max;
    out->hp_current = (uint16_t)hp_max;
    out->hp_aux = (uint16_t)hp_max;

    sp_max = mm2_create_primary_sp_per_level(pending->class_id, &pending->modified);
    out->sp_max = (uint16_t)sp_max;
    out->sp_current = (uint16_t)sp_max;

    ac = mm2_create_speed_ac_bonus(pending->modified.speed);
    out->armor_class = (uint8_t)ac;
}

void mm2_roster_refresh_spell_points(Mm2RosterRecord *rec)
{
    Mm2CreateStats stats;
    int per_level;
    int total;

    if (!rec || rec->spell_level == 0 || rec->sp_max != 0) {
        return;
    }

    stats.might = rec->might_current;
    stats.intelligence = rec->intelligence_current;
    stats.personality = rec->personality_current;
    stats.endurance = rec->endurance_current;
    stats.speed = rec->speed_current;
    stats.accuracy = rec->accuracy_current;
    stats.luck = rec->luck_current;

    per_level = mm2_create_primary_sp_per_level(rec->class_id, &stats);
    if (per_level <= 0) {
        return;
    }

    total = per_level * (int)rec->spell_level;
    if (total <= 0) {
        return;
    }

    rec->sp_max = (uint16_t)total;
    if (rec->sp_current == 0) {
        rec->sp_current = rec->sp_max;
    }
}

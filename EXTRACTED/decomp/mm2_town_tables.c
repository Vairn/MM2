#include "mm2_town_tables.h"

/*
 * Per-town commerce constants (map id 0..4). Values cross-checked against
 * EXTRACTED/docs/28-town-services.md §13.1 and the 68k disassembly:
 *
 *   training index  [1, 5, 2, 4, 2]   FAQ §3-6 (doc 34 §13.2) -- NOT map order
 *   stat add        [5, 20, 10, 10, 3] A4-$6720 (training_stat_apply 0x1C898)
 *   stat cap        [100,100,100,100,50] A4-$671A (cmp @ 0x1C8A8)
 *   donation gold   [20, 250, 40, 120, 40] A4-$6742 (debit 0x1CA3A, show 0x1D556)
 *   donation bit    [1, 2, 4, 8, 16]   A4-$66B1 (OR into A4-$799E, doc 28 §5.2)
 */
static const Mm2TownCommerce kTowns[MM2_TOWN_COUNT] = {
    /* idx, add, cap, donation, bit */
    {1, 5, 100, 20, 0x01},   /* 0 Middlegate */
    {5, 20, 100, 250, 0x02}, /* 1 Atlantium  */
    {2, 10, 100, 40, 0x04},  /* 2 Tundara    */
    {4, 10, 100, 120, 0x08}, /* 3 Vulcania   */
    {2, 3, 50, 40, 0x10},    /* 4 Sandsobar  */
};

/* Stat id 0..6 -> roster record byte offset, jump table @ 0x1C86C / 0x1C898. */
static const int8_t kStatFieldOffset[MM2_TOWN_STAT_COUNT] = {
    0x6B, /* 0 might_base       */
    0x6F, /* 1 accuracy_base    */
    0x6D, /* 2 personality_base */
    0x6C, /* 3 intelligence_base*/
    0x71, /* 4 level            */
    0x72, /* 5 spell_level      */
    0x6E, /* 6 speed_base       */
};

int mm2_town_commerce(int map_id, Mm2TownCommerce *out)
{
    if (!out || map_id < 0 || map_id >= MM2_TOWN_COUNT) {
        return 0;
    }
    *out = kTowns[map_id];
    return 1;
}

int mm2_town_stat_field_offset(int stat_id)
{
    if (stat_id < 0 || stat_id >= MM2_TOWN_STAT_COUNT) {
        return -1;
    }
    return kStatFieldOffset[stat_id];
}

uint32_t mm2_town_training_cost(int level, int town_index)
{
    if (level < 0 || town_index < 0) {
        return 0;
    }
    return (uint32_t)level * (uint32_t)town_index * 50u;
}

uint32_t mm2_town_healing_cost(int level, int town_index)
{
    if (level < 0 || town_index < 0) {
        return 0;
    }
    return (uint32_t)level * (uint32_t)town_index * 10u;
}

/* ------------------------------------------------------------------------- *
 * Training-hall level progression (doc 32 "Experience to next level").
 *
 * Group A (~25% less XP): Knight(0), Cleric(3), Robber(5), Barbarian(7).
 * Group B:               Paladin(1), Archer(2), Sorcerer(4), Ninja(6).
 * Tables are in raw XP (doc 32 prints thousands). Index = level-2 (levels 2..77).
 */
#define MM2_XP_FIRST_LEVEL 2
#define MM2_XP_LAST_LEVEL 77
/* Raw XP = doc-32 "thousands" column * 1000. Index = level - 2 (levels 2..77). */
static const uint32_t kXpGroupA[MM2_XP_LAST_LEVEL - MM2_XP_FIRST_LEVEL + 1] = {
    1500u,     3000u,     6000u,     12000u,     /* 2..5   */
    24000u,    48000u,    96000u,    192000u,    /* 6..9   */
    384000u,   576000u,   768000u,   960000u,    /* 10..13 */
    1344000u,  1728000u,  2496000u,  3264000u,   /* 14..17 */
    4032000u,  4800000u,  5568000u,  7104000u,   /* 18..21 */
    8640000u,  10276000u, 11712000u, 13248000u,  /* 22..25 */
    14784000u, 16320000u, 17856000u, 19392000u,  /* 26..29 */
    20928000u, 24000000u, 27072000u, 30144000u,  /* 30..33 */
    33216000u, 36288000u, 39360000u, 42432000u,  /* 34..37 */
    45504000u, 48576000u, 51648000u, 54720000u,  /* 38..41 */
    57792000u, 60864000u, 63936000u, 67008000u,  /* 42..45 */
    70080000u, 73152000u, 76224000u, 79296000u,  /* 46..49 */
    82368000u, 88512000u, 94656000u, 100800000u, /* 50..53 */
    106944000u, 113088000u, 119232000u, 125376000u, /* 54..57 */
    131520000u, 137664000u, 143808000u, 149952000u, /* 58..61 */
    156096000u, 162240000u, 168384000u, 174428000u, /* 62..65 */
    180672000u, 186816000u, 192960000u, 199104000u, /* 66..69 */
    205248000u, 211392000u, 217536000u, 223680000u, /* 70..73 */
    229824000u, 235968000u, 252352000u, 268736000u, /* 74..77 */
};
static const uint32_t kXpGroupB[MM2_XP_LAST_LEVEL - MM2_XP_FIRST_LEVEL + 1] = {
    2000u,     4000u,     8000u,     16000u,     /* 2..5   */
    32000u,    64000u,    128000u,   256000u,    /* 6..9   */
    512000u,   768000u,   1024000u,  1280000u,   /* 10..13 */
    1728000u,  2304000u,  3328000u,  4352000u,   /* 14..17 */
    5376000u,  6400000u,  7424000u,  9472000u,   /* 18..21 */
    11520000u, 13568000u, 15616000u, 17664000u,  /* 22..25 */
    19712000u, 21760000u, 23808000u, 25832000u,  /* 26..29 */
    27856000u, 32000000u, 36096000u, 40192000u,  /* 30..33 */
    44288000u, 48384000u, 52480000u, 56576000u,  /* 34..37 */
    60672000u, 64768000u, 68864000u, 72960000u,  /* 38..41 */
    77056000u, 81152000u, 85248000u, 89344000u,  /* 42..45 */
    93440000u, 97536000u, 101632000u, 105728000u, /* 46..49 */
    109824000u, 118016000u, 126208000u, 134400000u, /* 50..53 */
    142592000u, 150784000u, 158976000u, 167168000u, /* 54..57 */
    175360000u, 183552000u, 191744000u, 199936000u, /* 58..61 */
    208128000u, 216320000u, 224512000u, 232704000u, /* 62..65 */
    240896000u, 249088000u, 257280000u, 265472000u, /* 66..69 */
    273664000u, 281856000u, 290048000u, 298240000u, /* 70..73 */
    306432000u, 314624000u, 331008000u, 347392000u, /* 74..77 */
};

int mm2_class_xp_group(uint8_t class_id)
{
    switch (class_id) {
    case 0: /* Knight    */
    case 3: /* Cleric    */
    case 5: /* Robber    */
    case 7: /* Barbarian */
        return 0;
    case 1: /* Paladin   */
    case 2: /* Archer    */
    case 4: /* Sorcerer  */
    case 6: /* Ninja     */
        return 1;
    default:
        return -1;
    }
}

uint32_t mm2_class_xp_for_level(uint8_t class_id, int level)
{
    const int group = mm2_class_xp_group(class_id);
    if (group < 0 || level <= 1) {
        return 0;
    }
    if (level < MM2_XP_FIRST_LEVEL || level > MM2_XP_LAST_LEVEL) {
        return 0xFFFFFFFFu;
    }
    return (group == 0) ? kXpGroupA[level - MM2_XP_FIRST_LEVEL]
                        : kXpGroupB[level - MM2_XP_FIRST_LEVEL];
}

/* HP per level at END 11-12 (doc 32), indexed by class id 0..7. */
static const uint8_t kClassHpPerLevel[8] = {
    12, /* 0 Knight    */
    10, /* 1 Paladin   */
    10, /* 2 Archer    */
    8,  /* 3 Cleric    */
    6,  /* 4 Sorcerer  */
    8,  /* 5 Robber    */
    10, /* 6 Ninja     */
    15, /* 7 Barbarian */
};

int mm2_class_hp_per_level(uint8_t class_id)
{
    return (class_id < 8) ? (int)kClassHpPerLevel[class_id] : 0;
}

int mm2_attr_bonus(int v)
{
    /* doc 32 "Attribute -> bonuses per level" HP(END)/AC(SPD) column. */
    if (v < 13) return 0;
    if (v < 15) return 1;
    if (v < 17) return 2;
    if (v < 19) return 3;
    if (v < 22) return 4;
    if (v < 26) return 5;
    if (v < 30) return 6;
    if (v < 45) return 7;
    if (v < 60) return 8;
    if (v < 75) return 9;
    if (v < 90) return 10;
    if (v < 105) return 11;
    if (v < 120) return 12;
    if (v < 135) return 13;
    if (v < 150) return 14;
    if (v < 175) return 15;
    if (v < 200) return 16;
    if (v < 225) return 17;
    return 18;
}

int mm2_class_caster_stat(uint8_t class_id)
{
    switch (class_id) {
    case 2: /* Archer   */
    case 4: /* Sorcerer */
        return 1; /* INT */
    case 1: /* Paladin  */
    case 3: /* Cleric   */
        return 2; /* PER */
    default:
        return 0;
    }
}

int mm2_class_spell_level_for(uint8_t class_id, int char_level)
{
    if (char_level < 1) {
        return 0;
    }
    switch (class_id) {
    case 3: /* Cleric   */
    case 4: /* Sorcerer */
        /* pure casters: gain spell level X at char level 2X-1 -> X = (L+1)/2. */
        return (char_level + 1) / 2;
    case 1: /* Paladin  */
    case 2: /* Archer   */ {
        /* fighter-mages: gain X at char level 2X+5; natural cap 7. */
        int x = (char_level - 5) / 2;
        if (x < 0) x = 0;
        if (x > 7) x = 7;
        return x;
    }
    default:
        return 0;
    }
}

/* Blacksmith static item-id tables [category][map][slot], decoded byte-exact from
 * the data hunk (A4-$68EE/$683A/$68B2/$6876). town*6 + slot @ 0x1C00E. */
static const uint8_t kSmithIds[MM2_SMITH_CATEGORY_COUNT][MM2_TOWN_COUNT][MM2_SMITH_SLOTS] = {
    /* Weapons (A4-$68EE) */
    {
        {4, 6, 7, 10, 12, 13},     /* Middlegate */
        {15, 19, 20, 17, 16, 10},  /* Atlantium  */
        {8, 18, 17, 22, 23, 24},   /* Tundara    */
        {9, 18, 12, 14, 4, 13},    /* Vulcania   */
        {16, 15, 14, 19, 21, 20},  /* Sandsobar  */
    },
    /* Specials / Today's Specials (A4-$683A) */
    {
        {66, 67, 68, 92, 93, 95},  /* Middlegate */
        {97, 77, 78, 75, 73, 23},  /* Atlantium  */
        {96, 71, 74, 15, 19, 20},  /* Tundara    */
        {24, 72, 76, 22, 17, 21},  /* Vulcania   */
        {94, 69, 70, 9, 10, 11},   /* Sandsobar  */
    },
    /* Armor (A4-$68B2) */
    {
        {115, 127, 128, 129, 130, 155}, /* Middlegate */
        {155, 117, 127, 131, 132, 134}, /* Atlantium  */
        {117, 131, 132, 133, 134, 155}, /* Tundara    */
        {155, 116, 127, 129, 130, 133}, /* Vulcania   */
        {116, 128, 129, 130, 131, 132}, /* Sandsobar  */
    },
    /* Misc (A4-$6876) */
    {
        {161, 162, 160, 163, 164, 208}, /* Middlegate */
        {170, 167, 163, 187, 189, 211}, /* Atlantium  */
        {169, 176, 177, 178, 181, 189}, /* Tundara    */
        {171, 172, 195, 186, 184, 210}, /* Vulcania   */
        {168, 175, 162, 165, 166, 209}, /* Sandsobar  */
    },
};

/* Blacksmith meta bytes [category][map][slot] (A4-$68D0 weapons, $6894 armor,
 * $6858 misc). Specials meta is date-rolled @ 0x1C146 and is left 0 here. */
static const uint8_t kSmithMeta[MM2_SMITH_CATEGORY_COUNT][MM2_TOWN_COUNT][MM2_SMITH_SLOTS] = {
    /* Weapons (A4-$68D0) */
    {
        {0, 0, 0, 0, 0, 0},  /* Middlegate */
        {3, 2, 2, 3, 3, 5},  /* Atlantium  */
        {1, 0, 0, 0, 0, 0},  /* Tundara    */
        {1, 1, 2, 2, 4, 1},  /* Vulcania   */
        {0, 0, 0, 0, 0, 0},  /* Sandsobar  */
    },
    /* Specials: bonus is DATE-ROLLED @ 0x1C146 (A4-$79B6 -> $681C/$6816), not a
     * static table — deferred, kept 0. */
    {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
    },
    /* Armor (A4-$6894) */
    {
        {0, 0, 0, 0, 0, 0},  /* Middlegate */
        {4, 1, 3, 2, 1, 1},  /* Atlantium  */
        {0, 0, 0, 0, 0, 1},  /* Tundara    */
        {2, 1, 1, 1, 1, 1},  /* Vulcania   */
        {0, 0, 0, 0, 0, 0},  /* Sandsobar  */
    },
    /* Misc (A4-$6858) — these are item charge counts, not magic '+' bonuses. */
    {
        {1, 20, 3, 0, 1, 0},    /* Middlegate */
        {2, 50, 0, 25, 20, 0},  /* Atlantium  */
        {5, 8, 20, 8, 20, 10},  /* Tundara    */
        {5, 2, 20, 35, 5, 0},   /* Vulcania   */
        {3, 1, 20, 250, 50, 0}, /* Sandsobar  */
    },
};

int mm2_smith_inventory(int map_id, int category, Mm2SmithSlot out[MM2_SMITH_SLOTS])
{
    int slot;
    if (!out || map_id < 0 || map_id >= MM2_TOWN_COUNT || category < 0 ||
        category >= MM2_SMITH_CATEGORY_COUNT) {
        return 0;
    }
    for (slot = 0; slot < MM2_SMITH_SLOTS; ++slot) {
        out[slot].item_id = kSmithIds[category][map_id][slot];
        out[slot].meta = kSmithMeta[category][map_id][slot];
    }
    return 1;
}

void mm2_smith_buy_fields(int category, uint8_t meta, uint8_t *out_price_meta,
                          uint8_t *out_charges, uint8_t *out_flags)
{
    uint8_t price_meta = 0;
    uint8_t charges = 0;
    uint8_t flags = 0;
    if (category == MM2_SMITH_MISC) {
        /* 0x1C1CC swap: meta -> charges (+$40), flags (+$46) cleared, price uses 0. */
        charges = meta;
    } else if (category == MM2_SMITH_WEAPONS || category == MM2_SMITH_ARMOR) {
        /* -$5A1E holds the '+' bonus: drives price (0x1BF16) and lands in flags. */
        price_meta = (uint8_t)(meta & 0x3F);
        flags = meta;
    }
    /* Specials: -$5A1E overwritten by the date-roll (deferred); fields stay 0. */
    if (out_price_meta) {
        *out_price_meta = price_meta;
    }
    if (out_charges) {
        *out_charges = charges;
    }
    if (out_flags) {
        *out_flags = flags;
    }
}

uint32_t mm2_smith_price(uint32_t base_gold, uint8_t meta)
{
    uint8_t bonus = (uint8_t)(meta & 0x3F);
    uint32_t price = base_gold;
    if (bonus != 0) {
        /* 0x1BF16: price = base*2, then +1000 for each remaining '+' point. */
        price = base_gold * 2u;
        --bonus;
        while (bonus != 0) {
            price += 1000u;
            --bonus;
        }
    }
    return price;
}

uint32_t mm2_smith_sell_price(uint32_t buy_style_price)
{
    /* 0x1BFCC: divu.w #2 on the buy-style price (Merchant /2 deferred). */
    return buy_style_price / 2u;
}

uint32_t mm2_smith_identify_cost(uint8_t backpack_flags)
{
    const uint8_t meta = (uint8_t)(backpack_flags & 0x3Fu);
    if (meta == 0) {
        return 10u;
    }
    /* Midpoint of rng(1, meta*100) @ 0x1BF60 — deterministic until shop RNG wired. */
    return (1u + (uint32_t)meta * 100u) / 2u;
}

/* Mage guild sorcerer spells [map][slot] = {spell_index 0..47, gold}. Decoded
 * from EXTRACTED/shop_tables.json static_by_town.mage_guild_spells (data hunk
 * A4-$66E2 ids, cost decode 0x1D97A). */
static const Mm2SpellShopSlot kMageGuildStock[MM2_TOWN_COUNT][MM2_MAGE_GUILD_SLOTS] = {
    {{0, 10}, {2, 1000}, {6, 50}, {9, 100}},         /* Middlegate */
    {{41, 50000}, {42, 50000}, {44, 100000}, {45, 100000}}, /* Atlantium */
    {{21, 600}, {22, 2000}, {26, 3000}, {28, 3000}},        /* Tundara */
    {{31, 5000}, {33, 5000}, {35, 5000}, {37, 25000}},      /* Vulcania */
    {{13, 400}, {14, 200}, {17, 1000}, {20, 500}},          /* Sandsobar */
};

/* Temple cleric spell-purchase slots (menu D/E/F) [map][slot] = {spell_index
 * 0..47, gold}. spell_index = spells_dat_index - 48 (cleric school offset).
 * Decoded from EXTRACTED/shop_tables.json static_by_town.temple_spells (data
 * hunk A4-$66F6, decode 0x1DAC6). */
static const Mm2SpellShopSlot kTempleSpellStock[MM2_TOWN_COUNT][MM2_TEMPLE_SPELL_SLOTS] = {
    {{0, 10}, {1, 10}, {5, 1000}},        /* Middlegate */
    {{42, 20000}, {46, 50000}, {47, 100000}}, /* Atlantium */
    {{14, 400}, {18, 100}, {23, 500}},        /* Tundara */
    {{25, 2000}, {30, 3000}, {37, 10000}},    /* Vulcania */
    {{8, 250}, {11, 300}, {13, 200}},         /* Sandsobar */
};

/* Guild-membership bit in record+0x79 (class_quest_guild_mask), data hunk
 * A4-$66A9. Writable via the generic event-script field selector 0x74 (ported,
 * EventFieldMap.h) or the unported/buggy 0x9D76 class-quest reward loop @
 * 0x9FE0 -- see mm2_town_tables.h and doc 36-class-quest-hp-bug.md. */
static const uint8_t kMageGuildMemberMask[MM2_TOWN_COUNT] = {0x02, 0x04, 0x08, 0x10, 0x20};

int mm2_mage_guild_stock(int map_id, Mm2SpellShopSlot out[MM2_MAGE_GUILD_SLOTS])
{
    int i;
    if (!out || map_id < 0 || map_id >= MM2_TOWN_COUNT) {
        return 0;
    }
    for (i = 0; i < MM2_MAGE_GUILD_SLOTS; ++i) {
        out[i] = kMageGuildStock[map_id][i];
    }
    return 1;
}

int mm2_temple_spell_stock(int map_id, Mm2SpellShopSlot out[MM2_TEMPLE_SPELL_SLOTS])
{
    int i;
    if (!out || map_id < 0 || map_id >= MM2_TOWN_COUNT) {
        return 0;
    }
    for (i = 0; i < MM2_TEMPLE_SPELL_SLOTS; ++i) {
        out[i] = kTempleSpellStock[map_id][i];
    }
    return 1;
}

uint8_t mm2_mage_guild_member_mask(int map_id)
{
    if (map_id < 0 || map_id >= MM2_TOWN_COUNT) {
        return 0;
    }
    return kMageGuildMemberMask[map_id];
}

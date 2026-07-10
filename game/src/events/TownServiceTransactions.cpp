#include "mm2/events/TownServiceTransactions.h"

#include "mm2/gameplay/ExploreActions.h"
#include "mm2/gameplay/RosterSkills.h"
#include "mm2/gameplay/SpellBook.h"

#include "mm2/CppStdCompat.h"

#include "mm2_found_items.h"
#include "mm2_gamestate.h"
#include "mm2_items_codec.h"
#include "mm2_town_tables.h"

namespace mm2::events {

namespace {

/* -$7F56 / 0x4442 — same table as Rest SP / training HP (A4-$7486). */
uint8_t attrTableBonus(uint8_t attr)
{
    static const uint8_t kThresh[] = {4,  6,  9,  13, 15, 17, 19, 22, 26, 30, 45,
                                      60, 75, 90, 105, 120, 135, 150, 175, 200, 225, 250, 255};
    uint8_t bonus = 0xFD;
    for (size_t i = 0; i < sizeof(kThresh); ++i) {
        if (attr <= kThresh[i]) {
            break;
        }
        ++bonus;
    }
    return bonus;
}

/* A4-$6738 costs; BE u16 ×6 — tavern B. */
static const uint16_t kStatBoostCosts[6] = {5, 5, 20, 20, 50, 100};
/* A4-$672C BE u16 ×6 — tavern B purchase-count limits @ 0x1CBEA. */
static const uint16_t kStatBoostLimits[6] = {2, 3, 3, 3, 3, 5};
/* Runtime counters live at A4-$577E (word[6]); BSS, start at 0. */
constexpr int32_t kGsTavernStatBuyCount = -0x577E;

/* Today's Specials date tables @ A4-$6816 / -$681C (0x1C146). */
static const uint8_t kSpecialsDayBonus[30] = {0, 1, 0, 1, 2, 0, 1, 0, 1, 2, 0, 1, 0, 1, 3,
                                             0, 1, 0, 1, 4, 0, 1, 0, 2, 0, 3, 0, 4, 0, 0};
static const uint8_t kSpecialsMonthBonus[6] = {5, 6, 7, 8, 9, 12};

constexpr uint8_t kSkillMerchant = 10;

/* A4-$6760 BE u16[town*3+menu] — specialties gold @ 0x1CEA4. */
static const uint16_t kSpecialtyGold[5][3] = {
    {10, 50, 100}, {1000, 2000, 3000}, {200, 100, 1000}, {5000, 500, 1000}, {20, 50, 250},
};

/* A4-$786C BE u16[town*3+menu] — OR'd into record+$76 @ 0x1C8D4. */
static const uint16_t kSpecialtyMask[5][3] = {
    {1, 2, 4}, {4096, 8192, 16384}, {64, 128, 256}, {512, 1024, 2048}, {8, 16, 32},
};

/* A4-$6B08 tier rows (8 bytes × menu A/B/C) + A4-$6B1A addends (6 × menu). */
static const uint8_t kFoodTier[3][8] = {
    {56, 24, 13, 6, 3, 8, 2, 1},
    {66, 29, 6, 7, 7, 15, 2, 1},
    {37, 12, 7, 10, 2, 5, 1, 1},
};
static const uint8_t kFoodAddends[3][6] = {
    {1, 66, 92, 115, 127, 155},
    {25, 79, 98, 118, 135, 157},
    {54, 85, 105, 125, 150, 159},
};

/* A4-$6AF0 base / A4-$6AED addend — drink encode @ 0x18F78. */
static const uint8_t kDrinkBase[6] = {48, 64, 48, 31, 79, 143};
static const uint8_t kDrinkAddend[6] = {31, 79, 143, 48, 64, 80};

/* A4-$6AEA thresholds / A4-$6AE0 gold longs — drink apply @ 0x18D3A (data hunk). */
static const uint8_t kDrinkGoldThresh[10] = {0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0};
static const uint32_t kDrinkGoldAward[10] = {2000, 4000, 5000, 7000, 10000, 15000, 25000, 50000, 100000, 250000};

/* Encode food gold byte @ 0x18EC0 (no gp deduct). */
uint8_t encodeFoodByte(int menu, gameplay::Rng *rng)
{
    if (menu < 0 || menu > 2) {
        menu = 0;
    }
    const uint8_t *tiers = kFoodTier[menu];
    const int hi = static_cast<int>(tiers[0]);
    int roll = rng ? rng->range(1, hi > 0 ? hi : 1) : 1;
    uint8_t enc = static_cast<uint8_t>(roll);
    if (enc > 0) {
        --enc;
    }
    int i = 1;
    for (; i < 7; ++i) {
        if (enc < tiers[i]) {
            break;
        }
        enc = static_cast<uint8_t>(enc - tiers[i + 1]);
    }
    --i;
    if (i < 0) {
        i = 0;
    }
    if (i > 5) {
        i = 5;
    }
    enc = static_cast<uint8_t>(enc + kFoodAddends[menu][i]);
    return enc;
}

uint8_t encodeDrinkByte(int drink, gameplay::Rng *rng)
{
    if (drink < 0 || drink > 5) {
        drink = 0;
    }
    const int hi = static_cast<int>(kDrinkBase[drink]);
    int roll = rng ? rng->range(1, hi > 0 ? hi : 1) : 1;
    return static_cast<uint8_t>(static_cast<uint8_t>(roll) + kDrinkAddend[drink]);
}

void writePartyEncoding(Mm2RosterFile *roster, const Mm2PartyLaunch *launch, uint8_t encoding,
                        bool drink_mode, TownSvcFoodEncodeResult &r)
{
    if (!roster || !launch) {
        return;
    }
    r.encoding = encoding;
    for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch->roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        auto *raw = reinterpret_cast<uint8_t *>(&roster->records[idx]);
        raw[0x78] = encoding;
        raw[0x7C] = static_cast<uint8_t>((raw[0x7C] & 0xFE) | (drink_mode ? 1u : 0u));
        ++r.members_written;
    }
    r.ok = r.members_written > 0;
}


/* 0x452c: saturate-subtract amount from *field (unsigned byte). */
void satSubByte(uint8_t *field, uint8_t amount)
{
    if (!field) {
        return;
    }
    if (*field < amount) {
        *field = 0;
    } else {
        *field = static_cast<uint8_t>(*field - amount);
    }
}

/* 0x4552: saturate-add amount to *field (cap 0xFF). */
void satAddByte(uint8_t *field, uint8_t amount)
{
    if (!field) {
        return;
    }
    const int sum = static_cast<int>(*field) + static_cast<int>(amount);
    *field = static_cast<uint8_t>(sum > 0xFF ? 0xFF : sum);
}

/* 0xA3AE jump table keyed on skill nibble 1..15 (0 / 3 / 4 / 10..13 = nop). */
void generalStoreApplySkillNibble(Mm2RosterRecord &rec, uint8_t skill_id)
{
    auto *raw = reinterpret_cast<uint8_t *>(&rec);
    switch (skill_id) {
    case 1: /* 0xA3BC: −5 accuracy current+base */
        satSubByte(raw + 0x14, 5);
        satSubByte(raw + 0x6F, 5);
        break;
    case 2: /* 0xA3E4: −5 speed current+base */
        satSubByte(raw + 0x13, 5);
        satSubByte(raw + 0x6E, 5);
        break;
    case 5: /* 0xA40C: −5 personality current+base */
        satSubByte(raw + 0x12, 5);
        satSubByte(raw + 0x6D, 5);
        break;
    case 6: /* 0xA434: −5 luck current+base */
        satSubByte(raw + 0x15, 5);
        satSubByte(raw + 0x70, 5);
        break;
    case 7: /* 0xA45C: −5 might current+base */
        satSubByte(raw + 0x10, 5);
        satSubByte(raw + 0x6B, 5);
        break;
    case 8: /* 0xA484: −1 to all six currents then all six bases */
        for (int off : {0x10, 0x11, 0x12, 0x13, 0x14, 0x15}) {
            satSubByte(raw + off, 1);
        }
        for (int off : {0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70}) {
            satSubByte(raw + off, 1);
        }
        satSubByte(raw + 0x27, 1); /* endurance_current */
        satSubByte(raw + 0x73, 1); /* endurance_base */
        break;
    case 9: /* 0xA596: −5 intelligence current+base */
        satSubByte(raw + 0x11, 5);
        satSubByte(raw + 0x6C, 5);
        break;
    case 14: /* 0xA5BC: −15 age */
        satSubByte(raw + 0x1E, 15);
        break;
    case 15: /* 0xA5D0: −5 endurance current + endurance base */
        satSubByte(raw + 0x27, 5);
        satSubByte(raw + 0x73, 5);
        break;
    default:
        break;
    }
}

constexpr const char *kPubDrinkNames[6] = {
    "Orc Beer", "Straight Shot", "ID Elixir", "Academic Ale", "Rare Vintage", "Mystic Brew",
};

}  // namespace

bool townSvcCharGoldDeduct(Mm2RosterRecord &rec, uint32_t amount)
{
    /* 0x1C9C0: cmp.l amount, $66(a0); scc -> (gold >= amount). Only deduct when
     * the character can afford the full amount (no partial spend). */
    if (rec.gold >= amount) {
        rec.gold -= amount;
        return true;
    }
    return false;
}

bool townSvcRestoreHp(Mm2RosterRecord &rec)
{
    /* 0x1DD48: max = $60 (hp_aux); if $5E (hp_max) < max -> $5E = $74 = max. */
    const uint16_t permanent_max = rec.hp_aux;
    if (rec.hp_max < permanent_max) {
        rec.hp_max = permanent_max;
        rec.hp_current = permanent_max;
        return true;
    }
    return false;
}

void townSvcRestoreCondition(Mm2RosterRecord &rec)
{
    /* 0x1D736: clr.b $26(a0). */
    rec.condition = 0;
}

uint32_t townSvcTempleHealCost(const Mm2RosterRecord &rec, int map_id)
{
    /* 0x1DCA2: base by condition, then ×level ×A4-$6714. */
    Mm2TownCommerce town;
    if (!mm2_town_commerce(map_id, &town)) {
        return 0;
    }
    uint32_t base = 0;
    if (rec.condition == 0xFF) {
        base = 1000u; /* eradicated */
    } else if (rec.condition >= 0x80) {
        base = 100u; /* dead */
    } else if (rec.condition != 0 || rec.hp_max < rec.hp_aux) {
        base = 10u; /* afflicted or HP needs restore */
    } else {
        return 0;
    }
    uint32_t cost = base;
    if (rec.level != 0) {
        cost *= rec.level;
    }
    cost *= town.temple_cost_index;
    return cost;
}

uint32_t townSvcTempleAlignCost(const Mm2RosterRecord &rec, int map_id)
{
    /* 0x1DC3A: 0 if +$0D == +$6A; else 100 × level × A4-$6714. */
    if (rec.alignment_current == rec.alignment_base) {
        return 0;
    }
    Mm2TownCommerce town;
    if (!mm2_town_commerce(map_id, &town)) {
        return 0;
    }
    uint32_t cost = 100u;
    if (rec.level != 0) {
        cost *= rec.level;
    }
    cost *= town.temple_cost_index;
    return cost;
}

TownSvcHealResult townSvcHeal(Mm2RosterRecord &rec, uint32_t cost)
{
    /* 0x1D716: gold check (0x1D90C) -> on success 0x1DD48 (HP) + clr.b $26. */
    TownSvcHealResult r;
    r.cost = cost;
    if (!townSvcCharGoldDeduct(rec, cost)) {
        r.cost = 0;
        return r;
    }
    r.paid = true;
    r.hp_restored = townSvcRestoreHp(rec);
    townSvcRestoreCondition(rec);
    return r;
}

TownSvcAlignResult townSvcRestoreAlignment(Mm2RosterRecord &rec, uint32_t cost)
{
    /* 0x1D758: gold check then move.b $d(a0), $6a(a1). */
    TownSvcAlignResult r;
    r.cost = cost;
    if (cost > 0 && !townSvcCharGoldDeduct(rec, cost)) {
        r.cost = 0;
        return r;
    }
    if (cost == 0 && rec.alignment_current == rec.alignment_base) {
        /* Already matched — ASM still "succeeds" with zero debit. */
        r.paid = true;
        return r;
    }
    r.paid = true;
    rec.alignment_base = rec.alignment_current;
    r.restored = true;
    return r;
}

/* stat id 0..6 -> the decoded record's base-stat byte member. The ASM @ 0x1C86C
 * indexes the record by raw offset (0x6B/0x6F/0x6D/0x6C/0x71/0x72/0x6E); the
 * remake's Mm2RosterRecord is the *decoded* struct (compiler-aligned, NOT the
 * packed on-disk layout), so we resolve to the named members rather than raw
 * byte offsets into &rec. mm2_town_stat_field_offset() still records the literal
 * ASM offsets for documentation / the on-disk codec. */
static uint8_t *statFieldPtr(Mm2RosterRecord &rec, int stat_id)
{
    switch (stat_id) {
    case 0: return &rec.might_base;        /* 0x6B */
    case 1: return &rec.accuracy_base;     /* 0x6F */
    case 2: return &rec.personality_base;  /* 0x6D */
    case 3: return &rec.intelligence_base; /* 0x6C */
    case 4: return &rec.level;             /* 0x71 */
    case 5: return &rec.spell_level;       /* 0x72 */
    case 6: return &rec.speed_base;        /* 0x6E */
    default: return nullptr;
    }
}

bool townSvcTrainStat(Mm2RosterRecord &rec, int stat_id, int map_id)
{
    uint8_t *field = statFieldPtr(rec, stat_id);
    Mm2TownCommerce town;
    if (!field || !mm2_town_commerce(map_id, &town)) {
        return false;
    }
    /* 0x1C898: rolled = field + add (byte). Write only when add <= rolled < cap
     * (cap guard @ 0x1C8A8 bcc; overflow guard @ 0x1C8C2 bcs). */
    const uint8_t add = town.stat_train_add;
    const uint8_t cap = town.stat_train_cap;
    const uint8_t rolled = static_cast<uint8_t>(*field + add);
    if (rolled < cap && rolled >= add) {
        *field = rolled;
        return true;
    }
    return false;
}

TownSvcTrainResult townSvcTrainLevelUp(Mm2RosterRecord &rec, int map_id, gameplay::Rng *rng)
{
    TownSvcTrainResult r;
    r.old_level = rec.level;
    r.spell_level = rec.spell_level;
    Mm2TownCommerce town;
    if (!mm2_town_commerce(map_id, &town)) {
        return r;
    }

    /* XP gate: need the threshold for the NEXT level on this class's curve. */
    const int next_level = static_cast<int>(rec.level) + 1;
    const uint32_t threshold = mm2_class_xp_for_level(rec.class_id, next_level);
    r.new_level = static_cast<uint8_t>(next_level);
    r.required_xp = threshold;
    if (threshold == 0 || rec.experience < threshold) {
        r.new_level = rec.level;
        return r;
    }
    r.eligible = true;

    const uint32_t cost = mm2_town_training_cost(rec.level, town.training_town_index);
    r.cost = cost;
    if (!townSvcCharGoldDeduct(rec, cost)) {
        r.cost = 0;
        return r;
    }
    r.paid = true;

    rec.level = static_cast<uint8_t>(next_level);

    /* HP @ 0x20390..0x20478: class*$64EE[map]/$64E4[map] (+rem bump) +
     * -$7F56(+$27). Writes +$60/+ $74/+ $5E. Not 0x9BCA (bash door). */
    (void)rng;
    int hp_gain = mm2_train_hp_gain(rec.class_id, map_id, rec.endurance_current);
    if (hp_gain < 0) {
        hp_gain = 0;
    }
    const uint16_t gain16 = static_cast<uint16_t>(hp_gain);
    rec.hp_aux = static_cast<uint16_t>(rec.hp_aux + gain16);
    rec.hp_current = static_cast<uint16_t>(rec.hp_current + gain16);
    rec.hp_max = static_cast<uint16_t>(rec.hp_max + gain16);
    r.hp_gain = gain16;

    const int caster_stat = mm2_class_caster_stat(rec.class_id);
    if (caster_stat != 0) {
        const uint8_t stat_val =
            (caster_stat == 1) ? rec.intelligence_base : rec.personality_base;
        const int sp_gain = mm2_attr_bonus(stat_val) + 3;
        rec.sp_max = static_cast<uint16_t>(rec.sp_max + sp_gain);
        rec.sp_current = rec.sp_max;
        r.sp_gain = static_cast<uint16_t>(sp_gain);
    }

    const int new_sl = mm2_class_spell_level_for(rec.class_id, next_level);
    if (new_sl > rec.spell_level) {
        rec.spell_level = static_cast<uint8_t>(new_sl);
    }
    r.spell_level = rec.spell_level;
    r.leveled = true;
    return r;
}

TownSvcDonateResult townSvcTempleDonate(uint8_t *a4, Mm2RosterRecord &rec, int map_id,
                                        gameplay::Rng *rng)
{
    TownSvcDonateResult r;
    Mm2TownCommerce town;
    if (!mm2_town_commerce(map_id, &town)) {
        return r;
    }
    /* 0x1DC1A: A4-$6714[map] × 100 — not A4-$6742 (feeding frenzy). */
    r.cost = mm2_town_temple_donate_cost(map_id);
    if (!townSvcCharGoldDeduct(rec, r.cost)) {
        r.cost = 0;
        return r;
    }
    r.paid = true;
    if (a4) {
        /* 0x1D7B8: A4-$799E |= A4-$66B1[map]; 0x1F = all five temples done. */
        const uint8_t bits = static_cast<uint8_t>(
            mm2_gs_u8(a4, MM2_GS_TEMPLE_DONATION) | town.donation_quest_bit);
        mm2_gs_set_u8(a4, MM2_GS_TEMPLE_DONATION, bits);
        if (bits == MM2_TEMPLE_DONATION_ALL_BITS) {
            r.all_temples_donated = true;
            /* 0x1D7E8: move.b #$FE,-$794C; move.b #$D4,-$3F1C; clr.b -$799E. */
            mm2_gs_set_u8(a4, MM2_GS_FOUND_SENTINEL, MM2_GS_FOUND_SENTINEL_PENDING);
            mm2_gs_set_u8(a4, MM2_GS_FOUND_ITEM_ID, 0xD4);
            mm2_gs_set_u8(a4, MM2_GS_TEMPLE_DONATION, 0);
            r.reward_queued = true;
        }
        /* 0x1D7C4: -$7BB4(100,1); sgt vs 0x5A → blessed buff @ 0x1D7FE.
         * Independent of the 0x1F Farthing path. */
        const int bless_roll = rng ? rng->range(1, 100) : 1;
        if (bless_roll > 0x5A) {
            r.blessed = true;
            mm2_gs_set_u8(a4, MM2_GS_LIGHT_FACTOR, 0xC8);
            mm2_gs_set_u8(a4, MM2_GS_MAGIC_PROTECT, 0x3C);
            mm2_gs_set_u8(a4, MM2_GS_FORCES_PROTECT, 0x3C);
            mm2_gs_set_u8(a4, MM2_GS_LEVITATE_FLAG, 1);
            mm2_gs_set_u8(a4, MM2_GS_WALK_WATER_FLAG, 1);
            mm2_gs_set_u8(a4, MM2_GS_GUARD_DOG_FLAG, 1);
            mm2_gs_set_u8(a4, -0x79A5, 0); /* clr.b -$79A5 @ 0x1D82A */
            mm2_gs_set_u8(a4, -0x79A1, 1);
            mm2_gs_set_u8(a4, -0x79A2, 1);
            mm2_gs_set_u8(a4, -0x79A3, 1);
            mm2_gs_set_u8(a4, -0x79A4, 1); /* overlaps TALISMAN_BASE[0] — ASM writes it */
            mm2_gs_set_u8(a4, MM2_GS_CLASS_QUEST_CNT, 0xC8); /* -$79A0 */
            mm2_gs_set_u8(a4, MM2_GS_QUEST_COUNTER_B, 0xC8); /* -$799F */
            const uint16_t ctr = mm2_gs_u16(a4, MM2_GS_TEMPLE_BLESS_CTR);
            mm2_gs_set_u16(a4, MM2_GS_TEMPLE_BLESS_CTR, static_cast<uint16_t>(ctr + 1));
        }
    }
    return r;
}

uint32_t townSvcTrainingCost(int level, int map_id)
{
    Mm2TownCommerce town;
    if (!mm2_town_commerce(map_id, &town)) {
        return 0;
    }
    return mm2_town_training_cost(level, town.training_town_index);
}

uint32_t townSvcHealingCost(int level, int map_id)
{
    Mm2TownCommerce town;
    if (!mm2_town_commerce(map_id, &town)) {
        return 0;
    }
    /* Healthy heal path: level × A4-$6714 × 10 (0x1DCA2 base=10). */
    return mm2_town_healing_cost(level, town.temple_cost_index);
}

TownSvcBuyResult townSvcSmithBuy(Mm2RosterRecord &rec, uint8_t item_id, uint8_t charges,
                                 uint8_t flags, uint32_t price)
{
    TownSvcBuyResult r;
    const uint32_t pay = townSvcSmithMerchantBuyPrice(price, rec);
    r.price = pay;

    /* 0x1BE4C: a character with any condition ($26 != 0) cannot buy. */
    if (rec.condition != 0) {
        r.price = 0;
        r.reject = TownSvcBuyReject::Condition;
        return r;
    }

    /* 0x1BE60: first empty backpack slot (id-run +$3A); full -> caption 2. */
    int slot = -1;
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        if (mm2_roster_backpack(&rec, i).item_id == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        r.price = 0;
        r.reject = TownSvcBuyReject::BackpackFull;
        return r;
    }

    /* 0x1BE96 -> 0x1BDD6: gold check + deduct from the char's own gold. */
    if (!townSvcCharGoldDeduct(rec, pay)) {
        r.price = 0;
        r.reject = TownSvcBuyReject::NotEnoughGold;
        return r;
    }

    /* 0x1BEBC: write id/charges/flags into the empty backpack slot. */
    Mm2RosterItemSlot bought;
    bought.item_id = item_id;
    bought.charges = charges;
    bought.flags = flags;
    mm2_roster_set_backpack(&rec, slot, bought);

    r.bought = true;
    r.slot = slot;
    return r;
}

uint32_t townSvcSmithMerchantBuyPrice(uint32_t price, const Mm2RosterRecord &rec)
{
    /* 0x1BFF2: non-sell mode — Merchant (skill 0x0A) → price / 2. */
    if (gameplay::rosterHasSkillId(rec, kSkillMerchant)) {
        return price / 2u;
    }
    return price;
}

uint32_t townSvcSmithMerchantSellPrice(uint32_t sell_half_price, const Mm2RosterRecord &rec)
{
    /* 0x1BFCE..0x1BFEC: sell already /2; if NO Merchant, /2 again. */
    if (!gameplay::rosterHasSkillId(rec, kSkillMerchant)) {
        return sell_half_price / 2u;
    }
    return sell_half_price;
}

uint8_t townSvcSmithSpecialsDateBonus(const uint8_t *a4)
{
    /* 0x1C146: day = -$79DE[era]; rem = day%30; mon = day/30.
     * rem==$1D → -$681C[mon]; else -$6816[rem]. */
    if (!a4) {
        return 0;
    }
    const uint16_t era = mm2_gs_u16(a4, MM2_GS_ERA);
    const int ei = (era < MM2_GS_ERA_COUNT) ? static_cast<int>(era) : 0;
    const uint16_t day = mm2_gs_u16(a4, MM2_GS_DAY + ei * 2);
    const uint16_t rem = static_cast<uint16_t>(day % 30u);
    const uint16_t mon = static_cast<uint16_t>(day / 30u);
    if (rem == 0x1D) {
        const int mi = (mon < 6) ? static_cast<int>(mon) : 5;
        return kSpecialsMonthBonus[mi];
    }
    if (rem < 30) {
        return kSpecialsDayBonus[rem];
    }
    return 0;
}

TownSvcSellResult townSvcSmithSell(Mm2RosterRecord &rec, int backpack_slot, uint32_t price)
{
    TownSvcSellResult r;
    const uint32_t credit = townSvcSmithMerchantSellPrice(price, rec);
    r.price = credit;
    r.slot = backpack_slot;

    /* 0x1BC2E: afflicted characters cannot sell. */
    if (rec.condition != 0) {
        r.price = 0;
        r.reject = TownSvcSellReject::Condition;
        return r;
    }

    if (backpack_slot < 0 || backpack_slot >= MM2_ROSTER_ITEM_SLOTS) {
        r.price = 0;
        r.reject = TownSvcSellReject::NoItem;
        return r;
    }

    const Mm2RosterItemSlot slot = mm2_roster_backpack(&rec, backpack_slot);
    if (slot.item_id == 0) {
        r.price = 0;
        r.reject = TownSvcSellReject::NoItem;
        return r;
    }

    /* 0x1B62A: credit gold to the character's purse (record+$66). */
    rec.gold += credit;

    /* Clear the backpack slot (-$7F26 path @ 0x1BC26). */
    Mm2RosterItemSlot empty{};
    mm2_roster_set_backpack(&rec, backpack_slot, empty);

    r.sold = true;
    return r;
}

static void appendIdentifyEffect(const Mm2ItemRecord *item, char *buf, size_t cap)
{
    if (!item || !buf || cap == 0 || item->effect_byte == 0) {
        return;
    }
    const uint8_t eff = item->effect_byte;
    char tmp[48];
    if (eff >= 0x01 && eff <= 0x7F) {
        static const char *kKinds[] = {"MaxHP", "Mgt", "Spd", "Acc", "?",
                                       "Level", "SpLvl"};
        const unsigned kind = (eff >> 4) & 0x7u;
        const unsigned amt = eff & 0xFu;
        const char *label = (kind < 7) ? kKinds[kind] : "?";
        std::snprintf(tmp, sizeof(tmp), " use +%u %s", amt, label);
    } else if (eff >= 0x81 && eff <= 0xB0) {
        const int idx = eff - 0x81;
        const char *nm = (idx < mm2::gameplay::kSpellsPerSchool)
                             ? mm2::gameplay::kSorcererSpells[idx].name
                             : "Sorcerer spell";
        std::snprintf(tmp, sizeof(tmp), " casts %s", nm);
    } else if (eff >= 0xB1 && eff <= 0xE0) {
        const int idx = eff - 0xB0;
        const char *nm = (idx < mm2::gameplay::kSpellsPerSchool)
                             ? mm2::gameplay::kClericSpells[idx].name
                             : "Cleric spell";
        std::snprintf(tmp, sizeof(tmp), " casts %s", nm);
    } else {
        std::snprintf(tmp, sizeof(tmp), " effect 0x%02X", eff);
    }
    if (std::strlen(buf) + std::strlen(tmp) + 1 < cap) {
        std::strncat(buf, tmp, cap - std::strlen(buf) - 1);
    }
}

TownSvcIdentifyResult townSvcSmithIdentify(Mm2RosterRecord &rec, int backpack_slot,
                                           const Mm2ItemsFile *items, uint32_t cost,
                                           char *summary, size_t summary_cap)
{
    TownSvcIdentifyResult r;
    r.cost = cost;

    if (rec.condition != 0) {
        r.cost = 0;
        r.reject = TownSvcIdentifyReject::Condition;
        return r;
    }

    if (backpack_slot < 0 || backpack_slot >= MM2_ROSTER_ITEM_SLOTS) {
        r.cost = 0;
        r.reject = TownSvcIdentifyReject::NoItem;
        return r;
    }

    const Mm2RosterItemSlot slot = mm2_roster_backpack(&rec, backpack_slot);
    if (slot.item_id == 0) {
        r.cost = 0;
        r.reject = TownSvcIdentifyReject::NoItem;
        return r;
    }

    /* 0x1BDD6: gold check + deduct (same leaf as buy). */
    if (!townSvcCharGoldDeduct(rec, cost)) {
        r.cost = 0;
        r.reject = TownSvcIdentifyReject::NotEnoughGold;
        return r;
    }

    if (summary && summary_cap > 0) {
        summary[0] = '\0';
        char name[20] = "item";
        if (items) {
            const Mm2ItemRecord *irec = mm2_items_lookup(items, slot.item_id);
            if (irec) {
                mm2_item_name_to_cstr(irec, name, sizeof(name));
                const uint8_t plus = static_cast<uint8_t>(slot.flags & 0x3Fu);
                if (plus != 0) {
                    std::snprintf(summary, summary_cap, "%s +%u", name, static_cast<unsigned>(plus));
                } else {
                    std::snprintf(summary, summary_cap, "%s", name);
                }
                if (slot.charges != 0) {
                    char chg[24];
                    std::snprintf(chg, sizeof(chg), ", %u charges", static_cast<unsigned>(slot.charges));
                    if (std::strlen(summary) + std::strlen(chg) + 1 < summary_cap) {
                        std::strncat(summary, chg, summary_cap - std::strlen(summary) - 1);
                    }
                }
                appendIdentifyEffect(irec, summary, summary_cap);
            } else {
                std::snprintf(summary, summary_cap, "Item #%u", static_cast<unsigned>(slot.item_id));
            }
        }
    }

    r.identified = true;
    return r;
}

/* Party food cap used by feeding frenzy (0x1CA84 cmpi.b #$28) and trade UI. */
static constexpr uint8_t kTownPartyFoodMax = 0x28;

TownSvcFeedingResult townSvcFeedingFrenzy(Mm2RosterRecord &payer, const Mm2PartyLaunch *launch,
                                          Mm2RosterFile *roster, int map_id)
{
    TownSvcFeedingResult r;
    Mm2TownCommerce town{};
    if (!launch || !roster || !mm2_town_commerce(map_id, &town)) {
        return r;
    }
    r.cost = town.feeding_frenzy_gold;

    /* 0x1CA48 -> 0x1C9C0: payer gold check + deduct. */
    if (!townSvcCharGoldDeduct(payer, r.cost)) {
        return r;
    }
    r.paid = true;

    /* 0x1CA6C: walk party slots 0..party_count-1, top food to 40 when below. */
    for (int i = 0; i < launch->party_count; ++i) {
        const int roster_idx = launch->roster_slots[i];
        if (roster_idx < 0 || roster_idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        Mm2RosterRecord &member = roster->records[roster_idx];
        if (member.food < kTownPartyFoodMax) {
            member.food = kTownPartyFoodMax;
            ++r.members_topped;
        }
    }
    r.fed = true;
    return r;
}

bool townSvcMageGuildMember(const Mm2RosterRecord &rec, int map_id)
{
    const uint8_t mask = mm2_mage_guild_member_mask(map_id);
    return mask != 0 && (rec.class_quest_guild_mask & mask) != 0;
}

bool townSvcPartyHasMageGuildMember(const Mm2RosterRecord *const *members, int count,
                                    int map_id)
{
    if (!members) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (members[i] && townSvcMageGuildMember(*members[i], map_id)) {
            return true;
        }
    }
    return false;
}

TownSvcSpellResult townSvcBuySpell(Mm2RosterRecord &rec, int spell_index, uint32_t cost)
{
    TownSvcSpellResult r;
    r.cost = cost;

    /* 0x1D882: an unpopulated/zero-cost slot is not for sale. */
    if (cost == 0) {
        r.cost = 0;
        r.reject = TownSvcSpellReject::NotForSale;
        return r;
    }
    /* 0x1D898: afflicted characters cannot buy. */
    if (rec.condition != 0) {
        r.cost = 0;
        r.reject = TownSvcSpellReject::Condition;
        return r;
    }
    /* 0x1D8BA -> 0x1D90C (== 0x1C9C0): gold check + deduct. */
    if (!townSvcCharGoldDeduct(rec, cost)) {
        r.cost = 0;
        r.reject = TownSvcSpellReject::NotEnoughGold;
        return r;
    }

    /* 0x1D8D4/0x1D8FA: OR the flat-index bit into the record+0x51 spellbook. */
    mm2::gameplay::spellLearnInBook(rec, spell_index);
    r.learned = true;
    return r;
}

TownSvcGeneralStoreResult townSvcGeneralStoreConvert(Mm2RosterRecord &rec)
{
    /* 0xA62C buy path after Y + member pick: 100gp gate @ 0xA75E, apply both
     * +$50 nibbles via 0xA3AE (-$7F44 saturate-sub), clear +$50, success text. */
    TownSvcGeneralStoreResult r;
    auto *raw = reinterpret_cast<uint8_t *>(&rec);
    const uint8_t pack = raw[0x50];
    if (pack == 0) {
        r.message = "The secondary skills are gone.";
        return r;
    }
    if (!townSvcCharGoldDeduct(rec, 100u)) {
        r.message = "Sorry, you must have 100 gold.";
        return r;
    }
    r.paid = true;
    generalStoreApplySkillNibble(rec, static_cast<uint8_t>(pack & 0x0F));
    generalStoreApplySkillNibble(rec, static_cast<uint8_t>((pack >> 4) & 0x0F));
    raw[0x50] = 0;
    r.converted = true;
    r.message = "The secondary skills are gone.";
    return r;
}

void townSvcCircusWinBoost(Mm2RosterRecord &rec, int attr_choice)
{
    /* 0xDD18: if +$7D bit1 set, clear it, +10 to chosen CURRENT attr (cap 100).
     * Menu 1..6 (0-based): +$10 might / +$12 per / +$14 acc / +$27 end /
     * +$13 speed / +$15 luck. Default → +$11 intelligence. FAQ's 7th game
     * (Shell Game / intellect as key 7) is not in this leaf. */
    auto *raw = reinterpret_cast<uint8_t *>(&rec);
    if ((raw[0x7D] & 0x02) == 0) {
        return;
    }
    raw[0x7D] = static_cast<uint8_t>(raw[0x7D] & 0xFD);
    uint8_t *field = nullptr;
    switch (attr_choice) {
    case 0:
        field = raw + 0x10; /* might */
        break;
    case 1:
        field = raw + 0x12; /* personality */
        break;
    case 2:
        field = raw + 0x14; /* accuracy */
        break;
    case 3:
        field = raw + 0x27; /* endurance */
        break;
    case 4:
        field = raw + 0x13; /* speed */
        break;
    case 5:
        field = raw + 0x15; /* luck */
        break;
    default:
        field = raw + 0x11; /* intelligence fallback */
        break;
    }
    uint8_t v = *field;
    if (v > 0x5A) {
        v = 0x64;
    } else {
        v = static_cast<uint8_t>(v + 0x0A);
    }
    *field = v;
}

bool townSvcCircusGiveCupieDoll(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                gameplay::Rng *rng)
{
    /* 0xDE2C: rng(1,0xFE); if roll > 0x7F → print loss path; else place 0xDA. */
    if (!roster || !launch) {
        return false;
    }
    const int roll = rng ? rng->range(1, 0xFE) : 1;
    if (roll > 0x7F) {
        return false;
    }
    for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch->roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        Mm2RosterRecord &rec = roster->records[idx];
        for (int s = 0; s < MM2_ROSTER_ITEM_SLOTS; ++s) {
            if (rec.backpack_id[s] == 0) {
                rec.backpack_id[s] = 0xDA;
                rec.backpack_charges[s] = 0;
                rec.backpack_flags[s] = 0;
                return true;
            }
        }
    }
    return false;
}

void townSvcArmDrinkMatchOnKill(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                uint8_t monster_type)
{
    /* 0x10C66: for each party member, if +$78==type and +$7C bit0 → bset bit1. */
    if (!roster || !launch) {
        return;
    }
    for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch->roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        auto *raw = reinterpret_cast<uint8_t *>(&roster->records[idx]);
        if (raw[0x78] == 0 || raw[0x78] != monster_type) {
            continue;
        }
        if ((raw[0x7C] & 0x01) == 0) {
            continue;
        }
        raw[0x7C] = static_cast<uint8_t>(raw[0x7C] | 0x02);
    }
}

TownSvcDrinkApplyResult townSvcApplyDrinkEncoding(Mm2RosterRecord &rec)
{
    /* 0x18D3A: bit1 of +$7C; tier from +$78 vs A4-$6AEA; gold A4-$6AE0 → +$62. */
    TownSvcDrinkApplyResult r;
    auto *raw = reinterpret_cast<uint8_t *>(&rec);
    if ((raw[0x7C] & 0x02) == 0) {
        return r;
    }
    const uint8_t enc = raw[0x78];
    if (enc == 0) {
        return r;
    }
    if ((rec.condition & 0xFF) >= 0x80) {
        return r;
    }
    int tier = 0;
    while (tier < 10 && enc >= kDrinkGoldThresh[tier]) {
        ++tier;
    }
    if (tier >= 10) {
        tier = 9;
    }
    r.gold = kDrinkGoldAward[tier];
    rec.experience += r.gold; /* 0x18DC0: add.l → +$62 experience (table is gold-sized XP). */
    raw[0x78] = 0;
    raw[0x7C] = static_cast<uint8_t>(raw[0x7C] & 0xFD);
    r.applied = true;
    return r;
}

TownSvcFoodApplyResult townSvcApplyFoodEncoding(const Mm2ItemsFile *items, Mm2RosterRecord &rec)
{
    /* 0x18DE0 leaf (XP arm @ 0x18E82): enc=+78 as items.dat id; gold×8 → +$62; clr +$78.
     * A4-$3EEC = items.dat base (-$3EFE) + $12 gold word. */
    TownSvcFoodApplyResult r;
    auto *raw = reinterpret_cast<uint8_t *>(&rec);
    const uint8_t enc = raw[0x78];
    if (enc == 0) {
        return r;
    }
    if ((rec.condition & 0xFF) >= 0x80) {
        return r;
    }
    uint16_t item_gold = 0;
    if (items) {
        const Mm2ItemRecord *item = mm2_items_lookup(items, enc);
        if (item) {
            item_gold = item->gold;
        }
    }
    r.xp = static_cast<uint32_t>(item_gold) << 3; /* asl.l #3 */
    rec.experience += r.xp;
    raw[0x78] = 0;
    r.applied = true;
    return r;
}

int townSvcPartyFindBackpackItem(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                 uint8_t item_id)
{
    /* 0x18C74 + 0x18CE6: scan party backpacks for id; return slot+1 or 0. */
    if (!roster || !launch || item_id == 0) {
        return 0;
    }
    for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch->roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        Mm2RosterRecord &rec = roster->records[idx];
        for (int s = 0; s < MM2_ROSTER_ITEM_SLOTS; ++s) {
            if (rec.backpack_id[s] == item_id) {
                return s + 1;
            }
        }
    }
    return 0;
}

bool townSvcPartyConsumeBackpackItem(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                     uint8_t item_id)
{
    /* 0x18D06: find via 0x18C74 then -$7F26 clear id/charges/flags. */
    if (!roster || !launch || item_id == 0) {
        return false;
    }
    for (int i = 0; i < launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch->roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        Mm2RosterRecord &rec = roster->records[idx];
        for (int s = 0; s < MM2_ROSTER_ITEM_SLOTS; ++s) {
            if (rec.backpack_id[s] == item_id) {
                rec.backpack_id[s] = 0;
                rec.backpack_charges[s] = 0;
                rec.backpack_flags[s] = 0;
                return true;
            }
        }
    }
    return false;
}

TownSvcDrinkResult townSvcPubDrink(Mm2RosterRecord &rec, int drink_idx, gameplay::Rng *rng)
{
    /* Caption stub — encode is townSvcDrinkEncodePurchase; apply is 0x18D3A. */
    (void)rec;
    (void)rng;
    TownSvcDrinkResult r;
    if (drink_idx < 0 || drink_idx >= 6) {
        return r;
    }
    r.name = kPubDrinkNames[drink_idx];
    r.ok = true;
    return r;
}

TownSvcStatBoostResult townSvcTavernStatBoost(uint8_t *a4, Mm2RosterRecord &rec, int slot,
                                              int map_id, gameplay::Rng *rng)
{
    /* 0x1CAC4: A–F → cost A4-$6738[slot]; apply 0x1C7EC→0x1C898 (base stats).
     * Count gate 0x1CBDE: -$577E[slot] < -$672C[slot] → pay+inc only. */
    TownSvcStatBoostResult r;
    if (slot < 0 || slot >= 6) {
        return r;
    }
    r.slot = slot;
    r.cost = kStatBoostCosts[slot];
    if (rec.condition != 0) {
        return r;
    }
    if (!townSvcCharGoldDeduct(rec, r.cost)) {
        r.cost = 0;
        return r;
    }
    r.paid = true;

    uint16_t count = 0;
    if (a4) {
        count = mm2_gs_u16(a4, kGsTavernStatBuyCount + slot * 2);
    }
    const uint16_t limit = kStatBoostLimits[slot];
    if (count < limit) {
        /* 0x1CC1C: increment counter; skip apply + speed side-effect. */
        if (a4) {
            mm2_gs_set_u16(a4, kGsTavernStatBuyCount + slot * 2,
                           static_cast<uint16_t>(count + 1));
        }
    } else {
        (void)townSvcTrainStat(rec, slot, map_id);
        /* 0x1CC0A: if speed_base (+$6E) >= 2, subq #2. */
        if (rec.speed_base >= 2) {
            rec.speed_base = static_cast<uint8_t>(rec.speed_base - 2);
        }
        r.applied = true;
    }

    /* Sick: RNG(1, end_base+10)==2 → bset #3,$26 @ 0x1CC4C. */
    const int end_hi = static_cast<int>(rec.endurance_base) + 10;
    const int sick_roll = rng ? rng->range(1, end_hi > 0 ? end_hi : 1) : 1;
    if (sick_roll == 2) {
        r.sick = true;
        rec.condition = static_cast<uint8_t>(rec.condition | 0x08);
    }
    r.ok = true;
    return r;
}

TownSvcSpecialtyResult townSvcTavernSpecialty(Mm2RosterRecord &rec, int map_id, int menu,
                                              gameplay::Rng *rng)
{
    /* 0x1CD2E / 0x1CEA4: pay A4-$6760; then sick OR mask — never both.
     * ASM @ 0x1CECA: -$7F56(+$73) +5 → RNG; ==1 → bset #2,$26 and skip 0x1C8D4.
     * Else 0x1CF0A → 0x1C8D4 OR A4-$786C into +$76. No 0x18EC0 encode here. */
    TownSvcSpecialtyResult r;
    const int town = (map_id >= 0 && map_id < 5) ? map_id : 0;
    if (menu < 0 || menu > 2) {
        return r;
    }
    r.menu = menu;
    r.cost = kSpecialtyGold[town][menu];
    if (rec.condition != 0) {
        return r;
    }
    if (!townSvcCharGoldDeduct(rec, r.cost)) {
        r.cost = 0;
        return r;
    }
    r.paid = true;
    /* Stub -$7F56 as endurance_base until that helper is traced. */
    const int end_hi = static_cast<int>(rec.endurance_base) + 5;
    const int sick_roll = rng ? rng->range(1, end_hi > 0 ? end_hi : 1) : 2;
    if (sick_roll == 1) {
        r.sick = true;
        rec.condition = static_cast<uint8_t>(rec.condition | 0x04);
    } else {
        rec.temp_score_word =
            static_cast<uint16_t>(rec.temp_score_word | kSpecialtyMask[town][menu]);
    }
    r.ok = true;
    return r;
}

TownSvcFoodEncodeResult townSvcFoodEncodePurchase(Mm2RosterFile *roster,
                                                  const Mm2PartyLaunch *launch, int menu,
                                                  gameplay::Rng *rng)
{
    TownSvcFoodEncodeResult r;
    writePartyEncoding(roster, launch, encodeFoodByte(menu, rng), /*drink*/ false, r);
    return r;
}

TownSvcFoodEncodeResult townSvcDrinkEncodePurchase(Mm2RosterFile *roster,
                                                   const Mm2PartyLaunch *launch, int drink,
                                                   gameplay::Rng *rng)
{
    TownSvcFoodEncodeResult r;
    writePartyEncoding(roster, launch, encodeDrinkByte(drink, rng), /*drink*/ true, r);
    return r;
}

int townSvcTipDayPairBase(uint16_t day_of_year)
{
    /* 0x1C962: day from -$79DE[era]; if ==$B4 → 3; else if day%30==0 → day/30;
     * else (day&1) xor 1. Result 0/1/3 then caller asl #1 → pair base 0/2/6. */
    if (day_of_year == 0xB4) {
        return 3 * 2;
    }
    if ((day_of_year % 30u) == 0) {
        return static_cast<int>((day_of_year / 30u) * 2u);
    }
    return static_cast<int>(((day_of_year & 1u) ^ 1u) * 2u);
}

TownSvcTipResult townSvcTavernTip(Mm2RosterRecord &rec, uint16_t day_of_year, gameplay::Rng *rng)
{
    TownSvcTipResult r;
    /* 0x1CFD2 */
    if (rec.condition != 0) {
        r.reject = TownSvcTipReject::Condition;
        return r;
    }
    /* 0x1CFE6: deduct 1 gp via 0x1C9C0 */
    if (!townSvcCharGoldDeduct(rec, 1)) {
        r.reject = TownSvcTipReject::NotEnoughGold;
        return r;
    }
    /* 0x1D006: hi = -$7F56(+$73)+5; rng(1,hi)==1 to show tip. */
    const uint8_t factor = attrTableBonus(rec.endurance_base);
    const int hi = static_cast<int>(factor) + 5;
    const int roll = rng ? rng->range(1, hi > 0 ? hi : 1) : 1;
    if (roll != 1) {
        r.reject = TownSvcTipReject::NoTip;
        return r;
    }
    r.pair_base = townSvcTipDayPairBase(day_of_year);
    if (r.pair_base < 0) {
        r.pair_base = 0;
    }
    if (r.pair_base > 6) {
        r.pair_base = 6;
    }
    r.ok = true;
    return r;
}

TownSvcTipResult townSvcTavernRumor(Mm2RosterRecord &rec, uint16_t day_of_year)
{
    TownSvcTipResult r;
    /* 0x1D0B8: cond != 0 → caption 4 */
    if (rec.condition != 0) {
        r.reject = TownSvcTipReject::Condition;
        return r;
    }
    r.pair_base = townSvcTipDayPairBase(day_of_year);
    if (r.pair_base < 0) {
        r.pair_base = 0;
    }
    if (r.pair_base > 6) {
        r.pair_base = 6;
    }
    r.ok = true;
    return r;
}

}  // namespace mm2::events

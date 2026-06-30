#include "mm2/events/TownServiceTransactions.h"

#include "mm2_town_tables.h"

namespace mm2::events {

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

TownSvcTrainResult townSvcTrainLevelUp(Mm2RosterRecord &rec, int map_id)
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
        /* Not enough experience for the next level — engine charges nothing. */
        r.new_level = rec.level;
        return r;
    }
    r.eligible = true;

    /* Fee gate: fee = current level * training index * 50 (doc 34 §13.2). */
    const uint32_t cost = mm2_town_training_cost(rec.level, town.training_town_index);
    r.cost = cost;
    if (!townSvcCharGoldDeduct(rec, cost)) {
        r.cost = 0;
        return r;
    }
    r.paid = true;

    /* Level up (record+0x71) and recompute HP/SP/spell level (doc-32 model). */
    rec.level = static_cast<uint8_t>(next_level);

    const int hp_gain = mm2_class_hp_per_level(rec.class_id) + mm2_attr_bonus(rec.endurance_base);
    rec.hp_aux = static_cast<uint16_t>(rec.hp_aux + hp_gain); /* permanent max +$60 */
    rec.hp_max = rec.hp_aux;                                  /* working max    +$5E */
    rec.hp_current = rec.hp_aux;                              /* current HP      +$74 */
    r.hp_gain = static_cast<uint16_t>(hp_gain);

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

TownSvcDonateResult townSvcTempleDonate(uint8_t *a4, Mm2RosterRecord &rec, int map_id)
{
    TownSvcDonateResult r;
    Mm2TownCommerce town;
    if (!mm2_town_commerce(map_id, &town)) {
        return r;
    }
    r.cost = town.donation_gold;
    if (!townSvcCharGoldDeduct(rec, town.donation_gold)) {
        r.cost = 0;
        return r;
    }
    r.paid = true;
    if (a4) {
        /* doc 28 §5.2: A4-$799E |= A4-$66B1[map]; 0x1F = all five temples done. */
        const uint8_t bits = static_cast<uint8_t>(
            mm2_gs_u8(a4, MM2_GS_TEMPLE_DONATION) | town.donation_quest_bit);
        mm2_gs_set_u8(a4, MM2_GS_TEMPLE_DONATION, bits);
        r.all_temples_donated = (bits == MM2_TEMPLE_DONATION_ALL_BITS);
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
    return mm2_town_healing_cost(level, town.training_town_index);
}

TownSvcBuyResult townSvcSmithBuy(Mm2RosterRecord &rec, uint8_t item_id, uint8_t charges,
                                 uint8_t flags, uint32_t price)
{
    TownSvcBuyResult r;
    r.price = price;

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
    if (!townSvcCharGoldDeduct(rec, price)) {
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

}  // namespace mm2::events

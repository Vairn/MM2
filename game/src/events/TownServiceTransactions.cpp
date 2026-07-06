#include "mm2/events/TownServiceTransactions.h"

#include "mm2/gameplay/SpellBook.h"

#include "mm2/CppStdCompat.h"

#include "mm2_items_codec.h"
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

TownSvcSellResult townSvcSmithSell(Mm2RosterRecord &rec, int backpack_slot, uint32_t price)
{
    TownSvcSellResult r;
    r.price = price;
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
    rec.gold += price;

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
    r.cost = town.donation_gold;

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

}  // namespace mm2::events

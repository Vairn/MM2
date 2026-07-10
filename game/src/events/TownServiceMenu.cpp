#include "mm2/events/TownServiceMenu.h"
#include "mm2/events/TownServiceTransactions.h"

namespace mm2::events {

Mm2RosterRecord *townSvcMemberRecord(const TownServiceContext &ctx, int party_slot)
{
    if (!ctx.roster || !ctx.launch) {
        return nullptr;
    }
    if (party_slot < 0 || party_slot >= ctx.launch->party_count ||
        party_slot >= MM2_PARTY_LAUNCH_SLOTS) {
        return nullptr;
    }
    const int idx = ctx.launch->roster_slots[party_slot];
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
        return nullptr;
    }
    Mm2RosterRecord &rec = ctx.roster->records[idx];
    if (mm2_roster_slot_is_empty(&rec)) {
        return nullptr;
    }
    return &rec;
}

void townSvcRunTemple(ITownServiceUi &ui, const TownServiceContext &ctx)
{
    for (;;) {
        TempleOption opt = TempleOption::Exit;
        int spell_slot = -1;
        if (!ui.chooseTempleOption(ctx, opt, spell_slot) || opt == TempleOption::Exit) {
            return;
        }

        Mm2SpellShopSlot spells[MM2_TEMPLE_SPELL_SLOTS];
        const bool have_spells = mm2_temple_spell_stock(ctx.map_id, spells) != 0;
        if (opt == TempleOption::BuySpell &&
            (!have_spells || spell_slot < 0 || spell_slot >= MM2_TEMPLE_SPELL_SLOTS)) {
            continue;
        }

        int slot = -1;
        if (!ui.selectMember(ctx, slot)) {
            continue; /* member-select cancelled -> back to menu */
        }
        Mm2RosterRecord *rec = townSvcMemberRecord(ctx, slot);
        if (!rec) {
            continue;
        }

        switch (opt) {
        case TempleOption::Heal: {
            const uint32_t cost = townSvcTempleHealCost(*rec, ctx.map_id);
            const TownSvcHealResult r = townSvcHeal(*rec, cost);
            if (cost > 0 && !r.paid) {
                ui.reportNotEnoughGold();
            } else {
                ui.reportHeal(r);
            }
            break;
        }
        case TempleOption::RestoreAlignment: {
            const uint32_t cost = townSvcTempleAlignCost(*rec, ctx.map_id);
            const TownSvcAlignResult r = townSvcRestoreAlignment(*rec, cost);
            if (cost > 0 && !r.paid) {
                ui.reportNotEnoughGold();
            } else {
                ui.reportAlign(r);
            }
            break;
        }
        case TempleOption::Donate: {
            const TownSvcDonateResult r = townSvcTempleDonate(ctx.a4, *rec, ctx.map_id);
            if (!r.paid) {
                ui.reportNotEnoughGold();
            } else {
                ui.reportDonate(r);
            }
            break;
        }
        case TempleOption::BuySpell: {
            const TownSvcSpellResult r =
                townSvcBuySpell(*rec, spells[spell_slot].spell_index, spells[spell_slot].gold);
            if (r.learned) {
                ui.reportSpellLearned(r);
            } else if (r.reject == TownSvcSpellReject::NotEnoughGold) {
                ui.reportNotEnoughGold();
            } else {
                ui.reportSpellRejected(r);
            }
            break;
        }
        case TempleOption::Exit:
            return;
        }
    }
}

void townSvcRunTraining(ITownServiceUi &ui, const TownServiceContext &ctx)
{
    for (;;) {
        TrainingOption opt = TrainingOption::Exit;
        int stat_id = 0;
        if (!ui.chooseTrainingOption(ctx, opt, stat_id) || opt == TrainingOption::Exit) {
            return;
        }

        int slot = -1;
        if (!ui.selectMember(ctx, slot)) {
            continue;
        }
        Mm2RosterRecord *rec = townSvcMemberRecord(ctx, slot);
        if (!rec) {
            continue;
        }

        (void)stat_id; /* the Training Hall has no stat sub-option (level-up only) */
        const TownSvcTrainResult r = townSvcTrainLevelUp(*rec, ctx.map_id, ctx.rng);
        if (r.eligible && !r.paid) {
            ui.reportNotEnoughGold();
        } else {
            ui.reportTrain(r); /* reports level-up OR "not enough experience" */
        }
    }
}

void townSvcRunMageGuild(ITownServiceUi &ui, const TownServiceContext &ctx)
{
    /* 0x1E410: the shop only opens when >=1 living party member already holds
     * the per-town membership bit (record+0x79). See TownServiceTransactions.h
     * for the confirmed write paths (event-script selector 0x74, or the
     * unported 0x9D76 reward loop). */
    bool any_member = false;
    if (ctx.roster && ctx.launch) {
        for (int i = 0; i < ctx.launch->party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            const Mm2RosterRecord *rec = townSvcMemberRecord(ctx, i);
            if (rec && townSvcMageGuildMember(*rec, ctx.map_id)) {
                any_member = true;
                break;
            }
        }
    }
    if (!any_member) {
        ui.reportGuildNotMember(ctx);
        return;
    }

    Mm2SpellShopSlot slots[MM2_MAGE_GUILD_SLOTS];
    if (!mm2_mage_guild_stock(ctx.map_id, slots)) {
        return;
    }

    for (;;) {
        int spell_slot = -1;
        if (!ui.chooseMageGuildSpell(ctx, slots, spell_slot)) {
            return;
        }
        if (spell_slot < 0 || spell_slot >= MM2_MAGE_GUILD_SLOTS) {
            continue;
        }

        int member = -1;
        if (!ui.selectMember(ctx, member)) {
            continue;
        }
        Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
        if (!rec) {
            continue;
        }

        const TownSvcSpellResult r =
            townSvcBuySpell(*rec, slots[spell_slot].spell_index, slots[spell_slot].gold);
        if (r.learned) {
            ui.reportSpellLearned(r);
        } else if (r.reject == TownSvcSpellReject::NotEnoughGold) {
            ui.reportNotEnoughGold();
        } else {
            ui.reportSpellRejected(r);
        }
    }
}

void townSvcRunSmith(ITownServiceUi &ui, const TownServiceContext &ctx)
{
    if (!ctx.items) {
        return; /* pricing needs items.dat gold (0x1BF16 reads $12(items_rec)) */
    }
    for (;;) {
        int category = -1;
        if (!ui.chooseSmithCategory(ctx, category)) {
            return;
        }
        Mm2SmithSlot slots[MM2_SMITH_SLOTS];
        if (!mm2_smith_inventory(ctx.map_id, category, slots)) {
            continue;
        }

        /* Build the priced view (0x1C00E inventory + 0x1BF16 prices). */
        SmithItemView view[MM2_SMITH_SLOTS];
        for (int i = 0; i < MM2_SMITH_SLOTS; ++i) {
            uint8_t price_meta = 0, charges = 0, flags = 0;
            mm2_smith_buy_fields(category, slots[i].meta, &price_meta, &charges, &flags);
            view[i].item_id = slots[i].item_id;
            view[i].bonus = flags;
            view[i].charges = charges;
            if (slots[i].item_id != 0) {
                const Mm2ItemRecord *rec = mm2_items_lookup(ctx.items, slots[i].item_id);
                const uint32_t base = rec ? rec->gold : 0u;
                view[i].price = mm2_smith_price(base, price_meta);
            }
        }

        int slot = -1;
        if (!ui.selectSmithItem(ctx, category, view, slot)) {
            continue; /* back to category menu */
        }
        if (slot < 0 || slot >= MM2_SMITH_SLOTS || view[slot].item_id == 0) {
            continue;
        }

        int member = -1;
        if (!ui.selectMember(ctx, member)) {
            continue;
        }
        Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
        if (!rec) {
            continue;
        }

        const TownSvcBuyResult r =
            townSvcSmithBuy(*rec, view[slot].item_id, view[slot].charges, view[slot].bonus,
                            view[slot].price);
        if (r.bought) {
            ui.reportSmithBuy(r);
        } else if (r.reject == TownSvcBuyReject::NotEnoughGold) {
            ui.reportNotEnoughGold();
            ui.reportBuyRejected(r);
        } else {
            ui.reportBuyRejected(r);
        }
    }
}

/* --------------------------------------------------------------------------
 * Pub / tavern tables and runner
 * -------------------------------------------------------------------------- */

/* str.dat RUMOR pool (A4-$594E): per-town strings for option E "Listen for rumors".
 * The game init fills this table FIRST (40 strings = 8 per town × 5 towns).
 * Day-based selector (0x1c962) returns 0, 1, or 3; only pairs {0,1}, {2,3},
 * {6,7} are ever displayed.  Entries at index 4 and 5 are never shown. */
static const char *const kPubRumors[5][kPubRumorCount] = {
    /* 0 Middlegate */
    {
        "Children at 0,15",
        "Goblets at 0,7",
        "Meal A, then C1 2,10",
        "Meal B, then C1 2,6",
        "(unused)",                             /* index 4 – never shown */
        "(unused)",                             /* index 5 – never shown */
        "Meal C, then C1 1,8",
        "'Murray's rejuvenates'",
    },
    /* 1 Atlantium */
    {
        "Hirelings at 15,10",
        "Cast C2,3 day 93\n   gain S9,3",
        "Meal C-an experience",
        "B2, day 140-170,\n     14,4",
        "(unused)",
        "(unused)",
        "Meal B is a riot",
        "Mandagaul D2 6,8",
    },
    /* 2 Tundara */
    {
        "The Gourmet A3 7,6",
        "C9,2 C1 south",
        "Meal C, 7,3-Sarakin",
        "See Nordon at 10,2",
        "(unused)",
        "(unused)",
        "Meal C, then D1 2,7",
        "Hirelings at 0,14",
    },
    /* 3 Vulcania */
    {
        "Transmutations 8,8\nthe corners",
        "Castle Xabran holds\nall clues",
        "Meal A, then C4 14,8",
        "C2,3 C3 1,9",
        "(unused)",
        "(unused)",
        "Meal B, then C3 1,9",
        "Visit Dawn's, D4 3,7",
    },
    /* 4 Sandsobar */
    {
        "Slayer seeks death",
        "S5,2 C1 1,8",
        "Meal C, then E1 2,3",
        "Tavern drinks give a\nbonus",
        "(unused)",
        "(unused)",
        "Meal B, then E4 3,10",
        "See Nordon at 10,2",
    },
};

/* str.dat TIP pool (A4-$58AE): per-town strings for option D "Tip the bartender".
 * The game init fills this table SECOND (40 more strings), AFTER the rumor table.
 * Same day-based selector as rumors; D costs 1 gold and only succeeds on a 1-in-N
 * RNG roll (N = char_attr_0x73 + 5).  Exact original strings are stored encoded
 * in the game binary (+0x1C cipher, blob @ A4-$77CE); these are approximations
 * from game guides until the decoder is ported.  Same {0,1}/{2,3}/{6,7} pair
 * scheme applies — indices 4 and 5 are never displayed. */
static const char *const kPubTips[5][kPubTipCount] = {
    /* 0 Middlegate */
    {
        "S7,1 B2 15,11",
        "Time travel at\n  Pinehurst",
        "Lord Haart B1 5,5",
        "C3 Jail - show your\nticket",
        "(unused)",
        "(unused)",
        "Keys add castle gold",
        "Nordon has S2,1",
    },
    /* 1 Atlantium */
    {
        "C3,6 C2 11,1",
        "Lord Haart B1 5,5",
        "Hoardall seeks items",
        "Keys add castle gold",
        "(unused)",
        "(unused)",
        "Castle Xabran holds\nall clues",
        "Donate at all\ntemples",
    },
    /* 2 Tundara */
    {
        "Nordon has S2,1",
        "Donate at all\ntemples",
        "Transmutations 8,8\nthe corners",
        "C2,3 C3 1,9",
        "(unused)",
        "(unused)",
        "Hirelings at 0,14",
        "C9,2 C1 south",
    },
    /* 3 Vulcania */
    {
        "Hoardall seeks items",
        "Keys add castle gold",
        "S3,6 7,4",
        "Meal A, then 3,11",
        "(unused)",
        "(unused)",
        "Slayer seeks death",
        "S5,2 C1 1,8",
    },
    /* 4 Sandsobar */
    {
        "C2,3 C3 1,9",
        "Hirelings at 15,10",
        "Lord Haart B1 5,5",
        "Meal A, then C4 14,8",
        "(unused)",
        "(unused)",
        "S3,6 7,4",
        "Meal A, then 3,11",
    },
};

/* str.dat ~214-239: per-town food menu (A/B/C options).
 * Each entry is two adjacent str.dat lines joined. */
static const char *const kPubFood[5][kPubFoodOptions] = {
    /* 0 Middlegate */
    {"Horrors d'oeuvres", "Soup de Ghoul\nw/garlic toast", "Dragon Steak\nTartar"},
    /* 1 Atlantium */
    {"Lightly salted\ntongue of toad", "Puree of Gnome", "Devils Food\nBrownie"},
    /* 2 Tundara */
    {"Sizzling\nSwine Soup", "Red Hot Wolf\nNipple Chips", "Roast Leg of\nWyvern"},
    /* 3 Vulcania */
    {"Pickled Pixie\nBrains", "Deep fried\nTroll liver", "Cream of\nKobold soup"},
    /* 4 Sandsobar */
    {"Gourmet Dinner", "Wyrm Chop Suey", "Steak and Taters"},
};

/* str.dat ~208-213: drink labels (selector 0xCA encode path — not tavern B). */
static const char *const kPubDrinks[kPubDrinkCount] = {
    "Orc Beer",
    "Straight shot",
    "Id Elixir",
    "Academic Ale",
    "Rare Vintage",
    "Mystic Brew",
};

/* A4-$6738 costs; captions approximate A4-$580E (0x1C7EC field order). */
static const char *const kStatBoostLabels[kPubStatBoostCount] = {
    "Might", "Accuracy", "Personality", "Intelligence", "Level", "Spell Level",
};
static const uint16_t kStatBoostGold[kPubStatBoostCount] = {5, 5, 20, 20, 50, 100};

/* A4-$6760 specialties gold (town × menu). */
static const uint16_t kSpecialtyGoldTable[5][kPubFoodOptions] = {
    {10, 50, 100}, {1000, 2000, 3000}, {200, 100, 1000}, {5000, 500, 1000}, {20, 50, 250},
};

void townSvcPubTables(int map_id, TavernMenuData &out)
{
    const int town = (map_id >= 0 && map_id < 5) ? map_id : 0;
    for (int i = 0; i < kPubRumorCount; ++i) {
        out.rumors[i] = kPubRumors[town][i];
    }
    for (int i = 0; i < kPubTipCount; ++i) {
        out.tips[i] = kPubTips[town][i];
    }
    for (int i = 0; i < kPubFoodOptions; ++i) {
        out.food.options[i] = kPubFood[town][i];
        out.food.costs[i] = kSpecialtyGoldTable[town][i];
    }
    for (int i = 0; i < kPubDrinkCount; ++i) {
        out.drinks[i].label = kPubDrinks[i];
    }
    for (int i = 0; i < kPubStatBoostCount; ++i) {
        out.boosts[i].label = kStatBoostLabels[i];
        out.boosts[i].cost = kStatBoostGold[i];
    }
}

void townSvcRunTavern(ITownServiceUi &ui, const TownServiceContext &ctx)
{
    TavernMenuData data{};
    townSvcPubTables(ctx.map_id, data);

    TavernOption opt = TavernOption::Exit;
    while (ui.chooseTavernOption(ctx, data, opt)) {
        switch (opt) {
        case TavernOption::FeedingFrenzy: {
            int member = 0;
            if (ui.selectMember(ctx, member)) {
                Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
                if (rec) {
                    townSvcFeedingFrenzy(*rec, ctx.launch, ctx.roster, ctx.map_id);
                }
            }
            break;
        }
        case TavernOption::StatBoost: {
            /* B @ 0x1CAC4 — A–F stat boost, NOT drinks. */
            int slot = -1;
            if (ui.chooseTavernStatBoost(ctx, data, slot) && slot >= 0 &&
                slot < kPubStatBoostCount) {
                int member = 0;
                if (ui.selectMember(ctx, member)) {
                    Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
                    if (rec) {
                        const TownSvcStatBoostResult r =
                            townSvcTavernStatBoost(*rec, slot, ctx.map_id, ctx.rng);
                        ui.reportTavernStatBoost(r);
                    }
                }
            }
            break;
        }
        case TavernOption::Specialties: {
            /* C @ 0x1CD2E — A4-$6760 pay + A4-$786C → +$76. Also run 0x18EC0
             * encode onto party +$78 (selector 0xC9 leaf shares the meal index). */
            int food = -1;
            if (ui.chooseTavernFood(ctx, data, food) && food >= 0 && food < kPubFoodOptions) {
                int member = 0;
                if (ui.selectMember(ctx, member)) {
                    Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
                    if (rec) {
                        const TownSvcSpecialtyResult r =
                            townSvcTavernSpecialty(*rec, ctx.map_id, food, ctx.rng);
                        ui.reportTavernSpecialty(r);
                        /* 0x18EC0 encode is selector 0xC9, not 0x1CD2E. Remake
                         * also writes +$78 on specialty buy so meal encoding is
                         * visible without inventing FAQ tile coords; gold was
                         * already taken via A4-$6760 above (encoder itself never
                         * deducts). */
                        if (r.paid) {
                            (void)townSvcFoodEncodePurchase(ctx.roster, ctx.launch, food,
                                                            ctx.rng);
                        }
                    }
                }
                ui.reportTavernFood(ctx, food);
            }
            break;
        }
        case TavernOption::Tip: {
            const char *tip = data.tips[0];
            for (int i = 0; i < kPubTipCount; ++i) {
                if (data.tips[i] && data.tips[i][0] && data.tips[i][0] != '(') {
                    tip = data.tips[i];
                    break;
                }
            }
            ui.reportTavernTip(ctx, tip);
            break;
        }
        case TavernOption::Rumors: {
            const char *rumor = data.rumors[0];
            for (int i = 0; i < kPubRumorCount; ++i) {
                if (data.rumors[i] && data.rumors[i][0] && data.rumors[i][0] != '(') {
                    rumor = data.rumors[i];
                    break;
                }
            }
            ui.reportTavernRumor(ctx, rumor);
            break;
        }
        case TavernOption::Exit:
        default:
            return;
        }
    }
}

}  // namespace mm2::events

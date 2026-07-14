#include "mm2/events/TownServiceMenu.h"
#include "mm2/events/TownServiceTransactions.h"
#include "mm2/events/EventVmHelpers.h"

#include "mm2/CppStdCompat.h"

#include "mm2_gamestate.h"

#include <cstdio>

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
            const TownSvcDonateResult r =
                townSvcTempleDonate(ctx.a4, *rec, ctx.map_id, ctx.rng);
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

/* tip-cipher C-strings after 40 pre-rumor fills (0x1D2B0/0x1D2EE via -$7DE2).
 * Bank1 @ str.dat+$5BA (0x9666 / A4-$71E8[1]); day-pair {0,1}/{2,3}/{6,7}.
 * Fallback only — live path fills A4-$594E/-$58AE via eventVmFillTavernStrTables. */
static const char *const kPubRumors[5][kPubRumorCount] = {
    {
        "Children at 0,15",
        "Goblets at 0,7",
        "Meal A, then C1 2,10",
        "Meal B, then C1 2,6",
        "Time travel at",
        "Pinehurst",
        "Meal B, then A2",
        "14,10",
    },
    {
        "S7,1 B2 15,11",
        "Meal C, then C1 1,8",
        "'Murray's rejuvenates'",
        "Hirelings at 15,10",
        "Cast C2,3 day 93",
        "gain S9,3",
        "Meal C-an experience",
        "B2, day 140-170,",
    },
    {
        "14,4",
        "C3,6 C2 11,1",
        "Lord Haart B1 5,5",
        "Meal B is a riot",
        "Mandagaul D2 6,8",
        "The Gourmet A3 7,6",
        "C9,2 C1 south",
        "Meal C, 7,3-Sarakin",
    },
    {
        "See Nordon at 10,2",
        "Donate at all",
        "temples",
        "Nordon has S2,1",
        "Meal C, then D1 2,7",
        "Hirelings at 0,14",
        "Transmutations 8,8",
        "the corners",
    },
    {
        "Castle Xabran holds",
        "all clues",
        "Meal A, then C4 14,8",
        "C2,3 C3 1,9",
        "Hoardall seeks items",
        "Keys add castle gold",
        "Meal B, then C3 1,9",
        "Visit Dawn's, D4 3,7",
    },
};
/* Linear 0x976E fill after rumors: town0 tips then drink/food chrome (ASM-faithful). */
static const char *const kPubTips[5][kPubTipCount] = {
    {
        "Slayer seeks death",
        "S5,2 C1 1,8",
        "Meal C, then E1 2,3",
        "Tavern drinks give a",
        "bonus",
        "S3,6 7,4",
        "Meal A, then 3,11",
        "Meal B, then E4 3,10",
    },
    {
        "A) Orc Beer      - ",
        "B) Straight shot - ",
        "C) Id Elixir     - ",
        "D) Academic Ale  - ",
        "E) Rare Vintage  - ",
        "F) Mystic Brew   - ",
        "A) Horrors",
        "d'oeuvres",
    },
    {
        "B) Soup de Ghoul",
        "w/garlic toast",
        "C) Dragon Steak",
        "Tartar",
        "A) Lightly salted",
        "tongue of toad",
        "B) Puree of Gnome",
        "C) Devils Food",
    },
    {
        "Brownie",
        "A) Sizzling",
        "Swine Soup",
        "B) Red Hot Wolf",
        "Nipple Chips",
        "C) Roast Leg of",
        "Wyvern",
        "A) Pickled Pixie",
    },
    {
        "Brains",
        "B) Deep fried",
        "Troll liver",
        "C) Cream of",
        "Kobold soup",
        "A) Gourmet Dinner",
        "B Wyrm Chop Suey",
        "B) Roast Peasant",
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

/* str.dat ~208-213: the six drink names. These are the tavern key-B captions
 * (A4-$580E / MM2_GS_TAVERN_BOOST_LBL); headless fallback when no live str.dat. */
static const char *const kPubDrinks[kPubDrinkCount] = {
    "Orc Beer",
    "Straight shot",
    "Id Elixir",
    "Academic Ale",
    "Rare Vintage",
    "Mystic Brew",
};

/* Tavern key B @ 0x1CAC4 prints the six A4-$580E captions — the DRINK names
 * ("A) Orc Beer" … "F) Mystic Brew"), each of which boosts a stat when drunk
 * (doc 34 §5 / FAQ §3-10-3). Fallback for the headless path (no live str.dat);
 * costs are A4-$6738. */
static const uint16_t kStatBoostGold[kPubStatBoostCount] = {5, 5, 20, 20, 50, 100};

/* A4-$6760 specialties gold (town × menu). */
static const uint16_t kSpecialtyGoldTable[5][kPubFoodOptions] = {
    {10, 50, 100}, {1000, 2000, 3000}, {200, 100, 1000}, {5000, 500, 1000}, {20, 50, 250},
};

/* str.dat specialty lines already embed an "A) "/"B) "/"C) " label (retail 0x1CD76
 * prints them verbatim). The UI adds its own "%c)" prefix, so drop the embedded one
 * here to avoid a doubled "A) A)" label. */
static const char *stripSpecialtyLabel(const char *s, int index)
{
    if (!s) {
        return s;
    }
    const char letter = static_cast<char>('A' + index);
    if (s[0] == letter && s[1] == ')') {
        s += 2;
        while (*s == ' ') {
            ++s;
        }
    }
    return s;
}

void townSvcPubTables(int map_id, TavernMenuData &out, uint8_t *a4)
{
    const int town = (map_id >= 0 && map_id < 5) ? map_id : 0;
    for (int i = 0; i < kPubRumorCount; ++i) {
        const char *s = nullptr;
        if (a4) {
            s = eventVmGsRelCString(
                a4, mm2_gs_u32(a4, MM2_GS_TAVERN_RUMORS + town * 0x20 + i * 4));
        }
        /* nullptr = no GS; "" = real empty day-pair slot. */
        out.rumors[i] = s ? s : kPubRumors[town][i];
    }
    for (int i = 0; i < kPubTipCount; ++i) {
        const char *s = nullptr;
        if (a4) {
            s = eventVmGsRelCString(a4, mm2_gs_u32(a4, MM2_GS_TAVERN_TIPS + town * 0x20 + i * 4));
        }
        /* Empty NUL slots are valid (day-pair second line often blank). */
        out.tips[i] = s ? s : kPubTips[town][i];
    }
    for (int i = 0; i < kPubFoodOptions; ++i) {
        /* 0x1CD76: print -$57F6[town][2*i] then [2*i+1] (two gotoxy lines). */
        const char *a = nullptr;
        const char *b = nullptr;
        if (a4) {
            a = eventVmGsRelCString(a4, mm2_gs_u32(a4, MM2_GS_TAVERN_FOOD + town * 0x18 + (2 * i) * 4));
            b = eventVmGsRelCString(a4,
                                    mm2_gs_u32(a4, MM2_GS_TAVERN_FOOD + town * 0x18 + (2 * i + 1) * 4));
        }
        const char *a_txt = stripSpecialtyLabel(a, i);
        out.food_joined[i][0] = '\0';
        if (a_txt && a_txt[0]) {
            if (b && b[0]) {
                std::snprintf(out.food_joined[i], sizeof(out.food_joined[i]), "%s\n%s", a_txt, b);
            } else {
                std::snprintf(out.food_joined[i], sizeof(out.food_joined[i]), "%s", a_txt);
            }
            out.food.options[i] = out.food_joined[i];
        } else {
            out.food.options[i] = kPubFood[town][i];
        }
        out.food.costs[i] =
            a4 ? mm2_gs_u16(a4, MM2_GS_TAVERN_SPECIALTY_GOLD + (town * 3 + i) * 2)
               : kSpecialtyGoldTable[town][i];
    }
    for (int i = 0; i < kPubDrinkCount; ++i) {
        const char *s = nullptr;
        if (a4) {
            s = eventVmGsRelCString(a4, mm2_gs_u32(a4, MM2_GS_TAVERN_DRINK_LBL + i * 4));
        }
        out.drinks[i].label = (s && s[0]) ? s : kPubDrinks[i];
    }
    for (int i = 0; i < kPubStatBoostCount; ++i) {
        /* 0x1CB26: caption = A4-$580E[i] (MM2_GS_TAVERN_BOOST_LBL) = the drink name,
         * NOT a stat name. The live str.dat text embeds its own "A) "/"B) " letter
         * (like specialties), so strip it — the UI adds its own "%c)" prefix. */
        const char *s = nullptr;
        if (a4) {
            s = eventVmGsRelCString(a4, mm2_gs_u32(a4, MM2_GS_TAVERN_BOOST_LBL + i * 4));
        }
        out.boosts[i].label = stripSpecialtyLabel((s && s[0]) ? s : kPubDrinks[i], i);
        out.boosts[i].cost =
            a4 ? mm2_gs_u16(a4, MM2_GS_TAVERN_BOOST_GOLD + i * 2) : kStatBoostGold[i];
    }
}

void townSvcRunTavern(ITownServiceUi &ui, const TownServiceContext &ctx)
{
    TavernMenuData data{};
    townSvcPubTables(ctx.map_id, data, ctx.a4);

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
            /* B @ 0x1CAC4 — "Have a drink": A–F drinks (each boosts a stat). */
            int slot = -1;
            if (ui.chooseTavernStatBoost(ctx, data, slot) && slot >= 0 &&
                slot < kPubStatBoostCount) {
                int member = 0;
                if (ui.selectMember(ctx, member)) {
                    Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
                    if (rec) {
                        const TownSvcStatBoostResult r =
                            townSvcTavernStatBoost(ctx.a4, *rec, slot, ctx.map_id, ctx.rng);
                        ui.reportTavernStatBoost(r);
                    }
                }
            }
            break;
        }
        case TavernOption::Specialties: {
            /* C @ 0x1CD2E — A4-$6760 pay + (sick OR A4-$786C → +$76). No 0x18EC0. */
            int food = -1;
            if (ui.chooseTavernFood(ctx, data, food) && food >= 0 && food < kPubFoodOptions) {
                int member = 0;
                if (ui.selectMember(ctx, member)) {
                    Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
                    if (rec) {
                        const TownSvcSpecialtyResult r =
                            townSvcTavernSpecialty(ctx.a4, *rec, ctx.map_id, food, ctx.rng);
                        ui.reportTavernSpecialty(r);
                    }
                }
                ui.reportTavernFood(ctx, food);
            }
            break;
        }
        case TavernOption::Tip: {
            /* D @ 0x1CFCA: member → 1gp + RNG + day-pair tips (0x1C962). */
            int member = 0;
            if (!ui.selectMember(ctx, member)) {
                break;
            }
            Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
            if (!rec) {
                break;
            }
            const uint16_t day = ctx.a4 ? mm2_gs_day(ctx.a4, mm2_gs_u16(ctx.a4, MM2_GS_ERA)) : 1;
            const TownSvcTipResult tip_r = townSvcTavernTip(*rec, day, ctx.rng);
            if (!tip_r.ok) {
                if (tip_r.reject == TownSvcTipReject::NotEnoughGold) {
                    ui.reportNotEnoughGold();
                } else if (tip_r.reject == TownSvcTipReject::NoTip) {
                    ui.reportTavernTip(ctx, "Thank you -\nPlease come again");
                }
                break;
            }
            char tip_buf[160];
            const int b = tip_r.pair_base;
            const char *a = (b >= 0 && b < kPubTipCount) ? data.tips[b] : nullptr;
            const char *c = (b + 1 < kPubTipCount) ? data.tips[b + 1] : nullptr;
            if (a && c && a[0] && a[0] != '(' && c[0] && c[0] != '(') {
                std::snprintf(tip_buf, sizeof(tip_buf), "%s\n%s", a, c);
            } else if (a && a[0] && a[0] != '(') {
                std::snprintf(tip_buf, sizeof(tip_buf), "%s", a);
            } else {
                std::snprintf(tip_buf, sizeof(tip_buf), "Thank you -\nPlease come again");
            }
            ui.reportTavernTip(ctx, tip_buf);
            break;
        }
        case TavernOption::Rumors: {
            /* E @ 0x1D0B4: cond gate + day-pair rumors (no gold). */
            int member = 0;
            if (!ui.selectMember(ctx, member)) {
                break;
            }
            Mm2RosterRecord *rec = townSvcMemberRecord(ctx, member);
            if (!rec) {
                break;
            }
            const uint16_t day = ctx.a4 ? mm2_gs_day(ctx.a4, mm2_gs_u16(ctx.a4, MM2_GS_ERA)) : 1;
            const TownSvcTipResult rum_r = townSvcTavernRumor(*rec, day);
            if (!rum_r.ok) {
                break;
            }
            char rum_buf[160];
            const int b = rum_r.pair_base;
            const char *a = (b >= 0 && b < kPubRumorCount) ? data.rumors[b] : nullptr;
            const char *c = (b + 1 < kPubRumorCount) ? data.rumors[b + 1] : nullptr;
            if (a && c && a[0] && a[0] != '(' && c[0] && c[0] != '(') {
                std::snprintf(rum_buf, sizeof(rum_buf), "%s\n%s", a, c);
            } else if (a && a[0] && a[0] != '(') {
                std::snprintf(rum_buf, sizeof(rum_buf), "%s", a);
            } else {
                rum_buf[0] = '\0';
            }
            if (rum_buf[0]) {
                ui.reportTavernRumor(ctx, rum_buf);
            }
            break;
        }
        case TavernOption::Exit:
        default:
            return;
        }
    }
}

}  // namespace mm2::events

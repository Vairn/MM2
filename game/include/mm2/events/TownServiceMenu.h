#pragma once
// Faithful, headless-testable town-service menu driver. The transaction LOGIC is
// ASM-canonical (TownServiceTransactions.h); the interactive prompt/selection is
// the swappable UI backend (ITownServiceUi) per CLAUDE.md's pluggable-UI rule.
//
// The original engine drives these menus through A4 vtable thunks (-$7be4 print,
// -$7bfc gotoxy, -$7ddc keyread, member-select -$7e0c, RNG caption tables) which
// are presentation; an ITownServiceUi implementation supplies that interaction
// (real Amiga UI, or a scripted test double) while the driver enforces the real
// option set, costs, and state mutations.

#include "mm2/events/TownServiceTransactions.h"

#include "mm2_items_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"
#include "mm2_town_tables.h"

#include <cstdint>

namespace mm2::gameplay {
class Rng;
}

namespace mm2::events {

/* Temple menu (OP_0E 0x04 → -$7D16 → shell 0x1DD8E, keys 0x1E0AA). */
enum class TempleOption : uint8_t {
    Heal = 0,             /* A: 0x1D716 heal HP + clr $26 */
    RestoreAlignment,     /* B: 0x1D758 +$0D → +$6A */
    Donate,               /* C: 0x1D796 A4-$6714×100 + quest bit */
    BuySpell,             /* D/E/F: 0x1D872 cleric spell */
    Exit,
};

/* Mage guild menu (handler A4 thunk -$7D10 -> 0x1E3E6). Sorcerer spell buy only
 * (menu A-D); see TownServiceTransactions.h for the membership gate. */
enum class MageGuildOption : uint8_t {
    BuySpell = 0, /* slot 0..3 (A-D) */
    Exit,
};

/* Training Hall menu (handler -$7D16). Levels up via townSvcTrainLevelUp;
 * HP gain is ASM leaf 0x20390 (not 0x9BCA bash-door). */
enum class TrainingOption : uint8_t {
    LevelUp = 0,
    Exit,
};

struct TownServiceContext {
    Mm2RosterFile *roster = nullptr;
    const Mm2PartyLaunch *launch = nullptr;
    const Mm2ItemsFile *items = nullptr; /* items.dat, for blacksmith pricing */
    uint8_t *a4 = nullptr;
    int map_id = 0;
    gameplay::Rng *rng = nullptr; /* 0x22BC6 / -$7BB4; drink sick roll @ 0x19D64 */
};

/* One blacksmith shop slot as presented to the UI (item id + computed buy price
 * and the per-instance fields the purchase will store). item_id 0 = not stocked. */
struct SmithItemView {
    uint8_t item_id = 0;
    uint8_t bonus = 0;   /* magic '+' (lands in backpack flags +$46 for weapons/armor) */
    uint8_t charges = 0; /* misc charges (lands in backpack charges +$40) */
    uint32_t price = 0;  /* mm2_smith_price(items.dat gold, price_meta) */
};

/* --------------------------------------------------------------------------
 * Pub / tavern (OP_0E 0x03, handler 0x1d208 / jump table 0x1d650).
 *
 * ASM keys A–E (0x1D58E..):
 *   A 0x1CA2E Feeding frenzy
 *   B 0x1CAC4 Stat-boost submenu (A4-$580E / A4-$6738) — NOT drinks
 *   C 0x1CD2E Food/specialties (A4-$6760 prices + A4-$786C → +$76)
 *   D 0x1CFCA Tip bartender
 *   E 0x1D0B4 Rumors
 * Drinks live on selector 0xCA → 0x18F78 / 0x019030 (+$78 encoding), not key B.
 * -------------------------------------------------------------------------- */
enum class TavernOption : uint8_t {
    FeedingFrenzy = 0, /* A) Feeding frenzy (all you can carry) */
    StatBoost,         /* B) Buy (A-F) — A4-$6738 costs / 0x1C7EC apply */
    Specialties,       /* C) Specialties — A4-$6760 + meal effect */
    Tip,               /* D) Tip the bartender */
    Rumors,            /* E) Listen for rumors */
    Exit,
};

/* Drink menu items (str.dat ~208-213) — used by selector 0xCA encode path, not
 * tavern key B. Kept for UI that still surfaces drinks via that leaf. */
struct TavernDrinkView {
    const char *label; /* "Orc Beer", "Straight shot", ... */
};

/* Stat-boost submenu (tavern B @ 0x1CAC4): six A–F options, costs A4-$6738. */
struct TavernStatBoostView {
    const char *label; /* presentation caption (A4-$580E); FAQ-ish until decoded */
    uint16_t cost;     /* A4-$6738 BE u16 */
};

/* Food options for one town (A/B/C) — specialties menu C. */
struct TavernFoodView {
    const char *options[3]; /* A, B, C */
    uint16_t costs[3];      /* A4-$6760 BE u16 */
};

/* Max entries per town in each pub string pool.
 * The game init reads 40 strings for the RUMOR table (A4-$594E, shown by E)
 * then 40 more for the TIP table (A4-$58AE, shown by D): 8 per town × 5 towns.
 * The day-based selector (0x1c962) returns 0, 1, or 3, so only entry pairs
 * {0,1}, {2,3} and {6,7} are ever displayed; entries 4 and 5 are unused. */
static const int kPubRumorCount = 8; /* entries in the RUMOR pool (E) per town */
static const int kPubTipCount = 8;   /* entries in the TIP pool   (D) per town */
static const int kPubDrinkCount = 6;
static const int kPubFoodOptions = 3;
static const int kPubStatBoostCount = 6;

/* Per-location rumor/tip texts, food menu, and drink/stat-boost menus.
 * Filled by townSvcPubTables(). */
struct TavernMenuData {
    const char *rumors[kPubRumorCount]; /* E "Listen for rumors"  (A4-$594E) */
    const char *tips[kPubTipCount];     /* D "Tip the bartender"  (A4-$58AE) */
    TavernFoodView food;
    TavernDrinkView drinks[kPubDrinkCount];
    TavernStatBoostView boosts[kPubStatBoostCount];
};

/* Swappable interaction backend. Return false from a choose/select call to leave
 * the service (matches the engine's ESC/exit key in the menu loop). */
class ITownServiceUi {
public:
    virtual ~ITownServiceUi() = default;

    /* out_spell_slot is filled (0..MM2_TEMPLE_SPELL_SLOTS-1) only when out ==
     * TempleOption::BuySpell; unused otherwise. */
    virtual bool chooseTempleOption(const TownServiceContext &ctx, TempleOption &out,
                                    int &out_spell_slot) = 0;
    virtual bool chooseTrainingOption(const TownServiceContext &ctx, TrainingOption &out,
                                      int &stat_id) = 0;
    /* Pick a living party member; out_party_slot in [0, party_count). */
    virtual bool selectMember(const TownServiceContext &ctx, int &out_party_slot) = 0;

    /* Mage guild (0x1E3E6). Choose a spell slot 0..MM2_MAGE_GUILD_SLOTS-1 from
     * the priced stock, or return false to leave. Default: not supported. */
    virtual bool chooseMageGuildSpell(const TownServiceContext &,
                                      const Mm2SpellShopSlot[MM2_MAGE_GUILD_SLOTS],
                                      int & /*out_slot*/)
    {
        return false;
    }
    /* The shop-open membership gate (0x1E410) failed for the whole party. */
    virtual void reportGuildNotMember(const TownServiceContext &) {}
    virtual void reportSpellLearned(const TownSvcSpellResult &) {}
    virtual void reportSpellRejected(const TownSvcSpellResult &) {}

    /* Blacksmith (handler 0x1C54A). Choose a buy category (0..3 =
     * Weapons/Specials/Armor/Misc; mm2_town_tables Mm2SmithCategory) or return
     * false to leave the smith. Default: not supported -> driver exits. */
    virtual bool chooseSmithCategory(const TownServiceContext &, int & /*out_category*/)
    {
        return false;
    }
    /* Pick one of the six presented shop slots to buy (out_slot 0..5), or return
     * false to go back to the category menu. */
    virtual bool selectSmithItem(const TownServiceContext &, int /*category*/,
                                 const SmithItemView[MM2_SMITH_SLOTS], int & /*out_slot*/)
    {
        return false;
    }

    virtual void reportHeal(const TownSvcHealResult &) {}
    virtual void reportAlign(const TownSvcAlignResult &) {}
    virtual void reportTrain(const TownSvcTrainResult &) {}
    virtual void reportDonate(const TownSvcDonateResult &) {}
    virtual void reportSmithBuy(const TownSvcBuyResult &) {}
    virtual void reportBuyRejected(const TownSvcBuyResult &) {}
    virtual void reportNotEnoughGold() {}

    /* Pub / tavern (0x1D208). chooseTavernStatBoost: A–F slot 0..5 (key B).
     * chooseTavernFood: specialties A–C (key C). chooseTavernDrink retained for
     * selector-0xCA encode UI if bound separately. */
    virtual bool chooseTavernOption(const TownServiceContext &, const TavernMenuData &,
                                    TavernOption & /*out*/)
    {
        return false;
    }
    virtual bool chooseTavernStatBoost(const TownServiceContext &, const TavernMenuData &,
                                       int & /*out_slot*/)
    {
        return false;
    }
    virtual bool chooseTavernDrink(const TownServiceContext &, const TavernMenuData &,
                                   int & /*out_drink*/)
    {
        return false;
    }
    virtual bool chooseTavernFood(const TownServiceContext &, const TavernMenuData &,
                                  int & /*out_food*/)
    {
        return false;
    }
    virtual void reportTavernTip(const TownServiceContext &, const char * /*tip*/) {}
    virtual void reportTavernRumor(const TownServiceContext &, const char * /*rumor*/) {}
    virtual void reportTavernFood(const TownServiceContext &, int /*food_idx*/) {}
    virtual void reportTavernDrink(const TownServiceContext &, int /*drink_idx*/) {}
    virtual void reportTavernStatBoost(const TownSvcStatBoostResult &) {}
    virtual void reportTavernSpecialty(const TownSvcSpecialtyResult &) {}
};

/* Resolve a party slot (0..7) to its mutable roster record, or nullptr. */
Mm2RosterRecord *townSvcMemberRecord(const TownServiceContext &ctx, int party_slot);

/* Run the temple / training menu loop until the UI backend exits. Each accepted
 * option applies the faithful transaction and reports the result via the UI. */
void townSvcRunTemple(ITownServiceUi &ui, const TownServiceContext &ctx);
void townSvcRunTraining(ITownServiceUi &ui, const TownServiceContext &ctx);

/* Run the mage guild sorcerer spell shop (0x1E3E6). The shop-open membership
 * gate (0x1E410) is checked ONCE against the whole party before any menu is
 * shown (townSvcPartyHasMageGuildMember); if it fails the driver reports
 * reportGuildNotMember() and returns without opening a menu, matching the
 * ASM's "shop doesn't open" branch. See TownServiceTransactions.h for how a
 * party earns membership (event-script field selector 0x74, or the unported
 * class-quest reward loop). */
void townSvcRunMageGuild(ITownServiceUi &ui, const TownServiceContext &ctx);

/* Run the blacksmith buy menu (0x1C54A). Requires ctx.items for pricing; each
 * accepted purchase computes the shop price from items.dat gold and applies
 * townSvcSmithBuy to the selected member. Sell (0x1BC26) and Identify (0x1B6E0)
 * are not driven here (documented gap). */
void townSvcRunSmith(ITownServiceUi &ui, const TownServiceContext &ctx);

/* Populate TavernMenuData for a given map_id (0-4 for the five towns). */
void townSvcPubTables(int map_id, TavernMenuData &out);

void townSvcRunTavern(ITownServiceUi &ui, const TownServiceContext &ctx);

}  // namespace mm2::events

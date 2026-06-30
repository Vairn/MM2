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

namespace mm2::events {

/* Temple menu (handler 0x1D208). Option set as traced: per-character HP/condition
 * restore (heal), donation (doc 28 §5.2). The cleric-spell purchase slots (D-F,
 * 0x1DAC6 cost decode -> spellbook bit) are engine-deferred (spell system). */
enum class TempleOption : uint8_t {
    Heal = 0,           /* 0x1D716: pay heal cost, restore HP + clear condition */
    RestoreCondition,   /* clr.b $26 only (0x1D736) */
    Donate,             /* 0x1CA2E/§5.2: pay donation_gold, set quest bit */
    Exit,
};

/* Training Hall menu (handler -$7D16). The hall levels the chosen character up
 * from experience (townSvcTrainLevelUp); there is no stat sub-menu. The exact
 * per-level HP RNG roll (0x9BCA) and calendar mode select (0x9B48) are deferred. */
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
};

/* One blacksmith shop slot as presented to the UI (item id + computed buy price
 * and the per-instance fields the purchase will store). item_id 0 = not stocked. */
struct SmithItemView {
    uint8_t item_id = 0;
    uint8_t bonus = 0;   /* magic '+' (lands in backpack flags +$46 for weapons/armor) */
    uint8_t charges = 0; /* misc charges (lands in backpack charges +$40) */
    uint32_t price = 0;  /* mm2_smith_price(items.dat gold, price_meta) */
};

/* Swappable interaction backend. Return false from a choose/select call to leave
 * the service (matches the engine's ESC/exit key in the menu loop). */
class ITownServiceUi {
public:
    virtual ~ITownServiceUi() = default;

    virtual bool chooseTempleOption(const TownServiceContext &ctx, TempleOption &out) = 0;
    virtual bool chooseTrainingOption(const TownServiceContext &ctx, TrainingOption &out,
                                      int &stat_id) = 0;
    /* Pick a living party member; out_party_slot in [0, party_count). */
    virtual bool selectMember(const TownServiceContext &ctx, int &out_party_slot) = 0;

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
    virtual void reportTrain(const TownSvcTrainResult &) {}
    virtual void reportDonate(const TownSvcDonateResult &) {}
    virtual void reportSmithBuy(const TownSvcBuyResult &) {}
    virtual void reportBuyRejected(const TownSvcBuyResult &) {}
    virtual void reportNotEnoughGold() {}
};

/* Resolve a party slot (0..7) to its mutable roster record, or nullptr. */
Mm2RosterRecord *townSvcMemberRecord(const TownServiceContext &ctx, int party_slot);

/* Run the temple / training menu loop until the UI backend exits. Each accepted
 * option applies the faithful transaction and reports the result via the UI. */
void townSvcRunTemple(ITownServiceUi &ui, const TownServiceContext &ctx);
void townSvcRunTraining(ITownServiceUi &ui, const TownServiceContext &ctx);

/* Run the blacksmith buy menu (0x1C54A). Requires ctx.items for pricing; each
 * accepted purchase computes the shop price from items.dat gold and applies
 * townSvcSmithBuy to the selected member. Sell (0x1BC26) and Identify (0x1B6E0)
 * are not driven here (documented gap). */
void townSvcRunSmith(ITownServiceUi &ui, const TownServiceContext &ctx);

}  // namespace mm2::events

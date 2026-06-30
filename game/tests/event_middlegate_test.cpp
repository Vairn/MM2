// Offline decode + VM smoke test for Middlegate (loc 0) events:
//   All OP_04 door labels — scanner fires at correct (x,y,facing)
//   Event 20 — OP_01 + OP_09 city gates Y/N at (5,15)/ENTER + OP_0C exit to map 11
//   Shop tiles — OP_0B service sign survives OP_0E stub
//
// Usage: event_middlegate_test <data_dir>

#include <cstdio>
#include <cstring>

#include "mm2/GameState.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/ServiceSignResolver.h"
#include "mm2/events/TownServiceMenu.h"
#include "mm2/events/TownServiceTransactions.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/ViewportAnmOverlay.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/PlayTownServiceUi.h"
#include "mm2/world/MapWorld.h"

#include "mm2_items_codec.h"
#include "mm2_party_launch.h"
#include "mm2_gamestate.h"

namespace {

bool expect(bool cond, const char *msg, int &fails)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++fails;
        return false;
    }
    return true;
}

bool expectTableIdLoadsAnm(const char *data_dir, int table_id, int &fails)
{
    mm2::gfx::ViewportAnmOverlay overlay;
    char tag[96];
    std::snprintf(tag, sizeof(tag), "OP_0B table id %d loads %02d.anm", table_id, table_id);
    if (!expect(overlay.loadFromId(data_dir, table_id), tag, fails)) {
        return false;
    }
    return expect(overlay.loaded(), tag, fails);
}

struct DoorCase {
    int event_id;
    int x;
    int y;
    char facing;
    const char *label_snippet;
};

/* Cond bits match context_mask_tbl (W=0x10 S=0x20 E=0x40 N=0x80) per scanner @ 0x17684. */
constexpr DoorCase kOp04Doors[] = {
    {1, 7, 5, 'S', "Middlegate Inn"},
    {2, 5, 4, 'W', "Blacksmith"},
    {3, 5, 6, 'W', "Slaughtered Lamb"},
    {4, 7, 6, 'N', "Gateway Temple"},
    {5, 9, 7, 'E', "Turkov"},
    {6, 13, 6, 'S', "Arena"},
    {7, 1, 5, 'W', "Poorman"},
    {8, 7, 13, 'N', "Mage Guild"},
    {10, 14, 6, 'S', "Exit Only"},
    {11, 2, 8, 'W', "Lock and Key"},
    {12, 1, 15, 'W', "Otto Mapper"},
    {13, 2, 12, 'E', "Edmund"},
    {14, 2, 9, 'W', "Track and Trail"},
    {15, 12, 11, 'S', "Brain Detox"},
    {28, 13, 8, 'E', "Travel Moore"},
    {37, 10, 3, 'N', "Skeleton Closet"},
};

bool scanDoorCase(mm2::events::EventRuntime &runtime, mm2::GameStateView &gs,
                  mm2::world::MapWorld &world, const DoorCase &door, int &fails)
{
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(static_cast<uint8_t>(door.x));
    gs.setCoordY(static_cast<uint8_t>(door.y));
    gs.setFacingKey(door.facing);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);

    char tag[96];
    std::snprintf(tag, sizeof(tag), "event %02d OP_04 fires at (%d,%d) facing %c", door.event_id,
                  door.x, door.y, door.facing);
    if (!expect(runtime.scanAndRun(gs, world), tag, fails)) {
        return false;
    }
    expect(!runtime.blocksMovement(), tag, fails);
    expect(runtime.textView().layerCount() > 0, "OP_04 layer persists after script end", fails);
    return true;
}

bool scanDoorNoFire(mm2::events::EventRuntime &runtime, mm2::GameStateView &gs,
                    mm2::world::MapWorld &world, const DoorCase &door, int &fails)
{
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(static_cast<uint8_t>(door.x));
    gs.setCoordY(static_cast<uint8_t>(door.y));
    gs.setFacingKey(door.facing);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);

    char tag[96];
    std::snprintf(tag, sizeof(tag), "event %02d OP_04 does NOT fire at (%d,%d) facing %c",
                  door.event_id, door.x, door.y, door.facing);
    expect(!runtime.scanAndRun(gs, world), tag, fails);
    expect(runtime.textView().layerCount() == 0, "no OP_04 layer when cond mismatches", fails);
    return true;
}

void scanDoorWrongFacings(mm2::events::EventRuntime &runtime, mm2::GameStateView &gs,
                          mm2::world::MapWorld &world, const DoorCase &door, int &fails)
{
    const char facings[] = {'N', 'E', 'S', 'W'};
    for (char f : facings) {
        if (f == door.facing) {
            continue;
        }
        scanDoorNoFire(runtime, gs, world, DoorCase{door.event_id, door.x, door.y, f, door.label_snippet},
                       fails);
    }
}

/* Scripted town-service UI backend: replays a fixed sequence of menu choices so
 * the faithful menu driver can be exercised headlessly. */
class ScriptedTownUi : public mm2::events::ITownServiceUi {
public:
    struct Step {
        bool is_temple;
        mm2::events::TempleOption temple;
        mm2::events::TrainingOption train;
        int stat_id;
        int member;  /* party slot, or -1 to cancel member select */
        bool exit;   /* true -> exit the menu instead of choosing */
    };
    void push(const Step &s) { steps_[count_++] = s; }

    bool chooseTempleOption(const mm2::events::TownServiceContext &, mm2::events::TempleOption &out) override
    {
        if (cursor_ >= count_ || steps_[cursor_].exit) {
            return false;
        }
        out = steps_[cursor_].temple;
        return true;
    }
    bool chooseTrainingOption(const mm2::events::TownServiceContext &, mm2::events::TrainingOption &out,
                              int &stat_id) override
    {
        if (cursor_ >= count_ || steps_[cursor_].exit) {
            return false;
        }
        out = steps_[cursor_].train;
        stat_id = steps_[cursor_].stat_id;
        return true;
    }
    bool selectMember(const mm2::events::TownServiceContext &, int &out_slot) override
    {
        if (cursor_ >= count_) {
            return false;
        }
        out_slot = steps_[cursor_].member;
        ++cursor_;  /* consume the step after member select */
        return out_slot >= 0;
    }
    void reportHeal(const mm2::events::TownSvcHealResult &r) override { last_heal = r; ++heals; }
    void reportTrain(const mm2::events::TownSvcTrainResult &r) override { last_train = r; ++trains; }
    void reportDonate(const mm2::events::TownSvcDonateResult &r) override { last_donate = r; ++donates; }
    void reportNotEnoughGold() override { ++poor; }

    mm2::events::TownSvcHealResult last_heal{};
    mm2::events::TownSvcTrainResult last_train{};
    mm2::events::TownSvcDonateResult last_donate{};
    int heals = 0, trains = 0, donates = 0, poor = 0;

private:
    Step steps_[16]{};
    int count_ = 0;
    int cursor_ = 0;
};

/* Scripted blacksmith UI: one category -> one item slot -> one member, then exit. */
class ScriptedSmithUi : public mm2::events::ITownServiceUi {
public:
    int category = MM2_SMITH_WEAPONS;
    int item_slot = 0;
    int member = 0;

    /* The temple/training virtuals are unused here but pure, so stub them. */
    bool chooseTempleOption(const mm2::events::TownServiceContext &,
                            mm2::events::TempleOption &) override { return false; }
    bool chooseTrainingOption(const mm2::events::TownServiceContext &,
                              mm2::events::TrainingOption &, int &) override { return false; }

    bool chooseSmithCategory(const mm2::events::TownServiceContext &, int &out) override
    {
        if (category_consumed_) {
            return false; /* leave after one category pass */
        }
        category_consumed_ = true;
        out = category;
        return true;
    }
    bool selectSmithItem(const mm2::events::TownServiceContext &, int,
                         const mm2::events::SmithItemView view[MM2_SMITH_SLOTS],
                         int &out_slot) override
    {
        last_view_price = (item_slot >= 0 && item_slot < MM2_SMITH_SLOTS)
                              ? view[item_slot].price
                              : 0u;
        last_view_id = (item_slot >= 0 && item_slot < MM2_SMITH_SLOTS)
                           ? view[item_slot].item_id
                           : 0;
        out_slot = item_slot;
        return item_slot >= 0;
    }
    bool selectMember(const mm2::events::TownServiceContext &, int &out_slot) override
    {
        out_slot = member;
        return member >= 0;
    }
    void reportSmithBuy(const mm2::events::TownSvcBuyResult &r) override { last_buy = r; ++buys; }
    void reportBuyRejected(const mm2::events::TownSvcBuyResult &r) override
    {
        last_reject = r;
        ++rejects;
    }
    void reportNotEnoughGold() override { ++poor; }

    mm2::events::TownSvcBuyResult last_buy{};
    mm2::events::TownSvcBuyResult last_reject{};
    uint32_t last_view_price = 0;
    uint8_t last_view_id = 0;
    int buys = 0, rejects = 0, poor = 0;

private:
    bool category_consumed_ = false;
};

void setupMember(Mm2RosterFile &roster, int idx, uint8_t level, uint32_t gold)
{
    Mm2RosterRecord &rec = roster.records[idx];
    std::memset(&rec, 0, sizeof(rec));
    mm2_roster_set_name(&rec, "Tester");
    rec.level = level;
    rec.gold = gold;
    rec.condition = 0x10;       /* some affliction to clear on heal */
    rec.hp_aux = 40;            /* permanent max (record+0x60) */
    rec.hp_max = 10;            /* working max (record+0x5E) */
    rec.hp_current = 5;         /* current HP (record+0x74) */
    rec.might_base = 20;        /* record+0x6B */
    rec.class_id = 0;           /* Knight (XP Group A) — record+0x0F */
    rec.endurance_base = 14;    /* attr bonus 1 (record+0x73) */
    rec.experience = 0;         /* record+0x62 (callers set for level-up tests) */
}

void testTownServiceTransactions(int &fails)
{
    using namespace mm2::events;

    /* ---- Cost helpers via the per-town table (doc 34 §13.2). ---- */
    expect(townSvcHealingCost(5, 0) == 50u, "townSvc heal cost Middlegate L5 = 50", fails);
    expect(townSvcTrainingCost(5, 0) == 250u, "townSvc train cost Middlegate L5 = 250", fails);
    expect(townSvcTrainingCost(5, 1) == 1250u, "townSvc train cost Atlantium L5 = 1250", fails);

    /* ---- Temple heal (0x1D716/0x1DD48): deduct cost, restore HP, clear cond. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        Mm2RosterRecord &rec = roster.records[0];
        const TownSvcHealResult r = townSvcHeal(rec, townSvcHealingCost(rec.level, 0));
        expect(r.paid, "temple heal paid", fails);
        expect(r.cost == 50u, "temple heal charged level*index*10 = 50", fails);
        expect(rec.gold == 9950u, "temple heal deducts 50 from char gold", fails);
        expect(rec.hp_max == 40 && rec.hp_current == 40, "temple heal restores HP to permanent max", fails);
        expect(rec.condition == 0, "temple heal clears condition byte", fails);
    }

    /* ---- Heal affordability gate (0x1C9C0 scc): no gold change on failure. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10);  /* < 50 cost */
        Mm2RosterRecord &rec = roster.records[0];
        const TownSvcHealResult r = townSvcHeal(rec, townSvcHealingCost(rec.level, 0));
        expect(!r.paid, "temple heal fails when gold < cost", fails);
        expect(rec.gold == 10u, "failed heal leaves gold untouched", fails);
        expect(rec.hp_current == 5, "failed heal leaves HP untouched", fails);
        expect(rec.condition == 0x10, "failed heal leaves condition untouched", fails);
    }

    /* ---- Training Hall = XP-based LEVEL UP (not a stat raise). ----
     * The Training Hall advances a character one level when they have the
     * experience for the next level (class XP curve, doc 32) and pay the fee
     * (level*index*50). class_id 0 (Knight) is XP Group A: L1->L2 needs 1500 XP. */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 1, 10000);   /* L1 Knight */
        Mm2RosterRecord &rec = roster.records[0];
        rec.experience = 2000;              /* >= 1500 (Group A L2 threshold) */
        const TownSvcTrainResult r = townSvcTrainLevelUp(rec, 0 /*Middlegate*/);
        expect(r.eligible, "L1 Knight with 2000 XP is eligible to reach L2", fails);
        expect(r.paid, "training fee paid", fails);
        expect(r.cost == 50u, "training fee = level*index*50 = 1*1*50 = 50", fails);
        expect(rec.gold == 9950u, "training deducts the fee from char gold", fails);
        expect(r.leveled && rec.level == 2, "Knight advanced from L1 to L2", fails);
        /* Knight HP/level = 12 + END(14)->bonus 1 = 13 added to permanent max. */
        expect(r.hp_gain == 13 && rec.hp_aux == 53, "level-up adds class HP + END bonus", fails);
        expect(rec.hp_current == rec.hp_aux, "level-up heals to new max", fails);
    }

    /* ---- Training XP gate: under threshold -> not eligible AND no charge. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 1, 10000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.experience = 1000;              /* < 1500 needed for L2 */
        const TownSvcTrainResult r = townSvcTrainLevelUp(rec, 0);
        expect(!r.eligible, "below XP threshold is not eligible", fails);
        expect(!r.paid && rec.gold == 10000u, "ineligible training charges nothing", fails);
        expect(rec.level == 1, "ineligible training does not raise level", fails);
    }

    /* ---- Training fee gate: eligible by XP but can't afford the fee. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10);      /* L5: fee = 5*1*50 = 250 > 10 gold */
        Mm2RosterRecord &rec = roster.records[0];
        rec.experience = 100000000;         /* plenty for L6 */
        const TownSvcTrainResult r = townSvcTrainLevelUp(rec, 0);
        expect(r.eligible && !r.paid, "eligible but unaffordable -> not paid", fails);
        expect(rec.level == 5 && rec.gold == 10u, "unaffordable training leaves level/gold", fails);
    }

    /* ---- Class XP curves: Group B (e.g. Sorcerer 4) needs more XP than A. ---- */
    {
        expect(mm2_class_xp_group(0) == 0 && mm2_class_xp_group(7) == 0,
               "Knight + Barbarian are XP Group A", fails);
        expect(mm2_class_xp_group(4) == 1 && mm2_class_xp_group(1) == 1,
               "Sorcerer + Paladin are XP Group B", fails);
        expect(mm2_class_xp_for_level(0, 2) == 1500u, "Group A L2 threshold = 1500 XP", fails);
        expect(mm2_class_xp_for_level(4, 2) == 2000u, "Group B L2 threshold = 2000 XP", fails);
    }

    /* ---- Stat SHRINE leaf (0x1C898) — NOT the Training Hall. Retained for the
     * separate beautify/olympic stat-add path; cap + overflow guards. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        Mm2RosterRecord &rec = roster.records[0];
        const bool raised = townSvcTrainStat(rec, 0 /*might*/, 0 /*Middlegate add 5*/);
        expect(raised && rec.might_base == 25, "stat shrine adds +5 might (20->25)", fails);
        rec.might_base = 49;  /* 49 + add 3 = 52 >= cap 50 (Sandsobar) -> no write */
        expect(!townSvcTrainStat(rec, 0, 4), "stat shrine respects the cap", fails);
        expect(rec.might_base == 49, "stat stays at 49 when over cap", fails);
        rec.might_base = 254; /* 254 + 5 = 259 & 0xFF = 3 < add 5 -> overflow skip */
        expect(!townSvcTrainStat(rec, 0, 0), "stat shrine overflow guard blocks wrap", fails);
        expect(rec.might_base == 254, "stat unchanged on overflow", fails);
    }

    /* ---- Temple donation (doc 28 §5.2): gold + quest bit into A4-$799E. ---- */
    {
        uint8_t a4img[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};
        uint8_t *a4 = mm2_gs_base_from_image(a4img);
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        Mm2RosterRecord &rec = roster.records[0];
        TownSvcDonateResult r = townSvcTempleDonate(a4, rec, 0 /*Middlegate cost 20 bit 0x01*/);
        expect(r.paid && r.cost == 20u, "Middlegate donation costs 20", fails);
        expect(rec.gold == 9980u, "donation deducts 20 from char gold", fails);
        expect(mm2_gs_u8(a4, MM2_GS_TEMPLE_DONATION) == 0x01, "donation sets Middlegate quest bit", fails);
        expect(!r.all_temples_donated, "one donation is not all five", fails);
        /* Donate at the other four towns -> bitfield reaches 0x1F. */
        townSvcTempleDonate(a4, rec, 1);
        townSvcTempleDonate(a4, rec, 2);
        townSvcTempleDonate(a4, rec, 3);
        r = townSvcTempleDonate(a4, rec, 4);
        expect(mm2_gs_u8(a4, MM2_GS_TEMPLE_DONATION) == 0x1F, "all five donation bits set", fails);
        expect(r.all_temples_donated, "5th donation reports all temples donated", fails);
    }

    /* ---- Menu driver with scripted UI: heal member 0 then exit. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        uint8_t a4img[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};

        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.a4 = mm2_gs_base_from_image(a4img);
        ctx.map_id = 0;

        ScriptedTownUi ui;
        ui.push({true, TempleOption::Heal, TrainingOption::Exit, 0, 0, false});
        ui.push({true, TempleOption::Exit, TrainingOption::Exit, 0, -1, true});
        townSvcRunTemple(ui, ctx);
        expect(ui.heals == 1, "menu driver applied one heal", fails);
        expect(roster.records[0].hp_current == 40, "menu heal restored HP via driver", fails);
        expect(roster.records[0].gold == 9950u, "menu heal deducted cost via driver", fails);
    }

    /* ---- Menu driver training: train might on member 0 then exit. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        uint8_t a4img[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};

        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.a4 = mm2_gs_base_from_image(a4img);
        ctx.map_id = 0;

        roster.records[0].level = 1;
        roster.records[0].experience = 2000; /* enough for Knight L2 (1500) */

        ScriptedTownUi ui;
        ui.push({false, TempleOption::Exit, TrainingOption::LevelUp, 0, 0, false});
        ui.push({false, TempleOption::Exit, TrainingOption::Exit, 0, -1, true});
        townSvcRunTraining(ui, ctx);
        expect(ui.trains == 1, "menu driver applied one training", fails);
        expect(roster.records[0].level == 2, "menu training leveled char L1->L2 via driver", fails);
        expect(roster.records[0].gold == 9950u, "menu training deducted the 50 fee via driver", fails);
        expect(ui.last_train.leveled, "driver reported a level-up", fails);
    }
}

uint32_t itemGold(const Mm2ItemsFile &items, uint8_t id)
{
    const Mm2ItemRecord *rec = mm2_items_lookup(&items, id);
    return rec ? rec->gold : 0u;
}

void testSmithTransactions(int &fails, const Mm2ItemsFile &items)
{
    using namespace mm2::events;

    /* ---- Inventory + price table sanity (0x1C00E ids + 0x1BF16 price). ---- */
    {
        Mm2SmithSlot slots[MM2_SMITH_SLOTS];
        expect(mm2_smith_inventory(0, MM2_SMITH_WEAPONS, slots) == 1, "smith weapons inventory ok", fails);
        expect(slots[0].item_id == 4 && slots[5].item_id == 13,
               "Middlegate weapons = Dagger..Nunchakas (ids 4,13)", fails);
        /* Middlegate weapon meta is 0 -> price equals items.dat gold. */
        expect(mm2_smith_price(itemGold(items, 4), 0) == itemGold(items, 4),
               "weapon meta 0 -> price == items.dat gold", fails);
        /* Atlantium Spear (id 15) meta 3 -> base*2 + (3-1)*1000. */
        const uint32_t spear = itemGold(items, 15);
        expect(mm2_smith_price(spear, 3) == spear * 2u + 2000u,
               "weapon meta 3 -> base*2 + 2000", fails);
    }

    /* ---- Buy: deduct items.dat gold, add item to first empty backpack slot. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0; /* must be Good to buy */
        const uint8_t id = 4 /*Dagger*/;
        const uint32_t price = mm2_smith_price(itemGold(items, id), 0);
        const TownSvcBuyResult r = townSvcSmithBuy(rec, id, 0, 0, price);
        expect(r.bought && r.slot == 0, "smith buy placed item in backpack slot 0", fails);
        expect(r.price == price, "smith buy charged the shop price", fails);
        expect(rec.gold == 100000u - price, "smith buy deducts items.dat gold price", fails);
        expect(mm2_roster_backpack(&rec, 0).item_id == id, "bought item id in backpack", fails);
        expect(mm2_roster_backpack(&rec, 0).charges == 0 && mm2_roster_backpack(&rec, 0).flags == 0,
               "weapon buy: charges/flags 0", fails);
    }

    /* ---- Misc buy: meta -> backpack charges, price uses base gold (no '+' mult). ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0;
        Mm2SmithSlot slots[MM2_SMITH_SLOTS];
        mm2_smith_inventory(0, MM2_SMITH_MISC, slots);
        uint8_t pm = 0, ch = 0, fl = 0;
        mm2_smith_buy_fields(MM2_SMITH_MISC, slots[1].meta, &pm, &ch, &fl);
        expect(pm == 0 && fl == 0 && ch == slots[1].meta,
               "misc buy fields: charges=meta, flags/price_meta 0", fails);
        const uint32_t price = mm2_smith_price(itemGold(items, slots[1].item_id), pm);
        const TownSvcBuyResult r = townSvcSmithBuy(rec, slots[1].item_id, ch, fl, price);
        expect(r.bought, "misc buy placed", fails);
        expect(mm2_roster_backpack(&rec, 0).charges == slots[1].meta,
               "misc buy stored charges in backpack +$40", fails);
    }

    /* ---- Insufficient gold is rejected; no gold change, no item added. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0;
        const uint8_t id = 4;
        const uint32_t price = mm2_smith_price(itemGold(items, id), 0);
        const TownSvcBuyResult r = townSvcSmithBuy(rec, id, 0, 0, price);
        expect(!r.bought && r.reject == TownSvcBuyReject::NotEnoughGold,
               "smith buy rejected when gold < price", fails);
        expect(rec.gold == 1u, "rejected buy leaves gold untouched", fails);
        expect(mm2_roster_backpack(&rec, 0).item_id == 0, "rejected buy adds no item", fails);
    }

    /* ---- Condition guard (0x1BE4C): an afflicted char cannot buy. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0x10; /* afflicted */
        const TownSvcBuyResult r = townSvcSmithBuy(rec, 4, 0, 0, 10);
        expect(!r.bought && r.reject == TownSvcBuyReject::Condition, "afflicted char cannot buy", fails);
        expect(rec.gold == 100000u, "condition reject leaves gold untouched", fails);
    }

    /* ---- Backpack-full guard (0x1BE82): all six slots used -> reject. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0;
        for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
            Mm2RosterItemSlot full;
            full.item_id = 99;
            full.charges = 0;
            full.flags = 0;
            mm2_roster_set_backpack(&rec, i, full);
        }
        const TownSvcBuyResult r = townSvcSmithBuy(rec, 4, 0, 0, 10);
        expect(!r.bought && r.reject == TownSvcBuyReject::BackpackFull,
               "full backpack rejects buy", fails);
        expect(rec.gold == 100000u, "full-backpack reject leaves gold untouched", fails);
    }

    /* ---- Menu driver: buy Middlegate Dagger on member 0 via the bound UI. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100000);
        roster.records[0].condition = 0;
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        uint8_t a4img[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};

        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.items = &items;
        ctx.a4 = mm2_gs_base_from_image(a4img);
        ctx.map_id = 0;

        ScriptedSmithUi ui;
        ui.category = MM2_SMITH_WEAPONS;
        ui.item_slot = 0; /* Dagger */
        ui.member = 0;
        townSvcRunSmith(ui, ctx);
        const uint32_t price = mm2_smith_price(itemGold(items, 4), 0);
        expect(ui.buys == 1, "smith menu driver applied one buy", fails);
        expect(ui.last_view_id == 4 && ui.last_view_price == price,
               "smith menu priced Dagger from items.dat", fails);
        expect(roster.records[0].gold == 100000u - price, "smith menu deducted price via driver", fails);
        expect(mm2_roster_backpack(&roster.records[0], 0).item_id == 4,
               "smith menu added Dagger to backpack via driver", fails);
    }
}

/* Drive the interactive (multi-frame) PlayTownServiceUi backend through one temple
 * heal and one blacksmith buy via simulated keypresses — the same backend bound into
 * the playable game loop (GameSession). Exercises the capture path (townSvcRun* ->
 * pending) + the per-frame state machine (begin/handleKey) + a render() pass. */
void testPlayTownServiceUi(int &fails, const Mm2ItemsFile &items)
{
    using namespace mm2::events;

    mm2::gfx::ScreenCompositor comp;

    /* ---- Temple: A) Heal on member 1, then Esc to leave. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);  /* L5 Middlegate heal cost = 50 */
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        uint8_t a4img[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};

        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.items = &items;
        ctx.a4 = mm2_gs_base_from_image(a4img);
        ctx.map_id = 0;

        mm2::ui::PlayTownServiceUi ui;
        /* The blocking driver only records the request (presentation is deferred). */
        townSvcRunTemple(ui, ctx);
        expect(ui.pending(), "play temple selector records a pending menu", fails);
        ui.begin();
        expect(ui.active() && !ui.pending(), "play temple menu active after begin", fails);
        ui.render(comp); /* Menu phase render must not crash */

        ui.handleKey('A', false); /* Heal -> member select */
        ui.render(comp);          /* Member phase render */
        ui.handleKey('1', false); /* member 1 -> apply heal */
        expect(roster.records[0].gold == 9950u, "play temple heal deducted 50 gp", fails);
        expect(roster.records[0].hp_current == 40, "play temple heal restored HP to max", fails);
        expect(roster.records[0].condition == 0, "play temple heal cleared condition", fails);

        ui.handleKey(0, true); /* Esc from top menu -> close */
        expect(!ui.active(), "play temple menu closes on Esc", fails);
    }

    /* ---- Training Hall: opens straight on the trainee prompt (no stat menu),
     * member 1 levels up from XP, then Esc to leave. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 1, 10000);   /* L1 Knight */
        roster.records[0].experience = 2000; /* enough for L2 (1500) */
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        uint8_t a4img[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};

        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.items = &items;
        ctx.a4 = mm2_gs_base_from_image(a4img);
        ctx.map_id = 0;

        mm2::ui::PlayTownServiceUi ui;
        townSvcRunTraining(ui, ctx);
        expect(ui.pending(), "play training selector records a pending menu", fails);
        ui.begin();
        ui.render(comp);          /* opens directly on Member prompt (no stat menu) */
        ui.handleKey('1', false); /* trainee 1 -> level up */
        expect(roster.records[0].level == 2, "play training leveled L1->L2", fails);
        expect(roster.records[0].gold == 9950u, "play training deducted the 50 fee", fails);
        expect(ui.active(), "play training stays open after a level-up", fails);

        ui.handleKey(0, true);    /* Esc from trainee prompt -> close */
        expect(!ui.active(), "play training closes on Esc", fails);
    }

    /* ---- Blacksmith: A) Weapons -> item 1 (Dagger) -> member 1 buys. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100000);
        roster.records[0].condition = 0; /* must be Good to buy */
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        uint8_t a4img[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};

        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.items = &items;
        ctx.a4 = mm2_gs_base_from_image(a4img);
        ctx.map_id = 0;

        mm2::ui::PlayTownServiceUi ui;
        townSvcRunSmith(ui, ctx);
        expect(ui.pending(), "play smith selector records a pending menu", fails);
        ui.begin();
        ui.handleKey('A', false); /* Weapons category -> item list */
        ui.render(comp);          /* SmithItems phase render */
        ui.handleKey('1', false); /* slot 0 = Dagger -> member select */
        ui.handleKey('1', false); /* member 1 -> buy */

        const uint32_t price = mm2_smith_price(itemGold(items, 4), 0);
        expect(roster.records[0].gold == 100000u - price, "play smith buy deducted Dagger price",
               fails);
        expect(mm2_roster_backpack(&roster.records[0], 0).item_id == 4,
               "play smith buy added Dagger to backpack", fails);
        expect(ui.active(), "play smith menu stays open after a buy", fails);

        ui.handleKey(0, true); /* Esc back to category menu */
        ui.handleKey(0, true); /* Esc from top menu -> close */
        expect(!ui.active(), "play smith menu closes after two Esc", fails);
    }
}

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : "../..";
    int fails = 0;

    Mm2ItemsFile items{};
    char items_path[512];
    std::snprintf(items_path, sizeof(items_path), "%s/items.dat", data_dir);
    if (mm2_items_load_file(items_path, &items) == MM2_ITEMS_OK) {
        const Mm2ItemRecord *club = mm2_items_lookup(&items, 1);
        char name[32];
        if (club) {
            mm2_item_name_to_cstr(club, name, sizeof(name));
            expect(std::strcmp(name, "Small Club") == 0, "item #1 name is 'Small Club'", fails);
        } else {
            expect(false, "item #1 lookup", fails);
        }
    } else {
        expect(false, "load items.dat", fails);
    }

    /* Town-service cost formulas (FAQ §3-6, doc 34 §13.2). town_index from the
     * map→index table [1,5,2,4,2]; these feed the deferred temple/training
     * engine (OP_0E 0x02/0x04). */
    expect(mm2::events::eventVmTrainingTownIndex(0) == 1, "Middlegate train index = 1", fails);
    expect(mm2::events::eventVmTrainingTownIndex(1) == 5, "Atlantium train index = 5", fails);
    expect(mm2::events::eventVmTrainingTownIndex(4) == 2, "Sandsobar train index = 2", fails);
    /* Training = level × town_index × 50: Middlegate L5 = 250, Atlantium L5 = 1250. */
    expect(mm2::events::eventVmTrainingCostPerChar(5, mm2::events::eventVmTrainingTownIndex(0)) == 250u,
           "Middlegate L5 training cost = 250 gp", fails);
    expect(mm2::events::eventVmTrainingCostPerChar(5, mm2::events::eventVmTrainingTownIndex(1)) == 1250u,
           "Atlantium L5 training cost = 1250 gp", fails);
    /* Healing (living) = level × town_index × 10: Middlegate L5 = 50, Vulcania L3 = 120. */
    expect(mm2::events::eventVmHealingCostPerChar(5, mm2::events::eventVmTrainingTownIndex(0)) == 50u,
           "Middlegate L5 healing cost = 50 gp", fails);
    expect(mm2::events::eventVmHealingCostPerChar(3, mm2::events::eventVmTrainingTownIndex(3)) == 120u,
           "Vulcania L3 healing cost = 120 gp", fails);

    /* ASM-faithful town-service transaction layer (temple heal/restore/donate +
     * stat training) and the pluggable menu driver. */
    testTownServiceTransactions(fails);

    /* Blacksmith (OP_0E 0x06) buy transactions + menu driver (needs items.dat). */
    testSmithTransactions(fails, items);

    /* Interactive multi-frame backend bound into the game loop (PlayTownServiceUi). */
    testPlayTownServiceUi(fails, items);

    mm2::events::EventRuntime runtime;
    if (!runtime.load(data_dir)) {
        std::fprintf(stderr, "FAIL: load event.dat\n");
        return 1;
    }

    mm2::world::MapWorld world;
    if (!world.load(data_dir) || !world.enterScreen(0)) {
        std::fprintf(stderr, "FAIL: load map/attrib\n");
        return 1;
    }

    uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
    mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
    gs.initCalendarDefaults();

    Mm2PartyLaunch launch{};
    mm2_party_launch_build(&launch, 1, nullptr, 0);
    mm2_party_launch_apply(gs.a4(), &launch);

    if (!runtime.enterLocation(0, gs, world)) {
        std::fprintf(stderr, "FAIL: enterLocation(0)\n");
        return 1;
    }

    for (const DoorCase &door : kOp04Doors) {
        scanDoorCase(runtime, gs, world, door, fails);
        scanDoorWrongFacings(runtime, gs, world, door, fails);
    }

    /* Event 01 @ (7,5) facing S — re-scan must not clear triplet permanently. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(5);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 01 re-scan at (7,5) S", fails);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 01 fires again at (7,5) S", fails);

    /* Turn away from inn — stale OP_04 label must clear. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(5);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 01 at (7,5) S", fails);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(!runtime.scanAndRun(gs, world), "event 01 does not fire facing N", fails);
    expect(runtime.textView().layerCount() == 0, "inn label cleared after turn to N", fails);

    /* Tavern @ (6,4) DIR_W — OP_0B sign persists through OP_0E stub. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(4);
    gs.setCoordY(6);
    gs.setFacingKey('W');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 24 tavern fires at (4,6) facing W", fails);
    expect(runtime.textView().layerCount() > 0, "OP_0B tavern sign persists after OP_0E", fails);
    /* OP_0E 0x03 shows the faithful str.dat pub intro (Middlegate = Amber
     * waitress "Do you wish to order from our vast menu of drinks (y/n)?"). */
    expect(runtime.textView().containsText("Amber") || runtime.textView().containsText("drinks"),
           "OP_0E 0x03 tavern shows pub intro (str.dat)", fails);
    expect(!runtime.textView().containsText("May I aid you"),
           "OP_0E 0x03 must not show temple priest intro", fails);
    expect(runtime.blocksMovement(), "tavern OP_0E waits for Y/N", fails);

    /* Blacksmith shop @ (4,4) DIR_W — not on N/E/S. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(4);
    gs.setCoordY(4);
    gs.setFacingKey('W');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 22 blacksmith fires at (4,4) facing W", fails);
    expect(runtime.textView().layerCount() > 0, "OP_0B blacksmith sign persists after OP_0E", fails);
    scanDoorNoFire(runtime, gs, world, DoorCase{22, 4, 4, 'N', "Blacksmith"}, fails);

    /* Temple @ (7,7) DIR_N — event 23 OP_0E 0x04 (authoritative event.dat
     * tile; the old doc-28 matrix wrongly labeled this as Training). Confirms the
     * Temple selector fires at its real tile (and ONLY facing N). */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(7);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 23 temple fires at (7,7) facing N", fails);
    expect(runtime.textView().layerCount() > 0, "OP_0B temple sign persists after OP_0E", fails);
    expect(runtime.textView().containsText("May I aid you"),
           "OP_0E 0x04 temple shows priest intro (str.dat)", fails);
    expect(runtime.blocksMovement(), "temple OP_0E waits for Y/N", fails);
    scanDoorNoFire(runtime, gs, world, DoorCase{23, 7, 7, 'W', "Temple"}, fails);

    /* Mage guild spell shop @ (7,14) DIR_N — evt 27: OP_0B sign (str idx 0x14 → 37.anm)
     * + OP_0E 0x05 str.dat hall intro block + Y/N; must not show event str[20] farthing line. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(14);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 27 mage guild fires at (7,14) facing N", fails);
    expect(!runtime.textView().containsText("farthing"),
           "mage guild must not show farthing error string", fails);
    expect(runtime.textView().containsText("archmage"),
           "mage guild shows spell-shop hall intro (str.dat)", fails);
    expect(runtime.textView().containsText("Interested"),
           "mage guild hall intro includes Y/N prompt line", fails);
    expect(runtime.blocksMovement(), "mage guild spell prompt waits for Y/N", fails);
    scanDoorNoFire(runtime, gs, world, DoorCase{27, 7, 14, 'W', "Mage Guild"}, fails);

    /* Event 20 @ (5,15) ENTER — OP_01 str[24] then OP_09 Y/N. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(5);
    gs.setCoordY(15);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);

    const bool fired_gates = runtime.scanAndRun(gs, world);
    expect(fired_gates, "event 20 fires at (5,15) ENTER facing N", fails);
    expect(runtime.blocksMovement(), "event 20 waits for Y/N", fails);
    expect(runtime.textView().exitBit0(), "OP_01 sets exit bit 0", fails);

    mm2::platform::KeyState keys{};
    keys.last_ascii = 'Y';
    const bool still_waiting_y = runtime.continueInput(gs, world, keys);
    expect(!still_waiting_y, "Y/N answered Y — script ended", fails);
    expect(!runtime.blocksMovement(), "movement unblocked after Y", fails);
    expect(gs.screenId() == 11, "OP_0C exits to overland map 11", fails);
    expect(gs.coordX() == 7 && gs.coordY() == 3, "OP_0C lands at tile 0x37 (7,3)", fails);
    expect(runtime.screenChanged(), "OP_0C sets screen-changed flag", fails);

    keys.last_ascii = 'N';
    runtime.enterLocation(0, gs, world);
    world.enterScreen(0);
    gs.setScreenId(0);
    gs.setCoordX(5);
    gs.setCoordY(15);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    runtime.scanAndRun(gs, world);
    const bool still_waiting_n = runtime.continueInput(gs, world, keys);
    expect(!still_waiting_n, "Y/N answered N — script ended", fails);
    expect(gs.screenId() == 0, "N stays in Middlegate", fails);

    /* C2 portcullis loc 11 evt 01: OP_0B str[24] → 29.anm (env 3 Vulcania @ 0x15756). */
    expect(mm2::events::ServiceSignResolver::envIdForScreen(11, nullptr) == 3,
           "C2 screen 11 area_env_lookup -> env 3 Vulcania", fails);
    world.enterScreen(11);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(11, &world.attribFile().records[11], 24) == 29,
           "C2 OP_0B str[24] resolves to 29.anm portcullis", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(11, &world.attribFile().records[11], 24) != 61,
           "C2 OP_0B str[24] must not use wrong env 61.anm warrior", fails);

    /* Town screens 0..4 share area_env_lookup range 0 → env 0 (Middlegate table @ 0x15756).
     * Blacksmith OP_0B str[2] → id 62; str[3] Slaughtered Lamb → 63; evt 26 str[6] → 68.
     * sign_sprite_load @ 0x316E/0x9A30: table byte maps directly to NN.anm (no extra −1). */
    expect(mm2::events::ServiceSignResolver::envIdForScreen(0, nullptr) == 0,
           "Middlegate screen 0 -> env 0", fails);
    expect(mm2::events::ServiceSignResolver::envIdForScreen(1, nullptr) == 0,
           "Atlantium town screen 1 -> env 0 (not env 1)", fails);
    expect(mm2::events::ServiceSignResolver::envIdForScreen(2, nullptr) == 0,
           "Tundara town screen 2 -> env 0", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 2) == 62,
           "Middlegate blacksmith OP_0B str[2] -> id 62", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 3) == 63,
           "Middlegate str[3] Slaughtered Lamb OP_0B -> id 63", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 6) == 68,
           "Middlegate evt 26 OP_0B str[6] -> id 68", fails);
    expectTableIdLoadsAnm(data_dir, 62, fails);
    expectTableIdLoadsAnm(data_dir, 63, fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 2) !=
               mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 6),
           "Middlegate blacksmith vs inn sign ids differ", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(1, &world.attribFile().records[1], 2) == 62,
           "Atlantium blacksmith OP_0B str[2] -> id 62", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(1, &world.attribFile().records[1], 2) != 33,
           "Atlantium blacksmith must not use Atlantium-table warrior id 33", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(2, &world.attribFile().records[2], 2) == 62,
           "Tundara blacksmith OP_0B str[2] -> id 62 via shared town table", fails);
    expect(mm2::events::ServiceSignResolver::envIdForScreen(55, nullptr) == 2,
           "Hillstone screen 55 -> env 2 Tundara", fails);

    /* C2 portcullis loc 11 evt 01 @ (3,7)/S: OP_0B loads sign overlay + OP_02 y/n text. */
    runtime.enterLocation(11, gs, world);
    world.enterScreen(11);
    gs.setScreenId(11);
    mm2::events::ServiceSignResolver::syncSignEnvId(gs.a4(), 11, &world.attrib());
    gs.setCoordX(7);
    gs.setCoordY(3);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "C2 portcullis evt 01 fires at (3,7) facing S", fails);
    expect(runtime.textView().hasServicePortrait(), "C2 portcullis OP_0B loads 29.anm overlay", fails);
    expect(runtime.textView().containsText("portcullis"), "C2 portcullis OP_02 text", fails);
    expect(runtime.blocksMovement(), "C2 portcullis waits for Y/N", fails);

    /* Hillstone Lord Slayer evt 15: OP_0B str[14] → 49.anm (env 2 Tundara table @ 0x15756). */
    expect(mm2::events::ServiceSignResolver::resolveForScreen(55, &world.attribFile().records[55], 14) == 49,
           "Hillstone OP_0B str[14] resolves to 49.anm", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(55, &world.attribFile().records[55], 14) != 53,
           "Hillstone OP_0B str[14] must not resolve to 53.anm (Middlegate str[13] mummy)", fails);
    world.enterScreen(55);
    gs.setScreenId(55);
    runtime.enterLocation(55, gs, world);
    gs.setCoordX(5);
    gs.setCoordY(2);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "Hillstone evt 15 fires at tile (2,5) facing S", fails);
    expect(runtime.textView().hasServicePortrait(), "Lord Slayer OP_0B loads portrait overlay", fails);
    expect(runtime.textView().containsText("Lord Slayer"),
           "Lord Slayer Crusader rejection text", fails);

    /* Atlantium evt 17 blacksmith @ (6,13)/S: OP_0B str[2] + OP_0E 0x06 — sign id 62, not warrior 33. */
    world.enterScreen(1);
    gs.setScreenId(1);
    runtime.enterLocation(1, gs, world);
    gs.setCoordX(6);
    gs.setCoordY(13);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "Atlantium evt 17 blacksmith fires at (6,13) facing S", fails);
    expect(runtime.textView().hasServicePortrait(), "Atlantium blacksmith OP_0B loads sign overlay", fails);
    expect(mm2::events::ServiceSignResolver::resolveForGameState(gs.a4(), 1, &world.attrib(), 2) == 62,
           "Atlantium blacksmith synced env resolves str[2] -> 62", fails);

    /* Middlegate evt 22 blacksmith vs evt 26 training hall — different str_idx, different sign ids. */
    world.enterScreen(0);
    gs.setScreenId(0);
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(4);
    gs.setCoordY(4);
    gs.setFacingKey('W');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "Middlegate evt 22 blacksmith at (4,4) W", fails);
    expect(runtime.textView().hasServicePortrait(), "Middlegate blacksmith OP_0B overlay", fails);
    {
        const int frame0 = runtime.textView().serviceSignFrame();
        bool saw_change = false;
        for (int t = 0; t < 50; ++t) {
            runtime.textView().tickAnimation();
            if (runtime.textView().serviceSignFrame() != frame0) {
                saw_change = true;
                break;
            }
        }
        expect(saw_change, "Middlegate blacksmith 62.anm sign animates across ticks", fails);
    }
    const int mg_blacksmith_id =
        mm2::events::ServiceSignResolver::resolveForGameState(gs.a4(), 0, &world.attrib(), 2);
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(10);
    gs.setCoordY(7);
    gs.setFacingKey('E');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "Middlegate evt 26 training at (7,10) E", fails);
    expect(runtime.textView().hasServicePortrait(), "Middlegate training OP_0B overlay", fails);
    const int mg_training_id =
        mm2::events::ServiceSignResolver::resolveForGameState(gs.a4(), 0, &world.attrib(), 6);
    expect(mg_blacksmith_id == 62, "Middlegate blacksmith str[2] -> 62", fails);
    expect(mg_training_id == 68, "Middlegate evt 26 str[6] -> 68", fails);
    expectTableIdLoadsAnm(data_dir, mg_blacksmith_id, fails);
    expectTableIdLoadsAnm(data_dir, mg_training_id, fails);
    expect(mg_blacksmith_id != mg_training_id, "training tile must not reuse blacksmith sign id", fails);

    /* C2 loc 11 evt 22 @ (14,8)/N: era 9 (Year 900 / 10th century) shows ruins, must not OP_0C to 59. */
    world.enterScreen(11);
    gs.setScreenId(11);
    runtime.enterLocation(11, gs, world);
    mm2_gs_set_u16(gs.a4(), MM2_GS_ERA, 9);
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERA_LOW, 9);
    gs.setCoordX(14);
    gs.setCoordY(8);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    const uint8_t c2_screen_before = gs.screenId();
    expect(runtime.scanAndRun(gs, world), "C2 evt 22 Castle Xabran gate at (14,8) N", fails);
    expect(gs.screenId() == c2_screen_before, "era 9 must not teleport to Castle Xabran (59)", fails);
    expect(runtime.textView().containsText("Ruins"), "era 9 shows ruins text not enter prompt", fails);

    /* C2 loc 11 evt 04 @ (4,7)/N: Pegasus greeting via event VM (not scripted-scene hack).
     * The "seen Pegasus" 0x40 bit is stored PER CHARACTER at record offset 0x79
     * (class_quest_guild_mask), written by OP_18 selector 0x74 — so the test needs
     * a real party member (the rest of this test runs with an empty launch). The
     * runtime is not bound to a roster, so the write lands in the GS-image roster
     * block at MM2_GS_ROSTER_BASE + idx*stride + 0x79. */
    int pegasus_member[1] = {0};
    Mm2PartyLaunch pegasus_launch{};
    mm2_party_launch_build(&pegasus_launch, 1, pegasus_member, 1);
    mm2_party_launch_apply(gs.a4(), &pegasus_launch);
    const int pegasus_idx = static_cast<int>(mm2_gs_u16(gs.a4(), MM2_GS_ROSTER_INDEX_TBL));
    const int pegasus_off = MM2_GS_ROSTER_BASE + pegasus_idx * MM2_GS_ROSTER_STRIDE + 0x79;
    mm2_gs_set_u8(gs.a4(), pegasus_off, 0);
    runtime.enterLocation(11, gs, world);
    mm2::events::ServiceSignResolver::syncSignEnvId(gs.a4(), 11, &world.attrib());
    gs.setCoordX(7);
    gs.setCoordY(4);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "C2 evt 04 Pegasus at (4,7) facing N", fails);
    expect(runtime.textView().containsText("Guardian Pegasus"),
           "Pegasus OP_03 shows greeting text", fails);
    expect(runtime.textView().hasPegasusIllustration(), "Pegasus OP_03 loads 21.anm overlay", fails);
    expect(runtime.blocksMovement(), "Pegasus evt waits for SPACE", fails);
    expect((mm2_gs_u8(gs.a4(), pegasus_off) & 0x40) != 0,
           "Pegasus OP_18 sets per-character record[0x79] bit 0x40", fails);

    if (fails == 0) {
        std::printf("OK: event_middlegate_test\n");
        return 0;
    }
    return 1;
}

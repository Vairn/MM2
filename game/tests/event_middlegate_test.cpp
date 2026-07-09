// Offline decode + VM smoke test for Middlegate (loc 0) events:
//   All OP_04 door labels — scanner fires at correct (x,y,facing)
//   Event 20 — OP_01 + OP_09 city gates Y/N at (5,15)/ENTER + OP_0C exit to map 11
//   Shop tiles — OP_0B service sign survives OP_0E stub
//
// Usage: event_middlegate_test <data_dir>

#include <cstdio>
#include <cstring>

#include "mm2/GameState.h"
#include "mm2/combat/CombatSession.h"
#include "mm2/events/EventCombatEncounter.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/events/EventTownServices.h"
#include "mm2/gameplay/RosterSkills.h"
#include "mm2/gameplay/SpellBook.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/ServiceSignResolver.h"
#include "mm2/events/TownServiceMenu.h"
#include "mm2/events/TownServiceTransactions.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/GfxBackend.h"
#include "mm2/gfx/ViewportAnmOverlay.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/PlayTownServiceUi.h"
#include "mm2/world/MapWorld.h"

#include "mm2_items_codec.h"
#include "mm2_monsters_codec.h"
#include "mm2_party_launch.h"
#include "mm2_gamestate.h"
#include "mm2_map_codec.h"

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
    std::snprintf(tag, sizeof(tag), "OP_0B table id %d loads MONSTERS picture", table_id);
    if (!expect(overlay.loadFromId(data_dir, table_id), tag, fails)) {
        return false;
    }
    return expect(overlay.loaded() && overlay.width() > 0 && overlay.height() > 0, tag, fails);
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
        int spell_slot = 0; /* temple BuySpell: slot 0..MM2_TEMPLE_SPELL_SLOTS-1 */
    };
    void push(const Step &s) { steps_[count_++] = s; }

    bool chooseTempleOption(const mm2::events::TownServiceContext &, mm2::events::TempleOption &out,
                            int &out_spell_slot) override
    {
        if (cursor_ >= count_ || steps_[cursor_].exit) {
            return false;
        }
        out = steps_[cursor_].temple;
        out_spell_slot = steps_[cursor_].spell_slot;
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
                            mm2::events::TempleOption &, int &) override { return false; }
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

/* Scripted mage-guild UI: records the not-member report and replays one
 * spell-slot + member choice. */
class ScriptedGuildUi : public mm2::events::ITownServiceUi {
public:
    int slot = 0;
    int member = 0;
    bool exit_immediately = false;

    bool chooseTempleOption(const mm2::events::TownServiceContext &,
                            mm2::events::TempleOption &, int &) override { return false; }
    bool chooseTrainingOption(const mm2::events::TownServiceContext &,
                              mm2::events::TrainingOption &, int &) override { return false; }
    bool chooseMageGuildSpell(const mm2::events::TownServiceContext &,
                              const Mm2SpellShopSlot[MM2_MAGE_GUILD_SLOTS], int &out) override
    {
        if (consumed_ || exit_immediately) {
            return false;
        }
        consumed_ = true;
        out = slot;
        return true;
    }
    bool selectMember(const mm2::events::TownServiceContext &, int &out_slot) override
    {
        out_slot = member;
        return member >= 0;
    }
    void reportGuildNotMember(const mm2::events::TownServiceContext &) override { ++denied; }
    void reportSpellLearned(const mm2::events::TownSvcSpellResult &r) override
    {
        last = r;
        ++learned;
    }
    void reportSpellRejected(const mm2::events::TownSvcSpellResult &r) override
    {
        last = r;
        ++rejected;
    }
    void reportNotEnoughGold() override { ++poor; }

    mm2::events::TownSvcSpellResult last{};
    int denied = 0, learned = 0, rejected = 0, poor = 0;

private:
    bool consumed_ = false;
};

void testMageGuildAndTemple(int &fails)
{
    using namespace mm2::events;
    using mm2::gameplay::kClericSpells;
    using mm2::gameplay::kSorcererSpells;
    using mm2::gameplay::spellKnownInBook;

    /* ---- Static stock + membership-mask data sanity (doc 28, doc 36). ---- */
    {
        Mm2SpellShopSlot slots[MM2_MAGE_GUILD_SLOTS];
        expect(mm2_mage_guild_stock(0, slots) != 0, "Middlegate mage guild stock decodes", fails);
        expect(slots[0].spell_index == 0 && slots[0].gold == 10,
               "Middlegate guild slot A = Awaken 10gp", fails);
        expect(std::strcmp(kSorcererSpells[slots[0].spell_index].name, "Awaken") == 0,
               "guild slot A spell index resolves to Awaken", fails);

        Mm2SpellShopSlot temple[MM2_TEMPLE_SPELL_SLOTS];
        expect(mm2_temple_spell_stock(0, temple) != 0, "Middlegate temple spell stock decodes", fails);
        expect(std::strcmp(kClericSpells[temple[0].spell_index].name, "Apparition") == 0,
               "Middlegate temple slot D = Apparition", fails);

        expect(mm2_mage_guild_member_mask(0) == 0x02 && mm2_mage_guild_member_mask(4) == 0x20,
               "per-town membership mask matches data hunk A4-$66A9", fails);
    }

    /* ---- Membership gate (0x1E410): denied by default, granted once record+0x79
     * has the town bit (mirrors the currently-unported 0x9D76 reward write). ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        Mm2RosterRecord &rec = roster.records[0];
        expect(!townSvcMageGuildMember(rec, 0), "fresh roster is not a guild member (ASM 0x1E410)",
               fails);
        rec.class_quest_guild_mask = static_cast<uint8_t>(mm2_mage_guild_member_mask(0));
        expect(townSvcMageGuildMember(rec, 0), "member bit grants Middlegate membership", fails);
        expect(!townSvcMageGuildMember(rec, 1), "Middlegate bit does not grant Atlantium", fails);
    }

    /* ---- Spell purchase leaf (0x1D872): gold deduct + grant, reject paths. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0;
        const TownSvcSpellResult r = townSvcBuySpell(rec, 0 /*Awaken*/, 10);
        expect(r.learned && rec.gold == 990u, "buy spell deducts gold and learns it", fails);
        expect(spellKnownInBook(rec, 0), "spellbook bit set after purchase", fails);

        const TownSvcSpellResult again = townSvcBuySpell(rec, 0, 10);
        expect(again.learned && rec.gold == 980u,
               "re-buying an already-known spell is idempotent, not rejected (no ASM check found)",
               fails);

        const TownSvcSpellResult broke = townSvcBuySpell(rec, 2, 1000000);
        expect(!broke.learned && broke.reject == TownSvcSpellReject::NotEnoughGold,
               "unaffordable spell purchase rejected without charging", fails);
        expect(rec.gold == 980u, "failed purchase leaves gold untouched", fails);

        const TownSvcSpellResult empty = townSvcBuySpell(rec, 3, 0);
        expect(!empty.learned && empty.reject == TownSvcSpellReject::NotForSale,
               "zero-cost (unpopulated) slot is rejected as not for sale", fails);

        rec.condition = 0x10;
        const TownSvcSpellResult afflicted = townSvcBuySpell(rec, 6, 50);
        expect(!afflicted.learned && afflicted.reject == TownSvcSpellReject::Condition,
               "afflicted character cannot buy spells", fails);
    }

    /* ---- Menu driver: whole-party gate denies entry when no one qualifies. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.map_id = 0;

        ScriptedGuildUi ui;
        townSvcRunMageGuild(ui, ctx);
        expect(ui.denied == 1 && ui.learned == 0,
               "guild driver denies entry when no party member has the bit", fails);
    }

    /* ---- Menu driver: qualifying party can buy through the driver. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        roster.records[0].condition = 0;
        roster.records[0].class_quest_guild_mask = static_cast<uint8_t>(mm2_mage_guild_member_mask(0));
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.map_id = 0;

        ScriptedGuildUi ui;
        ui.slot = 0; /* Awaken, 10gp */
        ui.member = 0;
        townSvcRunMageGuild(ui, ctx);
        expect(ui.denied == 0 && ui.learned == 1, "qualifying party buys via the driver", fails);
        expect(roster.records[0].gold == 9990u, "driver purchase deducted gold", fails);
        expect(spellKnownInBook(roster.records[0], 0), "driver purchase learned the spell", fails);
    }

    /* ---- Temple menu driver: cleric spell purchase (menu D/E/F). ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 10000);
        roster.records[0].condition = 0;
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
        ui.push({true, TempleOption::BuySpell, TrainingOption::Exit, 0, 0, false});
        ui.push({true, TempleOption::Exit, TrainingOption::Exit, 0, -1, true});
        townSvcRunTemple(ui, ctx);
        expect(roster.records[0].gold == 9990u, "temple spell purchase deducted 10gp (Apparition)",
               fails);
        expect(spellKnownInBook(roster.records[0], 0), "temple purchase learned cleric spell 0", fails);
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
        expect(mm2_smith_sell_price(100u) == 50u, "sell price is half buy-style price", fails);
        expect(mm2_smith_identify_cost(0) == 10u, "identify plain item costs 10 gp", fails);
        expect(mm2_smith_identify_cost(3) == 150u, "identify +3 uses midpoint of 1..300", fails);
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

    /* ---- Sell: credit half buy-style price and clear the backpack slot. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0;
        const uint8_t id = 4;
        Mm2RosterItemSlot slot{};
        slot.item_id = id;
        slot.charges = 0;
        slot.flags = 0;
        mm2_roster_set_backpack(&rec, 0, slot);
        const uint32_t sell =
            mm2_smith_sell_price(mm2_smith_price(itemGold(items, id), 0));
        const TownSvcSellResult r = townSvcSmithSell(rec, 0, sell);
        expect(r.sold && r.price == sell, "smith sell credits half buy price", fails);
        expect(rec.gold == 1000u + sell, "smith sell adds gold to character", fails);
        expect(mm2_roster_backpack(&rec, 0).item_id == 0, "smith sell clears backpack slot", fails);
    }

    /* ---- Sell guards: afflicted / empty slot. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0x10;
        Mm2RosterItemSlot slot{};
        slot.item_id = 4;
        mm2_roster_set_backpack(&rec, 0, slot);
        TownSvcSellResult r = townSvcSmithSell(rec, 0, 5);
        expect(!r.sold && r.reject == TownSvcSellReject::Condition,
               "afflicted char cannot sell", fails);
        rec.condition = 0;
        r = townSvcSmithSell(rec, 1, 5);
        expect(!r.sold && r.reject == TownSvcSellReject::NoItem,
               "empty backpack slot rejects sell", fails);
    }

    /* ---- Identify: deduct fee and leave item bytes unchanged. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1000);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0;
        Mm2RosterItemSlot slot{};
        slot.item_id = 4;
        slot.charges = 0;
        slot.flags = 0;
        mm2_roster_set_backpack(&rec, 0, slot);
        char summary[96];
        const uint32_t cost = mm2_smith_identify_cost(0);
        const TownSvcIdentifyResult r =
            townSvcSmithIdentify(rec, 0, &items, cost, summary, sizeof(summary));
        expect(r.identified && r.cost == cost, "identify deducts 10 gp for plain item", fails);
        expect(rec.gold == 1000u - cost, "identify gold deducted", fails);
        expect(mm2_roster_backpack(&rec, 0).item_id == 4, "identify leaves item in pack", fails);
        expect(summary[0] != '\0', "identify fills summary text", fails);
    }

    /* ---- Identify guards: afflicted / no gold. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1);
        Mm2RosterRecord &rec = roster.records[0];
        rec.condition = 0;
        Mm2RosterItemSlot slot{};
        slot.item_id = 4;
        slot.flags = 3;
        mm2_roster_set_backpack(&rec, 0, slot);
        const uint32_t cost = mm2_smith_identify_cost(3);
        expect(cost > 1u, "identify +3 costs more than 1 gp", fails);
        TownSvcIdentifyResult r =
            townSvcSmithIdentify(rec, 0, &items, cost, nullptr, 0);
        expect(!r.identified && r.reject == TownSvcIdentifyReject::NotEnoughGold,
               "identify rejected when gold < cost", fails);
        rec.condition = 0x10;
        rec.gold = 10000;
        r = townSvcSmithIdentify(rec, 0, &items, cost, nullptr, 0);
        expect(!r.identified && r.reject == TownSvcIdentifyReject::Condition,
               "afflicted char cannot identify", fails);
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

        ui.handleKey('A', false); /* Heal active member (slot 1) */
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
        ui.handleKey('T', false); /* Press 'T' to train (ASM 0x020bc8) */
        expect(roster.records[0].level == 2, "play training leveled L1->L2", fails);
        expect(roster.records[0].gold == 9950u, "play training deducted the 50 fee", fails);
        expect(ui.active(), "play training stays open after a level-up", fails);

        ui.handleKey(0, true);    /* Esc from trainee prompt -> close */
        expect(!ui.active(), "play training closes on Esc", fails);
    }

    /* ---- Blacksmith: A) Weapons -> item 1 (Dagger) buys for active member. ---- */
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
        ui.handleKey('A', false); /* slot A = Dagger -> buy for active member */

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

    /* ---- Blacksmith: E) Sell backpack slot A. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100000);
        roster.records[0].condition = 0;
        Mm2RosterItemSlot slot{};
        slot.item_id = 4;
        slot.charges = 0;
        slot.flags = 0;
        mm2_roster_set_backpack(&roster.records[0], 0, slot);
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

        const uint32_t sell =
            mm2_smith_sell_price(mm2_smith_price(itemGold(items, 4), 0));

        mm2::ui::PlayTownServiceUi ui;
        townSvcRunSmith(ui, ctx);
        ui.begin();
        ui.handleKey('E', false); /* Sell list */
        ui.handleKey('A', false); /* sell Dagger in slot A */
        expect(mm2_roster_backpack(&roster.records[0], 0).item_id == 0,
               "play smith sell clears backpack slot", fails);
        expect(roster.records[0].gold == 100000u + sell,
               "play smith sell credits half buy price", fails);
    }

    /* ---- Blacksmith: F) Identify — pick a backpack slot (A–F). ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100000);
        roster.records[0].condition = 0;
        Mm2RosterItemSlot slot{};
        slot.item_id = 4;
        slot.charges = 0;
        slot.flags = 0;
        mm2_roster_set_backpack(&roster.records[0], 2, slot); /* backpack slot C */
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
        ui.begin();
        ui.handleKey('F', false); /* backpack list for identify */
        ui.handleKey('C', false); /* choose slot C */
        expect(roster.records[0].gold == 100000u - 10u,
               "play smith identify deducts 10 gp", fails);
        expect(mm2_roster_backpack(&roster.records[0], 2).item_id == 4,
               "play smith identify keeps chosen backpack item", fails);
        ui.handleKey('X', false); /* dismiss identify text (0x1BBD6), stay on backpack list */
    }

    /* ---- Tavern: A) Feeding frenzy tops party food; no food sub-menu. ---- */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1000);
        setupMember(roster, 1, 5, 1000);
        roster.records[0].food = 10;
        roster.records[1].food = 5;
        int member_idx[2] = {0, 1};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 2, member_idx, 2);
        uint8_t a4img[static_cast<size_t>(MM2_A4_ANCHOR) + 0x100u]{};

        TownServiceContext ctx;
        ctx.roster = &roster;
        ctx.launch = &launch;
        ctx.items = &items;
        ctx.a4 = mm2_gs_base_from_image(a4img);
        ctx.map_id = 0;

        mm2::ui::PlayTownServiceUi ui;
        townSvcRunTavern(ui, ctx);
        ui.begin();
        ui.handleKey('A', false);
        expect(roster.records[0].gold == 980u, "play tavern A feeding frenzy charges payer", fails);
        expect(roster.records[0].food == 40 && roster.records[1].food == 40,
               "play tavern A tops all members to 40 food", fails);
        expect(ui.active(), "play tavern stays on top menu after feeding frenzy", fails);
    }
}

void testFeedingFrenzy(int &fails)
{
    using namespace mm2::events;

    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1000);
        setupMember(roster, 1, 5, 1000);
        roster.records[0].food = 10;
        roster.records[1].food = 5;
        int member_idx[2] = {0, 1};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 2, member_idx, 2);

        const TownSvcFeedingResult r =
            townSvcFeedingFrenzy(roster.records[0], &launch, &roster, 0);
        expect(r.fed && r.paid && r.cost == 20u, "feeding frenzy deducts town cost", fails);
        expect(roster.records[0].gold == 980u, "feeding frenzy charges active member", fails);
        expect(roster.records[0].food == 40 && roster.records[1].food == 40,
               "feeding frenzy tops each member to 40 food", fails);
        expect(r.members_topped == 2, "feeding frenzy counted both members", fails);
    }

    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 1);
        roster.records[0].food = 10;
        int member_idx[1] = {0};
        Mm2PartyLaunch launch{};
        mm2_party_launch_build(&launch, 1, member_idx, 1);
        const TownSvcFeedingResult r =
            townSvcFeedingFrenzy(roster.records[0], &launch, &roster, 0);
        expect(!r.fed && !r.paid, "feeding frenzy rejected when gold < cost", fails);
        expect(roster.records[0].food == 10, "rejected feeding frenzy leaves food", fails);
    }
}

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : "../..";
    int fails = 0;

    mm2::gfx::initPcGfxFallbackDir(data_dir, nullptr);
    mm2::gfx::resolveGfxBackend(data_dir);

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

    Mm2MonstersFile monsters{};
    char monsters_path[512];
    std::snprintf(monsters_path, sizeof(monsters_path), "%s/monsters.dat", data_dir);
    const bool has_monsters = mm2_monsters_load_file(monsters_path, &monsters) == MM2_MONSTERS_OK;
    expect(has_monsters, "load monsters.dat", fails);

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

    /* OP_0E default-path binning @ 0x15EDC (Vulcania evt 32: selector 0x43 bins
     * to category 0x3E). Corrected 2026-07: the default-range thunk -$7DFA
     * resolves to event_dat_loader (0x92F2), NOT the Arena Games engine
     * (0x9D76, reached only via explicit selector 0x08's thunk -$7DBE) — see
     * EventVmHelpers.h. This test only locks in the byte-exact category/index
     * binning math, independent of what the (not yet ASM-traced) reinvocation
     * actually does with it. */
    {
        const auto bin = mm2::events::eventVmBinExecSelector(0x43);
        expect(bin.matched, "0x43 matches a default-range bin", fails);
        expect(bin.category == 0x3E, "0x43 → category 0x3E", fails);
        expect(bin.index == 0x0C, "0x43 → adjusted index 12 (0x43−0x37)", fails);
    }

    /* Token-skip length table (ROM opcode_len_tbl @ A4-$6CC8, data hunk offset
     * 0x1336; ground truth via tools/dump_event_token_table.py). Locks in the
     * full 0x00..0x32 table byte-exact, including the 2 opcodes where the
     * ROM's skip length differs from "1 + argc": op 0x00 (invalid, skip=0)
     * and op 0x25 (check-code16, skip=2 though the handler reads 2 args when
     * actually executed) — a genuine ROM quirk that desyncs OP_10/11/2B skip
     * walks that pass over an OP_25 token. */
    {
        using mm2::events::eventVmTokenDelta;
        static const uint8_t kExpectedDelta[0x33] = {
            0,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  3,  3,  2,  2,  1,  2,  2,  13, 11, 1,
            4,  3,  3,  5,  5,  3,  2,  2,  2,  2,  7,  7,  4,  3,  3,  3,  2,  1,  1,  3,  1,
            15, 2,  2,  3,  3,  1,  11, 4,  2,
        };
        bool all_match = true;
        for (int op = 0; op <= 0x32; ++op) {
            if (eventVmTokenDelta(static_cast<uint8_t>(op)) != kExpectedDelta[op]) {
                all_match = false;
            }
        }
        expect(all_match, "eventVmTokenDelta[0x00..0x32] matches ROM opcode_len_tbl byte-exact", fails);
        expect(eventVmTokenDelta(0x00) == 0, "OP_00 (invalid) skip delta = 0, not 1+argc", fails);
        expect(eventVmTokenDelta(0x25) == 2,
               "OP_25 (check-code16) skip delta = 2 though handler reads 2 args (ROM quirk)", fails);
        expect(eventVmTokenDelta(0x12) == 13, "OP_12 skip delta = 13 (12 args + opcode)", fails);
        expect(eventVmTokenDelta(0x2B) == 2, "OP_2B skip delta = 2 (count byte + opcode)", fails);
    }

    /* Arena Games ticket engine (asm 0x9D76): area index / gold reward tables
     * extracted byte-exact from the data hunk (0xE74 / 0xE7A) and the monster-
     * type formula (asm 0x9E86-0x9EC2). */
    {
        using mm2::events::eventVmArenaGoldReward;
        using mm2::events::eventVmArenaMonsterType;
        using mm2::events::kArenaAreaIndex;
        expect(kArenaAreaIndex[0] == 0 && kArenaAreaIndex[1] == 2 && kArenaAreaIndex[2] == 0 &&
                   kArenaAreaIndex[3] == 0 && kArenaAreaIndex[4] == 1,
               "arena area index table = [0,2,0,0,1] (data 0xE74)", fails);
        expect(eventVmArenaGoldReward(0, 0) == 200u, "green ticket @ Middlegate = 200 gp", fails);
        expect(eventVmArenaGoldReward(0, 1) == 1500u, "green ticket @ Atlantium = 1500 gp", fails);
        expect(eventVmArenaGoldReward(3, 1) == 50000u, "black ticket @ Atlantium = 50000 gp", fails);
        expect(eventVmArenaGoldReward(3, 4) == 30000u,
               "black ticket @ Sandsobar (tier 2) = 30000 gp", fails);
        /* monster_type = ((color*3 + area[screen]) * 16) + rng(1,16). Vulcania
         * (screen 3, area 0), green ticket (color 0): (0*3+0)*16 + roll. */
        expect(eventVmArenaMonsterType(0, 3, 1) == 1, "green ticket @ Vulcania, roll=1 -> type 1", fails);
        expect(eventVmArenaMonsterType(0, 3, 16) == 16, "green ticket @ Vulcania, roll=16 -> type 16",
               fails);
        /* Atlantium (screen 1, area 2), black ticket (color 3): (3*3+2)*16=176, +roll. */
        expect(eventVmArenaMonsterType(3, 1, 1) == 177, "black ticket @ Atlantium, roll=1 -> type 177",
               fails);
    }

    /* Arena ticket detection/consumption (asm 0x9D9C-0x9E4C): backpack-only scan,
     * member-major then slot-minor, first match wins; consuming clears the slot. */
    {
        Mm2RosterFile arenaRoster{};
        setupMember(arenaRoster, 0, 5, 100);
        setupMember(arenaRoster, 1, 5, 100);
        Mm2PartyLaunch arenaLaunch{};
        arenaLaunch.party_count = 2;
        arenaLaunch.roster_slots[0] = 0;
        arenaLaunch.roster_slots[1] = 1;
        arenaRoster.records[0].backpack_id[2] = 0xD2; /* red ticket, member 0 slot 2 */
        const auto ticket =
            mm2::events::eventVmFindArenaTicket(nullptr, &arenaRoster, &arenaLaunch);
        expect(ticket.found, "arena ticket found in backpack", fails);
        expect(ticket.color == 2, "arena ticket color = 2 (red, 0xD2)", fails);
        expect(ticket.member_slot == 0, "arena ticket owner = party slot 0", fails);
        expect(ticket.backpack_slot == 2, "arena ticket backpack slot = 2", fails);
        mm2::events::eventVmConsumeArenaTicket(&arenaRoster, &arenaLaunch, ticket);
        expect(arenaRoster.records[0].backpack_id[2] == 0, "consumed ticket clears backpack slot",
               fails);
        const auto none =
            mm2::events::eventVmFindArenaTicket(nullptr, &arenaRoster, &arenaLaunch);
        expect(!none.found, "no ticket found after consumption", fails);
    }

    /* eventRunFixedEncounter -> CombatSession wiring (plan Phase 1): OP_12's A4
     * field writes, the synchronous jsr -$7EDE combat entry, and OP_2B's gate
     * (COMBAT_VICTORY_LATCH, asm 0x16D74) getting set by a real fight outcome. */
    {
        mm2::world::MapWorld fixedWorld; /* mode=0x80 (fixed) never touches attrib. */

        Mm2MonstersFile monsters{};
        Mm2MonsterRecord &mon = monsters.records[9];
        mon.fields[MM2_MON_OFF_HP - MM2_MONSTER_NAME_SIZE] = 0x00; /* 1 HP: dies to any hit */
        mon.fields[MM2_MON_OFF_SPEED - MM2_MONSTER_NAME_SIZE] = 0x00;

        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100);
        roster.records[0].might_current = 99;
        roster.records[0].speed_current = 99; /* acts before the monster */
        roster.records[0].hp_current = 999;
        Mm2PartyLaunch launch{};
        launch.party_count = 1;
        launch.roster_slots[0] = 0;

        mm2::gameplay::Rng rng(1);
        mm2::combat::CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);

        uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
        mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));

        const uint8_t block[12] = {9, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*overflow=*/0, /*count=*/1};
        mm2::events::EventTextView text;
        mm2::events::EventVmWait wait = mm2::events::EventVmWait::None;
        mm2::events::eventRunFixedEncounter(gs, text, wait, block, 12, /*variant_b=*/false, &combat,
                                             &fixedWorld);

        expect(mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE) == 0x80, "OP_12 seeds mode = 0x80 (fixed)", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS) == 9, "OP_12 seeds monster_slots[0]", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_SCRIPT_ABORT) == 1,
               "OP_12 sets SCRIPT_ABORT so the VM yields to combat", fails);
        expect(combat.active(), "bound CombatSession starts the fight synchronously (jsr -$7EDE)", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 0,
               "OP_2B's gate is clear before the fight resolves", fails);

        mm2::platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, fixedWorld, keys); /* party options -> round loop -> member turn */
        keys.last_ascii = 'A';
        combat.tick(gs, fixedWorld, keys); /* attack resolves; kill message awaits a key */
        keys.last_ascii = ' ';
        const bool ended = combat.tick(gs, fixedWorld, keys); /* ack -> victory */
        expect(ended, "acknowledging the kill message ends the fight", fails);
        expect(!combat.active(), "combat inactive after victory", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 1,
               "OP_2B's gate (COMBAT_VICTORY_LATCH) set after victory", fails);
    }

    /* Arena ticket -> combat queued (asm 0x9D76/0x9F04): eventExecTownSelector's
     * 0x08 case consumes the ticket, arms the color/screen gold reward, and
     * runs the SAME eventRunFixedEncounter path as OP_12. */
    {
        mm2::world::MapWorld arenaWorld;

        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100);
        roster.records[0].might_current = 99;
        roster.records[0].speed_current = 99;
        roster.records[0].hp_current = 999;
        roster.records[0].backpack_id[0] = 0xD0; /* green ticket, member 0 slot 0 */
        Mm2PartyLaunch launch{};
        launch.party_count = 1;
        launch.roster_slots[0] = 0;

        /* Middlegate (location 0) green ticket, roll=1: monster_type = 1. */
        Mm2MonstersFile monsters{};
        Mm2MonsterRecord &mon = monsters.records[1];
        mon.fields[MM2_MON_OFF_HP - MM2_MONSTER_NAME_SIZE] = 0x00;
        mon.fields[MM2_MON_OFF_SPEED - MM2_MONSTER_NAME_SIZE] = 0x00;

        mm2::gameplay::Rng rng(1);
        mm2::combat::CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);

        mm2::events::EventRuntime rt;
        rt.bindCombat(&combat);

        uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
        mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
        mm2::events::EventTextView text;
        mm2::events::EventVmWait wait = mm2::events::EventVmWait::None;
        char title[] = "Arena";

        mm2::events::eventExecTownSelector(rt, gs, arenaWorld, 0x08, &roster, &launch, nullptr, text,
                                            wait, /*location_id=*/0, title, &rng);

        expect(roster.records[0].backpack_id[0] == 0, "arena ticket consumed on entry", fails);
        expect(combat.active(), "arena selector queues CombatSession via eventRunFixedEncounter", fails);

        mm2::platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, arenaWorld, keys); /* party options -> member turn */
        keys.last_ascii = 'A';
        combat.tick(gs, arenaWorld, keys); /* attack kills; message awaits a key */
        keys.last_ascii = ' ';
        combat.tick(gs, arenaWorld, keys); /* ack -> victory */
        expect(!combat.active(), "arena combat resolves after Attack", fails);
        expect(combat.lastOutcome() == mm2::combat::CombatOutcome::Victory,
               "arena combat outcome == Victory", fails);
        const uint32_t expected_gold = mm2::events::eventVmArenaGoldReward(0, 0);
        expect(roster.records[0].gold == 100 + expected_gold,
               "arena victory grants the ticket-color/screen gold reward", fails);
    }

    /* ASM-faithful town-service transaction layer (temple heal/restore/donate +
     * stat training) and the pluggable menu driver. */
    testTownServiceTransactions(fails);

    /* Mage guild (OP_0E 0x05) + temple spell purchase (menu D-F): ASM-confirmed
     * membership gate (0x1E410) and shared spell-buy leaf (0x1D872). */
    testMageGuildAndTemple(fails);

    /* Blacksmith (OP_0E 0x06) buy transactions + menu driver (needs items.dat). */
    testSmithTransactions(fails, items);
    testFeedingFrenzy(fails);

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

    /* Middlegate Cavern (screen 17) @ (1,2): event 7 is OP_2B + OP_12 fight.
     * Confirms the live event scanner + bound CombatSession actually start combat
     * when walking onto a known fight square (not just the direct helper test). */
    if (has_monsters) {
        Mm2RosterFile cavern_roster{};
        setupMember(cavern_roster, 0, 5, 100);
        cavern_roster.records[0].might_current = 50;
        cavern_roster.records[0].speed_current = 50;
        cavern_roster.records[0].hp_current = 200;
        Mm2PartyLaunch cavern_launch{};
        cavern_launch.party_count = 1;
        cavern_launch.roster_slots[0] = 0;
        mm2::gameplay::Rng cavern_rng(42);
        mm2::combat::CombatSession cavern_combat;
        cavern_combat.bindParty(&cavern_roster, &cavern_launch);
        cavern_combat.bindMonsters(&monsters);
        cavern_combat.bindRng(&cavern_rng);
        runtime.bindParty(&cavern_roster, &cavern_launch);
        runtime.bindCombat(&cavern_combat);

        world.enterScreen(17);
        gs.setScreenId(17);
        runtime.enterLocation(17, gs, world);
        gs.setCoordX(2);
        gs.setCoordY(1);
        gs.setFacingKey('N');
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
        expect(runtime.scanAndRun(gs, world), "cavern event scan at (1,2)", fails);
        expect(cavern_combat.active(), "cavern (1,2) OP_12 starts combat via event VM", fails);

        runtime.bindCombat(nullptr);
        runtime.enterLocation(0, gs, world);
        world.enterScreen(0);
        gs.setScreenId(0);
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

    /* Regression tests for the OP_0B text-fabrication bug + Arena mis-dispatch
     * (2026-07 user reports: "the inn was wrong", "mage guild says farthing",
     * "goblet quest said farthing too", "Corak mentions tickets", "brain detox
     * says farthing", "farthing quest asks for a ticket", "arena is wrong").
     * Root cause #1: OP_0B's argument is a sign-graphic table key, NOT a
     * caption string index — EventRuntime.cpp no longer resolves+captures a
     * string from it (see EventRuntime.cpp OP_0B, doc 07 "OP_0B does not
     * display text"). Root cause #2: the Arena Games ticket engine (0x9D76)
     * is reached ONLY via explicit selector 0x08's thunk -$7DBE, not via the
     * default-range thunk -$7DFA (event_dat_loader) — see EventTownServices
     * case 0x08 vs default:. */

    /* Inn @ event 25 (3,7)/DIR_S: OP_0E 0x01 shows per-town exe innkeeper greet
     * + registry y/n (doc 29), not the OP_0B sign title fallback. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(3);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 25 inn fires at (7,3) facing S", fails);
    expect(!runtime.textView().containsText("Slaughtered Lamb"),
           "inn must not show pub sign caption (OP_0B fabrication)", fails);
    expect(!runtime.textView().containsText("farthing"), "inn must not show farthing text", fails);
    expect(runtime.textView().containsText("innkeeper"),
           "inn shows exe innkeeper registry greet (Middlegate)", fails);
    expect(runtime.textView().containsText("registry (y/n)"),
           "inn greet ends with registry y/n", fails);

    /* Enroll mage guild @ event 19 (12,1)/DIR_W: OP_0E 0x0D must show the
     * real shared-string-bank enroll prompt, not the unrelated str[20]
     * "Fool, you have no farthing to flick!" caption. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(1);
    gs.setCoordY(12);
    gs.setFacingKey('W');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 19 enroll fires at (1,12) facing W", fails);
    expect(!runtime.textView().containsText("farthing"),
           "enroll mage guild must not show farthing error string", fails);
    expect(runtime.textView().containsText("sleepy conjurer"),
           "enroll mage guild shows real shared-string-bank prompt", fails);
    expect(runtime.textView().containsText("enroll you in the local mage guild"),
           "enroll mage guild prompt mentions the guild (not farthing)", fails);

    /* Accept enroll: 20 gp + Middlegate membership bit in record+0x79. */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 5, 100);
        Mm2PartyLaunch launch{};
        launch.party_count = 1;
        launch.roster_slots[0] = 0;
        runtime.bindParty(&roster, &launch);
        mm2::platform::KeyState yes{};
        yes.last_ascii = 'Y';
        runtime.continueInput(gs, world, yes);
        expect(mm2::events::townSvcMageGuildMember(roster.records[0], 0),
               "enroll sets Middlegate guild membership (record+0x79)", fails);
        expect(roster.records[0].gold == 80u, "enroll deducted 20 gp", fails);
        expect(roster.records[0].town_flags == 1, "enroll sets home town (record+0x0B)", fails);
        runtime.bindParty(nullptr, nullptr);
    }

    /* Nordon goblet quest @ event 30 (10,2)/DIR_W: OP_0E 0x0A must show the
     * real Nordon intro, not Feldecarb Fountain text (that is selector 0x0E
     * @ (15,15) event 17). */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(10);
    gs.setCoordY(2);
    gs.setFacingKey('W');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 30 Nordon fires at (10,2) facing W", fails);
    expect(runtime.textView().containsText("Nordon"),
           "goblet quest shows Nordon intro", fails);
    expect(runtime.textView().containsText("do me a service"),
           "goblet quest shows Nordon service prompt", fails);
    expect(!runtime.textView().containsText("Feldecarb Fountain"),
           "Nordon must not show Feldecarb fountain text", fails);

    /* Corak ghost @ event 18 (4,7)/DIR_N: OP_0B 51.anm + OP_0E 0x09 → loc-60
     * queued dispatch. ASM rebuilds the string anchor as LE from work_buf[0..1]
     * (0x00FF) and pool_seeks id 1 from parse_pos=2 → OP_03 str[1] Corak
     * monologue. A BE-anchor + codec-string bytecode fallback previously ran
     * the Nordon goblet-award script here instead. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(4);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 18 Corak ghost fires at (7,4) facing N", fails);
    expect(runtime.textView().containsText("spirit of Corak"),
           "Corak ghost shows Corak monologue", fails);
    expect(runtime.textView().containsText("Fantastic adventure"),
           "Corak monologue includes adventure line", fails);
    expect(!runtime.textView().containsText("goblet"),
           "Corak ghost must not show goblet award/quest text", fails);
    expect(!runtime.textView().containsText("Nordon"),
           "Corak ghost must not show Nordon text", fails);
    expect(!runtime.textView().containsText("Eagle Eye"),
           "Corak ghost must not show goblet reward text", fails);

    /* Feldecarb Fountain @ event 17 (15,15)/NE: OP_0E 0x0E shows farthing prompt. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(15);
    gs.setCoordY(15);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 17 Feldecarb fires at (15,15)", fails);
    expect(runtime.textView().containsText("Feldecarb Fountain"),
           "Feldecarb shows fountain prompt", fails);
    expect(!runtime.textView().containsText("Nordon"),
           "Feldecarb must not show Nordon text", fails);

    /* Fountain of Clairvoyance @ event 42 (4,8)/DIR_S: OP_1A sets both eye timers. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(8);
    gs.setCoordY(4);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_CLASS_QUEST_CNT, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_QUEST_COUNTER_B, 0);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 42 clairvoyance fires at (4,8) facing S", fails);
    expect(runtime.textView().containsText("clairvoyance"),
           "clairvoyance fountain shows prompt", fails);
    expect(runtime.blocksMovement(), "clairvoyance waits for Y/N", fails);
    {
        mm2::platform::KeyState yes{};
        yes.last_ascii = 'Y';
        expect(!runtime.continueInput(gs, world, yes), "clairvoyance Y completes script", fails);
    }
    expect(mm2_gs_u8(gs.a4(), MM2_GS_CLASS_QUEST_CNT) == 0x32,
           "clairvoyance sets Eagle Eye timer (g=0x2B)", fails);
    expect(mm2_gs_u8(gs.a4(), MM2_GS_QUEST_COUNTER_B) == 0x32,
           "clairvoyance sets Wizard Eye timer (g=0x2C)", fails);

    /* Arena Games @ event 09 (2,13)/ANY_DIR: explicit OP_0E 0x08 is the SOLE
     * path into the Arena ticket engine — this tile has no OP_0B sign, so it
     * genuinely IS an arena-ticket prompt (unlike the false positives below). */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(13);
    gs.setCoordY(2);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 09 arena fires at (13,2) ANY_DIR", fails);
    expect(runtime.textView().containsText("ticket"),
           "explicit selector 0x08 genuinely is the arena ticket engine", fails);

    /* Default-range selector @ event 38 (4,10)/ANY_DIR, OP_0E 0x26: runs loc 61
     * str[22] (OP_12 arena tier) — must NOT show ticket-check text. */
    if (has_monsters) {
        Mm2RosterFile tier_roster{};
        setupMember(tier_roster, 0, 5, 100);
        tier_roster.records[0].might_current = 50;
        tier_roster.records[0].speed_current = 50;
        tier_roster.records[0].hp_current = 200;
        Mm2PartyLaunch tier_launch{};
        tier_launch.party_count = 1;
        tier_launch.roster_slots[0] = 0;
        mm2::gameplay::Rng tier_rng(42);
        mm2::combat::CombatSession arena_tier_combat;
        arena_tier_combat.bindParty(&tier_roster, &tier_launch);
        arena_tier_combat.bindMonsters(&monsters);
        arena_tier_combat.bindRng(&tier_rng);
        runtime.bindParty(&tier_roster, &tier_launch);
        runtime.bindCombat(&arena_tier_combat);

        runtime.enterLocation(0, gs, world);
        gs.setCoordX(10);
        gs.setCoordY(4);
        gs.setFacingKey('N');
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
        expect(runtime.scanAndRun(gs, world), "event 38 default-range selector fires at (10,4) ANY_DIR", fails);
        expect(!runtime.textView().containsText("ticket"),
               "default-range selector 0x26 must not fabricate arena ticket text", fails);
        expect(arena_tier_combat.active(),
               "event 38 selector 0x26 runs loc-61 OP_12 arena tier combat", fails);

        /* Middlegate outdoor combat squares (events 38–41): OP_0E 0x26..0x29. */
        struct CombatTile {
            int coord_x;
            int coord_y;
            uint8_t selector;
            const char *label;
        };
        static const CombatTile kCombatTiles[] = {
            {10, 4, 0x26, "event 38 @ (4,10)"},
            {13, 12, 0x27, "event 39 @ (12,13)"},
            {12, 14, 0x28, "event 40 @ (14,12)"},
            {8, 1, 0x29, "event 41 @ (1,8)"},
        };
        for (const CombatTile &tile : kCombatTiles) {
            mm2::combat::CombatSession tile_combat;
            tile_combat.bindParty(&tier_roster, &tier_launch);
            tile_combat.bindMonsters(&monsters);
            tile_combat.bindRng(&tier_rng);
            runtime.bindCombat(&tile_combat);

            runtime.enterLocation(0, gs, world);
            gs.setCoordX(static_cast<uint8_t>(tile.coord_x));
            gs.setCoordY(static_cast<uint8_t>(tile.coord_y));
            gs.setFacingKey('N');
            mm2_gs_set_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH, 0);
            mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
            expect(runtime.scanAndRun(gs, world), tile.label, fails);
            expect(tile_combat.active(), "selector combat arms CombatSession", fails);
        }

        /* map.dat collision 0x80 with no event.dat triplet @ 0x176F2. */
        {
            mm2::combat::CombatSession ambient_combat;
            ambient_combat.bindParty(&tier_roster, &tier_launch);
            ambient_combat.bindMonsters(&monsters);
            ambient_combat.bindRng(&tier_rng);
            runtime.bindCombat(&ambient_combat);

            runtime.enterLocation(0, gs, world);
            gs.setCoordX(6);
            gs.setCoordY(11);
            gs.setFacingKey('N');
            expect(mm2_map_collision_has_event(world.collisionAt(6, 11)),
                   "tile (6,11) has collision event flag", fails);
            mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
            expect(runtime.scanAndRun(gs, world), "tile-flag ambient fight at (6,11)", fails);
            expect(ambient_combat.active(), "tile-flag scan arms random combat", fails);
            expect(!mm2_map_collision_has_event(world.collisionAt(6, 11)),
                   "tile-flag cleared from map after fight", fails);
            mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
            expect(!runtime.scanAndRun(gs, world), "cleared tile does not re-fight", fails);

            runtime.bindCombat(nullptr);
        }

        runtime.bindCombat(nullptr);
        /* tier_roster/tier_launch are scope-locals — unbind before they die so
         * later sections (Pegasus OP_18) fall back to the GS-image roster. */
        runtime.bindParty(nullptr, nullptr);
    }

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

    /* Inn registry @ 0x1A1B2: roster+$0B = map+1 for each active party slot. */
    {
        Mm2RosterFile roster{};
        Mm2PartyLaunch launch{};
        launch.party_count = 2;
        launch.roster_slots[0] = 0;
        launch.roster_slots[1] = 3;
        roster.records[0].town_flags = 0x81;
        roster.records[3].town_flags = 0x85;
        mm2::events::eventInnApplyRegistry(&roster, &launch, 0);
        expect(roster.records[0].town_flags == 1, "inn registry clears bit7 and sets Middlegate home", fails);
        expect(roster.records[3].town_flags == 1, "inn registry sets home for each party slot", fails);
        expect(roster.records[1].town_flags == 0, "inn registry skips non-party slots", fails);
    }

    /* Otto Mapper cartographer @ event 33 (0,15)/W: sets roster+0x50 low nibble to 3. */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 1, 100);
        roster.records[0].condition = 0;
        Mm2PartyLaunch launch{};
        launch.party_count = 1;
        launch.roster_slots[0] = 0;
        runtime.bindParty(&roster, &launch);
        runtime.enterLocation(0, gs, world);
        gs.setCoordX(0);
        gs.setCoordY(15);
        gs.setFacingKey('W');
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
        expect(runtime.scanAndRun(gs, world), "event 33 Otto Mapper fires at (0,15) facing W", fails);
        expect(runtime.textView().containsText("Otto adds"),
               "Otto Mapper shows purchase prompt", fails);
        mm2::platform::KeyState yes{};
        yes.last_ascii = 'Y';
        runtime.continueInput(gs, world, yes);
        yes.last_ascii = '1';
        expect(!runtime.continueInput(gs, world, yes), "Otto skill buy completes after member pick", fails);
        expect(mm2::gameplay::rosterHasSkillId(roster.records[0], 3),
               "Otto Mapper grants Cartographer (id 3) at roster+0x50", fails);
        runtime.bindParty(nullptr, nullptr);
    }

    /* Brain detox @ event 16 (1,8)/W: OP_0E 0x10 clears roster+0x50 skills. */
    {
        Mm2RosterFile roster{};
        setupMember(roster, 0, 1, 200);
        roster.records[0].condition = 0;
        mm2::gameplay::rosterGrantSkillId(roster.records[0], 3);
        mm2::gameplay::rosterGrantSkillId(roster.records[0], 11);
        Mm2PartyLaunch launch{};
        launch.party_count = 1;
        launch.roster_slots[0] = 0;
        runtime.bindParty(&roster, &launch);
        runtime.enterLocation(0, gs, world);
        gs.setCoordX(1);
        gs.setCoordY(8);
        gs.setFacingKey('W');
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
        expect(runtime.scanAndRun(gs, world), "event 16 brain detox fires at (1,8) facing W", fails);
        expect(!runtime.textView().containsText("farthing"),
               "brain detox must not show Feldecarb farthing text", fails);
        expect(runtime.textView().containsText("Detoxification"),
               "brain detox shows specialist intro", fails);
        mm2::platform::KeyState yes{};
        yes.last_ascii = 'Y';
        runtime.continueInput(gs, world, yes);
        yes.last_ascii = '1';
        expect(!runtime.continueInput(gs, world, yes), "brain detox completes after member pick", fails);
        expect(mm2::gameplay::rosterSkillPackedByte(roster.records[0]) == 0,
               "brain detox clears roster+0x50 skill pack", fails);
        expect(roster.records[0].gold == 100u, "brain detox deducted 100 gp from selected char", fails);
        runtime.bindParty(nullptr, nullptr);
    }

    /* OP_30 @ 0x17034: 10-byte space-padded compare (toupper via -$7B78). */
    {
        using mm2::events::eventVmCheckOp30Password;
        uint8_t expected[10];
        const char *ans = "MEENU";
        for (int i = 0; i < 10; ++i) {
            const char c = (i < 5) ? ans[i] : ' ';
            expected[i] = static_cast<uint8_t>((0x11A - static_cast<uint8_t>(c)) & 0xFF);
        }
        uint8_t buf[11];
        for (int i = 0; i < 10; ++i) {
            buf[i] = (i < 5) ? static_cast<uint8_t>(ans[i]) : static_cast<uint8_t>(' ');
        }
        buf[10] = 0;
        expect(eventVmCheckOp30Password(buf, expected, 10), "OP_30 MEENU matches", fails);
        buf[0] = 'X';
        expect(!eventVmCheckOp30Password(buf, expected, 10), "OP_30 wrong answer fails", fails);
        uint8_t empty[10] = {};
        expect(!eventVmCheckOp30Password(empty, expected, 10), "OP_30 empty buffer fails", fails);
    }

    /* Castle blob locs leave anchor $FFFF (no 00 00 00 terminator). */
    {
        uint8_t castle_gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
        mm2::GameStateView castle_gs(mm2_gs_base_from_image(castle_gs_image));
        mm2::world::MapWorld castle_world;
        expect(castle_world.load(data_dir), "castle map", fails);
        mm2::events::EventRuntime castle_rt;
        expect(castle_rt.load(data_dir), "castle event", fails);
        if (castle_rt.enterLocation(63, castle_gs, castle_world)) {
            expect(mm2_gs_u16(castle_gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR) == 0xFFFF,
                   "castle blob loc 63 leaves anchor $FFFF", fails);
        }
    }

    if (fails == 0) {
        std::printf("OK: event_middlegate_test\n");
        return 0;
    }
    return 1;
}

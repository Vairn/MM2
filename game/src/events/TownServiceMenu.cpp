#include "mm2/events/TownServiceMenu.h"

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
        if (!ui.chooseTempleOption(ctx, opt) || opt == TempleOption::Exit) {
            return;
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
            const uint32_t cost = townSvcHealingCost(rec->level, ctx.map_id);
            const TownSvcHealResult r = townSvcHeal(*rec, cost);
            if (!r.paid) {
                ui.reportNotEnoughGold();
            } else {
                ui.reportHeal(r);
            }
            break;
        }
        case TempleOption::RestoreCondition:
            townSvcRestoreCondition(*rec);
            ui.reportHeal(TownSvcHealResult{}); /* no charge for cond-only path */
            break;
        case TempleOption::Donate: {
            const TownSvcDonateResult r = townSvcTempleDonate(ctx.a4, *rec, ctx.map_id);
            if (!r.paid) {
                ui.reportNotEnoughGold();
            } else {
                ui.reportDonate(r);
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
        const TownSvcTrainResult r = townSvcTrainLevelUp(*rec, ctx.map_id);
        if (r.eligible && !r.paid) {
            ui.reportNotEnoughGold();
        } else {
            ui.reportTrain(r); /* reports level-up OR "not enough experience" */
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

}  // namespace mm2::events

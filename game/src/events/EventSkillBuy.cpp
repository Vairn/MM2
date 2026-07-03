#include "mm2/events/EventSkillBuy.h"

#include "mm2/events/EventVmHelpers.h"
#include "mm2/gameplay/RosterSkills.h"

namespace mm2::events {

namespace {

Mm2RosterRecord *selectedRosterMember(Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                      const uint8_t *a4)
{
    if (!roster || !launch || !a4) {
        return nullptr;
    }
    const int slot = eventVmSelectedPartySlot(a4);
    if (slot < 0 || slot >= launch->party_count || slot >= MM2_PARTY_LAUNCH_SLOTS) {
        return nullptr;
    }
    const int idx = launch->roster_slots[slot];
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
        return nullptr;
    }
    return &roster->records[idx];
}

constexpr SkillBuyOffer kSkillBuyOffers[] = {
    /* Tundara — event 24 @ (8,13)/W, OP_0E 0x38 -> cat 0x3E idx 1 */
    {0x38, 12, 750u,
     "Tired of getting lost? For just 750 gold,\n"
     "Chris will teach you how to use a sextant\n"
     "properly. Learn (y/n)?",
     false},
    /* Vulcania — events 27..29, OP_0E 0x3E..0x40 -> cat 0x3E idx 7..9 */
    {0x3E, 15, 500u,
     "\"I am Sgt. Stephen Pain. For 500 gold join\n"
     "my private hell and increase your pathetic\n"
     "stamina.\" Train (y/n)?",
     false},
    {0x3F, 8, 500u,
     "The enormous gladiator Spartacus will\n"
     "mercilessly beat you for 500 gold. You will\n"
     "be stronger. Pay the brute (y/n)?",
     false},
    {0x40, 1, 500u,
     "Learn the ancient secrets every weapon hides.\n"
     "The master of accuracy charges 500 gold.\n"
     "Do you pay (y/n)?",
     false},
    /* Sandsobar — events 34..36, OP_0E 0x4D..0x4F -> cat 0x3F idx 2..4 */
    {0x4D, 5, 500u,
     "Rinaldo, the ultimate ambassador, will instruct\n"
     "you in the gentle art of diplomacy for 500 gold.\n"
     "Learn (y/n)?",
     false},
    {0x4E, 6, 200u,
     "Lucky Spade, the best gambler alive, will teach\n"
     "you his system for 200 gold. Learn (y/n)?",
     false},
    {0x4F, 14, 300u,
     "Sly, a seedy looking rogue, will teach you how\n"
     "to pickpocket for 300 gold. Pay (y/n)?",
     false},
    /* Atlantium Hero — event 29 runs y/n + OP_26, then OP_0E 0x52 -> cat 0x3F idx 7 */
    {0x52, 7, 1000u, nullptr, true},
};

}  // namespace

const SkillBuyOffer *skillBuyOfferForExecSelector(uint8_t sel)
{
    for (const SkillBuyOffer &offer : kSkillBuyOffers) {
        if (offer.exec_selector == sel) {
            return &offer;
        }
    }
    return nullptr;
}

bool eventStartSkillBuyOverlay(EventTextView &text, EventVmWait &wait, const SkillBuyOffer &offer)
{
    if (offer.member_already_selected || !offer.intro_yesno) {
        return false;
    }
    text.showOp02(offer.intro_yesno, 19);
    wait = EventVmWait::YesNo;
    return true;
}

bool eventApplySkillBuy(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                        EventTextView &text, EventVmWait &wait, uint8_t skill_id, uint32_t gold_cost)
{
    Mm2RosterRecord *rec = selectedRosterMember(roster, launch, gs.a4());
    if (!rec) {
        return false;
    }

    if (gameplay::rosterSkillSlotFull(*rec, false) && gameplay::rosterSkillSlotFull(*rec, true)) {
        text.showOp02("You already have two skills!", 19);
        text.showSpacePrompt();
        wait = EventVmWait::Space;
        return false;
    }

    const uint32_t have = eventVmPartyGoldTotal(gs.a4(), roster, launch);
    if (have < gold_cost) {
        text.showOp02("Not enough gold!", 19);
        text.showSpacePrompt();
        wait = EventVmWait::Space;
        return false;
    }

    gameplay::rosterGrantSkillId(*rec, skill_id);
    return true;
}

}  // namespace mm2::events

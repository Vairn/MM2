#pragma once

#include "mm2/events/EventRuntime.h"
#include "mm2/events/EventTextView.h"
#include "mm2/GameState.h"

#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::events {

/** One town skill vendor reached via OP_0E default-range exec_selector (loc-60
 * overlay reinvoke @ 0x92F2 — bytecode not decoded; prompts from FAQ/event.dat). */
struct SkillBuyOffer {
    uint8_t exec_selector;
    uint8_t skill_id;   /* 1..15, packed @ roster+0x50 via selector 0x6D in inline scripts */
    uint32_t gold_cost; /* OP_24 party-gold check in inline scripts; no OP_20 deduct there */
    const char *intro_yesno;
    /** When true, OP_0E fires after OP_26 member select (Atlantium Hero evt 29). */
    bool member_already_selected;
};

/** Lookup overlay skill vendor by exec_selector (0x38, 0x3E..0x40, 0x4D..0x4F, 0x52). */
const SkillBuyOffer *skillBuyOfferForExecSelector(uint8_t sel);

/** Start y/n + member-select flow for an overlay skill vendor. Returns true when handled. */
bool eventStartSkillBuyOverlay(EventTextView &text, EventVmWait &wait, const SkillBuyOffer &offer);

/** Grant skill after member pick (or immediately when member_already_selected).
 * Mirrors inline event.dat skill scripts: slot test, party gold check (OP_24),
 * apply_party_masked on roster+0x50 — no gold deduct in those bytecodes. */
bool eventApplySkillBuy(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                        EventTextView &text, EventVmWait &wait, uint8_t skill_id, uint32_t gold_cost);

}  // namespace mm2::events

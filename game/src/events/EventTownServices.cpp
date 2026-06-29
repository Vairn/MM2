#include "mm2/events/EventTownServices.h"

#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/TownServiceMenu.h"

#include "mm2/CppStdCompat.h"

namespace mm2::events {

/* ---------------------------------------------------------------------------
 * OP_0E town/building service selectors (dispatch @ 0x160C2, doc 07 §OP_0E).
 *
 * Selectors 1..8 each `jsr` into the town shop / temple / training / guild
 * engine. Those handlers are the runtime A4 vtable thunks (`-$7Dxx`) and the
 * static pub/temple/smith handlers (0x1A132 / 0x1D208 / 0x1C54A). That engine
 * is an INTERACTIVE multi-option menu UI (NPC intro y/n → A–F menu loop →
 * per-character heal / train / buy with gold deduction) and is NOT yet ported.
 *
 * To stay faithful to the ASM (CLAUDE.md: no invented behavior), each selector
 * below presents the REAL on-screen entry text transcribed byte-for-byte from
 * str.dat (EXTRACTED/docs/11-str-decoded.txt) and then DEFERS the interactive
 * transaction to the unported engine, documenting the precise handler address.
 * The earlier MVP placeholder prose + auto heal/train/rest/gold-deduction were
 * fabrications (the engine never auto-applies a whole-party transaction without
 * the player's menu selection) and have been removed. Cost formulas live as
 * documented helpers in EventVmHelpers (eventVmTrainingCostPerChar /
 * eventVmHealingCostPerChar) for the future engine port + tests.
 * ------------------------------------------------------------------------- */

namespace {

/* str.dat tavern NPC intros (lines 89–108). Indexed by map screen / location id
 * (A4-$79F2, 0..4) — same ordering the guild/temple intros use (the str pool is
 * sequential and the engine selects by map index). Note: doc 28 §3.1's NPC→town
 * play-matching put Belinthra at Vulcania / Gabrielle at Sandsobar, but the
 * sequential str order (confirmed map-ordered by the guild intros, doc 34 §6.3:
 * idx2 = Tundara) places Belinthra=idx2, Gabrielle=idx3, Lucindra=idx4. */
const char *tavernIntro(int location_id)
{
    static const char *kIntro[5] = {
        "A low mumble emerges from the middle\n"
        "of the road crowd.  Amber, a sultry\n"
        "waitress asks, \"Do you wish to order\n"
        "from our vast menu of drinks (y/n)?\"",
        "The barkeep Rowena boasts, \"We have\n"
        "the best brewmeister in the land!\n"
        "Interested (y/n)?\"",
        "Belinthra, the bawdy proprietress,\n"
        "raises her tankard of ale in a toast\n"
        "and booms, \"Would you like a sampling\n"
        "of my stuff (y/n)?\"",
        "Raising a cup of mead, Gabrielle, the\n"
        "gorgeous barmaid winks seductively\n"
        "while softly asking \"Would you like\n"
        "to use my services (y/n)?\"",
        "Amidst bloodthirsty brawling stands\n"
        "luscious Lucindra, the barmaid.\n"
        "Bottles fly through the air as she\n"
        "asks, \"Can I do ya something' (y/n)?\"",
    };
    if (location_id >= 0 && location_id < 5) {
        return kIntro[location_id];
    }
    return kIntro[0];
}

/* str.dat temple priest intros (lines 343–357), map-index ordered. */
const char *templeIntro(int location_id)
{
    static const char *kIntro[5] = {
        "A slim cleric in a cowled robe peers\n"
        "at you and asks in a serene voice,\n"
        "\"May I aid you, travelers (y/n)?\"",
        "An initiate tends embers of saffron\n"
        "in an ornate alterbowl.  He offers\n"
        "assistance.  Accept (y/n)?",
        "A drumbeat and candlelight lend the\n"
        "temple an aura of tranquility.  A\n"
        "priest offers help.  Accept (y/n)?",
        "Tending the smoke of the lava fire,\n"
        "a curate breaks his silent prayer.\n"
        "\"May I offer you our services (y/n)?\"",
        "Ambergris and woodcedar incense burns\n"
        "on the altar as the priest chants.\n"
        "He asks, \"Do you need any help (y/n)?\"",
    };
    if (location_id >= 0 && location_id < 5) {
        return kIntro[location_id];
    }
    return kIntro[0];
}

/* str.dat mage-guild hall intros (lines 328–342), map-index ordered (doc 34 §6.3). */
const char *mageGuildIntro(int location_id)
{
    static const char *kIntro[5] = {
        "Sages in multi-hued robes congregate\n"
        "in the hall.  The archmage offers\n"
        "spells for sale.  Interested (y/n)?",
        "The meeting shifts towards entropy as\n"
        "you step in.  A cabalist approaches\n"
        "you with a spell list.  Buy (y/n)?",
        "Lounging next to a roaring fire which\n"
        "burns no wood, mystics offer spells.\n"
        "Buy (y/n)?",
        "Magicians clad in furry robes sip\n"
        "wine and chat softly.  Listen (y/n)?",
        "Sorcerers sort phials of sands on\n"
        "the shelves.  A man barks, \"Spells\n"
        "(y/n)?\"",
    };
    if (location_id >= 0 && location_id < 5) {
        return kIntro[location_id];
    }
    return kIntro[0];
}

/* str.dat blacksmith intros (lines 255–273), map-index ordered (doc 28 §3.2:
 * Drewnhald = idx1 Atlantium). */
const char *blacksmithIntro(int location_id)
{
    static const char *kIntro[5] = {
        "The burly blacksmith Svendegard busily\n"
        "shapes a deadly sword in the forge.\n"
        "Do you care to see his work (y/n)?",
        "The famous blacksmith Morgan\n"
        "Drewnhald asks if you wish to view\n"
        "weapons he has made available for\n"
        "sale.  View (y/n)?",
        "Flames leap out from the brick\n"
        "smelter as the sweaty smith says,\n"
        "\"Do you wish to see my humble\n"
        "wares (y/n)?\"",
        "A friendly smith wipes his hands on a\n"
        "worn, leather apron.  He shakes your\n"
        "hand and asks, \"Care to see my wares\n"
        "(y/n)?\"",
        "A tough old blacksmith glares at\n"
        "your party and shouts \"Hey, only\n"
        "cowards browse!\"  View wares (y/n)?",
    };
    if (location_id >= 0 && location_id < 5) {
        return kIntro[location_id];
    }
    return kIntro[0];
}

const char *townName(int location_id)
{
    static const char *kNames[] = {"Middlegate", "Atlantium", "Tundara", "Vulcania", "Sandsobar"};
    if (location_id >= 0 && location_id < 5) {
        return kNames[location_id];
    }
    return "Town";
}

/* Service NPC intro + "(y/n)?" prompt. Mirrors the engine's OP_0E entry: block
 * text window + block-text exit flag (bit1) + Y/N wait. On "yes" the real engine
 * opens the deferred A–F menu (see per-selector handler notes); the port has no
 * shop UI yet, so the menu transaction is deferred. */
void showServiceIntro(GameStateView &gs, EventTextView &text, EventVmWait &wait, const char *intro)
{
    text.showOp02(intro, 19);
    if (gs.a4()) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 2));
    }
    wait = EventVmWait::YesNo;
}

/* Services that open their menu directly (no NPC y/n intro string in str.dat:
 * inn, training, general store, special shop). Show the real OP_0B sign title
 * as the window text and defer the menu. service_title is the event.dat string
 * captured by OP_0B (e.g. "Middlegate Inn", "Turkov's Training"). */
void showServiceTitle(EventTextView &text, EventVmWait &wait, const char *title)
{
    text.showOp02((title && title[0]) ? title : "", 19);
    text.showSpacePrompt();
    wait = EventVmWait::Space;
}

/* When a town-service UI backend is bound and the party is present, run the
 * faithful interactive menu (logic = ASM, presentation = backend) and return
 * true. Otherwise return false so the caller falls back to the str.dat intro +
 * deferral (no fabricated transaction). `kind` selects temple vs training. */
enum class MenuKind { Temple, Training, Smith };

bool runBoundMenu(EventRuntime &rt, GameStateView &gs, Mm2RosterFile *roster,
                  const Mm2PartyLaunch *launch, const Mm2ItemsFile *items, int location_id,
                  MenuKind kind)
{
    ITownServiceUi *ui = rt.townServiceUi();
    if (!ui || !roster || !launch || launch->party_count <= 0) {
        return false;
    }
    if (kind == MenuKind::Smith && !items) {
        return false; /* blacksmith pricing needs items.dat (0x1BF16) */
    }
    TownServiceContext ctx;
    ctx.roster = roster;
    ctx.launch = launch;
    ctx.items = items;
    ctx.a4 = gs.a4();
    ctx.map_id = location_id;
    switch (kind) {
    case MenuKind::Temple:
        townSvcRunTemple(*ui, ctx);
        break;
    case MenuKind::Training:
        townSvcRunTraining(*ui, ctx);
        break;
    case MenuKind::Smith:
        townSvcRunSmith(*ui, ctx);
        break;
    }
    return true;
}

}  // namespace

void eventExecTownSelector(EventRuntime &rt, GameStateView &gs, world::MapWorld &world,
                             uint8_t sel, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                             const Mm2ItemsFile *items, EventTextView &text, EventVmWait &wait,
                             int location_id, const char *service_title)
{
    (void)world;
    char buf[320];
    const char *title = (service_title && service_title[0]) ? service_title : townName(location_id);

    switch (sel) {
    case 0x01:
        /* Inn — handler @ 0x160C2 case 1 (rest / roster dismiss). DEFERRED: the
         * rest restores HP/SP and advances the calendar and the dismiss UI lets
         * you store members; exact rest/day-advance amounts are not ASM-confirmed
         * (the old port's full-heal + "+64 subday" were invented). No NPC intro
         * string in str.dat, so show the OP_0B sign title and defer the menu. */
        showServiceTitle(text, wait, title);
        break;
    case 0x02:
        /* Training Hall — handler @ 0x160C2 case 2 (doc 28 §9). The hall LEVELS the
         * chosen character UP from experience (record+0x62) when they meet the next
         * level's XP threshold on their class curve (TWO curves: Group A/B, doc 32)
         * and can pay the fee (level×index×50). It does NOT raise a stat — the prior
         * port's stat-add (0x1C898) was the wrong handler/semantics. When a UI
         * backend is bound, the faithful XP→level-up menu runs (TownServiceMenu →
         * townSvcTrainLevelUp). DEFERRED: the exact per-level HP RANDOM roll
         * (0x9BCA) is modelled by the doc-32 average; calendar mode @ 0x9B48. No
         * NPC intro string -> the OP_0B sign title fallback when no backend. */
        if (runBoundMenu(rt, gs, roster, launch, items, location_id, MenuKind::Training)) {
            break;
        }
        showServiceTitle(text, wait, title);
        break;
    case 0x03:
        /* Pub / food / drinks / rumors. ENGINE-GATED (doc 28 §3.1/§7, doc 34 §5/§6).
         * The 0x03 handler is the NPC intro + guild-enroll y/n; the food/drink
         * submenus enter via selectors 0xC9/0xCA -> 0x1980A and route through the
         * runtime vtable thunk -$7D40 (no static target). The cost leaves 0x18EC0
         * (food) / 0x18F78 (drinks) are RNG-gated (-$7BB4) and write an ENCODING
         * byte to roster +$78 (+ a +$7C mode bit) via 0x019030 — they do NOT
         * deduct gp on the feeding-frenzy path. Drink stat bonuses are applied by
         * unported VM opcode handlers; the sick/success roll is 0x19D64
         * (RNG(50)==2) / 0x19B28 (stat reset). Rumors (0x97FC) walk the per-location
         * event bytecode at A4-$119A (display-only). None of these is a deterministic
         * pure-state leaf, so the port shows the faithful str.dat intro + y/n and
         * defers (no fabricated cost/effect). */
        showServiceIntro(gs, text, wait, tavernIntro(location_id));
        break;
    case 0x04:
        /* Temple — handler @ 0x160C2 case 4 (doc 28 §5). DEFERRED: A) Restore Cond
         * B) Restore Algn C) Donations D–F) cleric spell purchase (str.dat
         * 362–367). Healing charges eventVmHealingCostPerChar() per selected
         * character (FAQ level×index×10, ×10 dead / ×100 eradicated) inside this
         * engine. Faithful priest intro + y/n only; per-character heal/donate is
         * deferred (the old port's whole-party auto-heal was fabricated). When a
         * town-service UI backend is bound, the faithful per-character heal /
         * restore-condition / donation menu runs instead (TownServiceMenu). */
        if (runBoundMenu(rt, gs, roster, launch, items, location_id, MenuKind::Temple)) {
            break;
        }
        showServiceIntro(gs, text, wait, templeIntro(location_id));
        break;
    case 0x05:
        /* Mage guild spell shop — handler A4 thunk -$7D10 → 0x1E3E6 (doc 28 §6).
         * DEFERRED: four sorcerer spells/town (A4-$66E2), membership gate
         * @ 0x1E410, cost decode 0x1D97A. Faithful hall intro + y/n. */
        showServiceIntro(gs, text, wait, mageGuildIntro(location_id));
        break;
    case 0x06:
        /* Blacksmith — handler 0x1C54A (doc 28 §4.1). Menu A) Weapons B) Today's
         * Specials C) Armor D) Misc E) Sell F) Identify (str.dat 275–280); item
         * ids built @ 0x1C00E from the per-town tables (A4-$68EE/$683A/$68B2/
         * $6876). When a town-service UI backend + items.dat are bound, the
         * faithful BUY menu runs (TownServiceMenu/townSvcRunSmith): price from
         * items.dat gold (0x1BF16), gold deducted from the selected char's $66,
         * item added to the first empty backpack slot (0x1BE44). Sell (0x1BC26)
         * / Identify (0x1B6E0) and the Today's-Specials date-roll (0x1C146) +
         * Merchant-skill discount (-$7F32) remain engine-deferred. With no
         * backend bound, show the faithful smith intro + y/n. */
        if (runBoundMenu(rt, gs, roster, launch, items, location_id, MenuKind::Smith)) {
            break;
        }
        showServiceIntro(gs, text, wait, blacksmithIntro(location_id));
        break;
    case 0x07:
        /* General store (0x07 -> -$7DB8) -> shared category-shop engine -$7DFA
         * -> 0x9D76 (doc 28 §4.2). Middlegate m11 and Vulcania k6 only. ENGINE-GATED:
         * 0x9D76 is a multi-service mega-handler reached via runtime vtable stubs
         * (init 0x24928). The store buy loop 0xA62C uses a hard `record+$66 >= 100 gp`
         * gate (0xA75E) — NOT the items.dat-gold + 0x1C9C0 path — and dispatches
         * the purchase effect through the 0xA3AE jump table (keyed on roster +$50
         * nibbles); the item pools live in A4-$7136/-$713C. Reproducing this
         * faithfully requires porting the whole 0x9D76 shell, so it is deferred
         * (no NPC intro string -> show the OP_0B sign title). */
        showServiceTitle(text, wait, title);
        break;
    case 0x08:
        /* Arena / special shop (0x08 -> -$7DBE) -> shared category-shop engine
         * -$7DFA -> 0x9D76 (doc 28 §4.2). ENGINE-GATED: same mega-handler as 0x07
         * but arena entrances and town-specific specials (e.g. Middlegate Monster
         * Bowl). Deferred — show the OP_0B sign title when present. */
        showServiceTitle(text, wait, title);
        break;
    case 0x0A:
        /* Goblet / farthing side-quest — handler A4 thunk -$7DAC (doc 28 §5.2).
         * KNOWN GAP: the Nordon farthing-flick quest dialogue is engine-driven and
         * the exact str.dat wording is not yet isolated; deferred, not fabricated. */
        showServiceTitle(text, wait, title);
        break;
    case 0x0D:
        /* Enroll in mage guild — handler A4 thunk -$7DA0 → 0x1A1B2 / 0x1A1F8
         * (doc 28 §6.2): y/n, then on success writes roster $0B = map index + 1.
         * KNOWN GAP: the enrollment UI + membership write are engine-driven and
         * deferred (the old "you enroll in the mages guild" line was invented). */
        showServiceTitle(text, wait, title);
        break;
    case 0x11:
        if (location_id == 0) {
            /* Poorman's Portal @ (0,5): OP_0E 0x11 → Sandsobar for 10 gp (FAQ §4-1,
             * doc 34 §3). Self-contained fixed-cost transaction (cost + y/n +
             * deduct + travel) — faithfully implemented via the pending-portal
             * path, not the deferred shop engine. */
            rt.setPendingPortal({10u, 4u, 0x61u});
            text.showOp02(
                "A uniformed brownie stands before a shimmering energy field offering "
                "instantaneous travel to Sandsobar for 10 gold. Travel (y/n)?",
                19);
            mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                          static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 2));
            wait = EventVmWait::YesNo;
            break;
        }
        /* Non-Middlegate 0x11 falls in the default-range bin (category 0x3D, doc
         * 07 §OP_0E): shop/service via 0x15EDC → -$7DFA. Deferred. */
        showServiceTitle(text, wait, title);
        break;
    case 0x64:
        /* Portal / travel — handler A4 thunk -$7D9A (doc 07 §OP_0E). KNOWN GAP:
         * the inter-town portal table (doc 34 §3) is engine-driven; deferred. */
        showServiceTitle(text, wait, title);
        break;
    default:
        if (eventVmIsTownServiceSelector(sel)) {
            /* Default-range category shop (0x15EDC → -$7DFA, doc 07 §OP_0E). The
             * selector bins to a category byte 0x3C–0x46; the underlying item pool
             * is the deferred shop engine. Show the OP_0B sign title and defer. */
            std::snprintf(buf, sizeof(buf), "%s", title);
            showServiceTitle(text, wait, buf);
        }
        break;
    }
}

}  // namespace mm2::events

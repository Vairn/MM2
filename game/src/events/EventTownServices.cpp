#include "mm2/events/EventTownServices.h"

#include "mm2/events/EventCombatEncounter.h"
#include "mm2/events/EventSkillBuy.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/TownServiceMenu.h"
#include "mm2/gameplay/RosterSkills.h"

#include "mm2/CppStdCompat.h"
#include "mm2_town_tables.h"

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

/* exe-only innkeeper greets @ 0x19D00..0x1A200 (doc 29), map-index ordered. */
const char *innIntro(int location_id)
{
    static const char *kIntro[5] = {
        "A jolly old innkeeper waves his quill\n"
        "in the air as he asks, \"Will you sign\n"
        "the registry (y/n)?\"",
        "The well-groomed whiskers of the old\n"
        "concierge twitch as he proclaims, \"We\n"
        "have the finest suites.  Sign\n"
        "in (y/n)?\"",
        "Ordigon, the elderly innkeeper, rubs\n"
        "his bushy white mustache and says,\n"
        "\"We have cozy, warm beds.  Sign-in\n"
        "(y/n)?\"",
        "The aging host of the sleezy inn\n"
        "leers at you from behind his desk and\n"
        "asks, \"I suppose your party only wants\n"
        "one room.  Stay (y/n)?\"",
        "The proprietor blows a pile of dust\n"
        "off the register and asks eagerly,\n"
        "\"Can I sign you in (y/n)?\"",
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
enum class MenuKind { Temple, Training, Smith, MageGuild, Tavern };

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
    ctx.rng = rt.rng();
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
    case MenuKind::MageGuild:
        townSvcRunMageGuild(*ui, ctx);
        break;
    case MenuKind::Tavern:
        townSvcRunTavern(*ui, ctx);
        break;
    }
    return true;
}

bool canRunBoundMenu(const EventRuntime &rt, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                     const Mm2ItemsFile *items, MenuKind kind)
{
    if (!rt.townServiceUi() || !roster || !launch || launch->party_count <= 0) {
        return false;
    }
    if (kind == MenuKind::Smith && !items) {
        return false;
    }
    return true;
}

}  // namespace

bool eventTownServiceRunBoundMenu(EventRuntime &rt, GameStateView &gs,
                                  Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                                  const Mm2ItemsFile *items, int location_id,
                                  EventRuntime::PendingTownMenu kind)
{
    switch (kind) {
    case EventRuntime::PendingTownMenu::Tavern:
        return runBoundMenu(rt, gs, roster, launch, items, location_id, MenuKind::Tavern);
    case EventRuntime::PendingTownMenu::Temple:
        return runBoundMenu(rt, gs, roster, launch, items, location_id, MenuKind::Temple);
    case EventRuntime::PendingTownMenu::Smith:
        return runBoundMenu(rt, gs, roster, launch, items, location_id, MenuKind::Smith);
    default:
        return false;
    }
}

void eventExecTownSelector(EventRuntime &rt, GameStateView &gs, world::MapWorld &world,
                             uint8_t sel, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                             const Mm2ItemsFile *items, EventTextView &text, EventVmWait &wait,
                             int location_id, const char *service_title, gameplay::Rng *rng)
{
    char buf[320];
    const char *title = (service_title && service_title[0]) ? service_title : townName(location_id);

    switch (sel) {
    case 0x01:
        /* Inn — handler @ 0x160C2 case 1 → 0x1A132 (doc 28). Per-town exe
         * innkeeper greet + registry y/n (0x19D00..0x1A200, doc 29). On Y the
         * engine @ 0x1A1B2 writes roster+$0B home town for party members,
         * saves roster (-$7FE0 @ 0x1A1FC), then returns to party choose (Goto
         * Town). Rest/day-advance submenu is deferred. */
        showServiceIntro(gs, text, wait, innIntro(location_id));
        rt.setPendingTownMenu(EventRuntime::PendingTownMenu::Inn);
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
         * Handler @ 0x160C2 case 3 -> jsr $1d208 (pub init: populates 5-town ×
         * 4-category handler tables at A4-$59EE/$599E/$5986/$594E/$58AE/$57F6).
         * Food submenus: selector 0xC9 -> jsr $19ab4 (feeding-frenzy / roster +$78).
         * Drink submenus: selector 0xCA -> jsr $19ac4 (stat bonus, RNG sick roll).
         * Costs via 0x18EC0/0x18F78 (RNG-gated); stat-bonus effect 0x19B28; rumor
         * walk 0x97FC; food satiation write 0x019030. These per-character effects
         * are not yet traced; when a UI backend is bound, the faithful A-E top menu
         * runs (TownServiceMenu → townSvcRunTavern) after the str.dat NPC intro
         * y/n. Without a backend, fall back to the intro + defer. */
        showServiceIntro(gs, text, wait, tavernIntro(location_id));
        if (canRunBoundMenu(rt, roster, launch, items, MenuKind::Tavern)) {
            rt.setPendingTownMenu(EventRuntime::PendingTownMenu::Tavern);
        }
        break;
    case 0x04:
        /* Temple — handler @ 0x160C2 case 4 (doc 28 §5). DEFERRED: A) Restore Cond
         * B) Restore Algn C) Donations D–F) cleric spell purchase (str.dat
         * 362–367). Healing charges eventVmHealingCostPerChar() per selected
         * character (FAQ level×index×10, ×10 dead / ×100 eradicated) inside this
         * engine. Faithful priest intro + y/n; when a town-service UI backend is
         * bound, the per-character heal / restore-condition / donation menu runs
         * after the intro y/n (TownServiceMenu). Without a backend, defer. */
        showServiceIntro(gs, text, wait, templeIntro(location_id));
        if (canRunBoundMenu(rt, roster, launch, items, MenuKind::Temple)) {
            rt.setPendingTownMenu(EventRuntime::PendingTownMenu::Temple);
        }
        break;
    case 0x05:
        /* Mage guild spell shop — handler A4 thunk -$7D10 → 0x1E3E6 (doc 28 §6).
         * Four sorcerer spells/town (A4-$66E2, byte-exact per-town cost) via
         * TownServiceMenu::townSvcRunMageGuild when a UI backend is bound; the
         * shop-open membership gate (0x1E410, record+0x79) is ASM-confirmed.
         * That byte is earned via event-script field selector 0x74
         * (OP_15/18/1F/20, already ported) or the unported, buggy 0x9D76
         * class-quest reward loop (doc 36-class-quest-hp-bug.md). No backend
         * -> faithful hall intro + y/n. */
        if (runBoundMenu(rt, gs, roster, launch, items, location_id, MenuKind::MageGuild)) {
            break;
        }
        showServiceIntro(gs, text, wait, mageGuildIntro(location_id));
        break;
    case 0x06:
        /* Blacksmith — handler 0x1C54A (doc 28 §4.1). ASM @ 0x1C668 gates the
         * category menu on A4-$7951 (event y/n cond) like pub/temple; str.dat
         * smith intros (~255–273) precede the A–F shop menu. */
        showServiceIntro(gs, text, wait, blacksmithIntro(location_id));
        if (canRunBoundMenu(rt, roster, launch, items, MenuKind::Smith)) {
            rt.setPendingTownMenu(EventRuntime::PendingTownMenu::Smith);
        }
        break;
    case 0x07:
        /* General store (0x07 -> thunk -$7DB8 -> 0xA62C, byte-verified, doc 28
         * §4.2). Middlegate m11 and Vulcania k6 only. ENGINE-GATED: -$7DB8 is
         * its OWN fixed handler, DISTINCT from both the Arena Games engine
         * (0x9D76, reached only via explicit selector 0x08's thunk -$7DBE)
         * and the default-range dispatch (-$7DFA -> 0x92F2 event_dat_loader).
         * The store buy loop 0xA62C uses a hard `record+$66 >= 100 gp`
         * gate (0xA75E) — NOT the items.dat-gold + 0x1C9C0 path — and dispatches
         * the purchase effect through the 0xA3AE jump table (keyed on roster +$50
         * nibbles); the item pools live in A4-$7136/-$713C. Reproducing this
         * faithfully requires porting the -$7DB8 shell, so it is deferred
         * (no NPC intro string -> show the OP_0B sign title). */
        showServiceTitle(text, wait, title);
        break;
    case 0x08: {
        /* Arena Games ticket engine (0x08 -> thunk -$7DBE -> 0x9D76). CORRECTED
         * 2026-07: byte-verified against EXTRACTED/ghidra/mm2_data_00.bin (the
         * A4 vtable trampoline table) — the earlier claim that the DEFAULT-range
         * dispatch thunk -$7DFA lands on 0x9D76 was backwards. Direct read of
         * the trampoline slots:
         *   -$7DFA (file off 0x0204): 4EF9 000092F2  -> 0x092F2 event_dat_loader
         *   -$7DBE (file off 0x0240): 4EF9 00009D76  -> 0x09D76 Arena Games
         * i.e. explicit selector 0x08 (this case) is the SOLE path into the
         * Arena Games ticket-combat-reward engine; the OP_0E default-range bin
         * (0x15EDC categories 0x3C-0x46) instead re-enters the EVENT LOADER
         * with a synthetic index — almost certainly to run one of event.dat's
         * non-map "string bank" pseudo-records (e.g. loc 60 "Nordon/Nordonna/
         * Corak", a pure string pool with no script segments of its own) for
         * whatever cross-town quest/reward text that category represents. That
         * reinvocation mechanism is NOT reverse-engineered yet, so the default
         * case below no longer fabricates ticket-check text for it (that was
         * the direct cause of unrelated events — e.g. the game-start Corak
         * monologue and other default-range quest/reward tiles — incorrectly
         * showing "you must have a ticket to compete"). See doc 07 §OP_0E and
         * doc 28 §1.2 for the corrected trace. */
        const Mm2ArenaTicket ticket = eventVmFindArenaTicket(gs.a4(), roster, launch);
        if (!ticket.found) {
            /* asm 0x9DF6-0x9E2E, str @ code 0xA082/0xA0A7 (byte-exact). */
            std::snprintf(buf, sizeof(buf), "%s",
                          "Sorry, but you must have a ticket to\ncompete in these games.");
            showServiceTitle(text, wait, buf);
            break;
        }
        /* asm 0x9E32-0x9E84: consume ticket, accept text, then arm combat. */
        eventVmConsumeArenaTicket(roster, launch, ticket);
        std::snprintf(buf, sizeof(buf), "%s",
                      "The games master accepts your ticket.\n Let the battle begin!");
        showServiceTitle(text, wait, buf);

        const int roll = rng ? rng->range(1, 16) : 1;
        const uint8_t monster_type = eventVmArenaMonsterType(ticket.color, location_id, roll);
        const int party_count = launch ? launch->party_count : 0;
        uint8_t block[12] = {0};
        for (int i = 0; i < party_count && i < MM2_PARTY_LAUNCH_SLOTS && i < 10; ++i) {
            block[i] = monster_type;
        }
        /* asm 0x9F04-0x9F0A: mode=0x80 fixed fight via the SAME battle-slot
         * array + combat thunk as OP_12 (eventRunFixedEncounter, variant_b=
         * false). Gold reward (eventVmArenaGoldReward, data hunk 0xE7A) and
         * the "Winner, you receive N gold" message are granted by
         * CombatSession::finishVictory on combat victory (A4-$77BD) when a
         * combat session is bound (armArenaReward). */
        if (rt.combat()) {
            rt.combat()->armArenaReward(ticket.color, location_id);
        }
        eventRunFixedEncounter(gs, text, wait, block, 12, false, rt.combat(), &world);
        break;
    }
    case 0x0A:
        /* Nordon goblet quest — handler A4 thunk -$7DAC -> 0xD634 (decoder
         * name goblet_quest). Middlegate event 30 @ (10,2)/W: OP_0B sign 0x14
         * (Nordon portrait) + this selector. Real intro transcribed from
         * event.dat shared string bank (decoder location 60, str[9]):
         * "The humble wizard Nordon asks, \"Will you do me a service (y/n)?\""
         * NOT Feldecarb Fountain — that is selector 0x0E @ (15,15) (event 17).
         * KNOWN GAP: quest-state branches (accept, return goblet, visit
         * Nordonna) and rewards are engine-driven inside 0xD634; deferred. */
        showServiceIntro(gs, text, wait,
                          "The humble wizard Nordon asks, \"Will\n"
                          "you do me a service (y/n)?\"");
        break;
    case 0x0D:
        /* Enroll in mage guild — handler A4 thunk -$7DA0 (doc 28 §6.2). Real
         * prompt from event.dat shared string bank loc 60 str[21]. On y the
         * engine deducts 20 gp, ORs record+0x79 with the per-town mask
         * (A4-$66A9, same gate as 0x1E410 / Sandsobar apply_party_masked 0x74),
         * and writes record+0x0B <- map+1 @ 0x1A1CE. Other towns use bespoke
         * event.dat scripts for enrollment; Middlegate uses this selector. */
        showServiceIntro(gs, text, wait,
                          "A sleepy conjurer yawns, \"My\n"
                          "friends, for only 20 gold I can\n"
                          "enroll you in the local mage guild.\"\n"
                          "Buy in (y/n)?");
        rt.setPendingTownMenu(EventRuntime::PendingTownMenu::GuildEnroll);
        break;
    case 0x10:
        /* Brain detox @ Middlegate (event 16, tile (1,8)/W): OP_0E default-range
         * selector 0x10 -> 0x15EDC bins to category 0x3C (event.dat loc 60) with
         * adjusted index 8 (ASM @ 0x160A2 writes A4-$5D46). The loc-60 overlay
         * bytecode for slot 8 is not yet decoded; service copy is FAQ §8-3 only.
         * Effect: y/n, member select, 100 gp from selected char, clear roster+0x50
         * skill pack (selector 0x6D / 0x17EA6). */
        if (location_id == 0) {
            showServiceIntro(gs, text, wait,
                              "The surgically garbed Cerebral\n"
                              "Detoxification Specialist will\n"
                              "cleanse a party member of all\n"
                              "secondary skills for 100 gold.\n"
                              "Pay (y/n)?");
            rt.setPendingTownMenu(EventRuntime::PendingTownMenu::BrainDetox);
            break;
        }
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
        if (const SkillBuyOffer *offer = skillBuyOfferForExecSelector(sel)) {
            if (offer->member_already_selected) {
                (void)eventApplySkillBuy(gs, roster, launch, text, wait, offer->skill_id,
                                         offer->gold_cost);
                break;
            }
            if (eventStartSkillBuyOverlay(text, wait, *offer)) {
                rt.setPendingSkillBuy(offer->skill_id, offer->gold_cost);
                mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                              static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 2));
                break;
            }
        }
        if (sel == 0x0E) {
            /* Feldecarb Fountain farthing-flick @ Middlegate (15,15) event 17:
             * OP_0E selector 0x0E (default-range category 0x3C). Real prompt
             * from event.dat string bank loc 60 str[23]. KNOWN GAP: farthing
             * check + castle-key reward (str[24]/str[25]) inside engine path
             * -$7DFA / 0xD634 family — deferred. */
            showServiceIntro(gs, text, wait,
                              "Fanciful Feldecarb Fountain flows\n"
                              "full as fluttering faeries frolic\n"
                              "fastidiously.  Flick a farthing (y/n)?");
            break;
        }
        if (eventVmIsTownServiceSelector(sel)) {
            /* Default-range dispatch (0x15EDC → -$7DFA event_dat_loader, then
             * queued slot @ A4-$5D46). Loc 61 (cat 0x3D) embeds OP_12 fights in
             * str[22..25] for Middlegate combat tiles (selectors 0x26..0x29).
             * Text-only overlay slots still fall back to the OP_0B sign title. */
            const Mm2ExecSelectorBin bin = eventVmBinExecSelector(sel);
            if (bin.matched && rt.runDefaultRangeOverlay(gs, world, bin.category, bin.index)) {
                break;
            }
            if (sel >= 0x26 && sel <= 0x29) {
                showServiceIntro(gs, text, wait,
                                  "Combat could not start.\n"
                                  "(monsters.dat missing or overlay failed)");
                break;
            }
            showServiceTitle(text, wait, title);
        }
        break;
    }
}

namespace {

constexpr uint32_t kGuildEnrollCost = 20u;

}  // namespace

bool eventApplyGuildEnroll(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                           EventTextView &text, EventVmWait &wait, int map_id)
{
    if (map_id < 0 || map_id >= MM2_TOWN_COUNT) {
        return false;
    }
    const uint8_t member_mask = mm2_mage_guild_member_mask(map_id);
    if (member_mask == 0) {
        return false;
    }

    const uint32_t have = eventVmPartyGoldTotal(gs.a4(), roster, launch);
    if (have < kGuildEnrollCost) {
        text.showOp02("Not enough gold!", 19);
        text.showSpacePrompt();
        wait = EventVmWait::Space;
        return false;
    }

    eventVmDeductPartyGold(gs.a4(), roster, launch, kGuildEnrollCost);

    if (roster && launch) {
        for (int i = 0; i < launch->party_count; ++i) {
            const int idx = launch->roster_slots[i];
            if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
                continue;
            }
            roster->records[idx].class_quest_guild_mask =
                static_cast<uint8_t>(roster->records[idx].class_quest_guild_mask | member_mask);
        }
    }

    eventInnApplyRegistry(roster, launch, map_id);
    return true;
}

namespace {

constexpr uint32_t kBrainDetoxCost = 100u;

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

}  // namespace

bool eventApplyBrainDetox(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                          EventTextView &text, EventVmWait &wait)
{
    Mm2RosterRecord *rec = selectedRosterMember(roster, launch, gs.a4());
    if (!rec) {
        return false;
    }

    if (rec->gold < kBrainDetoxCost) {
        text.showOp02("Not enough gold!", 19);
        text.showSpacePrompt();
        wait = EventVmWait::Space;
        return false;
    }

    /* Per-character gold @ record+0x66 (ASM gold_check_and_deduct @ 0x1C9C0). */
    rec->gold -= kBrainDetoxCost;
    gameplay::rosterClearAllSkills(*rec);
    return true;
}

void eventInnApplyRegistry(Mm2RosterFile *roster, const Mm2PartyLaunch *launch, int map_id)
{
    if (!roster || !launch || map_id < 0 || map_id >= 5) {
        return;
    }
    /* ASM @ 0x1A1B2: loop party slots 0..A4-$795a-1 via A4-$796A[], write
     * roster+$0B = A4-$79F2+1 with move.b (full byte replace @ 0x1A1D0).
     * Clears bit 7 (hireling / in-party flag); do not OR with the old byte. */
    const uint8_t home = static_cast<uint8_t>(map_id + 1);
    for (int i = 0; i < launch->party_count; ++i) {
        const int idx = launch->roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        roster->records[idx].town_flags = home;
    }
}

}  // namespace mm2::events

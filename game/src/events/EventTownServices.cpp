#include "mm2/events/EventTownServices.h"

#include "mm2/events/EventCombatEncounter.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/events/TownServiceMenu.h"
#include "mm2/events/TownServiceTransactions.h"

#include "mm2/CppStdCompat.h"
#include "mm2/DataPath.h"
#include "mm2/runtime/PathScratch.h"
#include "mm2_items_codec.h"
#include "mm2_town_tables.h"

#include <cstdio>

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

/* Intra-map portal XY tables @ A4-$70D0/-$70C6/-$70BC/-$70B2 (data 0xF2E). */
static const uint8_t kPortalSrcX[10] = {1, 1, 4, 4, 7, 7, 5, 10, 13, 13};
static const uint8_t kPortalSrcY[10] = {13, 2, 5, 10, 2, 13, 10, 10, 2, 13};
static const uint8_t kPortalDstX[10] = {1, 1, 4, 4, 7, 7, 10, 10, 13, 13};
static const uint8_t kPortalDstY[10] = {14, 1, 4, 11, 1, 14, 4, 11, 1, 14};
/* Found-item ranges @ -$70A8 / -$70A5 for selectors 0x81..0x83. */
static const uint8_t kFoundItemSpan[3] = {13, 6, 7};
static const uint8_t kFoundItemBase[3] = {66, 92, 127};

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
        /* 0x1D208: bank-1 tip/rumor fill before menu. */
        if (rt.dataDir() && gs.a4()) {
            char *path = mm2_path_scratch_a();
            if (mm2::joinDataPath(path, MM2_PATH_SCRATCH_CAP, rt.dataDir(), "str.dat")) {
                FILE *fp = std::fopen(path, "rb");
                if (fp) {
                    uint8_t buf[kStrDatSize];
                    const size_t n = std::fread(buf, 1, sizeof(buf), fp);
                    std::fclose(fp);
                    eventVmFillTavernStrTables(gs.a4(), buf, n);
                }
            }
        }
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
        /* Inn @ 0x1A132: Stay/sign-in Y/N → registry @ 0x1A1B2 + roster save +
         * Goto Town (0x1A1F8). World Rest is R @ 0x19E20 — not an inn submenu.
         * 0x160E4: -$7946 = 1; 0x1A136: -$7950 = 6. */
        mm2_gs_set_u16(gs.a4(), MM2_GS_OP0E_SUBMODE, 1);
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS, 6);
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
         * townSvcTrainLevelUp). HP gain = leaf 0x20390 ($64DA/$64EE/$64E4 +
         * -$7F56(+$27)); 0x9BCA is bash door, not training. */
        if (runBoundMenu(rt, gs, roster, launch, items, location_id, MenuKind::Training)) {
            break;
        }
        showServiceTitle(text, wait, title);
        break;
    case 0x03:
        /* Pub @ 0x1D208 / jump 0x1D650: A frenzy 0x1CA2E, B stat-boost 0x1CAC4
         * (A4-$6738), C specialties 0x1CD2E (A4-$6760 + sick-or-0x1C8D4 → +$76);
         * D tip; E rumors. Encode 0x18EC0/+78 is selector 0xC9 — not C.
         * Drinks = selector 0xCA → 0x18F78 / 0x019030 (not key B). */
        showServiceIntro(gs, text, wait, tavernIntro(location_id));
        if (canRunBoundMenu(rt, roster, launch, items, MenuKind::Tavern)) {
            rt.setPendingTownMenu(EventRuntime::PendingTownMenu::Tavern);
        }
        break;
    case 0x04:
        /* Temple — handler @ 0x160C2 case 4: A Restore Cond, B Restore Algn,
         * C Donations, D–F cleric spell buy. Bound UI → TownServiceMenu. */
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
    case 0x07: {
        /* General store -$7DB8 → 0xA62C: Y/N → member → 100gp + skill conversion. */
        static const char *kStoreIntro =
            "A greedy gnome offers to convert your\n"
            "secondary skills for 100 gold.\n"
            "Buy (y/n)?";
        showServiceIntro(gs, text, wait, kStoreIntro);
        rt.setPendingGeneralStore();
        break;
    }
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
    case 0x64: {
        /* Circus -$7D9A → 0xDF04 (loc 10 day-gated). Y/N → attr pick → win/lose. */
        static const char *kCircusIntro =
            "Cheerfully striped tents. Game booths\n"
            "line the circus grounds. Play (y/n)?";
        showServiceIntro(gs, text, wait, kCircusIntro);
        rt.setPendingTownMenu(EventRuntime::PendingTownMenu::Circus);
        break;
    }
    case 0x7E:
        /* Free teleport UI -$7DB2 → 0xD576: hex X/Y → set coords + latch. */
        rt.armFreeTeleportUi();
        break;
    case 0x7F: {
        /* Combat seed -$7DAC → 0xD634: type = (rng(1,16)-1)+(Y<<4); fill party. */
        uint8_t block[12] = {0};
        const int roll = rng ? rng->range(1, 16) : 1;
        const uint8_t mon = static_cast<uint8_t>((roll - 1) + (static_cast<unsigned>(gs.coordY()) << 4));
        const int party_count = launch ? launch->party_count : 0;
        for (int i = 0; i < party_count && i < MM2_PARTY_LAUNCH_SLOTS && i < 10; ++i) {
            block[i] = mon;
        }
        mm2_gs_set_u8(gs.a4(), -0x77BE, 0);
        eventRunFixedEncounter(gs, text, wait, block, 12, false, rt.combat(), &world);
        break;
    }
    case 0x80: {
        /* Intra-map portal -$7DA6 → 0xD6A4: match XY tables → dest + slide trap. */
        int hit = -1;
        const uint8_t cx = gs.coordX();
        const uint8_t cy = gs.coordY();
        for (int i = 0; i < 10; ++i) {
            if (kPortalSrcX[i] == cx && kPortalSrcY[i] == cy) {
                hit = i;
                break;
            }
        }
        if (hit < 0) {
            break;
        }
        eventVmClearTileEventFlag(gs.a4(), static_cast<int>(cy), static_cast<int>(cx));
        gs.setCoordX(kPortalDstX[hit]);
        gs.setCoordY(kPortalDstY[hit]);
        mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
        mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                      static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 5));
        text.showOp02("Magical slide trap!", 19);
        text.showSpacePrompt();
        wait = EventVmWait::Space;
        rt.armSlideTrapHalve();
        break;
    }
    case 0x81:
    case 0x82:
    case 0x83: {
        /* Found-item -$7DA0 → 0xD89C(arg 0/1/2). */
        const int arg = static_cast<int>(sel - 0x81);
        const int span = kFoundItemSpan[arg];
        const int base = kFoundItemBase[arg];
        const int roll = rng ? rng->range(1, span) : 1;
        const uint8_t item_id = static_cast<uint8_t>(roll + base - 1);
        char iname[40];
        iname[0] = '\0';
        if (items) {
            const Mm2ItemRecord *irec = mm2_items_lookup(items, item_id);
            if (irec) {
                mm2_item_name_to_cstr(irec, iname, sizeof(iname));
            }
        }
        if (!iname[0]) {
            std::snprintf(iname, sizeof(iname), "item #%u", static_cast<unsigned>(item_id));
        }
        if (eventVmPartyGiveItem(gs.a4(), roster, launch, item_id, 0, 0)) {
            eventVmClearTileEventFlag(gs.a4(), static_cast<int>(gs.coordY()),
                                      static_cast<int>(gs.coordX()));
            mm2_gs_set_u8(gs.a4(), MM2_GS_EXIT_FLAGS,
                          static_cast<uint8_t>(mm2_gs_u8(gs.a4(), MM2_GS_EXIT_FLAGS) | 2));
            std::snprintf(buf, sizeof(buf), "You have found a %s", iname);
            showServiceTitle(text, wait, buf);
        }
        break;
    }
    case 0xC9:
    case 0xCA: {
        /* 0x19AB4/0x19AC4 → 0x1980A: 0x193AC reward+apply, 0x1961E busy gate,
         * else Y/N + A–D (encode 0x19030 / lord arm 0x191A6). */
        const bool drink = (sel == 0xCA);
        const TownSvcQuestCompleteResult done =
            townSvcQuestCompleteReward(roster, launch, drink, items);
        if (done.activity > 0) {
            char msg[192];
            if (done.members_rewarded > 0) {
                std::snprintf(msg, sizeof(msg),
                              "You have done everyone a great service\n"
                              "and you shall be rewarded.\n"
                              "  %u experience points!",
                              static_cast<unsigned>(done.xp_each));
            } else {
                std::snprintf(msg, sizeof(msg), "Quest progress applied.");
            }
            showServiceTitle(text, wait, msg);
            break;
        }
        if (townSvcQuestBusy(roster, launch, drink)) {
            showServiceTitle(text, wait,
                             drink ? "Your party has already been quested\nto seek out the foe."
                                   : "Your party has already been quested\nto seek out the item.");
            break;
        }
        rt.armQuestEncodeUi(drink);
        wait = rt.waitKind();
        (void)gs;
        (void)world;
        (void)location_id;
        (void)title;
        (void)rng;
        break;
    }
    case 0xFD: {
        /* 0x161B2: jsr 0x1493C (bank-3 fill via -$7DE8/$7DE2), then SCRIPT_ABORT:
         *   ==2: -$79AC→-$79F2, -$7946=1, jsr 0x1A1F8 Goto Town
         *   ==3: jsr -$7ED2 → 0x5376 → 0x14106 (addq -$7972, town restore, Goto Town)
         *   else: SCRIPT_ABORT=1
         * Print/fight/name-entry @ 0x1493C hosted: PTR0 → fight ($83) → 0x14EA4
         * endgame.32 + WAFE entry → remaining ptr pages. */
        if (rt.dataDir()) {
            char *path = mm2_path_scratch_a();
            if (mm2::joinDataPath(path, MM2_PATH_SCRATCH_CAP, rt.dataDir(), "str.dat")) {
                FILE *fp = std::fopen(path, "rb");
                if (fp) {
                    uint8_t buf[kStrDatSize];
                    const size_t n = std::fread(buf, 1, sizeof(buf), fp);
                    std::fclose(fp);
                    eventVmFillOp0eFdStrTables(gs.a4(), buf, n);
                }
            }
        }
        rt.armOp0eFdPrintChrome();
        const uint8_t abort = mm2_gs_u8(gs.a4(), MM2_GS_SCRIPT_ABORT);
        if (abort == 2) {
            /* Endgame success already printed PTR tables; Goto Town only. */
            (void)rt.takePendingOp0eFdPrintChrome();
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCREEN_MODE_ID,
                          mm2_gs_u8(gs.a4(), MM2_GS_SAVED_TOWN_ID));
            gs.setScreenId(mm2_gs_u8(gs.a4(), MM2_GS_SAVED_TOWN_ID));
            mm2_gs_set_u16(gs.a4(), MM2_GS_OP0E_SUBMODE, 1);
            rt.armInnGotoTown();
        } else if (abort == 3) {
            /* 0x14106: addq -$7972; Death Strikes panel; -$79AC→-$79F2; 0x1A1F8.
             * Skip PTR print pages — endgame already consumed them before abort=3. */
            (void)rt.takePendingOp0eFdPrintChrome();
            const uint16_t ctr = mm2_gs_u16(gs.a4(), MM2_GS_OP0E_FD_CTR);
            mm2_gs_set_u16(gs.a4(), MM2_GS_OP0E_FD_CTR,
                           static_cast<uint16_t>(ctr < 0xFFFFu ? ctr + 1u : 0xFFFFu));
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCREEN_MODE_ID,
                          mm2_gs_u8(gs.a4(), MM2_GS_SAVED_TOWN_ID));
            gs.setScreenId(mm2_gs_u8(gs.a4(), MM2_GS_SAVED_TOWN_ID));
            rt.armDeathStrikes();
        } else {
            mm2_gs_set_u8(gs.a4(), MM2_GS_SCRIPT_ABORT, 1);
        }
        break;
    }
    default:
        /* Selectors 0x09–0x10 / 0x11–… (incl. Nordon 0x0A, Nordonna 0x0B,
         * enroll 0x0D, Feldecarb 0x0E, locksmith 0x10, Poorman's Portal 0x11)
         * are ASM default-range → 0x15EDC → loc overlay VM. No FAQ stubs. */
        /* Skill vendors (0x38 / 0x3E.. / 0x4D.. / 0x52) are default-range
         * overlay bytecode (OP_24 gold + OP_18 skill nibble) — do NOT intercept
         * with EventSkillBuy FAQ tables; that skipped OP_24 and never deducted. */
        if (eventVmIsTownServiceSelector(sel)) {
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

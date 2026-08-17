// Character sheet pending-mode input (doc 43 §6.1): R+1-6 remove equip slot;
// digits must not chain to another character while a sub-handler is active.

#include <cstdio>
#include <cstring>

#include "mm2/gameplay/InGameCharacterSheet.h"
#include "mm2/gameplay/RosterSkills.h"
#include "mm2/gameplay/SpellBook.h"
#include "mm2/ui/RosterSkillDisplay.h"
#include "mm2_create_character.h"
#include "mm2_items_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

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

void setupParty(Mm2RosterFile &roster, Mm2PartyLaunch &launch)
{
    mm2_roster_clear_record(&roster.records[0]);
    mm2_roster_clear_record(&roster.records[1]);
    mm2_roster_set_name(&roster.records[0], "Alpha");
    mm2_roster_set_name(&roster.records[1], "Beta");

    roster.records[0].equipped_id[2] = 7;
    roster.records[0].equipped_charges[2] = 0;
    roster.records[0].equipped_flags[2] = 0;

    launch.party_count = 2;
    launch.roster_slots[0] = 0;
    launch.roster_slots[1] = 1;
}

}  // namespace

int main()
{
    int fails = 0;

    expect(!mm2::gameplay::sheetSubModeBlocksCharacterSwitch(mm2::gameplay::SheetSubMode::Normal),
           "Normal mode allows digit chain", fails);
    expect(mm2::gameplay::sheetSubModeBlocksCharacterSwitch(mm2::gameplay::SheetSubMode::RemovePickEquip),
           "Remove mode blocks digit chain", fails);
    expect(mm2::gameplay::sheetSubModeBlocksCharacterSwitch(mm2::gameplay::SheetSubMode::TradePickTarget),
           "Trade target mode blocks digit chain", fails);

    Mm2RosterFile roster{};
    Mm2PartyLaunch launch{};
    setupParty(roster, launch);

    mm2::gameplay::InGameCharacterSheet sheet;
    mm2::gameplay::SheetSession session{};
    session.party_slot = 0;

    sheet.handleKey('R', session, roster, launch, nullptr);
    expect(session.sub_mode == mm2::gameplay::SheetSubMode::RemovePickEquip, "R enters remove mode", fails);

    sheet.handleKey('3', session, roster, launch, nullptr);
    expect(session.sub_mode == mm2::gameplay::SheetSubMode::Normal, "slot pick exits remove mode", fails);
    expect(session.party_slot == 0, "party slot unchanged after R+3", fails);
    expect(roster.records[0].equipped_id[2] == 0, "equip slot 3 cleared", fails);
    expect(roster.records[0].backpack_id[0] == 7, "item moved to first backpack slot", fails);

    session.party_slot = 0;
    roster.records[0].equipped_id[0] = 9;
    session.sub_mode = mm2::gameplay::SheetSubMode::RemovePickEquip;
    sheet.handleKey('2', session, roster, launch, nullptr);
    expect(session.party_slot == 0, "R+2 does not switch to char 2", fails);

    roster.records[0].gold = 50;
    roster.records[1].gold = 25;
    session = {};
    session.party_slot = 0;
    sheet.handleKey('G', session, roster, launch, nullptr);
    sheet.handleKey('1', session, roster, launch, nullptr);
    expect(roster.records[0].gold == 75, "gather gold pools party", fails);
    expect(roster.records[1].gold == 0, "other member gold cleared", fails);

    /* $7F68: hireling roster index >= $18 — fee at +$66 must not be pooled. */
    mm2_roster_clear_record(&roster.records[24]);
    mm2_roster_set_name(&roster.records[24], "HireA");
    roster.records[24].gold = 55; /* daily fee */
    roster.records[0].gold = 100;
    roster.records[1].gold = 40;
    launch.party_count = 3;
    launch.roster_slots[0] = 0;
    launch.roster_slots[1] = 1;
    launch.roster_slots[2] = 24;
    session = {};
    session.party_slot = 0;
    sheet.handleKey('G', session, roster, launch, nullptr);
    sheet.handleKey('1', session, roster, launch, nullptr);
    expect(roster.records[0].gold == 140, "gather gold skips hireling fee", fails);
    expect(roster.records[1].gold == 0, "hero gold cleared by gather", fails);
    expect(roster.records[24].gold == 55, "hireling fee untouched by gather", fails);

    session = {};
    session.party_slot = 2; /* hireling initiator */
    sheet.handleKey('G', session, roster, launch, nullptr);
    sheet.handleKey('1', session, roster, launch, nullptr);
    expect(roster.records[24].gold == 55, "hireling cannot gather gold", fails);
    expect(roster.records[0].gold == 140, "heroes unchanged when hireling gathers", fails);

    launch.party_count = 2;
    launch.roster_slots[2] = -1;

    roster.records[0].gold = 40;
    roster.records[1].gold = 0;
    session = {};
    session.party_slot = 0;
    sheet.handleKey('T', session, roster, launch, nullptr);
    sheet.handleKey('1', session, roster, launch, nullptr);
    sheet.handleKey('2', session, roster, launch, nullptr);
    expect(roster.records[0].gold == 0, "trade removes source gold", fails);
    expect(roster.records[1].gold == 40, "trade adds to target", fails);

    // Trade food (T,3,target) moves source->target food ($E3C6, rec +$25 u8).
    roster.records[0].food = 10;
    roster.records[1].food = 5;
    session = {};
    session.party_slot = 0;
    sheet.handleKey('T', session, roster, launch, nullptr);
    sheet.handleKey('3', session, roster, launch, nullptr);
    expect(session.trade_kind == mm2::gameplay::SheetTradeKind::Food, "T+3 selects food kind", fails);
    sheet.handleKey('2', session, roster, launch, nullptr);
    expect(roster.records[0].food == 0, "trade removes source food", fails);
    expect(roster.records[1].food == 15, "trade adds food to target", fails);

    // Food trade rejected when target would exceed the 40-food cap ($E444 guard).
    roster.records[0].food = 30;
    roster.records[1].food = 20;
    session = {};
    session.party_slot = 0;
    sheet.handleKey('T', session, roster, launch, nullptr);
    sheet.handleKey('3', session, roster, launch, nullptr);
    sheet.handleKey('2', session, roster, launch, nullptr);
    expect(roster.records[0].food == 30, "food trade over cap leaves source", fails);
    expect(roster.records[1].food == 20, "food trade over cap leaves target", fails);

    // Trade item (T,4,target,letter) moves one backpack slot ($E492).
    roster.records[0].backpack_id[1] = 11;
    roster.records[0].backpack_charges[1] = 3;
    roster.records[0].backpack_flags[1] = 2;
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        roster.records[1].backpack_id[i] = 0;
    }
    session = {};
    session.party_slot = 0;
    sheet.handleKey('T', session, roster, launch, nullptr);
    sheet.handleKey('4', session, roster, launch, nullptr);
    expect(session.trade_kind == mm2::gameplay::SheetTradeKind::Items, "T+4 selects item kind", fails);
    sheet.handleKey('2', session, roster, launch, nullptr);
    expect(session.sub_mode == mm2::gameplay::SheetSubMode::TradePickItemSlot,
           "item trade target enters backpack pick", fails);
    sheet.handleKey('B', session, roster, launch, nullptr);
    expect(roster.records[0].backpack_id[1] == 0, "item trade clears source slot", fails);
    expect(roster.records[1].backpack_id[0] == 11, "item trade moves id to target", fails);
    expect(roster.records[1].backpack_charges[0] == 3, "item trade moves charges", fails);
    expect(roster.records[1].backpack_flags[0] == 2, "item trade moves flags", fails);

    // Item trade rejected when the target backpack is full.
    roster.records[0].backpack_id[0] = 22;
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        roster.records[1].backpack_id[i] = 50 + i;
    }
    session = {};
    session.party_slot = 0;
    sheet.handleKey('T', session, roster, launch, nullptr);
    sheet.handleKey('4', session, roster, launch, nullptr);
    sheet.handleKey('2', session, roster, launch, nullptr);
    sheet.handleKey('A', session, roster, launch, nullptr);
    expect(roster.records[0].backpack_id[0] == 22, "item trade keeps source when target pack full", fails);

    // Item trade rejected when the source slot is empty.
    roster.records[0].backpack_id[2] = 0;
    for (int i = 0; i < MM2_ROSTER_ITEM_SLOTS; ++i) {
        roster.records[1].backpack_id[i] = 0;
    }
    session = {};
    session.party_slot = 0;
    sheet.handleKey('T', session, roster, launch, nullptr);
    sheet.handleKey('4', session, roster, launch, nullptr);
    sheet.handleKey('2', session, roster, launch, nullptr);
    sheet.handleKey('C', session, roster, launch, nullptr);
    expect(roster.records[1].backpack_id[0] == 0, "item trade from empty slot moves nothing", fails);

    // ---- Spell book (cast picker grid @ 0x65fa) ----------------------------
    using mm2::gameplay::SpellSchool;
    using mm2::gameplay::kClericSpells;
    using mm2::gameplay::kSorcererSpells;
    using mm2::gameplay::knownSpellCount;
    using mm2::gameplay::spellKnownInBook;
    using mm2::gameplay::spellLearnInBook;
    using mm2::gameplay::spellSchoolForClass;

    // School by class (ASM OP_2E class matches): Archer/Sorcerer -> Sorcerer,
    // Paladin/Cleric -> Cleric, everyone else -> None.
    expect(spellSchoolForClass(4) == SpellSchool::Sorcerer, "class 4 (Sorcerer) -> Sorcerer school", fails);
    expect(spellSchoolForClass(2) == SpellSchool::Sorcerer, "class 2 (Archer) -> Sorcerer school", fails);
    expect(spellSchoolForClass(3) == SpellSchool::Cleric, "class 3 (Cleric) -> Cleric school", fails);
    expect(spellSchoolForClass(1) == SpellSchool::Cleric, "class 1 (Paladin) -> Cleric school", fails);
    expect(spellSchoolForClass(0) == SpellSchool::None, "class 0 (Knight) -> no school", fails);
    expect(spellSchoolForClass(5) == SpellSchool::None, "class 5 (Robber) -> no school", fails);

    // Known-spell bits live at record offset $51..$56 (spells[5..10]): spell N is
    // byte $51 + (N>>3), bit (1 << (N&7)). Confirm the exact bytes are written.
    {
        Mm2RosterRecord sorc{};
        mm2_roster_clear_record(&sorc);
        spellLearnInBook(sorc, 0);   // S1/1 Awaken
        spellLearnInBook(sorc, 8);   // S2/2 Electric Arrow
        spellLearnInBook(sorc, 47);  // S9/4 Enchant Item
        expect(sorc.spells[5] == 0x01, "spell 0 sets record $51 bit 0", fails);
        expect(sorc.spells[6] == 0x01, "spell 8 sets record $52 bit 0", fails);
        expect(sorc.spells[10] == 0x80, "spell 47 sets record $56 bit 7", fails);
        expect(sorc.spells[4] == 0x00, "record $50 (class nibble) untouched by spell bits", fails);

        expect(spellKnownInBook(sorc, 0), "spell 0 reads back known", fails);
        expect(!spellKnownInBook(sorc, 1), "spell 1 reads back unknown", fails);
        expect(spellKnownInBook(sorc, 8), "spell 8 reads back known", fails);
        expect(spellKnownInBook(sorc, 47), "spell 47 reads back known", fails);
        expect(knownSpellCount(sorc, SpellSchool::Sorcerer) == 3, "known spell count == 3", fails);

        // Decoded bits map to the faithful Sorcerer names/levels.
        expect(std::strcmp(kSorcererSpells[0].name, "Awaken") == 0 && kSorcererSpells[0].level == 1,
               "Sorcerer 0 = Awaken (L1)", fails);
        expect(std::strcmp(kSorcererSpells[8].name, "Electric Arrow") == 0 && kSorcererSpells[8].level == 2,
               "Sorcerer 8 = Electric Arrow (L2)", fails);
        expect(std::strcmp(kSorcererSpells[47].name, "Enchant Item") == 0 && kSorcererSpells[47].level == 9,
               "Sorcerer 47 = Enchant Item (L9)", fails);
    }

    // Cleric school decodes to the Cleric name table (flat index 48..95 globally).
    {
        Mm2RosterRecord cler{};
        mm2_roster_clear_record(&cler);
        spellLearnInBook(cler, 3);   // C1/4 First Aid
        spellLearnInBook(cler, 47);  // C9/4 Uncurse Item
        expect(spellKnownInBook(cler, 3), "cleric spell 3 known", fails);
        expect(spellKnownInBook(cler, 47), "cleric spell 47 known", fails);
        expect(knownSpellCount(cler, SpellSchool::Cleric) == 2, "cleric known count == 2", fails);
        expect(std::strcmp(kClericSpells[0].name, "Apparition") == 0, "Cleric 0 = Apparition", fails);
        expect(std::strcmp(kClericSpells[3].name, "First Aid") == 0 && kClericSpells[3].level == 1,
               "Cleric 3 = First Aid (L1)", fails);
        expect(std::strcmp(kClericSpells[47].name, "Uncurse Item") == 0 && kClericSpells[47].level == 9,
               "Cleric 47 = Uncurse Item (L9)", fails);
    }

    // Opening the spell-book sub-mode via 'C' (exploration cast @ 0x6E30).
    {
        Mm2RosterFile sroster{};
        Mm2PartyLaunch slaunch{};
        mm2_roster_clear_record(&sroster.records[0]);
        mm2_roster_set_name(&sroster.records[0], "Mage");
        sroster.records[0].class_id = 4;  // Sorcerer
        sroster.records[0].spell_level = 3;
        mm2::gameplay::spellLearnInBook(sroster.records[0], 0); // L1/1 Awaken
        slaunch.party_count = 1;
        slaunch.roster_slots[0] = 0;

        mm2::gameplay::InGameCharacterSheet ssheet;
        mm2::gameplay::SheetSession ssession{};
        ssession.party_slot = 0;
        ssheet.handleKey('C', ssession, sroster, slaunch, nullptr);
        expect(ssession.sub_mode == mm2::gameplay::SheetSubMode::CastPicker, "caster 'C' opens cast picker", fails);
        expect(ssession.cast_phase == mm2::gameplay::CastPromptPhase::Level, "cast starts at level prompt", fails);

        ssheet.handleKey('1', ssession, sroster, slaunch, nullptr);
        expect(ssession.cast_phase == mm2::gameplay::CastPromptPhase::Number, "digit advances to number", fails);
        expect(ssession.cast_level == 1, "cast level stored", fails);

        ssheet.handleKey('1', ssession, sroster, slaunch, nullptr);
        expect(ssession.sub_mode == mm2::gameplay::SheetSubMode::Normal, "valid spell exits picker", fails);
        expect(ssession.cast_spell_flat == 0, "flat index 0 for L1/1", fails);

        // Combat sheet 'C' must not open cast UI (combat command path owns it).
        ssession = {};
        ssession.party_slot = 0;
        ssheet.handleKey('C', ssession, sroster, slaunch, nullptr, true);
        expect(ssession.sub_mode == mm2::gameplay::SheetSubMode::Normal, "combat sheet 'C' ignored", fails);

        // Combat sheet 'V' opens view-only SpellBook.
        ssheet.handleKey('V', ssession, sroster, slaunch, nullptr, true);
        expect(ssession.sub_mode == mm2::gameplay::SheetSubMode::SpellBook, "combat 'V' opens spell book", fails);
        ssheet.handleKey('D', ssession, sroster, slaunch, nullptr, true);
        expect(ssession.sub_mode == mm2::gameplay::SheetSubMode::SpellBook, "spell book swallows other keys",
               fails);

        // Non-caster: 'C' shows the empty case and does not enter the sub-mode.
        sroster.records[0].class_id = 0;  // Knight
        ssession = {};
        ssession.party_slot = 0;
        ssheet.handleKey('C', ssession, sroster, slaunch, nullptr);
        expect(ssession.sub_mode == mm2::gameplay::SheetSubMode::Normal, "non-caster 'C' stays normal", fails);
    }

    /* Equip @ 0xF36C / unequip @ 0xF270: weapon dice, armor +$1F→+$24, special power. */
    {
        Mm2ItemsFile items{};
        items.records[1].damage = 6;          /* melee die */
        items.records[1].bonus_byte = 0x03;   /* Might +3 */
        items.records[0x73].damage = 4;       /* armor AC */
        items.records[0x73].bonus_byte = 0x00;

        Mm2RosterFile eroster{};
        Mm2PartyLaunch elaunch{};
        mm2_roster_clear_record(&eroster.records[0]);
        mm2_roster_set_name(&eroster.records[0], "Equip");
        eroster.records[0].class_id = 0;
        eroster.records[0].speed_current = 1; /* -$7F56 → $FD ≥ $F0 → speed AC 0 */
        eroster.records[0].might_current = 15;
        eroster.records[0].might_base = 15;
        eroster.records[0].backpack_id[0] = 1;
        eroster.records[0].backpack_flags[0] = 2; /* +2 instance bonus */
        eroster.records[0].backpack_id[1] = 0x73;
        elaunch.party_count = 1;
        elaunch.roster_slots[0] = 0;

        mm2::gameplay::InGameCharacterSheet esheet;
        mm2::gameplay::SheetSession esession{};
        esession.party_slot = 0;

        esheet.handleKey('E', esession, eroster, elaunch, &items);
        esheet.handleKey('A', esession, eroster, elaunch, &items);
        expect(eroster.records[0].equipped_id[0] == 1, "club moved to equip", fails);
        expect(eroster.records[0].spells[0] == 6, "melee die from items.dat $10", fails);
        expect(eroster.records[0].spells[1] == 2, "melee bonus from flags&0x3F", fails);
        expect(eroster.records[0].might_current == 20, "Might +3 plus flags +2", fails);
        expect(eroster.records[0].might_base == 20, "base Might also boosted (type<=5)", fails);

        esheet.handleKey('E', esession, eroster, elaunch, &items);
        esheet.handleKey('B', esession, eroster, elaunch, &items);
        expect(eroster.records[0].equipped_id[1] == 0x73, "shield moved to equip", fails);
        expect(eroster.records[0].unknown_1a_20[5] == 4, "equipment AC accumulator +$1F", fails);
        expect(eroster.records[0].armor_class == 4, "displayed AC +$24 from +$1F", fails);

        esheet.handleKey('R', esession, eroster, elaunch, &items);
        esheet.handleKey('1', esession, eroster, elaunch, &items);
        expect(eroster.records[0].spells[0] == 0, "melee fields cleared after unequip", fails);
        expect(eroster.records[0].might_current == 15, "Might restored on unequip", fails);
        expect(eroster.records[0].might_base == 15, "base Might restored on unequip", fails);
        expect(eroster.records[0].armor_class == 4, "shield AC remains after weapon remove", fails);

        esheet.handleKey('R', esession, eroster, elaunch, &items);
        esheet.handleKey('2', esession, eroster, elaunch, &items);
        expect(eroster.records[0].unknown_1a_20[5] == 0, "+$1F cleared after shield remove", fails);
        expect(eroster.records[0].armor_class == 0, "displayed AC cleared after shield remove", fails);
    }

    /* Occupancy @ 0xEC02: one melee, missile, shield, armor, helm; 2H vs shield. */
    {
        Mm2ItemsFile items{};
        Mm2RosterFile oroster{};
        Mm2PartyLaunch olaunch{};
        mm2_roster_clear_record(&oroster.records[0]);
        mm2_roster_set_name(&oroster.records[0], "Slots");
        oroster.records[0].class_id = 0;
        oroster.records[0].equipped_id[0] = 1;    /* 1H club */
        oroster.records[0].backpack_id[0] = 0x0C; /* 1H sword */
        oroster.records[0].backpack_id[1] = 0x42; /* 2H staff */
        oroster.records[0].backpack_id[2] = 0x5E; /* short bow */
        oroster.records[0].backpack_id[3] = 0x73; /* small shield */
        oroster.records[0].backpack_id[4] = 0x7F; /* padded armor */
        oroster.records[0].backpack_id[5] = 0x7F; /* second armor */
        olaunch.party_count = 1;
        olaunch.roster_slots[0] = 0;

        mm2::gameplay::InGameCharacterSheet osheet;
        mm2::gameplay::SheetSession osession{};
        osession.party_slot = 0;

        osheet.handleKey('E', osession, oroster, olaunch, &items);
        osheet.handleKey('A', osession, oroster, olaunch, &items);
        expect(oroster.records[0].backpack_id[0] == 0x0C, "second melee stays in pack", fails);
        expect(std::strcmp(osession.status_line, "Already have weapon") == 0, "1H vs 1H blocked", fails);

        osheet.handleKey('E', osession, oroster, olaunch, &items);
        osheet.handleKey('B', osession, oroster, olaunch, &items);
        expect(oroster.records[0].backpack_id[1] == 0x42, "2H stays in pack with 1H on", fails);
        expect(std::strcmp(osession.status_line, "Already have weapon") == 0, "1H vs 2H blocked", fails);

        osheet.handleKey('E', osession, oroster, olaunch, &items);
        osheet.handleKey('C', osession, oroster, olaunch, &items);
        expect(oroster.records[0].equipped_id[1] == 0x5E, "missile allowed with melee", fails);

        osheet.handleKey('E', osession, oroster, olaunch, &items);
        osheet.handleKey('D', osession, oroster, olaunch, &items);
        expect(oroster.records[0].equipped_id[2] == 0x73, "shield allowed with 1H", fails);

        osheet.handleKey('E', osession, oroster, olaunch, &items);
        osheet.handleKey('E', osession, oroster, olaunch, &items);
        expect(oroster.records[0].equipped_id[3] == 0x7F, "first armor equipped", fails);

        osheet.handleKey('E', osession, oroster, olaunch, &items);
        osheet.handleKey('F', osession, oroster, olaunch, &items);
        expect(oroster.records[0].backpack_id[5] == 0x7F, "second armor stays in pack", fails);
        expect(std::strcmp(osession.status_line, "Already wearing armor") == 0, "two armors blocked",
               fails);

        /* 2H vs shield: clear melee, leave shield, try staff. */
        oroster.records[0].equipped_id[0] = 0;
        oroster.records[0].backpack_id[1] = 0x42;
        osheet.handleKey('E', osession, oroster, olaunch, &items);
        osheet.handleKey('B', osession, oroster, olaunch, &items);
        expect(oroster.records[0].backpack_id[1] == 0x42, "2H stays in pack with shield", fails);
        expect(std::strcmp(osession.status_line, "Not with shield") == 0, "2H vs shield blocked", fails);
    }

    /* Thievery: +$1E is the persistent skill; type-14 gear is added live and
     * must not mutate +$1E (pre-worn Hermit kit was dropping below 30 on Remove). */
    {
        Mm2ItemsFile items{};
        items.records[0xA3].bonus_byte = 0xEF; /* Thief's Pick: type 14 amount 15 */
        items.records[0xD5].bonus_byte = 0xE5; /* Castle Key: type 14 amount 5 */

        Mm2RosterFile troster{};
        Mm2PartyLaunch tlaunch{};
        mm2_roster_clear_record(&troster.records[0]);
        mm2_roster_set_name(&troster.records[0], "Hermit");
        troster.records[0].class_id = 5;
        troster.records[0].unknown_1a_20[4] = 30;
        troster.records[0].equipped_id[0] = 0xD5;
        troster.records[0].equipped_id[1] = 0xA3;
        troster.records[0].equipped_flags[1] = 4;
        troster.records[0].equipped_id[2] = 0xA3;
        troster.records[0].equipped_flags[2] = 4;
        tlaunch.party_count = 1;
        tlaunch.roster_slots[0] = 0;

        expect(mm2::ui::rosterDisplayThievery(troster.records[0], &items) == 73,
               "live thievery 30+5+19+19", fails);
        expect(troster.records[0].unknown_1a_20[4] == 30, "base +$1E unchanged with gear on", fails);

        mm2::gameplay::InGameCharacterSheet tsheet;
        mm2::gameplay::SheetSession tsession{};
        tsession.party_slot = 0;
        tsheet.handleKey('R', tsession, troster, tlaunch, &items);
        tsheet.handleKey('1', tsession, troster, tlaunch, &items);
        expect(troster.records[0].unknown_1a_20[4] == 30, "removing Castle Key keeps +$1E at 30", fails);
        expect(mm2::ui::rosterDisplayThievery(troster.records[0], &items) == 68,
               "live 30+19+19 after key remove", fails);

        tsheet.handleKey('R', tsession, troster, tlaunch, &items);
        tsheet.handleKey('2', tsession, troster, tlaunch, &items);
        tsheet.handleKey('R', tsession, troster, tlaunch, &items);
        tsheet.handleKey('3', tsession, troster, tlaunch, &items);
        expect(troster.records[0].unknown_1a_20[4] == 30, "naked robber still 30", fails);
        expect(mm2::ui::rosterDisplayThievery(troster.records[0], &items) == 30,
               "live thievery returns to class start", fails);

        Mm2PendingCharacter pending{};
        mm2_create_pending_init(&pending);
        pending.class_id = 5;
        pending.race = 1;
        pending.alignment = 1;
        pending.sex = 0;
        pending.modified.might = 15;
        pending.modified.intelligence = 15;
        pending.modified.personality = 15;
        pending.modified.endurance = 15;
        pending.modified.speed = 15;
        pending.modified.accuracy = 15;
        pending.modified.luck = 15;
        Mm2RosterRecord created{};
        mm2_create_build_record(&pending, &created);
        expect(created.unknown_1a_20[4] == 30, "create robber writes 30 to +$1E", fails);
        expect(created.thievery_percent == 0, "create does not write thievery to +$16", fails);
        pending.class_id = 6;
        mm2_create_build_record(&pending, &created);
        expect(created.unknown_1a_20[4] == 10, "create ninja writes 10 to +$1E", fails);
        pending.class_id = 0;
        mm2_create_build_record(&pending, &created);
        expect(created.unknown_1a_20[4] == 0, "create knight writes 0 to +$1E", fails);
    }

    if (fails == 0) {
        std::printf("OK: character_sheet_input_test\n");
        return 0;
    }
    return 1;
}

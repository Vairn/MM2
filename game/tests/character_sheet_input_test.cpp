// Character sheet pending-mode input (doc 43 §6.1): R+1-6 remove equip slot;
// digits must not chain to another character while a sub-handler is active.

#include <cstdio>
#include <cstring>

#include "mm2/gameplay/InGameCharacterSheet.h"
#include "mm2/gameplay/SpellBook.h"
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

    if (fails == 0) {
        std::printf("OK: character_sheet_input_test (58 checks)\n");
        return 0;
    }
    return 1;
}

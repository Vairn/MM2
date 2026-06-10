// Character sheet pending-mode input (doc 43 §6.1): R+1-6 remove equip slot;
// digits must not chain to another character while a sub-handler is active.

#include <cstdio>
#include <cstring>

#include "mm2/gameplay/InGameCharacterSheet.h"
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

    roster.records[0].equipped[2].item_id = 7;
    roster.records[0].equipped[2].bonus = 0;
    roster.records[0].equipped[2].flags = 0;

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
    expect(roster.records[0].equipped[2].item_id == 0, "equip slot 3 cleared", fails);
    expect(roster.records[0].backpack[0].item_id == 7, "item moved to first backpack slot", fails);

    session.party_slot = 0;
    roster.records[0].equipped[0].item_id = 9;
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

    if (fails == 0) {
        std::printf("OK: character_sheet_input_test (12 checks)\n");
        return 0;
    }
    return 1;
}

// CombatSession test: encounter setup (0x12C6E), round loop (0x12A22),
// victory (0x12430) / defeat (0x11646) transitions, arena gold reward, and
// the random picker (0x1213E/0x12072/0x11F0A) invariants.
//
// No data-dir argument needed: monsters/roster/attrib are synthesized in
// memory so the test is deterministic and self-contained.

#include <cstdio>
#include <cstring>

#include "mm2/GameState.h"
#include "mm2/combat/CombatSession.h"
#include "mm2/combat/EncounterPicker.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/gameplay/SpellBook.h"
#include "mm2/world/MapWorld.h"

#include "mm2_monsters_codec.h"
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

void setMonsterField(Mm2MonsterRecord &rec, int off, uint8_t value)
{
    rec.fields[off - MM2_MONSTER_NAME_SIZE] = value;
}

void setupParty(Mm2RosterFile &roster, Mm2PartyLaunch &launch, uint8_t might, uint8_t speed,
                 uint16_t hp)
{
    std::memset(&roster, 0, sizeof(roster));
    mm2_roster_set_name(&roster.records[0], "Tester");
    roster.records[0].might_current = might;
    roster.records[0].speed_current = speed;
    roster.records[0].hp_current = hp;

    launch = Mm2PartyLaunch{};
    launch.party_count = 1;
    launch.roster_slots[0] = 0;
}

void setupTwoMemberParty(Mm2RosterFile &roster, Mm2PartyLaunch &launch)
{
    std::memset(&roster, 0, sizeof(roster));
    mm2_roster_set_name(&roster.records[0], "Fighter");
    mm2_roster_set_name(&roster.records[1], "Cleric");
    roster.records[0].might_current = 99;
    roster.records[0].speed_current = 99;
    roster.records[0].hp_current = 999;
    roster.records[0].condition = 0;
    roster.records[1].might_current = 1;
    roster.records[1].speed_current = 1;
    roster.records[1].hp_current = 0; /* unconscious */
    roster.records[1].condition = 0;

    launch = Mm2PartyLaunch{};
    launch.party_count = 2;
    launch.roster_slots[0] = 0;
    launch.roster_slots[1] = 1;
}

/* Seeds a fixed (OP_12-style, mode=0x80) single-monster encounter directly
 * into A4 — mirrors eventRunFixedEncounter's own field writes. */
void seedFixedEncounter(mm2::GameStateView &gs, uint8_t monster_type)
{
    uint8_t *a4 = gs.a4();
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0x80);
    mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS, monster_type);
    for (int i = 1; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);
    }
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 1);
}

/* Drive combat with repeated key presses until it ends or we stall. */
bool fightToEnd(mm2::combat::CombatSession &combat, mm2::GameStateView &gs, const mm2::world::MapWorld &world,
                char key, int max_ticks = 64)
{
    for (int i = 0; i < max_ticks; ++i) {
        mm2::platform::KeyState keys{};
        keys.last_ascii = key;
        if (combat.tick(gs, world, keys)) {
            return true;
        }
        if (!combat.active()) {
            return true;
        }
    }
    return !combat.active();
}

}  // namespace

int main()
{
    using namespace mm2;
    using namespace mm2::combat;

    int fails = 0;

    static uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
    GameStateView gs(mm2_gs_base_from_image(gs_image));

    world::MapWorld world; /* default-constructed: zeroed attrib, unused by
                             * the mode=0x80 (fixed) fights below. */

    Mm2MonstersFile monsters{};
    Mm2RosterFile roster{};
    Mm2PartyLaunch launch{};
    gameplay::Rng rng(1);

    /* ---- Victory: a 1-HP monster always dies to the party's first Attack. */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[7];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x00);     /* (0+1)*1 = 1 HP */
        setMonsterField(mon, MM2_MON_OFF_XP, 0x2E);     /* (14+1)*10 = 150 XP */
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);  /* speed = 1 */
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x00); /* max dmg = 1 (unused, dies first) */

        setupParty(roster, launch, /*might=*/99, /*speed=*/99, /*hp=*/999);
        seedFixedEncounter(gs, 7);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.enter(gs, world);

        expect(combat.active(), "victory scenario: combat active after enter()", fails);
        expect(combat.awaitingPartyOptions(), "victory scenario: party options before fight", fails);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        expect(combat.awaitingCommand(), "victory scenario: party prompted first (higher speed)", fails);

        const bool ended = fightToEnd(combat, gs, world, 'A');
        expect(ended, "victory scenario: Attack kills the 1-HP monster", fails);
        expect(combat.lastOutcome() == CombatOutcome::Victory, "victory scenario: outcome == Victory", fails);
        expect(!combat.active(), "victory scenario: combat inactive after victory", fails);
        expect(roster.records[0].experience == 150, "victory scenario: 150 XP credited (sole survivor)",
               fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 1,
               "victory scenario: COMBAT_VICTORY_LATCH set (OP_2B gate)", fails);
    }

    /* ---- XP split @ 0x12430: unconscious (0 HP, condition < $80) gets a share. */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[9];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x00);
        setMonsterField(mon, MM2_MON_OFF_XP, 0x2E); /* 150 XP */
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x00);

        setupTwoMemberParty(roster, launch);
        seedFixedEncounter(gs, 9);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.enter(gs, world);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        const bool ended = fightToEnd(combat, gs, world, 'A');
        expect(ended, "xp split: fight ends on kill", fails);
        expect(combat.lastOutcome() == CombatOutcome::Victory, "xp split: victory", fails);
        expect(roster.records[0].experience == 75, "xp split: conscious member gets half", fails);
        expect(roster.records[1].experience == 75,
               "xp split: unconscious member gets half (condition < 0x80)", fails);
    }

    /* ---- XP split: dead/stoned (condition >= $80) excluded from share. */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[9];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x00);
        setMonsterField(mon, MM2_MON_OFF_XP, 0x2E); /* 150 XP */
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x00);

        setupTwoMemberParty(roster, launch);
        roster.records[1].condition = 0x81; /* dead — 0x12430 skips */
        seedFixedEncounter(gs, 9);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.enter(gs, world);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        fightToEnd(combat, gs, world, 'A');
        expect(roster.records[0].experience == 150,
               "xp split: sole eligible member receives full pool", fails);
        expect(roster.records[1].experience == 0, "xp split: dead member receives nothing", fails);
    }

    /* ---- Arena reward: victory also grants the color/screen gold table. */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[3];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x00);
        setMonsterField(mon, MM2_MON_OFF_XP, 0x00);
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x00);

        setupParty(roster, launch, /*might=*/99, /*speed=*/99, /*hp=*/999);
        seedFixedEncounter(gs, 3);
        roster.records[0].gold = 100;

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.armArenaReward(/*color=*/2 /* red */, /*screen=*/0 /* Middlegate */);
        combat.enter(gs, world);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        const bool ended = fightToEnd(combat, gs, world, 'A');
        expect(ended, "arena scenario: fight resolves", fails);
        expect(combat.lastOutcome() == CombatOutcome::Victory, "arena scenario: outcome == Victory", fails);
        const uint32_t expected_gold = events::eventVmArenaGoldReward(2, 0);
        expect(roster.records[0].gold == 100 + expected_gold,
               "arena scenario: gold reward credited to the winner", fails);
    }

    /* ---- Defeat: a fast, hard-hitting monster drops a 1-HP party before its turn. */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[1];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x3F);     /* (63+1)*1 = 64 HP: survives */
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x0F);  /* speed = 16: acts before the party */
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x00); /* max dmg = 1: enough to fell 1 HP */

        setupParty(roster, launch, /*might=*/1, /*speed=*/1, /*hp=*/1);
        seedFixedEncounter(gs, 1);
        /* 0x1164A: wipe restores from -$560C (attrib entry_coord via 0x923E). */
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENTRY_COORD, 0x57); /* Middlegate (7,5) */
        gs.setCoordX(3);
        gs.setCoordY(9);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.enter(gs, world);
        expect(combat.awaitingPartyOptions(), "defeat scenario: party options before fight", fails);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        fightToEnd(combat, gs, world, 'A');

        expect(!combat.active(), "defeat scenario: combat ends after choosing Attack", fails);
        expect(combat.lastOutcome() == CombatOutcome::Defeated, "defeat scenario: outcome == Defeated", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 0,
               "defeat scenario: COMBAT_VICTORY_LATCH stays clear", fails);
        expect(gs.coordX() == 7 && gs.coordY() == 5,
               "defeat scenario: wipe restores entry_coord (7,5)", fails);
    }

    /* ---- Run: a zero-difficulty screen (attrib byte 0x0D == 0) always flees. */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[5];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x3F); /* survives — party never attacks it */
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);

        setupParty(roster, launch, /*might=*/1, /*speed=*/99, /*hp=*/999);
        seedFixedEncounter(gs, 5);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.enter(gs, world);
        expect(combat.awaitingPartyOptions(), "run scenario: party options before fight", fails);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        expect(combat.awaitingCommand(), "run scenario: party prompted (higher speed)", fails);

        keys.last_ascii = 'R';
        const bool ended = combat.tick(gs, world, keys);
        expect(ended, "run scenario: Run succeeds against 0 retreat difficulty", fails);
        expect(combat.lastOutcome() == CombatOutcome::Fled, "run scenario: outcome == Fled", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 0,
               "run scenario: COMBAT_VICTORY_LATCH stays clear on flee", fails);
    }

    /* ---- Random picker (0x1213E/0x12072/0x11F0A) invariants, exercised
     * directly (no CombatSession/monsters.dat needed): a positive XP budget
     * and a non-trivial group-size gate must yield at least one monster
     * within the attrib min/max tier bounds, and terminate (PICKER_DONE). */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        mm2_gs_set_u8(gs.a4(), MM2_GS_DISPOSITION, 2); /* default: budget = totalHP/8 */

        Mm2RosterFile pickerRoster{};
        Mm2PartyLaunch pickerLaunch{};
        setupParty(pickerRoster, pickerLaunch, /*might=*/1, /*speed=*/1, /*hp=*/800);

        Mm2AttribRecord attrib{};
        attrib.raw[0x0A] = 50; /* group-size gate: generous */
        attrib.raw[0x0B] = 5;  /* max tier */
        attrib.raw[0x0C] = 1;  /* min tier */

        encounterInitXpBudget(gs, pickerRoster, pickerLaunch);
        expect(mm2_gs_u32(gs.a4(), MM2_GS_PARTY_XP_BUDGET) == 800 / 8,
               "picker: xp budget = total party HP / 8 (disposition 2)", fails);

        encounterRunRandomPicker(gs, attrib, rng);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_PICKER_DONE) == 1, "picker: terminates (PICKER_DONE set)", fails);
        const uint8_t live = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT);
        expect(live >= 1, "picker: adds at least one monster from a positive budget", fails);
        if (live >= 1) {
            const uint8_t type = mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS);
            const uint8_t tier = static_cast<uint8_t>((type >> 4) + 1);
            expect(tier >= attrib.raw[0x0C] && tier <= attrib.raw[0x0B],
                   "picker: picked tier within attrib [min,max]", fails);
        }
    }

    /* ---- Zero XP budget must not arm a 0-monster fight (stock inn starters
     * ship with hp_current=0; without the leave-inn wake that yields budget 0). */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        mm2_gs_set_u8(gs.a4(), MM2_GS_DISPOSITION, 2);
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE, 0);
        for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + i, 0);
        }
        mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_COUNT, 0);

        Mm2RosterFile zeroRoster{};
        Mm2PartyLaunch zeroLaunch{};
        setupParty(zeroRoster, zeroLaunch, /*might=*/50, /*speed=*/50, /*hp=*/0);
        zeroRoster.records[0].hp_current = 0;
        zeroRoster.records[0].hp_max = 48;
        zeroRoster.records[0].hp_aux = 48;

        Mm2MonstersFile monsters{};
        CombatSession combat;
        combat.bindParty(&zeroRoster, &zeroLaunch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);

        mm2::world::MapWorld world;
        if (world.load("../..") && world.enterScreen(0)) {
            expect(!combat.enter(gs, world),
                   "zero-budget random fight: CombatSession::enter refuses empty fight", fails);
            expect(!combat.active(), "zero-budget random fight: combat stays inactive", fails);
        }
    }

    /* ---- Town screens: no random step fights (doc 35; env type $11). */
    {
        mm2::world::MapWorld townWorld;
        if (townWorld.load("../..") && townWorld.enterScreen(0)) {
            std::memset(&gs_image, 0, sizeof(gs_image));
            mm2::GameStateView townGs(mm2_gs_base_from_image(gs_image));
            encounterSyncScreenContext(townGs, townWorld);
            gameplay::Rng townRng(1);
            expect(!encounterTryStepRandom(townGs, townWorld, townRng),
                   "town interior: random step encounter blocked", fails);
            if (townWorld.enterScreen(7)) {
                encounterSyncScreenContext(townGs, townWorld);
                expect(mm2_gs_u8(townGs.a4(), MM2_GS_RUNTIME_ENV) == 0x04,
                       "town gate: runtime env is $04 not wilderness $0A", fails);
            }
        }
    }

    /* ---- AGA multi-monster gallery: panelView groups alive slots by picture id. */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        /* Types 1..5 with pictures 10,10,20,30,40 — expect 4 distinct slots, pic10 stack=2. */
        setMonsterField(monsters.records[1], MM2_MON_OFF_HP, 0x0F);
        setMonsterField(monsters.records[1], MM2_MON_OFF_PICTURE, 10);
        setMonsterField(monsters.records[2], MM2_MON_OFF_HP, 0x0F);
        setMonsterField(monsters.records[2], MM2_MON_OFF_PICTURE, 10);
        setMonsterField(monsters.records[3], MM2_MON_OFF_HP, 0x0F);
        setMonsterField(monsters.records[3], MM2_MON_OFF_PICTURE, 20);
        setMonsterField(monsters.records[4], MM2_MON_OFF_HP, 0x0F);
        setMonsterField(monsters.records[4], MM2_MON_OFF_PICTURE, 30);
        setMonsterField(monsters.records[5], MM2_MON_OFF_HP, 0x0F);
        setMonsterField(monsters.records[5], MM2_MON_OFF_PICTURE, 40);
        setMonsterField(monsters.records[6], MM2_MON_OFF_HP, 0x0F);
        setMonsterField(monsters.records[6], MM2_MON_OFF_PICTURE, 50); /* 5th distinct — capped out */

        setupParty(roster, launch, /*might=*/99, /*speed=*/99, /*hp=*/999);
        uint8_t *a4 = gs.a4();
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0x80);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 0, 1);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 1, 2);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 2, 3);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 3, 4);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 4, 5);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 5, 6);
        for (int i = 6; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
            mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);
        }
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 6);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        expect(combat.enter(gs, world), "sprite gallery: enter multi-type fight", fails);

        const gfx::CombatPanelView view = combat.panelView();
        expect(view.sprite_disk_index == 10, "sprite gallery: first picture is 10", fails);
        expect(view.sprite_slot_count == gfx::kAgaCombatSpriteCap,
               "sprite gallery: capped at kAgaCombatSpriteCap distinct pictures", fails);
        expect(view.sprite_slots[0].disk_index == 10 && view.sprite_slots[0].stack_count == 2,
               "sprite gallery: pic 10 stacks to 2", fails);
        expect(view.sprite_slots[1].disk_index == 20 && view.sprite_slots[1].stack_count == 1,
               "sprite gallery: pic 20 alone", fails);
        expect(view.sprite_slots[2].disk_index == 30, "sprite gallery: pic 30 present", fails);
        expect(view.sprite_slots[3].disk_index == 40, "sprite gallery: pic 40 present", fails);
        /* picture 50 is the 5th distinct id — dropped by the AGA cap. */
        bool has50 = false;
        for (int i = 0; i < view.sprite_slot_count; ++i) {
            if (view.sprite_slots[i].disk_index == 50) {
                has50 = true;
            }
        }
        expect(!has50, "sprite gallery: 5th distinct picture dropped by cap", fails);
    }

    /* Combat cast @ 0x11A90 / 0x79EE: level+number on message band, no spell grid. */
    {
        Mm2RosterFile roster{};
        Mm2PartyLaunch launch{};
        Mm2MonstersFile monsters{};
        std::memset(&monsters, 0, sizeof(monsters));
        std::memcpy(monsters.records[1].name, "Rat", 3);
        setMonsterField(monsters.records[1], MM2_MON_OFF_HP, 0x0F);
        setMonsterField(monsters.records[1], MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(monsters.records[1], MM2_MON_OFF_DAMAGE, 0x01);

        setupParty(roster, launch, /*might=*/50, /*speed=*/99, /*hp=*/100);
        roster.records[0].class_id = 4; /* Sorcerer */
        roster.records[0].spell_level = 2;
        roster.records[0].sp_current = 20;
        mm2::gameplay::spellLearnInBook(roster.records[0], 0); /* L1/1 Awaken */

        uint8_t *a4 = gs.a4();
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0x80);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 0, 1);
        for (int i = 1; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
            mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);
        }
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 1);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        expect(combat.enter(gs, world), "cast: enter fight", fails);

        /* Skip surprise / party options into a command turn. */
        platform::KeyState keys{};
        keys.last_ascii = ' ';
        combat.tick(gs, world, keys);
        if (combat.state() == CombatState::AwaitingPartyOptions) {
            keys.last_ascii = 'A';
            combat.tick(gs, world, keys);
        }
        /* Drain action acks until a party command. */
        for (int i = 0; i < 32 && combat.active() && combat.state() != CombatState::AwaitingCommand; ++i) {
            keys.last_ascii = ' ';
            combat.tick(gs, world, keys);
        }
        expect(combat.state() == CombatState::AwaitingCommand, "cast: reached command turn", fails);
        expect(combat.panelView().opt_cast, "cast: opt_cast set for sorcerer", fails);

        keys.last_ascii = 'C';
        combat.tick(gs, world, keys);
        expect(combat.state() == CombatState::AwaitingCastLevel, "cast: C opens level prompt", fails);
        expect(combat.panelView().show_cast_level, "cast: panel shows Spell Level", fails);
        expect(!combat.panelView().show_command_options, "cast: command grid hidden during pick", fails);

        keys.last_ascii = '1';
        combat.tick(gs, world, keys);
        expect(combat.state() == CombatState::AwaitingCastNumber, "cast: level digit -> number", fails);
        expect(combat.panelView().show_cast_number && combat.panelView().cast_level == 1,
               "cast: panel shows Number after level", fails);

        keys.last_ascii = '1';
        combat.tick(gs, world, keys);
        expect(combat.state() == CombatState::AwaitingActionAck, "cast: number completes cast", fails);
        expect(std::strstr(combat.statusLine(), "Awaken") != nullptr, "cast: status names spell", fails);
    }

    /* ---- Seeded-random picker: different seeds → different type picks ---- */
    {
        Mm2AttribRecord attrib{};
        attrib.raw[0x0B] = 8;  /* max_monsters */
        attrib.raw[0x0C] = 1;  /* min_monsters */
        attrib.raw[0x0A] = 20; /* group_size_gate (high so picker can add) */

        auto pickType = [&](uint32_t seed) -> uint8_t {
            std::memset(&gs_image, 0, sizeof(gs_image));
            mm2_gs_set_u8(gs.a4(), MM2_GS_ENCOUNTER_MODE, 0); /* seeded-random */
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_SLOTS, 1);   /* seed slot 0 */
            mm2_gs_set_u8(gs.a4(), MM2_GS_MONSTER_COUNT, 1);
            mm2_gs_set_u32(gs.a4(), MM2_GS_PARTY_XP_BUDGET, 50000);
            mm2_gs_set_u8(gs.a4(), MM2_GS_PICKER_TIER_MOD, 2);
            mm2_gs_set_u8(gs.a4(), MM2_GS_PICKER_DONE, 0);
            mm2_gs_set_u8(gs.a4(), MM2_GS_DISPOSITION, 2);

            gameplay::Rng local(seed);
            encounterAddsFriends(gs, attrib, local, nullptr, nullptr);
            /* Adds into slots starting at live_count (we seeded count=1 → slot[1]). */
            return mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_SLOTS + 1);
        };

        const uint8_t t1 = pickType(1);
        const uint8_t t2 = pickType(99991);
        expect(t1 != t2, "encounterAddsFriends differs across seeds (monsters not always same)", fails);
    }

    if (fails == 0) {
        std::printf("OK: combat_setup_test\n");
        return 0;
    }
    return 1;
}

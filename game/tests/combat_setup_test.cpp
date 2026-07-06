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

        keys.last_ascii = 'A';
        const bool ended = combat.tick(gs, world, keys);
        expect(ended, "victory scenario: Attack kills the 1-HP monster this tick", fails);
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
        keys.last_ascii = 'A';
        const bool ended = combat.tick(gs, world, keys);
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
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
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
        keys.last_ascii = 'A';
        const bool ended = combat.tick(gs, world, keys);
        expect(ended, "arena scenario: fight resolves this tick", fails);
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

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.enter(gs, world);
        expect(combat.awaitingPartyOptions(), "defeat scenario: party options before fight", fails);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys); /* monster acts first and ends the fight synchronously */

        expect(!combat.active(), "defeat scenario: combat ends after choosing Attack", fails);
        expect(combat.lastOutcome() == CombatOutcome::Defeated, "defeat scenario: outcome == Defeated", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 0,
               "defeat scenario: COMBAT_VICTORY_LATCH stays clear", fails);
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

    if (fails == 0) {
        std::printf("OK: combat_setup_test\n");
        return 0;
    }
    return 1;
}

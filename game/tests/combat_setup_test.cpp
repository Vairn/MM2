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
#include "mm2/gameplay/Movement.h"
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
    roster.records[0].hp_current = hp; /* +$74 ceiling (XP budget) */
    roster.records[0].hp_max = hp;     /* +$5E working HP (combat damage) */

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
    roster.records[0].hp_max = 999;
    roster.records[0].condition = 0;
    roster.records[1].might_current = 1;
    roster.records[1].speed_current = 1;
    roster.records[1].hp_current = 0; /* unconscious — no ceiling */
    roster.records[1].hp_max = 0;     /* no working HP */
    roster.records[1].condition = 0;

    launch = Mm2PartyLaunch{};
    launch.party_count = 2;
    launch.roster_slots[0] = 0;
    launch.roster_slots[1] = 1;
}

/* Seeds OP_12 pack: 10 listed slots + overflow_type + extras (tail2).
 * 0x12CE0 total = #nonzero slots + extras when overflow_type != 0. */
void seedOverflowEncounter(mm2::GameStateView &gs, uint8_t monster_type, uint8_t overflow_extras)
{
    uint8_t *a4 = gs.a4();
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0x80);
    for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, monster_type);
    }
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, monster_type);
    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, overflow_extras);
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

/* Drive Full Auto until fight ends (surprise/options/ack/victory synth). */
bool autoFightToEnd(mm2::combat::CombatSession &combat, mm2::GameStateView &gs,
                    const mm2::world::MapWorld &world, int max_ticks = 128)
{
    using mm2::combat::CombatState;
    combat.setAutoEnabled(true);
    for (int i = 0; i < max_ticks; ++i) {
        if (!combat.active()) {
            return true;
        }
        bool ended = false;
        const CombatState st = combat.state();
        if (st == CombatState::AwaitingCommand) {
            ended = combat.runAutoCommand(gs);
        } else if (st == CombatState::AwaitingCastTarget || st == CombatState::AwaitingPartyPick ||
                   st == CombatState::AwaitingAttackTarget) {
            ended = combat.runAutoPicker(gs);
        } else if (st == CombatState::AwaitingPartyOptions) {
            mm2::platform::KeyState keys{};
            keys.last_ascii = 'A';
            ended = combat.tick(gs, world, keys);
        } else {
            mm2::platform::KeyState keys{};
            keys.last_ascii = ' ';
            keys.space = true;
            keys.any_key = true;
            ended = combat.tick(gs, world, keys);
        }
        if (ended || !combat.active()) {
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

    /* Each scenario reseeds so surprise/flee rolls stay independent of how many
     * RNG draws prior fights consumed (HP/damage path changes must not cascade). */

    /* ---- Victory: a 1-HP monster always dies to the party's first Attack. */
    {
        rng.reseed(1);
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
        expect(mm2_gs_u16(gs.a4(), MM2_GS_BATTLES_WON) == 1,
               "victory scenario: battles won incremented (-$7970 / 0x1215A)", fails);
    }

    /* ---- Overflow pack: kill promotes reservoir slot 10 into A–J (0x10CCE). */
    {
        rng.reseed(1);
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[7];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x00); /* 1 HP */
        setMonsterField(mon, MM2_MON_OFF_XP, 0x10);
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x00);
        std::memcpy(mon.name, "Orc\0\0\0\0\0\0\0\0\0\0\0\0", MM2_MONSTER_NAME_SIZE);

        setupParty(roster, launch, /*might=*/99, /*speed=*/99, /*hp=*/999);
        seedOverflowEncounter(gs, 7, /*overflow_extras=*/5);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        expect(combat.enter(gs, world), "overflow: enter ok", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT) == 15,
               "overflow: 0x12CE0 total = 10 listed + 5 extras (not collapsed to 11)", fails);
        {
            const gfx::CombatPanelView v0 = combat.panelView();
            expect(v0.overflow_more == 5, "overflow: +5 more before first kill", fails);
            int occ = 0;
            for (int i = 0; i < v0.monster_line_count && i < 10; ++i) {
                if (v0.monster_lines[i].occupied) {
                    ++occ;
                }
            }
            expect(occ == 10, "overflow: 10 visible A–J rows occupied at start", fails);
        }

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys); /* encounter options → command */
        for (int i = 0; i < 32 && combat.active() && combat.state() != CombatState::AwaitingCommand; ++i) {
            keys.last_ascii = ' ';
            combat.tick(gs, world, keys);
        }
        expect(combat.state() == CombatState::AwaitingCommand, "overflow: reached command turn", fails);

        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        if (combat.state() == CombatState::AwaitingAttackTarget) {
            keys.last_ascii = 'A';
            combat.tick(gs, world, keys);
        }
        for (int i = 0; i < 16 && combat.active() &&
             combat.state() == CombatState::AwaitingActionAck; ++i) {
            keys.last_ascii = ' ';
            combat.tick(gs, world, keys);
        }

        expect(mm2_gs_u8(gs.a4(), MM2_GS_MONSTER_COUNT) == 14,
               "overflow: -$77BE decremented once after kill", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_ENCOUNTER_OVERFLOW_TYPE) == 7,
               "overflow: type alias at MONSTER_SLOTS[10] survives compact", fails);
        {
            const gfx::CombatPanelView v1 = combat.panelView();
            expect(v1.overflow_more == 4, "overflow: +N drops by one after kill", fails);
            int occ = 0;
            for (int i = 0; i < v1.monster_line_count && i < 10; ++i) {
                if (v1.monster_lines[i].occupied) {
                    ++occ;
                }
            }
            expect(occ == 10, "overflow: kill promotes reservoir — still 10 visible", fails);
        }
        expect(combat.active(), "overflow: fight continues with remaining pack", fails);
    }

    /* ---- XP split @ 0x12430: unconscious (0 HP, condition < $80) gets a share. */
    {
        rng.reseed(1);
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
        rng.reseed(1);
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
        rng.reseed(1);
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
        rng.reseed(1);
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

    /* ---- Run: 0x116B0 success when rng(1,100) < -$560D (attrib 0x0D). */
    {
        rng.reseed(1);
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[5];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x3F); /* survives — party never attacks it */
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);

        setupParty(roster, launch, /*might=*/1, /*speed=*/99, /*hp=*/999);
        seedFixedEncounter(gs, 5);
        /* Thresh $65 → every roll 1..100 succeeds (ASM cmp/bcc). */
        mm2_gs_set_u8(gs.a4(), MM2_GS_RETREAT_DIFF, 0x65);
        /* 0x1164A: a successful flee restores from -$560C too (not just wipe).
         * Set a distinctive entry square and a current coord far from it. */
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENTRY_COORD, 0x12); /* (2,1) */
        gs.setCoordX(5);
        gs.setCoordY(7);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.enter(gs, world);
        /* enter() mirrors world.attrib()[0x0D]==0 over the seed — restore. */
        mm2_gs_set_u8(gs.a4(), MM2_GS_RETREAT_DIFF, 0x65);
        expect(combat.awaitingPartyOptions(), "run scenario: party options before fight", fails);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        expect(combat.awaitingCommand(), "run scenario: party prompted (higher speed)", fails);

        keys.last_ascii = 'R';
        combat.tick(gs, world, keys);
        /* 0x116B0 sets latch + shrink; leave happens at next 0x13282 check. */
        const bool ended = fightToEnd(combat, gs, world, ' ');
        expect(ended, "run scenario: Run succeeds when rng < -$560D", fails);
        expect(combat.lastOutcome() == CombatOutcome::Fled, "run scenario: outcome == Fled", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_COMBAT_VICTORY_LATCH) == 0,
               "run scenario: COMBAT_VICTORY_LATCH stays clear on flee", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_PARTY_RAN_LATCH) == 1,
               "run scenario: -$5E4C set on successful Run", fails);
        /* Manual Run: fled PC leaves the fight table only, then rejoins — not inn Dismiss. */
        expect(launch.party_count == 1 && launch.roster_slots[0] == 0,
               "run scenario: launch party not dismissed by Char-Run", fails);
        expect(mm2_gs_u16(gs.a4(), MM2_GS_PARTY_COUNT) == 1,
               "run scenario: -$795A restored after flee", fails);
        expect(gs.coordX() == 2 && gs.coordY() == 1,
               "run scenario: flee restores entry_coord (2,1)", fails);
    }

    /* Two-member Char-Run: shrink mid-fight must not drop the runner from launch_. */
    {
        rng.reseed(1);
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon2 = monsters.records[5];
        setMonsterField(mon2, MM2_MON_OFF_HP, 0x3F);
        setMonsterField(mon2, MM2_MON_OFF_SPEED, 0x00);

        Mm2RosterFile roster2{};
        Mm2PartyLaunch launch2{};
        std::memset(&roster2, 0, sizeof(roster2));
        mm2_roster_set_name(&roster2.records[0], "Runner");
        mm2_roster_set_name(&roster2.records[1], "Stayer");
        roster2.records[0].might_current = 1;
        roster2.records[0].speed_current = 99;
        roster2.records[0].hp_current = 999;
        roster2.records[0].hp_max = 999;
        roster2.records[1].might_current = 1;
        roster2.records[1].speed_current = 1;
        roster2.records[1].hp_current = 999;
        roster2.records[1].hp_max = 999;
        launch2.party_count = 2;
        launch2.roster_slots[0] = 0;
        launch2.roster_slots[1] = 1;
        seedFixedEncounter(gs, 5);
        mm2_gs_set_u8(gs.a4(), MM2_GS_RETREAT_DIFF, 0x65);
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENTRY_COORD, 0x12);

        CombatSession combat;
        combat.bindParty(&roster2, &launch2);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        combat.enter(gs, world);
        mm2_gs_set_u8(gs.a4(), MM2_GS_RETREAT_DIFF, 0x65);

        platform::KeyState keys{};
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        keys.last_ascii = 'R';
        combat.tick(gs, world, keys);
        const bool ended = fightToEnd(combat, gs, world, ' ');
        expect(ended, "run-2: fight ends after Char-Run", fails);
        expect(combat.lastOutcome() == CombatOutcome::Fled, "run-2: outcome Fled", fails);
        expect(launch2.party_count == 2, "run-2: launch still has 2", fails);
        expect(launch2.roster_slots[0] == 0 && launch2.roster_slots[1] == 1,
               "run-2: both roster slots kept", fails);
        expect(mm2_gs_u16(gs.a4(), MM2_GS_PARTY_COUNT) == 2,
               "run-2: GS party count restored to 2", fails);
    }

    /* ---- Random picker (0x1213E/0x12072/0x11F0A) invariants, exercised
     * directly (no CombatSession/monsters.dat needed): a positive XP budget
     * and a non-trivial group-size gate must yield at least one monster
     * within the attrib min/max tier bounds, and terminate (PICKER_DONE). */
    {
        rng.reseed(1);
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
        rng.reseed(1);
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
        rng.reseed(1);
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

    /* ---- 0xFD8C/0xFE00: KO expands -$5E4D so the next back-rank slot steps up. */
    {
        rng.reseed(1);
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[1];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x0F);
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x01);

        /* Outdoor + 6 members: 0x11DDE front = rng/2 + party - 2 → 4..5 (< 6). */
        mm2_gs_set_u8(gs.a4(), MM2_GS_VIEW_MODE, 1);
        std::memset(&roster, 0, sizeof(roster));
        launch = Mm2PartyLaunch{};
        launch.party_count = 6;
        for (int i = 0; i < 6; ++i) {
            char name[8];
            std::snprintf(name, sizeof(name), "P%d", i + 1);
            mm2_roster_set_name(&roster.records[i], name);
            roster.records[i].hp_current = 50;
            roster.records[i].hp_max = 50;
            roster.records[i].speed_current = 50;
            roster.records[i].condition = 0;
            launch.roster_slots[i] = static_cast<int8_t>(i);
        }
        seedFixedEncounter(gs, 1);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        expect(combat.enter(gs, world), "ko-expand: enter outdoor fight", fails);

        /* 0x11D0C rolls -$5E4D at round-loop entry (after party options). */
        platform::KeyState keys{};
        keys.last_ascii = ' ';
        combat.tick(gs, world, keys);
        if (combat.state() == CombatState::AwaitingPartyOptions) {
            keys.last_ascii = 'A';
            combat.tick(gs, world, keys);
        }
        for (int i = 0; i < 32 && combat.active() && combat.state() != CombatState::AwaitingCommand; ++i) {
            keys.last_ascii = ' ';
            combat.tick(gs, world, keys);
        }
        expect(combat.state() == CombatState::AwaitingCommand, "ko-expand: reached command turn", fails);

        int front_before = 0;
        for (int i = 0; i < 6; ++i) {
            if (combat.partySlotInFrontRank(i)) {
                ++front_before;
            }
        }
        expect(front_before >= 4 && front_before < 6,
               "ko-expand: outdoor front rank is 4..5 before KO", fails);
        expect(!combat.partySlotInFrontRank(front_before),
               "ko-expand: slot at cutoff is back-rank before KO", fails);

        combat.applyPartyDamage4AAA(gs, 0, 50, "Rat");
        expect((roster.records[0].condition & 0x40) != 0, "ko-expand: slot 0 unconscious", fails);
        expect(combat.partySlotInFrontRank(front_before),
               "ko-expand: next slot gains front-rank check after KO (0xFE00)", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_FRONT_RANK_N) == static_cast<uint8_t>(front_before + 1),
               "ko-expand: A4-$5E4D matches expanded cutoff", fails);
    }

    /* ---- 0x12796..0x1283E: status suffix from -$519; bit0 ("Hurt") is set only
     * by damage @ 0x10EEA (bset #0). Full-HP monsters stay status 0 → blank
     * 5-space path @ 0x1279C. */
    {
        rng.reseed(7);
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord mon0 = monsters.records[1];
        setMonsterField(mon0, MM2_MON_OFF_HP, 0x0F);     /* 16 HP */
        setMonsterField(mon0, MM2_MON_OFF_SPEED, 0x00);  /* slow: party acts first */
        setMonsterField(mon0, MM2_MON_OFF_DAMAGE, 0x01);
        monsters.records[1] = mon0;
        Mm2MonsterRecord mon1 = monsters.records[2];
        setMonsterField(mon1, MM2_MON_OFF_HP, 0x0F);
        setMonsterField(mon1, MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(mon1, MM2_MON_OFF_DAMAGE, 0x01);
        monsters.records[2] = mon1;

        setupParty(roster, launch, /*might=*/99, /*speed=*/99, /*hp=*/999);
        uint8_t *a4 = gs.a4();
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0x80);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 0, 1);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + 1, 2);
        for (int i = 2; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
            mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);
        }
        mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 2);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        expect(combat.enter(gs, world), "status-suffix: enter two-monster fight", fails);

        /* Party acts first (speed 99 > monster speed 1) → round roster is built. */
        platform::KeyState keys{};
        keys.last_ascii = ' ';
        combat.tick(gs, world, keys);
        if (combat.state() == CombatState::AwaitingPartyOptions) {
            keys.last_ascii = 'A';
            combat.tick(gs, world, keys);
        }
        for (int i = 0; i < 32 && combat.active() && combat.state() != CombatState::AwaitingCommand; ++i) {
            keys.last_ascii = ' ';
            combat.tick(gs, world, keys);
        }
        expect(combat.state() == CombatState::AwaitingCommand,
               "status-suffix: reached round command turn", fails);

        gfx::CombatPanelView view = combat.panelView();
        expect(view.label_monster_slots, "status-suffix: round roster visible", fails);
        /* Full HP → status 0 → blank suffix (not "Hurt"). */
        bool slot0_blank = false, slot1_blank = false;
        for (int i = 0; i < view.monster_line_count && i < 10; ++i) {
            if (view.monster_lines[i].letter == 'A' && view.monster_lines[i].occupied) {
                slot0_blank = view.monster_lines[i].status_suffix[0] == '\0';
            }
            if (view.monster_lines[i].letter == 'B' && view.monster_lines[i].occupied) {
                slot1_blank = view.monster_lines[i].status_suffix[0] == '\0';
            }
        }
        expect(slot0_blank && slot1_blank,
               "status-suffix: full-HP monsters have blank suffix (not Hurt)", fails);

        /* Attack slot A → 0x10EEA bset #0 → "Hurt" on A only. */
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        if (combat.state() == CombatState::AwaitingAttackTarget) {
            keys.last_ascii = 'A';
            combat.tick(gs, world, keys);
        }
        for (int i = 0; i < 16 && combat.active() &&
             combat.state() == CombatState::AwaitingActionAck; ++i) {
            keys.last_ascii = ' ';
            combat.tick(gs, world, keys);
        }
        view = combat.panelView();
        bool a_hurt = false, b_still_blank = false;
        for (int i = 0; i < view.monster_line_count && i < 10; ++i) {
            if (view.monster_lines[i].letter == 'A' && view.monster_lines[i].occupied) {
                a_hurt = std::strcmp(view.monster_lines[i].status_suffix, "Hurt") == 0;
            }
            if (view.monster_lines[i].letter == 'B' && view.monster_lines[i].occupied) {
                b_still_blank = view.monster_lines[i].status_suffix[0] == '\0';
            }
        }
        expect(a_hurt, "status-suffix: damaged monster shows Hurt", fails);
        expect(b_still_blank, "status-suffix: undamaged monster stays blank", fails);
    }

    /* Combat cast @ 0x11A90 / 0x79EE: level+number on message band, no spell grid. */
    {
        rng.reseed(1);
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

    /* Identify Monster @ 0xB760: one positioned band (not 0x132E6 queued lines). */
    {
        rng.reseed(1);
        std::memset(&gs_image, 0, sizeof(gs_image));
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
        roster.records[0].gems = 10;
        mm2::gameplay::spellLearnInBook(roster.records[0], 9); /* S2/3 Identify Monster */

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
        expect(combat.enter(gs, world), "identify: enter fight", fails);

        platform::KeyState keys{};
        keys.last_ascii = ' ';
        combat.tick(gs, world, keys);
        if (combat.state() == CombatState::AwaitingPartyOptions) {
            keys.last_ascii = 'A';
            combat.tick(gs, world, keys);
        }
        for (int i = 0; i < 32 && combat.active() && combat.state() != CombatState::AwaitingCommand; ++i) {
            keys.last_ascii = ' ';
            combat.tick(gs, world, keys);
        }
        expect(combat.state() == CombatState::AwaitingCommand, "identify: reached command turn", fails);

        keys.last_ascii = 'C';
        combat.tick(gs, world, keys);
        keys.last_ascii = '2';
        combat.tick(gs, world, keys);
        keys.last_ascii = '3';
        combat.tick(gs, world, keys);
        expect(combat.state() == CombatState::AwaitingCastTarget, "identify: D43C letter-pick", fails);
        keys.last_ascii = 'A';
        combat.tick(gs, world, keys);
        expect(combat.state() == CombatState::AwaitingActionAck, "identify: -$7DDC wait", fails);

        const char *block = combat.statusLine();
        expect(std::strchr(block, '\n') != nullptr, "identify: one newline-separated block", fails);
        expect(std::strstr(block, "#1 Rat:") != nullptr, "identify: #type name: header", fails);
        expect(std::strstr(block, "HP =") != nullptr && std::strstr(block, "AC =") != nullptr &&
                   std::strstr(block, "Undead") != nullptr && std::strstr(block, "Special Power") != nullptr &&
                   std::strstr(block, "Bonus on Touch") != nullptr &&
                   std::strstr(block, "Magic Resistance") != nullptr,
               "identify: HP/AC/flags in the same block", fails);

        keys.last_ascii = ' ';
        combat.tick(gs, world, keys);
        const bool still_paging = combat.state() == CombatState::AwaitingActionAck &&
                                  std::strstr(combat.statusLine(), "HP =") != nullptr &&
                                  std::strchr(combat.statusLine(), '\n') == nullptr;
        expect(!still_paging, "identify: one ack dismisses the whole block", fails);
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

    /* ---- Full Auto (remake): strike-only finishes a 1-HP fight. ---- */
    {
        rng.reseed(1);
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[7];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x00); /* 1 HP */
        setMonsterField(mon, MM2_MON_OFF_XP, 0x2E);
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x00);

        setupParty(roster, launch, /*might=*/99, /*speed=*/99, /*hp=*/999);
        seedFixedEncounter(gs, 7);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        expect(combat.enter(gs, world), "auto-strike: enter ok", fails);

        const bool ended = autoFightToEnd(combat, gs, world);
        expect(ended, "auto-strike: Auto finishes fight", fails);
        expect(combat.lastOutcome() == CombatOutcome::Victory, "auto-strike: Victory", fails);
        expect(!combat.autoEnabled(), "auto-strike: Auto cleared on exit", fails);
    }

    /* ---- Full Auto: cleric First Aid on hurt ally, then finish. ---- */
    {
        rng.reseed(1);
        std::memset(&gs_image, 0, sizeof(gs_image));
        std::memset(&monsters, 0, sizeof(monsters));
        Mm2MonsterRecord &mon = monsters.records[7];
        setMonsterField(mon, MM2_MON_OFF_HP, 0x00); /* 1 HP — dies after heal turn */
        setMonsterField(mon, MM2_MON_OFF_XP, 0x2E);
        setMonsterField(mon, MM2_MON_OFF_SPEED, 0x00);
        setMonsterField(mon, MM2_MON_OFF_DAMAGE, 0x00);

        std::memset(&roster, 0, sizeof(roster));
        mm2_roster_set_name(&roster.records[0], "Fighter");
        mm2_roster_set_name(&roster.records[1], "Cleric");
        roster.records[0].class_id = 0; /* Knight */
        roster.records[0].might_current = 99;
        roster.records[0].speed_current = 1;
        roster.records[0].hp_current = 100; /* ceiling */
        roster.records[0].hp_max = 10;      /* working — critically hurt */
        roster.records[0].condition = 0;
        roster.records[1].class_id = 3; /* Cleric */
        roster.records[1].might_current = 20;
        roster.records[1].speed_current = 99;
        roster.records[1].hp_current = 80;
        roster.records[1].hp_max = 80;
        roster.records[1].spell_level = 1;
        roster.records[1].sp_current = 20;
        roster.records[1].level = 1;
        roster.records[1].condition = 0;
        mm2::gameplay::spellLearnInBook(roster.records[1], 3); /* First Aid */

        launch = Mm2PartyLaunch{};
        launch.party_count = 2;
        launch.roster_slots[0] = 0;
        launch.roster_slots[1] = 1;

        seedFixedEncounter(gs, 7);

        CombatSession combat;
        combat.bindParty(&roster, &launch);
        combat.bindMonsters(&monsters);
        combat.bindRng(&rng);
        expect(combat.enter(gs, world), "auto-heal: enter ok", fails);

        const uint16_t hp_before = roster.records[0].hp_max;
        bool saw_heal = false;
        combat.setAutoEnabled(true);
        for (int i = 0; i < 128 && combat.active(); ++i) {
            const CombatState st = combat.state();
            bool ended = false;
            if (st == CombatState::AwaitingCommand) {
                ended = combat.runAutoCommand(gs);
            } else if (st == CombatState::AwaitingCastTarget || st == CombatState::AwaitingPartyPick ||
                       st == CombatState::AwaitingAttackTarget) {
                ended = combat.runAutoPicker(gs);
            } else if (st == CombatState::AwaitingPartyOptions) {
                platform::KeyState keys{};
                keys.last_ascii = 'A';
                ended = combat.tick(gs, world, keys);
            } else {
                platform::KeyState keys{};
                keys.last_ascii = ' ';
                keys.space = true;
                keys.any_key = true;
                ended = combat.tick(gs, world, keys);
            }
            if (std::strstr(combat.statusLine(), "First Aid") != nullptr) {
                saw_heal = true;
            }
            if (ended) {
                break;
            }
        }
        expect(saw_heal || roster.records[0].hp_max > hp_before, "auto-heal: First Aid applied", fails);
        expect(combat.lastOutcome() == CombatOutcome::Victory, "auto-heal: fight ends in Victory", fails);
    }

    /* Exploration/sheet cast @ 0x6E30: CombatSession.party_count_ is 0 until
     * combat enter. Without a sync from launch, castSpellFromSheet no-ops and
     * combat-only leaves never fail. */
    {
        std::memset(&gs_image, 0, sizeof(gs_image));
        Mm2RosterFile sroster{};
        Mm2PartyLaunch slaunch{};
        mm2_roster_clear_record(&sroster.records[0]);
        mm2_roster_set_name(&sroster.records[0], "Mage");
        sroster.records[0].class_id = 4;
        sroster.records[0].level = 6;
        sroster.records[0].spell_level = 3;
        sroster.records[0].sp_current = 42;
        sroster.records[0].sp_max = 42;
        sroster.records[0].gems = 10;
        gameplay::spellLearnInBook(sroster.records[0], 3);
        gameplay::spellLearnInBook(sroster.records[0], 4);
        gameplay::spellLearnInBook(sroster.records[0], 15);
        slaunch.party_count = 1;
        slaunch.roster_slots[0] = 0;

        CombatSession scast;
        scast.bindParty(&sroster, &slaunch);
        scast.bindRng(&rng);
        expect(!scast.active(), "explore-cast: starts Inactive", fails);
        expect(!scast.sheetCastPending(), "explore-cast: not pending", fails);

        scast.castSpellFromSheet(gs, 0, 4);
        expect(std::strstr(scast.statusLine(), "Light") != nullptr, "explore-cast: Light runs", fails);
        expect(!scast.sheetCastPending(), "explore-cast: Light does not linger", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_LIGHT_FACTOR) == 1, "explore-cast: Light sets factor", fails);

        /* Multi-member party: sheet slot 1 must cast as that roster member, not
         * fight_roster_slots_ default 0 (Sir Felgar / "? (stub)" regression). */
        {
            Mm2RosterFile multi{};
            Mm2PartyLaunch ml{};
            mm2_roster_clear_record(&multi.records[0]);
            mm2_roster_clear_record(&multi.records[1]);
            mm2_roster_set_name(&multi.records[0], "Felgar");
            mm2_roster_set_name(&multi.records[1], "Cassandra");
            multi.records[0].class_id = 0; /* Knight — no book */
            multi.records[1].class_id = 4; /* Sorcerer */
            multi.records[1].level = 6;
            multi.records[1].spell_level = 3;
            multi.records[1].sp_current = 42;
            multi.records[1].sp_max = 42;
            gameplay::spellLearnInBook(multi.records[1], 4);
            ml.party_count = 2;
            ml.roster_slots[0] = 0;
            ml.roster_slots[1] = 1;
            CombatSession mcast;
            mcast.bindParty(&multi, &ml);
            mcast.bindRng(&rng);
            mcast.castSpellFromSheet(gs, 1, 4);
            expect(std::strstr(mcast.statusLine(), "Cassandra") != nullptr,
                   "explore-cast: slot1 uses Cassandra name", fails);
            expect(std::strstr(mcast.statusLine(), "stub") == nullptr,
                   "explore-cast: slot1 is not stub", fails);
            expect(std::strstr(mcast.statusLine(), "Light") != nullptr,
                   "explore-cast: slot1 Light runs", fails);
        }

        scast.castSpellFromSheet(gs, 0, 3);
        expect(std::strstr(scast.statusLine(), "Spell Failed") != nullptr,
               "explore-cast: Flame Arrow fails out of combat", fails);
        expect(!scast.sheetCastPending(), "explore-cast: Flame Arrow skips target pick", fails);
        expect(!scast.active(), "explore-cast: Flame Arrow stays Inactive", fails);

        /* spells.dat combat-only buffs (byte0 0x40): must fail in explore before
         * bumping GS counters — Invisibility S3/3, Shield S4/5. */
        gameplay::spellLearnInBook(sroster.records[0], 16);
        gameplay::spellLearnInBook(sroster.records[0], 24);
        const uint8_t invis_before = mm2_gs_u8(gs.a4(), MM2_GS_INVIS_COUNTER);
        const uint8_t shield_before = mm2_gs_u8(gs.a4(), MM2_GS_SHIELD_COUNTER);
        const uint16_t sp_before_invis = sroster.records[0].sp_current;
        scast.castSpellFromSheet(gs, 0, 16);
        expect(std::strstr(scast.statusLine(), "Spell Failed") != nullptr,
               "explore-cast: Invisibility fails (dat combat-only)", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_INVIS_COUNTER) == invis_before,
               "explore-cast: Invisibility does not bump -$799C", fails);
        expect(sroster.records[0].sp_current == sp_before_invis,
               "explore-cast: Invisibility does not spend SP", fails);
        scast.castSpellFromSheet(gs, 0, 24);
        expect(std::strstr(scast.statusLine(), "Spell Failed") != nullptr,
               "explore-cast: Shield fails (dat combat-only)", fails);
        expect(mm2_gs_u8(gs.a4(), MM2_GS_SHIELD_COUNTER) == shield_before,
               "explore-cast: Shield does not bump -$799B", fails);

        /* Cleric Bless C1/3 — same combat-only gate. */
        {
            Mm2RosterFile croster{};
            Mm2PartyLaunch claunch{};
            mm2_roster_clear_record(&croster.records[0]);
            mm2_roster_set_name(&croster.records[0], "Priest");
            croster.records[0].class_id = 3;
            croster.records[0].level = 4;
            croster.records[0].spell_level = 1;
            croster.records[0].sp_current = 20;
            croster.records[0].sp_max = 20;
            gameplay::spellLearnInBook(croster.records[0], 2);
            claunch.party_count = 1;
            claunch.roster_slots[0] = 0;
            CombatSession ccast;
            ccast.bindParty(&croster, &claunch);
            ccast.bindRng(&rng);
            const uint8_t bless_before = mm2_gs_u8(gs.a4(), MM2_GS_BLESS_COUNTER);
            ccast.castSpellFromSheet(gs, 0, 2);
            expect(std::strstr(ccast.statusLine(), "Spell Failed") != nullptr,
                   "explore-cast: Bless fails (dat combat-only)", fails);
            expect(mm2_gs_u8(gs.a4(), MM2_GS_BLESS_COUNTER) == bless_before,
                   "explore-cast: Bless does not bump -$799D", fails);
        }

        mm2_gs_set_u8(gs.a4(), MM2_GS_ATTRIB_FLAGS, 0);
        scast.castSpellFromSheet(gs, 0, 15);
        expect(scast.sheetCastPending(), "explore-cast: Fly waits for A-E", fails);
        expect(!scast.active(), "explore-cast: Fly pending is not a fight", fails);
        expect(std::strstr(scast.statusLine(), "Fly to") != nullptr, "explore-cast: Fly prompt", fails);
        expect(scast.tickSheetCastAux(gs, 'A'), "explore-cast: Fly accepts A", fails);
        expect(scast.sheetCastPending(), "explore-cast: Fly waits for 1-4", fails);
        expect(std::strstr(scast.statusLine(), "1-4") != nullptr, "explore-cast: Fly sector prompt", fails);
        /* Completing A1 must write screen 5 from A4-$7130[0], not screen 0 from an
         * unseeded / wrong-offset table; X/Y stay $FF for entry_coord unpack. */
        expect(scast.tickSheetCastAux(gs, '1'), "explore-cast: Fly accepts 1", fails);
        expect(gs.screenId() == 5, "explore-cast: Fly A1 → screen 5", fails);
        expect(gs.coordX() == 0xFF && gs.coordY() == 0xFF, "explore-cast: Fly leaves $FF coords", fails);
        expect(mm2_gs_u8(gs.a4(), -0x79E4) == 1, "explore-cast: Fly sets -$79E4", fails);
        expect(!scast.sheetCastPending(), "explore-cast: Fly completes", fails);
        expect(!scast.active(), "explore-cast: Fly done stays Inactive", fails);

        /* 0x1C64 sentinel unpack (host does this after attrib materialize). */
        mm2_gs_set_u8(gs.a4(), MM2_GS_ENTRY_COORD, 0xA3); /* (3,10) */
        expect(gameplay::applyEntryCoordIfSentinel(gs), "explore-cast: $FF → entry_coord", fails);
        expect(gs.coordX() == 3 && gs.coordY() == 10, "explore-cast: entry unpack (3,10)", fails);
    }

    if (fails == 0) {
        std::printf("OK: combat_setup_test\n");
        return 0;
    }
    return 1;
}

// Bash / Unlock / Rest unit test.
//
// Covers the faithful explore-command decision logic traced from the 68k ASM:
//   - bashDoorRoll   (0x9BCA strength + 0x9C4C trap roll)
//   - unlockDoorRoll (0x20D26 pick + 0x20D5C trap roll)
//   - advanceTimeTick(0x55) (Rest clock advance @ 0x19CEC -> rollover 0x6A06)
//   - Rng.range (0x22BC6 inclusive (min,max) contract)

#include <cstdio>

#include "mm2/GameState.h"
#include "mm2/events/EventVmHelpers.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2/gameplay/Movement.h"

#include "mm2_gamestate.h"

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

}  // namespace

int main()
{
    using namespace mm2::gameplay;
    int fails = 0;

    /* ---- Bash strength decision (0x9BCA) -------------------------------- */
    /* roll_10_109=55 -> roll10=5 -> auto-success regardless of door strength;
     * trap_d100>=0x33 -> the door opens (clears lock). */
    {
        BashDecision d = bashDoorRoll(/*might*/ 1, /*door*/ 250, /*roll*/ 55, /*trap*/ 0x33);
        expect(d.outcome == BashOutcome::Opened && d.clears_lock, "bash roll10==5 auto-opens", fails);
    }
    /* strength (might+roll10) >= door -> success; trap miss -> opens. */
    {
        BashDecision d = bashDoorRoll(/*might*/ 10, /*door*/ 5, /*roll*/ 20 /*roll10=2*/, /*trap*/ 0x40);
        expect(d.outcome == BashOutcome::Opened, "bash strength beats weak door", fails);
    }
    /* strength < door -> Locked. */
    {
        BashDecision d = bashDoorRoll(/*might*/ 1, /*door*/ 200, /*roll*/ 20 /*roll10=2*/, /*trap*/ 0x40);
        expect(d.outcome == BashOutcome::Locked && d.msg == ObstructionMsg::Locked,
               "bash weak might fails -> Locked", fails);
    }
    /* success but trap_d100 < 0x33 -> trap springs. */
    {
        BashDecision d = bashDoorRoll(/*might*/ 99, /*door*/ 1, /*roll*/ 20, /*trap*/ 0x32);
        expect(d.outcome == BashOutcome::TrapSprung, "bash success + low trap roll springs trap", fails);
    }

    /* ---- Unlock pick decision (0x20CA2) --------------------------------- */
    /* thievery >= roll (< 0x60) -> Success. */
    {
        UnlockDecision d = unlockDoorRoll(/*thievery*/ 50, /*lock*/ 30, /*trap*/ 0xFF, /*traproll*/ 50);
        expect(d.outcome == UnlockOutcome::Success && d.msg == ObstructionMsg::Success && d.clears_lock,
               "unlock thievery>=roll -> Success", fails);
    }
    /* pick fails, trap byte holds (trap_byte >= roll) -> Locked. */
    {
        UnlockDecision d = unlockDoorRoll(/*thievery*/ 20, /*lock*/ 30, /*trap*/ 0xFF, /*traproll*/ 50);
        expect(d.outcome == UnlockOutcome::Locked && d.msg == ObstructionMsg::Locked,
               "unlock low thievery -> Locked", fails);
    }
    /* roll >= 0x60 always fails the pick even with high thievery. */
    {
        UnlockDecision d = unlockDoorRoll(/*thievery*/ 99, /*lock*/ 0x60, /*trap*/ 0xFF, /*traproll*/ 50);
        expect(d.outcome != UnlockOutcome::Success, "unlock roll>=0x60 cannot succeed", fails);
    }
    /* pick fails and trap_byte < roll -> trap springs. */
    {
        UnlockDecision d = unlockDoorRoll(/*thievery*/ 20, /*lock*/ 30, /*trap*/ 0, /*traproll*/ 50);
        expect(d.outcome == UnlockOutcome::TrapSprung, "unlock fail + low trap byte springs trap", fails);
    }

    /* ---- Rng.range inclusive (min,max) contract (0x22BC6) --------------- */
    {
        Rng rng(0xC0FFEEu);
        expect(rng.range(5, 5) == 5, "rng range(5,5)==5", fails);
        bool in_bounds = true;
        for (int i = 0; i < 5000; ++i) {
            int v = rng.range(10, 109);
            if (v < 10 || v > 109) {
                in_bounds = false;
            }
            int d = rng.range(1, 100);
            if (d < 1 || d > 100) {
                in_bounds = false;
            }
        }
        expect(in_bounds, "rng range stays within [min,max]", fails);
    }

    /* ---- Amiga entropy 0x24048: state = state*0x41C64E6D + 0x3039 -------- */
    {
        Rng a(1);
        /* First step from seed 1: 1*0x41C64E6D+0x3039 = 0x41C67EA6;
         * raw15 = (>>16)&0x7FFF = 0x41C6. */
        expect(a.range(0, 0x7FFF) == 0x41C6, "amiga LCG first raw15 from seed 1", fails);

        Rng b(1);
        Rng c(2);
        bool differ = false;
        for (int i = 0; i < 32; ++i) {
            if (b.range(1, 100) != c.range(1, 100)) {
                differ = true;
                break;
            }
        }
        expect(differ, "different seeds diverge within 32 rolls", fails);

        Rng d(0xDEADBEEFu);
        d.reseed(1);
        Rng e(1);
        expect(d.range(1, 1000) == e.range(1, 1000), "reseed(1) matches Rng(1)", fails);
    }

    /* ---- Rest clock advance: +0x55 sub-day, with rollover (0x19CEC) ----- */
    {
        static uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
        mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
        gs.initCalendarDefaults(); /* era 9, day 1, year 900, subday 1 */
        uint8_t *a4 = gs.a4();

        mm2_gs_set_u16(a4, MM2_GS_TIME_SUBDAY, 0);
        const uint16_t day0 = gs.day();
        advanceTimeTick(gs, 0x55);
        expect(mm2_gs_u16(a4, MM2_GS_TIME_SUBDAY) == 0x55, "rest advances subday by 0x55", fails);
        expect(gs.day() == day0, "single rest does not cross a day boundary from 0", fails);

        /* From 0xC0, +0x55 = 0x115 -> rolls one day, subday folds to 0x15. */
        mm2_gs_set_u16(a4, MM2_GS_TIME_SUBDAY, 0xC0);
        advanceTimeTick(gs, 0x55);
        expect(mm2_gs_u16(a4, MM2_GS_TIME_SUBDAY) == 0x15, "rest subday rolls 0xC0+0x55 -> 0x15", fails);
        expect(gs.day() == static_cast<uint16_t>(day0 + 1), "rest day-advance crosses midnight", fails);
    }

    /* ---- Rest SP recompute @ 0x19C30 (INT/PER × working level) ------------ */
    {
        Mm2RosterRecord cass{};
        cass.class_id = 4; /* Sorcerer */
        cass.level = 4;
        cass.spell_level = 2;
        cass.intelligence_current = 21; /* tier → bonus 4 → SP/level 7 */
        cass.personality_current = 7;
        cass.unknown_1a_20[6] = 1; /* stock roster drift: +$20 stuck at 1 */
        cass.unknown_22 = static_cast<uint16_t>(2u << 8);
        cass.sp_max = 7;
        cass.sp_current = 7;

        recomputeRestSpellPoints(cass);
        expect(cass.sp_max == 7 && cass.sp_current == 7,
               "without sync, stale +$20=1 keeps stock Cassandra at SP 7", fails);

        syncRosterWorkingLevelFields(cass);
        expect(cass.unknown_1a_20[6] == 4, "sync copies +$71 level into +$20", fails);
        recomputeRestSpellPoints(cass);
        expect(cass.sp_max == 28 && cass.sp_current == 28,
               "Cassandra L4 INT21 Rest SP = (4+3)*4 = 28", fails);

        Mm2RosterRecord gene{};
        gene.class_id = 3; /* Cleric */
        gene.level = 4;
        gene.spell_level = 2;
        gene.personality_current = 20; /* same 7 SP/level tier */
        gene.intelligence_current = 10;
        gene.sp_max = 7;
        gene.sp_current = 7;
        syncRosterWorkingLevelFields(gene);
        recomputeRestSpellPoints(gene);
        expect(gene.sp_max == 28 && gene.sp_current == 28,
               "Gene Eric L4 PER20 Rest SP = (4+3)*4 = 28", fails);

        Mm2RosterRecord knight{};
        knight.class_id = 0;
        knight.level = 4;
        knight.spell_level = 0;
        knight.sp_max = 0;
        syncRosterWorkingLevelFields(knight);
        recomputeRestSpellPoints(knight);
        expect(knight.sp_max == 0, "non-caster (+$23==0) Rest leaves SP alone", fails);
    }

    /* ---- Greatest Fountain (E2 11,9): OP_18 writes sheet/base only ----------
     * sel 0x22/26/2A/2D → +$6B might, +$71 level, +$73 endurance, +$70 luck.
     * Rest jsr -$7F50 (0x4476) restores from +$10/+ $20 / +$27 / +$15. */
    {
        Mm2RosterRecord rec{};
        rec.might_current = 18;
        rec.might_base = 18;
        rec.intelligence_current = 16;
        rec.intelligence_base = 16;
        rec.personality_current = 15;
        rec.personality_base = 15;
        rec.speed_current = 14;
        rec.speed_base = 14;
        rec.accuracy_current = 13;
        rec.accuracy_base = 13;
        rec.luck_current = 12;
        rec.luck_base = 12;
        rec.endurance_current = 17;
        rec.endurance_base = 17;
        rec.level = 7;
        rec.spell_level = 2;
        rec.unknown_1a_20[6] = 7; /* working +$20 */
        rec.unknown_22 = static_cast<uint16_t>(2u << 8); /* +$23 */

        rec.might_base = 200;
        rec.speed_base = 200;
        rec.accuracy_base = 200;
        rec.level = 50;
        rec.endurance_base = 200;
        rec.intelligence_base = 200;
        rec.personality_base = 200;
        rec.luck_base = 200;

        /* Do NOT copy +$71→+$20 here — that was the remake bug that made L50 stick. */
        recomputeRestSpellPoints(rec);
        applyRestSecondaryStatWriteback(rec);

        expect(rec.level == 7, "fountain level 50 restores from working +$20 on Rest", fails);
        expect(rec.might_base == 18, "fountain might 200 restores from +$10 on Rest", fails);
        expect(rec.speed_base == 14, "fountain speed 200 restores from +$13 on Rest", fails);
        expect(rec.accuracy_base == 13, "fountain accuracy 200 restores from +$14 on Rest", fails);
        expect(rec.endurance_base == 17, "fountain endurance 200 restores from +$27 on Rest", fails);
        expect(rec.intelligence_base == 16, "fountain int 200 restores from +$11 on Rest", fails);
        expect(rec.personality_base == 15, "fountain per 200 restores from +$12 on Rest", fails);
        expect(rec.luck_base == 12, "fountain luck 200 restores from +$15 on Rest", fails);
        expect(rec.spell_level == 2, "working spell-level +$23 restored onto +$72", fails);
    }

    /* ---- Chest trap @ 0x1AA70: place cels + 0x1A8A4 damage table -------- */
    {
        using mm2::events::eventVmSearchTrapPlaceFrame;
        using mm2::events::eventVmSearchTrapDamageAmount;
        expect(eventVmSearchTrapPlaceFrame(0) == 4, "trap type 0 → place cel 4", fails);
        expect(eventVmSearchTrapPlaceFrame(1) == 6, "trap type 1 → place cel 6", fails);
        expect(eventVmSearchTrapPlaceFrame(2) == 8, "trap type 2 → place cel 8", fails);
        expect(eventVmSearchTrapPlaceFrame(3) == 10, "trap type 3 → place cel 10", fails);
        /* nullptr a4 → env row 0, attrib+0x14=0 → -$690C[0]=3. */
        expect(eventVmSearchTrapDamageAmount(nullptr) == 3, "trap dmg env0 shift0 = 3", fails);
    }

    if (fails == 0) {
        std::printf("OK: explore_commands_test\n");
        return 0;
    }
    return 1;
}

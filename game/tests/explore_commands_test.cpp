// Bash / Unlock / Rest unit test.
//
// Covers the faithful explore-command decision logic traced from the 68k ASM:
//   - bashDoorRoll   (0x9BCA strength + 0x9C4C trap roll)
//   - unlockDoorRoll (0x20D26 pick + 0x20D5C trap roll)
//   - advanceTimeTick(0x55) (Rest clock advance @ 0x19CEC -> rollover 0x6A06)
//   - Rng.range (0x22BC6 inclusive (min,max) contract)

#include <cstdio>

#include "mm2/GameState.h"
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

    if (fails == 0) {
        std::printf("OK: explore_commands_test\n");
        return 0;
    }
    return 1;
}

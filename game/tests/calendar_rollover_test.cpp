// Calendar rollover test: day-advance @ 0x6A06 driven through applyStepTimeTick.
//
// Verifies subday wraps into [0,0x100), day-of-year advances by exactly one per
// 256 ticks, the day/night cycle (night branch @ subday>=0x80) recovers to day,
// period flags clear at day 60/120/180, and a new year rolls day 180->1 with
// year++ (capped 999).

#include <cstdio>

#include "mm2/GameState.h"
#include "mm2/gameplay/Movement.h"

#include "mm2/gamestate.h"

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
    int fails = 0;

    static uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
    mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
    gs.initCalendarDefaults(); /* era 9, day 1, year 900, subday 1 */

    uint8_t *a4 = gs.a4();
    const int era = static_cast<int>(gs.era());
    const int32_t day_off = MM2_GS_DAY + era * 2;

    /* Force a clean subday=0 so a known number of steps crosses 0x100. */
    mm2_gs_set_u16(a4, MM2_GS_TIME_SUBDAY, 0);
    const uint16_t day0 = gs.day();

    /* 0x80 steps -> subday 0x80: night branch active. */
    for (int i = 0; i < 0x80; ++i) {
        mm2::gameplay::applyStepTimeTick(gs, 0 /* non-dark */);
    }
    expect(mm2_gs_u16(a4, MM2_GS_TIME_SUBDAY) == 0x80, "subday reaches 0x80 (night)", fails);
    expect(gs.day() == day0, "day unchanged before first full day", fails);

    /* Another 0x80 steps -> subday hits 0x100 and folds to 0; day += 1. */
    for (int i = 0; i < 0x80; ++i) {
        mm2::gameplay::applyStepTimeTick(gs, 0);
    }
    const uint16_t subday_after = mm2_gs_u16(a4, MM2_GS_TIME_SUBDAY);
    expect(subday_after < 0x100, "subday wrapped into [0,0x100)", fails);
    expect(subday_after == 0, "subday == 0 after exactly 0x100 ticks", fails);
    expect(subday_after < 0x80, "time returned to day (subday < 0x80)", fails);
    expect(gs.day() == static_cast<uint16_t>(day0 + 1), "day-of-year advanced by 1", fails);

    /* Period flags clear exactly when the day-of-year becomes 60. */
    mm2_gs_set_u16(a4, day_off, 59);
    mm2_gs_set_u16(a4, MM2_GS_TIME_SUBDAY, 0xFF);
    mm2_gs_set_u8(a4, MM2_GS_PERIOD_FLAG_A, 1);
    mm2_gs_set_u8(a4, MM2_GS_PERIOD_FLAG_B, 1);
    mm2::gameplay::applyStepTimeTick(gs, 0); /* subday 0xFF -> 0x100 -> day 60 */
    expect(gs.day() == 60, "day rolled to 60", fails);
    expect(mm2_gs_u8(a4, MM2_GS_PERIOD_FLAG_A) == 0, "period flag A cleared at day 60", fails);
    expect(mm2_gs_u8(a4, MM2_GS_PERIOD_FLAG_B) == 0, "period flag B cleared at day 60", fails);

    /* New year: day 180 -> increment past 180 wraps to 1, year++ (uncapped). */
    mm2_gs_set_u16(a4, day_off, 180);
    mm2_gs_set_u16(a4, MM2_GS_YEAR + era * 2, 900);
    mm2_gs_set_u16(a4, MM2_GS_TIME_SUBDAY, 0xFF);
    mm2::gameplay::applyStepTimeTick(gs, 0); /* day 181 -> reset to 1 */
    expect(gs.day() == 1, "new year resets day to 1", fails);
    expect(gs.year() == 901, "new year increments year", fails);

    /* Year cap at 999: stays 999, day still wraps to 1. */
    mm2_gs_set_u16(a4, day_off, 180);
    mm2_gs_set_u16(a4, MM2_GS_YEAR + era * 2, 999);
    mm2_gs_set_u16(a4, MM2_GS_TIME_SUBDAY, 0xFF);
    mm2::gameplay::applyStepTimeTick(gs, 0);
    expect(gs.day() == 1, "year-999 rollover still resets day", fails);
    expect(gs.year() == 999, "year capped at 999", fails);

    if (fails == 0) {
        std::printf("OK: calendar_rollover_test (13 checks)\n");
        return 0;
    }
    return 1;
}

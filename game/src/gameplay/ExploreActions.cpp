#include "mm2/gameplay/ExploreActions.h"

namespace mm2::gameplay {

const char *obstructionMessage(ObstructionMsg msg)
{
    /* Wording transcribed from the original table A4-$7436 (doc 43 §3). */
    switch (msg) {
    case ObstructionMsg::Barrier: return "Barrier!";
    case ObstructionMsg::Solid: return "Solid!";
    case ObstructionMsg::Locked: return "Locked!";
    case ObstructionMsg::NotLocked: return "Not Locked!";
    case ObstructionMsg::Success: return "Success!";
    case ObstructionMsg::Impassable: return "Impassable!";
    case ObstructionMsg::CantSwim: return "Can't swim!";
    default: return "";
    }
}

uint16_t Rng::raw15()
{
    /* Port LCG standing in for the 0x24048 entropy source. Numerical Recipes
     * constants; we only consume the top 15 bits to match the original's
     * [0,0x7FFF] working range used by 0x22BC6's divide. */
    state_ = state_ * 1664525u + 1013904223u;
    return static_cast<uint16_t>((state_ >> 17) & 0x7FFFu);
}

int Rng::range(int lo, int hi)
{
    /* 0x22BC6: if (max <= min) return min; else min + raw*range/0x8000. */
    if (hi <= lo) {
        return lo;
    }
    const int span = hi - lo + 1;
    const uint32_t r = raw15();
    /* scale = 0x8000 / span (integer), result = min + r / scale. Mirrors the
     * two divu.w steps; for r in [0,0x7FFF] this lands in [min,max]. */
    const uint32_t scale = 0x8000u / static_cast<uint32_t>(span);
    int q = scale ? static_cast<int>(r / scale) : 0;
    if (q >= span) {
        q = span - 1; /* guard the divide's top edge (matches clamp to [min,max]) */
    }
    return lo + q;
}

BashDecision bashDoorRoll(int might_sum, int door_strength, int roll_10_109, int trap_d100)
{
    BashDecision d{};

    /* 0x9BCA: strength = might_sum (byte). 0x9C0E: roll10 = rng(10,0x6D)/10. */
    int strength = might_sum & 0xFF;
    const int roll10 = (roll_10_109 & 0xFF) / 10;

    bool success;
    if (roll10 == 5) {
        /* 0x9C16: roll10==5 -> auto-success (success_count++ without strength cmp). */
        success = true;
    } else {
        /* 0x9C1E: strength += roll10; 0x9C2A: cmp.b door_strength -> bcs = fail. */
        strength = (strength + roll10) & 0xFF;
        success = strength >= (door_strength & 0xFF); /* unsigned byte compare */
    }

    if (!success) {
        /* 0x9C40: "Locked!" (msg index 2). */
        d.outcome = BashOutcome::Locked;
        d.msg = ObstructionMsg::Locked;
        return d;
    }

    /* 0x9C4C: trap roll d100. <0x33 -> trap springs; >=0x33 -> open + step. */
    if ((trap_d100 & 0xFF) < 0x33) {
        d.outcome = BashOutcome::TrapSprung;
        d.msg = ObstructionMsg::None; /* trap path prints no obstruction msg */
        return d;
    }

    d.outcome = BashOutcome::Opened;
    d.msg = ObstructionMsg::None; /* 0x9C66: clears lock then steps forward */
    d.clears_lock = true;
    return d;
}

UnlockDecision unlockDoorRoll(int thievery, int lock_d100, int trap_byte, int trap_d100)
{
    UnlockDecision d{};

    const int lock = lock_d100 & 0xFF;
    const int thief = thievery & 0xFF;

    /* 0x20D4A: roll >= 0x60 -> straight to trap check. 0x20D52: thievery >= roll
     * (unsigned) -> success. Otherwise fall through to trap check. */
    const bool pick_ok = (lock < 0x60) && (thief >= lock);

    if (pick_ok) {
        /* 0x20DAE: clear lock, "Success!" (msg 4). */
        d.outcome = UnlockOutcome::Success;
        d.msg = ObstructionMsg::Success;
        d.clears_lock = true;
        return d;
    }

    /* 0x20D5C: trap roll d100; 0x20D78: trap_byte < roll -> trap springs, else
     * "Locked!" (msg 2). */
    if ((trap_byte & 0xFF) < (trap_d100 & 0xFF)) {
        d.outcome = UnlockOutcome::TrapSprung;
        d.msg = ObstructionMsg::None;
        return d;
    }

    d.outcome = UnlockOutcome::Locked;
    d.msg = ObstructionMsg::Locked;
    return d;
}

}  // namespace mm2::gameplay

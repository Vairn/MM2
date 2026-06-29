#pragma once
// Faithful Bash / Unlock decision logic (doc 43 §7.1 / §7.6).
//   Bash door   @ 0x9B48 / strength roll @ 0x9BCA-0x9CAA (thunk -$7DC4).
//   Unlock door @ 0x20CA2                               (thunk -$7CCE).
//
// These are pure functions over the traced rolls/comparisons so they can be
// unit-tested deterministically. The map/door runtime state they consume in
// the original (re-bundled tile tables -$55BA/-$54BA built by map_row_sampler
// @ 0x190C, door-strength byte -$5608, trap byte -$5607, and the lock-clear
// helper -$7F02 @ 0x4B06) is NOT modelled by the port yet — callers pass the
// best-available inputs and document the gap. See GameSession::handleExplore*.

#include "mm2/CppStdCompat.h"

namespace mm2::gameplay {

/* Obstruction / status message indices into the original table A4-$7436
 * (printer 0x5918 = thunk -$7EB4), doc 43 §3. */
enum class ObstructionMsg : uint8_t {
    Barrier = 0,
    Solid = 1,
    Locked = 2,
    NotLocked = 3,
    Success = 4,
    Impassable = 5,
    CantSwim = 6,
    None = 0xFF,
};

const char *obstructionMessage(ObstructionMsg msg);

/* rng(min,max) @ 0x22BC6 (thunk -$7BB4): returns a value in [min,max] inclusive
 * (min + raw*range/0x8000, range=max-min+1). The raw entropy source 0x24048 is
 * a seeded Amiga PRNG whose exact sequence the port does not replicate yet; the
 * (min,max)-inclusive contract IS faithful. Deterministic seed for tests. */
class Rng {
public:
    Rng() : state_(0x1234ABCDu) {}
    explicit Rng(uint32_t seed) : state_(seed ? seed : 1u) {}

    /* Inclusive [lo,hi]; matches 0x22BC6 (hi<=lo returns lo). */
    int range(int lo, int hi);

private:
    uint16_t raw15();  /* 0..0x7FFF, the 0x24048 analog return range */
    uint32_t state_;
};

/* ---- Bash door (0x9BCA) -------------------------------------------------- */

enum class BashOutcome : uint8_t {
    Opened,     /* strength beat the door and the d100 trap roll missed -> door opens, step through */
    Locked,     /* strength failed -> "Locked!" */
    TrapSprung, /* strength beat the door but the trap roll hit -> victim takes damage */
};

struct BashDecision {
    BashOutcome outcome = BashOutcome::Locked;
    ObstructionMsg msg = ObstructionMsg::Locked;
    bool clears_lock = false; /* would call -$7F02 (0x4B06) in the original */
};

/* 0x9BCA strength + 0x9C4C trap roll. Inputs:
 *   might_sum     = char0 +$6B (might base) [+ char1 +$6B if party>1]
 *   door_strength = -$5608 byte (GAP: not ported)
 *   roll_10_109   = rng(10,0x6D)   @ 0x9BFE  (then /10 -> 1..10; ==5 auto-success)
 *   trap_d100     = rng(1,100)     @ 0x9C54  (<0x33 -> trap springs)
 * All comparisons are byte-wise as in the ASM. */
BashDecision bashDoorRoll(int might_sum, int door_strength, int roll_10_109, int trap_d100);

/* ---- Unlock door (0x20CA2) ----------------------------------------------- */

enum class UnlockOutcome : uint8_t {
    Success,    /* pick succeeded -> clear lock, "Success!" */
    Locked,     /* pick failed and trap held -> "Locked!" */
    TrapSprung, /* pick failed and trap byte < roll -> victim takes damage */
};

struct UnlockDecision {
    UnlockOutcome outcome = UnlockOutcome::Locked;
    ObstructionMsg msg = ObstructionMsg::Locked;
    bool clears_lock = false; /* would call -$7F02 (0x4B06) in the original */
};

/* 0x20D26 pick + 0x20D5C trap roll. Inputs:
 *   thievery    = picker roster +$1E      @ 0x20D44
 *   lock_d100   = rng(1,100)              @ 0x20D2E
 *   trap_byte   = -$5607 byte (GAP: not ported)
 *   trap_d100   = rng(1,100)              @ 0x20D64
 * Pick succeeds iff lock_d100 < 0x60 AND thievery >= lock_d100 (unsigned). */
UnlockDecision unlockDoorRoll(int thievery, int lock_d100, int trap_byte, int trap_d100);

}  // namespace mm2::gameplay

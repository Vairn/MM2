#pragma once

#include "mm2_roster_codec.h"

namespace mm2::gameplay {

/* Packed secondary-skill nibbles @ roster+0x50 (ASM: event field selector 0x6D
 * -> adda #$50 @ 0x17EA6; OP_15/18 skill buy scripts in event.dat loc 00
 * events 33–35). Low nibble = first skill id, high nibble = second (max two).
 * Skill ids 1..15 match FAQ / character-sheet names (see RosterSkillDisplay). */

constexpr int kRosterSkillPackedOffset = 0x50;

inline uint8_t rosterSkillPackedByte(const Mm2RosterRecord &rec)
{
    return reinterpret_cast<const uint8_t *>(&rec)[kRosterSkillPackedOffset];
}

inline uint8_t &rosterSkillPackedByteMut(Mm2RosterRecord &rec)
{
    return reinterpret_cast<uint8_t *>(&rec)[kRosterSkillPackedOffset];
}

bool rosterHasSkillId(const Mm2RosterRecord &rec, uint8_t skill_id);
bool rosterSkillSlotFull(const Mm2RosterRecord &rec, bool high_nibble);
void rosterGrantSkillId(Mm2RosterRecord &rec, uint8_t skill_id);
void rosterClearAllSkills(Mm2RosterRecord &rec);

}  // namespace mm2::gameplay

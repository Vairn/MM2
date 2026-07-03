#include "mm2/gameplay/RosterSkills.h"

namespace mm2::gameplay {

namespace {

bool nibbleIsSkill(uint8_t nibble)
{
    return nibble >= 1 && nibble <= 15;
}

}  // namespace

bool rosterHasSkillId(const Mm2RosterRecord &rec, uint8_t skill_id)
{
    if (!nibbleIsSkill(skill_id)) {
        return false;
    }
    const uint8_t packed = rosterSkillPackedByte(rec);
    return (packed & 0x0F) == skill_id || ((packed >> 4) & 0x0F) == skill_id;
}

bool rosterSkillSlotFull(const Mm2RosterRecord &rec, bool high_nibble)
{
    const uint8_t packed = rosterSkillPackedByte(rec);
    const uint8_t nibble = high_nibble ? static_cast<uint8_t>((packed >> 4) & 0x0F)
                                       : static_cast<uint8_t>(packed & 0x0F);
    return nibbleIsSkill(nibble);
}

void rosterGrantSkillId(Mm2RosterRecord &rec, uint8_t skill_id)
{
    if (!nibbleIsSkill(skill_id)) {
        return;
    }
    uint8_t packed = rosterSkillPackedByte(rec);
    if (!rosterSkillSlotFull(rec, false)) {
        packed = static_cast<uint8_t>((packed & 0xF0) | skill_id);
    } else if (!rosterSkillSlotFull(rec, true)) {
        packed = static_cast<uint8_t>((packed & 0x0F) | static_cast<uint8_t>(skill_id << 4));
    } else {
        return;
    }
    rosterSkillPackedByteMut(rec) = packed;
}

void rosterClearAllSkills(Mm2RosterRecord &rec)
{
    rosterSkillPackedByteMut(rec) = 0;
}

}  // namespace mm2::gameplay

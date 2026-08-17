#pragma once

#include "mm2_items_codec.h"
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

/** Persistent thievery skill @ roster+$1E (create 0x2737C / training 0x20360). */
inline uint8_t rosterThieveryBase(const Mm2RosterRecord &rec)
{
    return rec.unknown_1a_20[4];
}

/** Type-14 equipped bonus: amount nibble + flags&$3F (0xF1C0), summed over slots. */
inline int rosterEquippedThieveryBonus(const Mm2RosterRecord &rec, const Mm2ItemsFile *items)
{
    if (!items) {
        return 0;
    }
    int sum = 0;
    for (int slot = 0; slot < MM2_ROSTER_ITEM_SLOTS; ++slot) {
        const uint8_t id = rec.equipped_id[slot];
        if (id == 0) {
            continue;
        }
        const Mm2ItemRecord *irec = mm2_items_lookup(items, id);
        if (!irec) {
            continue;
        }
        const uint8_t packed = irec->bonus_byte;
        const uint8_t amount = static_cast<uint8_t>(packed & 0x0F);
        if (amount == 0 || (packed >> 4) != 14) {
            continue;
        }
        sum += static_cast<int>(amount) + static_cast<int>(rec.equipped_flags[slot] & 0x3F);
    }
    return sum;
}

/** Sheet / Unlock / Search / Hide: +$1E plus currently equipped type-14 gear.
 *  Gear is not baked into +$1E so unequip cannot drop below the class start. */
inline uint8_t rosterLiveThievery(const Mm2RosterRecord &rec, const Mm2ItemsFile *items)
{
    int v = static_cast<int>(rosterThieveryBase(rec)) + rosterEquippedThieveryBonus(rec, items);
    if (v > 255) {
        return 255;
    }
    if (v < 0) {
        return 0;
    }
    return static_cast<uint8_t>(v);
}

}  // namespace mm2::gameplay

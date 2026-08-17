#pragma once
// Pure roster/GS cheat helpers for the remake developer menu (also unit-tested).

#include "mm2/GameState.h"
#include "mm2/gameplay/SpellBook.h"

#include "mm2_gamestate.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

#include <cstdint>

namespace mm2::gameplay {

inline constexpr uint32_t kDevGrantPresets[] = {100u, 1000u, 10000u, 100000u, 1000000u, 2000000u};
inline constexpr uint32_t kDevStatPresets[] = {1u, 5u, 10u, 20u, 40u};

inline int grantAmountPresetCount()
{
    return static_cast<int>(sizeof(kDevGrantPresets) / sizeof(kDevGrantPresets[0]));
}

inline uint32_t grantAmountPreset(int idx)
{
    if (idx < 0 || idx >= grantAmountPresetCount()) {
        return kDevGrantPresets[0];
    }
    return kDevGrantPresets[idx];
}

inline int statBoostPresetCount()
{
    return static_cast<int>(sizeof(kDevStatPresets) / sizeof(kDevStatPresets[0]));
}

inline uint32_t statBoostPreset(int idx)
{
    if (idx < 0 || idx >= statBoostPresetCount()) {
        return kDevStatPresets[0];
    }
    return kDevStatPresets[idx];
}

inline Mm2RosterRecord *partyRecord(Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                                    int party_slot)
{
    if (party_slot < 0 || party_slot >= launch.party_count || party_slot >= MM2_PARTY_LAUNCH_SLOTS) {
        return nullptr;
    }
    const int idx = launch.roster_slots[party_slot];
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
        return nullptr;
    }
    return &roster.records[idx];
}

inline const Mm2RosterRecord *partyRecord(const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                                          int party_slot)
{
    if (party_slot < 0 || party_slot >= launch.party_count || party_slot >= MM2_PARTY_LAUNCH_SLOTS) {
        return nullptr;
    }
    const int idx = launch.roster_slots[party_slot];
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
        return nullptr;
    }
    return &roster.records[idx];
}

inline int partyMemberCount(const Mm2PartyLaunch &launch) { return launch.party_count; }

inline uint8_t addSatU8(uint8_t v, uint32_t delta)
{
    const uint32_t sum = static_cast<uint32_t>(v) + delta;
    return sum > 255u ? static_cast<uint8_t>(255) : static_cast<uint8_t>(sum);
}

inline int grantXpAll(Mm2RosterFile &roster, const Mm2PartyLaunch &launch, uint32_t amount)
{
    int n = 0;
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        Mm2RosterRecord *rec = partyRecord(roster, launch, i);
        if (!rec) {
            continue;
        }
        const uint64_t sum = static_cast<uint64_t>(rec->experience) + amount;
        rec->experience = sum > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(sum);
        ++n;
    }
    return n;
}

/* Hireling roster page starts at slot 24 (A–X); +$66 gold is their daily fee. */
inline bool isHirelingRosterIndex(int roster_index) { return roster_index >= 0x18; }

inline int grantGoldAll(Mm2RosterFile &roster, const Mm2PartyLaunch &launch, uint32_t amount)
{
    int n = 0;
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch.roster_slots[i];
        if (isHirelingRosterIndex(idx)) {
            continue; /* do not inflate hireling daily fee */
        }
        Mm2RosterRecord *rec = partyRecord(roster, launch, i);
        if (!rec) {
            continue;
        }
        const uint64_t sum = static_cast<uint64_t>(rec->gold) + amount;
        rec->gold = sum > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(sum);
        ++n;
    }
    return n;
}

inline int grantGemsAll(Mm2RosterFile &roster, const Mm2PartyLaunch &launch, uint32_t amount)
{
    int n = 0;
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        Mm2RosterRecord *rec = partyRecord(roster, launch, i);
        if (!rec) {
            continue;
        }
        const uint32_t sum = static_cast<uint32_t>(rec->gems) + amount;
        rec->gems = sum > 65535u ? static_cast<uint16_t>(65535) : static_cast<uint16_t>(sum);
        ++n;
    }
    return n;
}

/* Temporary only (roster +$10..+$15 / +$27), like fountain OP_1A — not base +$6B..+$73. */
inline int boostStatsAll(Mm2RosterFile &roster, const Mm2PartyLaunch &launch, uint32_t delta)
{
    int n = 0;
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        Mm2RosterRecord *rec = partyRecord(roster, launch, i);
        if (!rec) {
            continue;
        }
        rec->might_current = addSatU8(rec->might_current, delta);
        rec->intelligence_current = addSatU8(rec->intelligence_current, delta);
        rec->personality_current = addSatU8(rec->personality_current, delta);
        rec->speed_current = addSatU8(rec->speed_current, delta);
        rec->accuracy_current = addSatU8(rec->accuracy_current, delta);
        rec->luck_current = addSatU8(rec->luck_current, delta);
        rec->endurance_current = addSatU8(rec->endurance_current, delta);
        ++n;
    }
    return n;
}

inline int healReviveAll(Mm2RosterFile &roster, const Mm2PartyLaunch &launch)
{
    int n = 0;
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        Mm2RosterRecord *rec = partyRecord(roster, launch, i);
        if (!rec) {
            continue;
        }
        rec->condition = 0;
        uint16_t ceiling = rec->hp_aux;
        if (rec->hp_max > ceiling) {
            ceiling = rec->hp_max;
        }
        if (rec->hp_current > ceiling) {
            ceiling = rec->hp_current;
        }
        if (ceiling == 0) {
            ceiling = 1;
        }
        rec->hp_aux = ceiling;
        rec->hp_max = ceiling;
        rec->hp_current = ceiling;
        if (rec->sp_max == 0 && rec->spell_level > 0) {
            rec->sp_max = 4;
        }
        rec->sp_current = rec->sp_max;
        ++n;
    }
    return n;
}

inline int fillFoodAll(Mm2RosterFile &roster, const Mm2PartyLaunch &launch)
{
    int n = 0;
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        Mm2RosterRecord *rec = partyRecord(roster, launch, i);
        if (!rec) {
            continue;
        }
        rec->food = 40;
        ++n;
    }
    return n;
}

inline int maxSpellsAll(Mm2RosterFile &roster, const Mm2PartyLaunch &launch)
{
    int n = 0;
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        Mm2RosterRecord *rec = partyRecord(roster, launch, i);
        if (!rec) {
            continue;
        }
        if (spellSchoolForClass(rec->class_id) == SpellSchool::None) {
            continue;
        }
        rec->spell_level = (rec->class_id == 1 || rec->class_id == 2) ? 7 : 10;
        for (int s = 0; s < kSpellsPerSchool; ++s) {
            spellLearnInBook(*rec, s);
        }
        if (rec->sp_max == 0) {
            rec->sp_max = 40;
        }
        rec->sp_current = rec->sp_max;
        ++n;
    }
    return n;
}

inline void applyPartyBuffs(GameStateView &gs)
{
    if (!gs.valid()) {
        return;
    }
    gs.setLightFactor(255);
    gs.setMagicProtect(255);
    gs.setForcesProtect(255);
    mm2_gs_set_u8(gs.a4(), MM2_GS_LEVITATE_FLAG, 1);
    mm2_gs_set_u8(gs.a4(), MM2_GS_WALK_WATER_FLAG, 1);
    mm2_gs_set_u8(gs.a4(), MM2_GS_GUARD_DOG_FLAG, 1);
}

inline void unlockHirelings(GameStateView &gs)
{
    if (!gs.valid()) {
        return;
    }
    for (int i = 0; i < MM2_ROSTER_TAIL_EVENT_BANK_LEN; ++i) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_EVENT_VAR_BANK + i, 1);
    }
}

/** Decode event.dat triplet pos = (y<<4)|x. */
inline void decodeTripletPos(uint8_t pos, int *out_x, int *out_y)
{
    if (out_x) {
        *out_x = pos & 0x0F;
    }
    if (out_y) {
        *out_y = (pos >> 4) & 0x0F;
    }
}

inline int clampDevMonsterType(int type)
{
    if (type < 0) {
        return 0;
    }
    if (type > 255) {
        return 255;
    }
    return type;
}

inline int clampDevMonsterCount(int count)
{
    if (count < 1) {
        return 1;
    }
    if (count > MM2_GS_MONSTER_SLOT_COUNT) {
        return MM2_GS_MONSTER_SLOT_COUNT;
    }
    return count;
}

inline constexpr int kDevMapScreenCount = 60;

inline int clampDevScreenId(int screen)
{
    if (screen < 0) {
        return 0;
    }
    if (screen >= kDevMapScreenCount) {
        return kDevMapScreenCount - 1;
    }
    return screen;
}

inline constexpr int kDevBattleDifficultyCount = 4;
inline constexpr const char *kDevBattleDifficultyNames[] = {"Easy", "Normal", "Hard", "Deadly"};

inline int clampDevBattleDifficulty(int idx)
{
    if (idx < 0) {
        return 0;
    }
    if (idx >= kDevBattleDifficultyCount) {
        return kDevBattleDifficultyCount - 1;
    }
    return idx;
}

inline const char *devBattleDifficultyName(int idx)
{
    idx = clampDevBattleDifficulty(idx);
    return kDevBattleDifficultyNames[idx];
}

/* attrib.dat picker bytes (doc 35): 0x0A gate, 0x0B max tier, 0x0C min tier. */
inline constexpr int kDevAttribOffGroupGate = 0x0A;
inline constexpr int kDevAttribOffMaxMon = 0x0B;
inline constexpr int kDevAttribOffMinMon = 0x0C;

struct DevEncounterDiffPatch {
    bool applied = false;
    bool patched_attrib = false;
    uint8_t saved_disposition = 2;
    uint8_t saved_min = 1;
    uint8_t saved_max = 1;
    uint8_t saved_gate = 1;
};

/** Temporarily scale the random picker (disposition + attrib min/max/gate).
 *  Normal (1) is a no-op. Caller must restore after CombatSession::enter. */
inline void applyDevRandomDifficulty(GameStateView &gs, uint8_t *attrib_raw, int difficulty,
                                     DevEncounterDiffPatch *out)
{
    if (out) {
        *out = DevEncounterDiffPatch{};
    }
    if (!gs.valid() || !out) {
        return;
    }
    difficulty = clampDevBattleDifficulty(difficulty);
    if (difficulty == 1) {
        return;
    }

    out->applied = true;
    out->saved_disposition = gs.disposition();

    uint8_t min_m = 1;
    uint8_t max_m = 14;
    uint8_t gate = 10;
    if (attrib_raw) {
        out->patched_attrib = true;
        out->saved_min = attrib_raw[kDevAttribOffMinMon];
        out->saved_max = attrib_raw[kDevAttribOffMaxMon];
        out->saved_gate = attrib_raw[kDevAttribOffGroupGate];
        min_m = out->saved_min;
        max_m = out->saved_max;
        gate = out->saved_gate;
    }

    uint8_t disp = 2;
    if (difficulty == 0) {
        disp = 0;
        min_m = 1;
        max_m = 2;
        gate = 3;
    } else if (difficulty == 2) {
        disp = 3;
        const int raised_max = static_cast<int>(max_m) + 4;
        max_m = raised_max > 14 ? 14 : static_cast<uint8_t>(raised_max);
        if (max_m < 6) {
            max_m = 6;
        }
        const int raised_gate = static_cast<int>(gate) + 4;
        gate = raised_gate > 11 ? 11 : static_cast<uint8_t>(raised_gate);
        if (gate < 6) {
            gate = 6;
        }
    } else {
        disp = 3;
        min_m = 8;
        max_m = 14;
        gate = 11;
    }
    if (min_m > max_m) {
        min_m = max_m;
    }

    mm2_gs_set_u8(gs.a4(), MM2_GS_DISPOSITION, disp);
    if (attrib_raw && out->patched_attrib) {
        attrib_raw[kDevAttribOffMinMon] = min_m;
        attrib_raw[kDevAttribOffMaxMon] = max_m;
        attrib_raw[kDevAttribOffGroupGate] = gate;
    }
}

inline void restoreDevRandomDifficulty(GameStateView &gs, uint8_t *attrib_raw,
                                       const DevEncounterDiffPatch &patch)
{
    if (!patch.applied) {
        return;
    }
    if (gs.valid()) {
        mm2_gs_set_u8(gs.a4(), MM2_GS_DISPOSITION, patch.saved_disposition);
    }
    if (patch.patched_attrib && attrib_raw) {
        attrib_raw[kDevAttribOffMinMon] = patch.saved_min;
        attrib_raw[kDevAttribOffMaxMon] = patch.saved_max;
        attrib_raw[kDevAttribOffGroupGate] = patch.saved_gate;
    }
}

/** Seed a random picker fight (mode 0, empty slots) — CombatSession::enter runs 0x1213E. */
inline void seedDevRandomEncounter(GameStateView &gs)
{
    if (!gs.valid()) {
        return;
    }
    uint8_t *a4 = gs.a4();
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0);
    mm2_gs_set_u16(a4, MM2_GS_ENCOUNTER_REDRAW, 0);
    for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, 0);
    }
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, 0);
}

/** Seed a fixed OP_12-style fight: `count` copies of `type`, mode 0x80. */
inline void seedDevFixedEncounter(GameStateView &gs, uint8_t type, int count)
{
    if (!gs.valid()) {
        return;
    }
    count = clampDevMonsterCount(count);
    uint8_t *a4 = gs.a4();
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_MODE, 0x80);
    mm2_gs_set_u16(a4, MM2_GS_ENCOUNTER_REDRAW, 0);
    for (int i = 0; i < MM2_GS_MONSTER_SLOT_COUNT; ++i) {
        mm2_gs_set_u8(a4, MM2_GS_MONSTER_SLOTS + i, i < count ? type : 0);
    }
    mm2_gs_set_u8(a4, MM2_GS_ENCOUNTER_OVERFLOW_TYPE, 0);
    mm2_gs_set_u8(a4, MM2_GS_MONSTER_COUNT, static_cast<uint8_t>(count));
}

}  // namespace mm2::gameplay

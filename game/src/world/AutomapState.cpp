#include "mm2/world/AutomapState.h"

namespace mm2::world {

void AutomapState::clearAll()
{
    for (AutomapScreenVis &s : screens_) {
        for (uint16_t &row : s.rows) {
            row = 0;
        }
    }
}

bool AutomapState::isVisited(int screen, int x, int y) const
{
    if (screen < 0 || screen >= MM2_MAP_SCREEN_COUNT || x < 0 || y < 0 ||
        x >= MM2_MAP_GRID_DIM || y >= MM2_MAP_GRID_DIM) {
        return false;
    }
    return (screens_[screen].rows[y] & static_cast<uint16_t>(1u << x)) != 0;
}

void AutomapState::markVisited(int screen, int x, int y)
{
    if (screen < 0 || screen >= MM2_MAP_SCREEN_COUNT || x < 0 || y < 0 ||
        x >= MM2_MAP_GRID_DIM || y >= MM2_MAP_GRID_DIM) {
        return;
    }
    screens_[screen].rows[y] |= static_cast<uint16_t>(1u << x);
}

bool AutomapState::rosterHasSkillId(const Mm2RosterRecord &rec, int skill_id)
{
    if (skill_id <= 0 || skill_id > 15) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        const uint8_t byte = rec.secondary_skills[i];
        if ((byte & 0x0F) == static_cast<uint8_t>(skill_id)) {
            return true;
        }
        if (((byte >> 4) & 0x0F) == static_cast<uint8_t>(skill_id)) {
            return true;
        }
    }
    return false;
}

bool AutomapState::partyHasCartographer(const Mm2RosterFile &roster, const Mm2PartyLaunch &launch)
{
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
        const int idx = launch.roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        const Mm2RosterRecord &rec = roster.records[idx];
        if (rec.condition >= 0x81) {
            continue;
        }
        if (rosterHasSkillId(rec, kAutomapSkillCartographer)) {
            return true;
        }
    }
    return false;
}

void AutomapState::markPartyTileIfCartographer(int screen, int x, int y, const Mm2RosterFile &roster,
                                               const Mm2PartyLaunch &launch)
{
    if (!partyHasCartographer(roster, launch)) {
        return;
    }
    markVisited(screen, x, y);
}

}  // namespace mm2::world

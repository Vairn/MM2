#include "mm2/world/AutomapState.h"

#include "mm2/gameplay/RosterSkills.h"

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
    return gameplay::rosterHasSkillId(rec, static_cast<uint8_t>(skill_id));
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

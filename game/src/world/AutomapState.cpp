#include "mm2/world/AutomapState.h"

#include "mm2/gameplay/RosterSkills.h"

namespace mm2::world {

namespace {

bool tileInRange(int screen, int x, int y)
{
    return screen >= 0 && screen < MM2_MAP_SCREEN_COUNT && x >= 0 && y >= 0 &&
           x < MM2_MAP_GRID_DIM && y < MM2_MAP_GRID_DIM;
}

}  // namespace

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
    if (!tileInRange(screen, x, y)) {
        return false;
    }
    return (screens_[screen].rows[y] & automapColMask(x)) != 0;
}

void AutomapState::markVisited(int screen, int x, int y)
{
    if (!tileInRange(screen, x, y)) {
        return;
    }
    screens_[screen].rows[y] |= automapColMask(x);
}

void AutomapState::markScreenVisited(int screen)
{
    if (screen < 0 || screen >= MM2_MAP_SCREEN_COUNT) {
        return;
    }
    for (int y = 0; y < MM2_MAP_GRID_DIM; ++y) {
        screens_[screen].rows[y] = 0xFFFFu;
    }
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
        if (gameplay::rosterHasSkillId(rec, static_cast<uint8_t>(kAutomapSkillCartographer))) {
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

void AutomapState::loadFromRosterTail(const Mm2RosterFile &roster)
{
    static_assert(MM2_MAP_SCREEN_COUNT * MM2_MAP_GRID_DIM * 2 == MM2_ROSTER_TAIL_AUTOMAP_SIZE,
                  "automap tail size must be 60 screens × 16 row-words");
    for (int s = 0; s < MM2_MAP_SCREEN_COUNT; ++s) {
        for (int y = 0; y < MM2_MAP_GRID_DIM; ++y) {
            screens_[s].rows[y] = mm2_roster_tail_u16(
                &roster, MM2_ROSTER_TAIL_AUTOMAP + (s * MM2_MAP_GRID_DIM + y) * 2);
        }
    }
}

void AutomapState::saveToRosterTail(Mm2RosterFile &roster) const
{
    for (int s = 0; s < MM2_MAP_SCREEN_COUNT; ++s) {
        for (int y = 0; y < MM2_MAP_GRID_DIM; ++y) {
            mm2_roster_tail_set_u16(&roster,
                                    MM2_ROSTER_TAIL_AUTOMAP + (s * MM2_MAP_GRID_DIM + y) * 2,
                                    screens_[s].rows[y]);
        }
    }
}

}  // namespace mm2::world

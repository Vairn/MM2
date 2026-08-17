#pragma once
// Per-screen explored tile flags (A4-$4F4C), marked by title_encounter_gate @0x28CE
// when count_active_party_nibble_matches(3) @0x4614 returns nonzero (Cartographer).

#include "mm2/CppStdCompat.h"

#include "mm2_map_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::world {

constexpr int kAutomapSkillCartographer = 3;

/** Column mask from DATA table A4-$75EE (0x2182 / 0x28CE): 8000 4000 … 0001. */
inline uint16_t automapColMask(int x)
{
    return static_cast<uint16_t>(1u << (15 - x));
}

struct AutomapScreenVis {
    uint16_t rows[MM2_MAP_GRID_DIM]{};
};

class AutomapState {
public:
    void clearAll();

    bool isVisited(int screen, int x, int y) const;
    void markVisited(int screen, int x, int y);
    /** Developer cheat: mark every tile on one screen as visited. */
    void markScreenVisited(int screen);

    /** Mark party tile when a conscious Cartographer is in the launch roster. */
    void markPartyTileIfCartographer(int screen, int x, int y, const Mm2RosterFile &roster,
                                     const Mm2PartyLaunch &launch);

    /** roster.dat global tail +$044: 60×16 LE row-words (save @0x89CA / load @0x8418). */
    void loadFromRosterTail(const Mm2RosterFile &roster);
    void saveToRosterTail(Mm2RosterFile &roster) const;

    static bool partyHasCartographer(const Mm2RosterFile &roster, const Mm2PartyLaunch &launch);
    static bool rosterHasSkillId(const Mm2RosterRecord &rec, int skill_id);

private:
    AutomapScreenVis screens_[MM2_MAP_SCREEN_COUNT]{};
};

}  // namespace mm2::world

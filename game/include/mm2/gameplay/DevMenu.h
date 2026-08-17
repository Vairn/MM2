#pragma once
// Remake developer overlay (PC F11 / Amiga Help) — not ASM-faithful retail UI.

#include "mm2/gameplay/DevMenuGrants.h"

#include "mm2/combat/CombatSession.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/gfx/AutomapView.h"
#include "mm2/gfx/EnvAssets.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/platform/Platform.h"
#include "mm2/world/AutomapState.h"
#include "mm2/world/MapWorld.h"

#include "mm2_monsters_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

#include <cstdint>

namespace mm2::gameplay {

enum class DevMenuPage : uint8_t {
    Cheats = 0,
    Battle,
    Movement,
    Teleport,
    StatusVm,
    MapEvents,
    Monsters,
    Spells,
    Count,
};

enum class DevCheatRow : uint8_t {
    AddXp = 0,
    AddGold,
    AddGems,
    BoostStats,
    HealRevive,
    FillFood,
    MaxSpells,
    PartyBuffs,
    RevealMap,
    UnlockHirelings,
    Count,
};

enum class DevMonsterRow : uint8_t {
    InstantWin = 0,
    InstantFlee,
    SleepAll,
    HealParty,
    KillSelected,
    Invulnerable,
    Count,
};

enum class DevBattleRow : uint8_t {
    Random = 0,
    Difficulty,
    Type,
    Qty,
    SpawnFixed,
    Count,
};

enum class DevMoveRow : uint8_t {
    Noclip = 0,
    NoEncounters,
    SkipEvents,
    Count,
};

enum class DevMenuAction : uint8_t {
    None = 0,
    Close,
    SpawnCombat,
    Teleport,
};

struct DevMenuState {
    DevMenuPage page = DevMenuPage::Cheats;
    int cheat_row = 0;
    int monster_row = 0;
    int monster_slot = 0;
    int amount_idx_xp = 5;   /* default 2,000,000 */
    int amount_idx_gold = 4; /* default 1,000,000 */
    int amount_idx_gems = 2; /* default 1,000 */
    int amount_idx_stats = 3; /* default +20 */
    int map_cursor_x = 0;
    int map_cursor_y = 0;
    int map_triplet_scroll = 0;
    bool map_cursor_seeded = false;
    bool enter_was_down = false;
    int spell_party_slot = 0;
    int spell_row = 0;
    int battle_row = 0;
    int battle_type = 1;
    int battle_count = 1;
    int battle_difficulty = 1; /* 0 Easy, 1 Normal, 2 Hard, 3 Deadly */
    int move_row = 0;
    int teleport_screen = 0;
    int teleport_x = 0;
    int teleport_y = 0;
    bool teleport_use_spawn = true;
    /* Remake-only explore cheats — persist across F11 open/close. */
    bool noclip = false;
    bool no_encounters = false;
    bool skip_events = false;
    char status_line[48]{};
};

class DevMenu {
public:
    void reset(DevMenuState &st) const;
    void render(gfx::ScreenCompositor &c, DevMenuState &st, GameStateView &gs,
                Mm2RosterFile &roster, const Mm2PartyLaunch &launch, const world::MapWorld &world,
                world::AutomapState &automap, const gfx::EnvAssets &env,
                const events::EventRuntime &events, const combat::CombatSession &combat,
                const Mm2MonstersFile *monsters, const char *screen_label) const;

    DevMenuAction handleKey(const platform::KeyState &keys, DevMenuState &st, GameStateView &gs,
                            Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                            const world::MapWorld &world, world::AutomapState &automap,
                            const events::EventRuntime &events, combat::CombatSession &combat) const;
};

}  // namespace mm2::gameplay

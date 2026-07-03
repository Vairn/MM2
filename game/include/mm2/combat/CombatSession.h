#pragma once
// Combat session — faithful port of the encounter-entry + round-loop shell:
//   -$7EDE -> 0x051C2  combat_encounter_entry (banner + turn-order prep)
//   0x12CE0            live-count recompute
//   0x1213E/.. 0x11F0A random picker (mm2/combat/EncounterPicker.h)
//   0x12A22            round loop / initiative scan
//   0x12430            combat_victory_rewards (XP split, victory latch)
//   0x11646            combat_defeat_retreat (ends the fight when the whole
//                       party is down; also used by a successful Run)
//
// Per-hit to-hit/damage math is NOT ASM-confirmed yet (doc 17 "to-hit/damage
// math" open item, deferred to plan Phase 5). This MVP round loop uses a
// documented, simplified roll (see CombatSession.cpp) so OP_12/13/arena/rest/
// step encounters can actually resolve to a win/lose outcome and drive
// victory rewards + OP_2B. Monster HP/XP ARE ASM-confirmed (0x4C8E unpacker,
// mm2_monster_decode_hp/_xp, doc 16). See EXTRACTED/docs/17-combat-system.md
// and 35-encounter-tables.md for the full ASM trace.

#include "mm2/GameState.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2/platform/Platform.h"
#include "mm2/world/MapWorld.h"

#include "mm2/gfx/CombatPanel.h"
#include "mm2_monsters_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::combat {

enum class CombatState : uint8_t {
    Inactive,                 /* no fight running */
    AwaitingSurpriseDismiss,  /* surprise banner; any key continues (0x12F4E) */
    AwaitingPartyOptions,     /* party-level A/B/H/R before round loop (0x12F74) */
    AwaitingCommand,          /* a living party member's turn; waiting for A/B/R */
};

enum class CombatOutcome : uint8_t { None, Victory, Fled, Defeated };

struct CombatMonster {
    uint8_t type = 0;
    int hp = 0;
    int max_hp = 0;
    int speed = 0;
    bool alive = false;
};

class CombatSession {
public:
    void bindParty(Mm2RosterFile *roster, const Mm2PartyLaunch *launch)
    {
        roster_ = roster;
        launch_ = launch;
    }
    void bindMonsters(const Mm2MonstersFile *monsters) { monsters_ = monsters; }
    void bindRng(gameplay::Rng *rng) { rng_ = rng; }

    /** True while a fight is running (blocks exploration input, like a scripted scene). */
    bool active() const { return state_ != CombatState::Inactive; }
    bool awaitingCommand() const { return state_ == CombatState::AwaitingCommand; }
    bool awaitingPartyOptions() const { return state_ == CombatState::AwaitingPartyOptions; }
    CombatOutcome lastOutcome() const { return outcome_; }

    /** Arena Games ticket combat (OP_0E selector 0x08 / asm 0x9F04-0x9F2C):
     *  call before enter() so a Victory grants the color/screen gold table
     *  (eventVmArenaGoldReward) to the first eligible party member. */
    void armArenaReward(uint8_t color, int screen) { arena_reward_ = {true, color, screen}; }

    /** -$7EDE(a4) -> 0x051C2 + setup. Call wherever the ROM calls the combat
     *  thunk (OP_12/13, arena ticket, rest ambush, step random, tile
     *  collision) right after the caller seeds MM2_GS_MONSTER_SLOTS /
     *  MM2_GS_ENCOUNTER_MODE / MM2_GS_ENCOUNTER_OVERFLOW_TYPE /
     *  MM2_GS_MONSTER_COUNT (right before it would set MM2_GS_SCRIPT_ABORT). */
    /** Returns false when bindings/data are missing (no fight fabricated). */
    bool enter(GameStateView &gs, const world::MapWorld &world);

    /** Advance the round loop: resolves automatic monster/initiative turns,
     *  and the player's command (A=Attack, B=Block, R=Run) when awaiting
     *  one. No-op when !active(). Returns true the tick combat ended
     *  (caller should check lastOutcome() and resume the event VM). */
    bool tick(GameStateView &gs, const world::MapWorld &world, const platform::KeyState &keys);

    /** One-line status text for the play-screen status line (doc 17 defers
     *  the full book.32 combat panel — plan Phase 3/5). */
    const char *statusLine() const { return status_line_; }

    /** Snapshot for gfx::drawCombatMonsterList / drawCombatOptionsBar. */
    gfx::CombatPanelView panelView() const;

    /** Party slot (0..7) whose turn is active, or -1. */
    int activePartySlot() const { return active_party_slot_; }

    /** Monster slot (0..10) currently acting in the round loop (A4-$4F7), or -1. */
    int activeMonsterSlot() const { return active_monster_slot_; }

private:
    struct ArenaReward {
        bool active = false;
        uint8_t color = 0;
        int screen = 0;
    };

    void beginRound(GameStateView &gs);
    void beginEncounterUi(GameStateView &gs, const world::MapWorld &world);
    void applySurpriseRoll(GameStateView &gs);
    void startRoundLoop(GameStateView &gs, const world::MapWorld &world);
    void resolvePartyAttack(GameStateView &gs, const world::MapWorld &world);
    void resolvePartyBribe(GameStateView &gs);
    void resolvePartyHide(GameStateView &gs, const world::MapWorld &world);
    void resolvePartyRun(GameStateView &gs, const world::MapWorld &world);
    void runUntilDecisionOrEnd(GameStateView &gs, const world::MapWorld &world);
    bool checkOutcome(GameStateView &gs, const world::MapWorld &world);
    void resolvePlayerAttack(GameStateView &gs);
    void resolvePlayerBlock();
    void resolvePlayerRun(GameStateView &gs, const world::MapWorld &world);
    void resolveMonsterTurn(GameStateView &gs, int slot);
    void finishVictory(GameStateView &gs);
    void finishLeave(GameStateView &gs, bool fled);
    void syncMonsterSlotsToGs(GameStateView &gs) const;
    int firstAliveMonster() const;
    int rosterIndexForPartySlot(int party_slot) const;
    /** Can act this round (MVP: hp > 0). */
    bool partySlotAlive(int party_slot) const;
    /** 0x13282: roster+$26 <= $10 — still in the fight (incl. unconscious). */
    bool partySlotInFight(int party_slot) const;
    /** 0x12430: roster+$26 < $80 — eligible for end-of-fight XP/gold share. */
    bool partySlotEligibleForRewards(int party_slot) const;
    void setStatus(const char *msg);

    CombatState state_ = CombatState::Inactive;
    CombatOutcome outcome_ = CombatOutcome::None;

    Mm2RosterFile *roster_ = nullptr;
    const Mm2PartyLaunch *launch_ = nullptr;
    const Mm2MonstersFile *monsters_ = nullptr;
    gameplay::Rng *rng_ = nullptr;

    CombatMonster slots_[MM2_GS_MONSTER_BATTLE_SLOTS]{};
    bool monster_acted_[MM2_GS_MONSTER_BATTLE_SLOTS]{};
    bool party_acted_[MM2_GS_PARTY_SIZE]{};
    bool party_blocking_[MM2_GS_PARTY_SIZE]{};

    int active_party_slot_ = -1; /* party slot (0..7) awaiting a command */
    int active_monster_slot_ = -1; /* monster slot (0..10) acting this step (A4-$4F7) */
    int encounter_live_total_ = 0; /* roster size at fight start (for "+N more") */
    uint8_t overflow_type_ = 0;
    uint8_t surprise_mode_ = 0;  /* 2 = party surprised, 3 = monsters surprised */
    uint32_t xp_pool_ = 0;       /* -$119E: combat XP accrued from kills this fight */
    uint8_t saved_panel_mode_ = 0;
    ArenaReward arena_reward_{};

    char status_line_[160] = {};
};

}  // namespace mm2::combat

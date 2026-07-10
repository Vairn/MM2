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
#include "mm2_items_codec.h"
#include "mm2/gameplay/Movement.h"

namespace mm2::combat {

enum class CombatState : uint8_t {
    Inactive,                 /* no fight running */
    AwaitingSurpriseDismiss,  /* surprise banner; any key continues (0x12F4E) */
    AwaitingPartyOptions,     /* party-level A/B/H/R before round loop (0x12F74) */
    AwaitingBribeKind,        /* 0x12FB8: 1-Food 2-Gold 3-Gems / ESC */
    AwaitingBribeAmount,      /* 0x3EE0 digit entry after kind (max 4 digits) */
    AwaitingCommand,          /* a living party member's turn; waiting for A/B/R */
    AwaitingCastLevel,        /* combat 'C' @ 0x11A90 → 0x79EE level digit (no spell grid) */
    AwaitingCastNumber,       /* 0x79EE number digit after level */
    AwaitingCastTarget,       /* 0xD43C / 0xD464: 'A'..'J' monster letter (ESC=$1B cancel) */
    AwaitingPartyPick,        /* 0xD2EA / 0xD312: '1'..'N' party member (Heroism/Frenzy) */
    AwaitingActionAck,        /* action message shown; any key advances the round loop */
};

enum class CombatOutcome : uint8_t { None, Victory, Fled, Defeated };

struct CombatMonster {
    uint8_t type = 0;
    uint8_t status = 0; /* A4-$519[slot]: bit0 awake; Sleep/Web OR bits via -$6F2E */
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
    void bindItems(const Mm2ItemsFile *items) { items_ = items; }
    void bindRng(gameplay::Rng *rng) { rng_ = rng; }

    /** True while a fight is running (blocks exploration input, like a scripted scene). */
    bool active() const { return state_ != CombatState::Inactive; }
    bool awaitingCommand() const { return state_ == CombatState::AwaitingCommand; }
    bool awaitingPartyOptions() const { return state_ == CombatState::AwaitingPartyOptions; }
    /** True while combat is blocked on a key (party options / command / cast / ack / surprise). */
    bool awaitingInput() const
    {
        return state_ == CombatState::AwaitingSurpriseDismiss || state_ == CombatState::AwaitingPartyOptions ||
               state_ == CombatState::AwaitingBribeKind || state_ == CombatState::AwaitingBribeAmount ||
               state_ == CombatState::AwaitingCommand || state_ == CombatState::AwaitingCastLevel ||
               state_ == CombatState::AwaitingCastNumber || state_ == CombatState::AwaitingCastTarget ||
               state_ == CombatState::AwaitingPartyPick || state_ == CombatState::AwaitingActionAck;
    }
    CombatState state() const { return state_; }
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

    /** Round-loop acted flag for party slot i (initiative skip @ 0x12BBE). */
    bool partySlotActed(int party_slot) const
    {
        return party_slot >= 0 && party_slot < MM2_GS_PARTY_SIZE && party_acted_[party_slot];
    }

    /** Combat strip check glyph @ 0x12892: slot < front-rank cutoff A4-$5E4D. */
    bool partySlotInFrontRank(int party_slot) const
    {
        return party_slot >= 0 && party_slot < front_rank_count_;
    }

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
    /** combat_state_recompute @ 0x11D0C: roll A4-$524 / A4-$5E4D at round-loop entry. */
    void recomputeRangeCounts(GameStateView &gs);
    /** Command capability flags @ 0x11866 for the active character. */
    void commandFlagsForActiveSlot(bool &melee, bool &shoot, bool &cast) const;
    void applySurpriseRoll(GameStateView &gs);
    void startRoundLoop(GameStateView &gs, const world::MapWorld &world);
    void resolvePartyAttack(GameStateView &gs, const world::MapWorld &world);
    void beginBribeSubmenu(GameStateView &gs);
    bool tickBribeSubmenu(GameStateView &gs, char key, bool escape);
    void resolveBribeTry(GameStateView &gs);
    void resolvePartyHide(GameStateView &gs, const world::MapWorld &world);
    void resolvePartyRun(GameStateView &gs, const world::MapWorld &world);
    void runUntilDecisionOrEnd(GameStateView &gs, const world::MapWorld &world);
    bool checkOutcome(GameStateView &gs, const world::MapWorld &world);
    void resolvePlayerAttack(GameStateView &gs, bool shooting);
    void resolvePlayerBlock();
    void resolvePlayerRun(GameStateView &gs, const world::MapWorld &world);
    /** Combat cast picker @ 0x11A90 / 0x79EE — level then number, no LAB_6622 grid. */
    void beginCastPicker();
    bool tickCastPicker(GameStateView &gs, char key);
    void resolvePlayerCast(GameStateView &gs, int flat0);
    /** 0xD464 letter pick → 0x133B6 damage word → 0x108BC apply to slot(s). */
    bool tickCastTarget(GameStateView &gs, char key);
    /** 0xD2EA: digit '1'..party_count → Heroism/Frenzy target. */
    bool tickPartyPick(GameStateView &gs, char key);
    void applyCastToMonsterTarget(GameStateView &gs, int slot, int flat0);
    void applyHeroismToPartySlot(GameStateView &gs, int party_slot);
    void applyFrenzyToPartySlot(GameStateView &gs, int party_slot);
    /** 0xC028 Turn Undead / Holy Word: arg0=all undead, else rng(1,arg)>=type. */
    int applyTurnUndeadC050(GameStateView &gs, uint8_t arg);
    /** 0x133B6: -$53E += (rng(1,sides)|0) + addend, once per caster level. */
    uint16_t rollSpellDamage133B6(GameStateView &gs, int level, uint8_t sides, uint8_t addend);
    /** 0x1333A: Sleep/Web/Silence hit-count helper. */
    uint8_t spellTargetPower13362(GameStateView &gs, int level) const;
    /** 0x4C8E: mres/undead + full -$11AA[0..6] flag bank for slot. */
    void loadMonsterCombatGlobals(GameStateView &gs, int slot);
    /** Seed data-hunk tables -$6F2E / -$7464 into GS (once per fight). */
    void seedCombatStaticTables(GameStateView &gs);
    /** 0x10894: resist/halve/status/damage; messages + 0x10ED4 HP apply. */
    void applySpell108BC(GameStateView &gs, int start_slot, int hit_count, uint8_t mode_d);
    /** 0x10ED4: status wake + HP subtract / kill; returns true if killed. */
    bool applyHp10ED4(GameStateView &gs, int slot, uint16_t dmg);
    /** 0x10CCE: shift parallel monster arrays left from slot; dec live. */
    void compactMonsterSlot(GameStateView &gs, int slot);
    /** 0x10E5E: drink-match + 0x10B74 treasure/XP + compact + overflow fold. */
    void onMonsterKilled(GameStateView &gs, int slot, uint32_t kill_xp);
    /** 0x10B74: gems/gold into found buffer; item-tier scratch -$5E28/-$5E29. */
    void applyTreasure10B74(GameStateView &gs, uint8_t mon_type);
    /** 0x10DFC host: "<name> goes down!" or " runs away!" via -$5E27. */
    void printMonsterDown10DFC(GameStateView &gs, int slot, const char *mon_name);
    /** 0x1075E: flee print + compact, no treasure/XP. */
    void monsterFlee1075E(GameStateView &gs, int slot);
    /** 0x123B0/0x12212: fill FOUND_ITEM_* from -$5E28/-$5E29. */
    void generateVictoryItems123B0(GameStateView &gs);
    void fillFoundItemSlot12212(GameStateView &gs, int slot_index);
    /** 0x132E6 host: queue per-target combat messages for ack pacing. */
    void enqueueCombatMessage(const char *msg);
    bool advanceCombatMessageQueue();
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
    const Mm2ItemsFile *items_ = nullptr;
    gameplay::Rng *rng_ = nullptr;

    CombatMonster slots_[MM2_GS_MONSTER_BATTLE_SLOTS]{};
    bool monster_acted_[MM2_GS_MONSTER_BATTLE_SLOTS]{};
    bool party_acted_[MM2_GS_PARTY_SIZE]{};
    bool party_blocking_[MM2_GS_PARTY_SIZE]{};

    int active_party_slot_ = -1; /* party slot (0..7) awaiting a command */
    int active_monster_slot_ = -1; /* monster slot (0..10) acting this step (A4-$4F7) */
    int melee_range_count_ = 0;  /* A4-$524: monsters within melee reach (0x11D0C) */
    int front_rank_count_ = 0;   /* A4-$5E4D: party front-rank cutoff (0x11D0C) */
    int encounter_live_total_ = 0; /* roster size at fight start (for "+N more") */
    int cast_level_ = 0;         /* 1..9 while AwaitingCastNumber (0x79EE) */
    int pending_cast_flat_ = -1; /* flat0 while AwaitingCastTarget / AwaitingPartyPick */
    int cast_target_slot_ = -1;  /* 0..9 after 'A'..'J' (0xD500 subi #$41) */
    int bribe_kind_ = 0;         /* 1=food 2=gold 3=gems (0x12FB8) */
    int bribe_digits_ = 0;       /* 0..4 entered digits for 0x3EE0 */
    uint16_t bribe_amount_ = 0;  /* parsed amount (gold packs hi+lo bytes) */
    uint8_t bribe_roll_ = 0;     /* rng(1,100) latched at party-options entry */
    uint8_t bribe_demand_ = 0;   /* attrib+0x11 → A4-$5609 @ 0x12F58 */
    uint8_t overflow_type_ = 0;
    uint8_t surprise_mode_ = 0;  /* 2 = party surprised, 3 = monsters surprised */
    uint32_t xp_pool_ = 0;       /* -$6FC6: combat XP accrued from kills this fight */
    uint8_t saved_panel_mode_ = 0;
    ArenaReward arena_reward_{};

    char status_line_[160] = {};
    /* 0x132E6 host: multi-target spell messages paced by AwaitingActionAck. */
    static constexpr int kMsgQueueCap = 12;
    char msg_queue_[kMsgQueueCap][160]{};
    int msg_queue_len_ = 0;
    int msg_queue_idx_ = 0;
};

}  // namespace mm2::combat

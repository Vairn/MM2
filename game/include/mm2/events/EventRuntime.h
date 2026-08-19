#pragma once
// event.dat runtime: loader @ 0x92F2, init @ 0x1754A, scanner @ 0x175E2, VM @ 0x172CA.

#include "mm2/GameState.h"
#include "mm2/events/EventOp.h"
#include "mm2/combat/CombatSession.h"
#include "mm2/events/EventTextView.h"
#include "mm2/gameplay/ExploreActions.h"
#include "mm2/platform/Platform.h"
#include "mm2/world/MapWorld.h"

#include "mm2_event_codec.h"
#include "mm2_gamestate.h"
#include "mm2_items_codec.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::events {

class ITownServiceUi;

enum class EventVmWait : uint8_t {
    None = 0,
    Space,
    YesNo,
    MemberSelect,
    /** OP_2F @ 0x16FEA: -$7F92 fills A4-$5C50 (10 chars, space-padded). */
    Answer,
    /** OP_0E 0x7E free-teleport @ 0xD576: hex digit 0..F via -$7F8C. */
    HexDigit,
    /** OP_0E 0xC9/0xCA @ 0x1980A: A–D quest difficulty pick. */
    LetterSelect,
    /** OP_1E @ 0x16780: timed wait (~arg/50 s, skip on a fresh key). Also used
     *  as a port hop-dwell after silent OP_0D 0x09 / OP_0C cruise teleports. */
    Delay,
};

class EventRuntime {
public:
    bool load(const char *data_dir);
    void unload();
    const char *dataDir() const { return data_dir_; }

    /** Copy current location record into work buf and init parse anchors (0x92F2 + 0x1754A). */
    bool enterLocation(int location_id, GameStateView &gs, const world::MapWorld &world);

    /** After movement when -$7952 latch set: tile scanner + run VM until wait/end. */
    bool scanAndRun(GameStateView &gs, world::MapWorld &world);

    /** Resume VM after SPACE or Y/N; returns false when script finished. */
    bool continueInput(GameStateView &gs, world::MapWorld &world, const platform::KeyState &keys);

    /** Host funeral / GameOver: drop Yes/No waits and console OP_02 layers so a
     *  post-wipe tile re-prompt cannot stack under the death band. */
    void haltUiForHostOverlay(GameStateView &gs);

    bool isActive() const { return script_active_; }
    bool blocksMovement() const { return script_active_ || wait_ != EventVmWait::None; }

    /** Set when OP_0C map transition ran; caller reloads env + 3D assets. */
    bool screenChanged() const { return screen_changed_; }
    void clearScreenChanged() { screen_changed_ = false; }
    /** Host (spell-driven) screen transition: mirror OP_0C so the caller
     *  reloads env + 3D assets for the new screen (Fly / recall teleports). */
    void markScreenChanged() { screen_changed_ = true; }

    EventTextView &textView() { return text_; }
    const EventTextView &textView() const { return text_; }

    EventVmWait waitKind() const { return wait_; }

    /** Optional party roster for gold/item checks (OP_24/OP_28). */
    void bindParty(Mm2RosterFile *roster, const Mm2PartyLaunch *launch);

    /** Optional items.dat for blacksmith (OP_0E 0x06) pricing. */
    void bindItems(const Mm2ItemsFile *items) { items_ = items; }
    const Mm2ItemsFile *items() const { return items_; }

    /** Town-service UI (temple/training). Null: str.dat intro only. */
    void bindTownServiceUi(ITownServiceUi *ui) { town_service_ui_ = ui; }
    ITownServiceUi *townServiceUi() const { return town_service_ui_; }

    /** rng(min,max) @ 0x22BC6 for OP_0E 0x08 arena (0x9D76 rng(1,16) -$7BB4).
     *  Null → fixed roll of 1. */
    void bindRng(gameplay::Rng *rng) { rng_ = rng; }
    gameplay::Rng *rng() const { return rng_; }

    /** Optional combat session: when bound, OP_12/OP_13 (and the arena ticket
     *  encounter routed through eventExecTownSelector) actually run the fight
     *  instead of only seeding A4 and aborting the script. */
    void bindCombat(combat::CombatSession *combat) { combat_ = combat; }
    combat::CombatSession *combat() const { return combat_; }

    /** After a service intro Y/N (OP_0E 0x02/0x03/0x04/0x06), open the bound
     *  town-service menu on "yes" (ASM: handler shell gates on A4-$7951). */
    enum class PendingTownMenu : uint8_t {
        None = 0,
        Tavern,
        Temple,
        Smith,
        Training,
        Inn,
        SkillBuy,
        GeneralStore,
        Circus,
    };
    void setPendingTownMenu(PendingTownMenu kind) { pending_town_menu_ = kind; }
    void setPendingSkillBuy(uint8_t skill_id, uint32_t gold_cost)
    {
        pending_skill_id_ = skill_id;
        pending_skill_cost_ = gold_cost;
        pending_town_menu_ = PendingTownMenu::SkillBuy;
    }
    void setPendingGeneralStore()
    {
        /* Y/N wait only — member prompt is armed in finishPendingTownMenu on accept. */
        pending_town_menu_ = PendingTownMenu::GeneralStore;
    }
    bool hasPendingTownMenu() const { return pending_town_menu_ != PendingTownMenu::None; }

    /** OP_0E 0x7E @ 0xD576: prompt X then Y (0–15 hex). */
    void armFreeTeleportUi();
    /** OP_0E 0xCF @ 0x1480A: Pinehurst Wayback machine, "What era do you desire (1-8)?" */
    void armWaybackMachineUi();
    /** OP_0E 0xC9/0xCA @ 0x19AB4/0x19AC4 → 0x1980A: Hoardall/Slayer Y/N then A–D. */
    void armQuestEncodeUi(bool drink);
    /** OP_0E 0xFD abort==2 / inn accept: arm Goto Town @ 0x1A1F8. */
    void armInnGotoTown() { pending_inn_goto_town_ = true; }
    /** OP_0E 0xFD abort==3 → 0x14106 Death Strikes panel, then Goto Town. */
    void armDeathStrikes() { pending_death_strikes_ = true; }
    /** OP_0E 0xFD / 0x1493C print chrome: PTR0..PTR5 message pages (no fight GS). */
    void armOp0eFdPrintChrome() { pending_fd_print_chrome_ = true; }
    /** OP_0E 0x80 @ 0xD6A4: after "Magical slide trap!" SPACE → halve party stats. */
    void armSlideTrapHalve() { pending_slide_trap_halve_ = true; }

    int locationId() const { return location_id_; }

    /** Current location record (triplets / scripts) for developer overlay. */
    const Mm2EventLocation *currentLocation() const { return loc_; }

    /** PORT DEVIATION (ASM unclear): mark this map tile's scripted triplet as
     *  resolved for the rest of the current map visit. Distinct from collision
     *  page bit7 (ambient-encounter gate). Reset by enterLocation. */
    void markTileEventResolved(int y, int x);

    /** OP_0E default-range dispatch (0x15EDC → event_dat_loader): run overlay
     *  location `category` string/script slot `index` (ASM stores index in
     *  A4-$5D46). String banks (e.g. loc 61 arena tiers) embed bytecode in
     *  str[index]; returns false when the slot is missing or not executable. */
    bool runDefaultRangeOverlay(GameStateView &gs, world::MapWorld &world, uint8_t category,
                                uint8_t index);

    /** Set when the player accepts the inn registry y/n (OP_0E 0x01 @ 0x1A1B2). */
    bool takePendingInnGotoTown()
    {
        const bool v = pending_inn_goto_town_;
        pending_inn_goto_town_ = false;
        return v;
    }
    bool takePendingDeathStrikes()
    {
        const bool v = pending_death_strikes_;
        pending_death_strikes_ = false;
        return v;
    }
    bool takePendingOp0eFdPrintChrome()
    {
        const bool v = pending_fd_print_chrome_;
        pending_fd_print_chrome_ = false;
        return v;
    }

private:
    void initParsed(GameStateView &gs);
    int poolSeek(uint8_t event_id) const;
    int poolSeekIn(const Mm2EventLocation *loc, uint8_t event_id) const;
    /** ASM pool_seek from work_buf parse_pos: skip `event_id` FF-delimited records. */
    int poolSeekWorkBuf(int start_pos, uint8_t event_id) const;
    uint8_t contextMask(const GameStateView &gs) const;
    bool eraGateOpen(const GameStateView &gs, const world::MapWorld &world) const;

    const char *resolveString(int idx, char *buf, size_t buf_cap) const;
    void normalizeAtToNewline(char *s) const;

    uint8_t readU8(GameStateView &gs);
    const char *readScriptString(GameStateView &gs, char *buf, size_t cap);
    bool cantSee(const GameStateView &gs) const;
    void skipTokens(GameStateView &gs, int count);
    bool runVmLoop(GameStateView &gs, world::MapWorld &world);
    void dispatchOp(GameStateView &gs, world::MapWorld &world, uint8_t op);
    void opInvalid(GameStateView &gs);
    void opText(GameStateView &gs);
    void opTextBlock(GameStateView &gs);
    void opText3(GameStateView &gs);
    void opTextDoor(GameStateView &gs);
    void opTextPopupA(GameStateView &gs);
    void opTextPopupB(GameStateView &gs);
    void opWaitSpace(GameStateView &gs);
    void opWaitSpaceScripted(GameStateView &gs);
    void opPromptYn(GameStateView &gs);
    void opPromptYnB(GameStateView &gs);
    void opShowServiceWindow(GameStateView &gs, world::MapWorld &world);
    void opMapTransition(GameStateView &gs, world::MapWorld &world);
    void opPlaySoundSeq(GameStateView &gs, world::MapWorld &world);
    void opExecSelector(GameStateView &gs, world::MapWorld &world);
    void opEndScript(GameStateView &gs);
    void opIfTrueSkiptok(GameStateView &gs);
    void opIfFalseSkiptok(GameStateView &gs);
    void opEncounterSetup(GameStateView &gs, world::MapWorld &world);
    void opEncounterSetupB(GameStateView &gs, world::MapWorld &world);
    void opClearTileEvent(GameStateView &gs, world::MapWorld &world);
    void opApplyParty(GameStateView &gs);
    void opScanPartyItems(GameStateView &gs);
    void opLoadVarRawToCond(GameStateView &gs);
    void opApplyPartyMasked(GameStateView &gs);
    void opGiveItem(GameStateView &gs);
    void opStoreVar8(GameStateView &gs);
    void opCondThreshold(GameStateView &gs);
    void opRngRollToCond(GameStateView &gs);
    void opAudioWait(GameStateView &gs);
    void opDelay(GameStateView &gs);
    void opPartyEffect(GameStateView &gs, bool mode_b);
    void opSetTile(GameStateView &gs, world::MapWorld &world);
    void opCheckEraRange(GameStateView &gs);
    void opCheckDayRange(GameStateView &gs);
    void opPayGoldToCond(GameStateView &gs);
    void opPayGemsToCond(GameStateView &gs);
    void opSelectMember(GameStateView &gs);
    void opSelectMemberB(GameStateView &gs);
    void opConsumeItemToCond(GameStateView &gs);
    void opSetAbort(GameStateView &gs);
    void opSetTreasure(GameStateView &gs);
    void opSkiptokIfVictory(GameStateView &gs);
    void opAddWordCounter(GameStateView &gs);
    void opCheckMemberAttr(GameStateView &gs);
    void opOrMemberField(GameStateView &gs);
    void opReadAnswer(GameStateView &gs);
    void opCheckAnswer(GameStateView &gs);
    void opPartyIterateDamage(GameStateView &gs);
    void opCountTitleNibble(GameStateView &gs);
    void opUnknown(GameStateView &gs, uint8_t op);
    void endScript(GameStateView &gs);
    void abortScript(GameStateView &gs);
    void resetDelayState();
    void applyMapTransition(GameStateView &gs, world::MapWorld &world, uint8_t dest_screen,
                            uint8_t dest_tile);
    /** Queued dispatch @ 0x176B6: seek queued id in current work_buf and run VM. */
    bool runQueuedDispatch(GameStateView &gs, world::MapWorld &world);
    /** OP_0C @ 0x15E12 bit6 / >=$80 dest remaps before map load. */
    void remapOp0cDest(uint8_t &dest_screen, uint8_t &dest_tile);
    /** Restore home location after OP_0E overlay swap when idle. */
    void restoreOverlayIfIdle(GameStateView &gs);
    bool finishPendingTownMenu(GameStateView &gs, bool accepted);

    Mm2EventFile file_{};
    bool loaded_ = false;
    const char *data_dir_ = nullptr;
    int location_id_ = -1;
    const Mm2EventLocation *loc_ = nullptr;
    uint8_t work_buf_[MM2_GS_EVENT_WORK_SIZE]{};

    bool script_active_ = false;
    EventVmWait wait_ = EventVmWait::None;
    /** OP_07 SPACE is level-sampled on the host; require a release after the wait
     *  arms so a held victory-dismiss / prior-page key cannot skip the plaque. */
    bool space_wait_armed_ = true;
    bool space_wait_active_ = false;
    /** OP_1E / hop-dwell remaining host frames (~60 Hz). */
    uint16_t delay_remaining_ = 0;
    /** Leftover Y/Enter from the prior Yes/No must not skip OP_1E. */
    bool delay_skip_armed_ = false;
    bool delay_key_skippable_ = false;
    /** This script executed OP_0D index 0x09 (pre-transition sound). */
    bool op0d_09_this_script_ = false;
    /** This script already paused (SPACE / Y/N / OP_1E); skip hop-dwell. */
    bool script_had_wait_ = false;
    bool screen_changed_ = false;
    char service_title_[128]{};
    EventTextView text_{};
    Mm2RosterFile *roster_ = nullptr;
    const Mm2PartyLaunch *launch_ = nullptr;
    const Mm2ItemsFile *items_ = nullptr;
    ITownServiceUi *town_service_ui_ = nullptr;
    gameplay::Rng *rng_ = nullptr;
    combat::CombatSession *combat_ = nullptr;
    PendingTownMenu pending_town_menu_ = PendingTownMenu::None;
    bool pending_inn_goto_town_ = false;
    bool pending_death_strikes_ = false;
    bool pending_fd_print_chrome_ = false;
    bool pending_skill_buy_member_ = false;
    bool pending_general_store_member_ = false;
    bool pending_circus_attr_ = false;
    bool pending_time_machine_ = false;
    bool pending_slide_trap_halve_ = false;
    uint8_t pending_free_teleport_stage_ = 0; /* 0=idle 1=X 2=Y */
    uint8_t pending_free_teleport_x_ = 0;
    uint8_t pending_quest_encode_stage_ = 0; /* 0=idle 1=Y/N 2=A–D */
    bool pending_quest_drink_ = false;
    /** Formatted A–C quest briefing (item/monster name via townSvcQuestTargetName). */
    char quest_msg_[256] = {};
    uint8_t pending_skill_id_ = 0;
    uint32_t pending_skill_cost_ = 0;

    int inline_script_end_ = -1;
    int saved_location_id_ = -1;
    const Mm2EventLocation *saved_loc_ = nullptr;
    uint8_t saved_work_buf_[MM2_GS_EVENT_WORK_SIZE]{};
    /** Runtime copy of A4-$7954 (event_script_anchor) for text resolve @ 0x15884. */
    uint16_t string_anchor_ = 0;

    /** OP_2F answer entry: chars typed so far (max 10), space-padded on commit. */
    int answer_len_ = 0;
    char answer_buf_[11]{};

    /** Per-tile scripted-event resolved flags for the current location visit.
     *  Index = (y<<4)|x. Not an ASM A4 field — see markTileEventResolved. */
    uint8_t tile_event_resolved_[256]{};
};

}  // namespace mm2::events

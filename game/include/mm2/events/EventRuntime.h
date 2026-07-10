#pragma once
// event.dat runtime: loader @ 0x92F2, init @ 0x1754A, scanner @ 0x175E2, VM @ 0x172CA.
// Milestone: text ops OP_01..OP_07, OP_09, OP_0F + OP_10/OP_11 skip (event 20).

#include "mm2/GameState.h"
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
};

struct EventPortalOffer {
    uint32_t cost = 0;
    uint8_t dest_screen = 0;
    uint8_t dest_tile = 0; /* packed (y<<4)|x */
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

    bool isActive() const { return script_active_; }
    bool blocksMovement() const { return script_active_ || wait_ != EventVmWait::None; }

    /** Set when OP_0C map transition ran; caller reloads env + 3D assets. */
    bool screenChanged() const { return screen_changed_; }
    void clearScreenChanged() { screen_changed_ = false; }

    EventTextView &textView() { return text_; }
    const EventTextView &textView() const { return text_; }

    EventVmWait waitKind() const { return wait_; }

    /** Optional party roster for gold/item checks (OP_24/OP_28). */
    void bindParty(Mm2RosterFile *roster, const Mm2PartyLaunch *launch);

    /** Optional items.dat for blacksmith (OP_0E 0x06) pricing. */
    void bindItems(const Mm2ItemsFile *items) { items_ = items; }
    const Mm2ItemsFile *items() const { return items_; }

    /** Optional pluggable town-service menu backend (temple/training). When bound
     *  the OP_0E temple/training selectors run the faithful interactive menu via
     *  this UI; when null they show the str.dat intro and defer (no fabrication). */
    void bindTownServiceUi(ITownServiceUi *ui) { town_service_ui_ = ui; }
    ITownServiceUi *townServiceUi() const { return town_service_ui_; }

    /** Optional rng(min,max) source (0x22BC6 contract) for the OP_0E default-range
     *  Arena Games ticket engine (asm 0x9D76's rng(1,16) @ thunk -$7BB4). When
     *  null, the arena's difficulty roll falls back to a fixed 1 (documented gap,
     *  no fabricated randomness). */
    void bindRng(gameplay::Rng *rng) { rng_ = rng; }
    gameplay::Rng *rng() const { return rng_; }

    /** Optional combat session: when bound, OP_12/OP_13 (and the arena ticket
     *  encounter routed through eventExecTownSelector) actually run the fight
     *  instead of only seeding A4 and aborting the script. */
    void bindCombat(combat::CombatSession *combat) { combat_ = combat; }
    combat::CombatSession *combat() const { return combat_; }

    void setPendingPortal(const EventPortalOffer &offer) { pending_portal_ = offer; pending_portal_active_ = true; }
    void clearPendingPortal() { pending_portal_active_ = false; }
    bool hasPendingPortal() const { return pending_portal_active_; }

    /** After a str.dat service intro Y/N (OP_0E 0x03/0x04/0x06), open the bound
     *  town-service menu on "yes" (ASM: handler shell gates on A4-$7951). */
    enum class PendingTownMenu : uint8_t {
        None = 0,
        Tavern,
        Temple,
        Smith,
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

    int locationId() const { return location_id_; }

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
    void skipTokens(GameStateView &gs, int count);
    bool runVmLoop(GameStateView &gs, world::MapWorld &world);
    void dispatchOp(GameStateView &gs, world::MapWorld &world, uint8_t op);
    void endScript(GameStateView &gs);
    void abortScript(GameStateView &gs);
    void applyMapTransition(GameStateView &gs, world::MapWorld &world, uint8_t dest_screen,
                            uint8_t dest_tile);
    /** Queued dispatch @ 0x176B6: seek queued id in current work_buf and run VM. */
    bool runQueuedDispatch(GameStateView &gs, world::MapWorld &world);
    /** OP_0C @ 0x15E12 bit6 / >=$80 dest remaps before map load. */
    void remapOp0cDest(uint8_t &dest_screen, uint8_t &dest_tile);
    /** Restore home location after OP_0E overlay swap when idle. */
    void restoreOverlayIfIdle(GameStateView &gs);
    bool finishPendingPortal(GameStateView &gs, world::MapWorld &world, bool accepted);
    bool finishPendingTownMenu(GameStateView &gs, bool accepted);

    Mm2EventFile file_{};
    bool loaded_ = false;
    const char *data_dir_ = nullptr;
    int location_id_ = -1;
    const Mm2EventLocation *loc_ = nullptr;
    uint8_t work_buf_[MM2_GS_EVENT_WORK_SIZE]{};

    bool script_active_ = false;
    EventVmWait wait_ = EventVmWait::None;
    bool screen_changed_ = false;
    char service_title_[128]{};
    EventTextView text_{};
    Mm2RosterFile *roster_ = nullptr;
    const Mm2PartyLaunch *launch_ = nullptr;
    const Mm2ItemsFile *items_ = nullptr;
    ITownServiceUi *town_service_ui_ = nullptr;
    gameplay::Rng *rng_ = nullptr;
    combat::CombatSession *combat_ = nullptr;
    bool pending_portal_active_ = false;
    EventPortalOffer pending_portal_{};
    PendingTownMenu pending_town_menu_ = PendingTownMenu::None;
    bool pending_inn_goto_town_ = false;
    bool pending_skill_buy_member_ = false;
    bool pending_general_store_member_ = false;
    bool pending_circus_attr_ = false;
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
};

}  // namespace mm2::events

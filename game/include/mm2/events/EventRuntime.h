#pragma once
// event.dat runtime: loader @ 0x92F2, init @ 0x1754A, scanner @ 0x175E2, VM @ 0x172CA.
// Milestone: text ops OP_01..OP_07, OP_09, OP_0F + OP_10/OP_11 skip (event 20).

#include "mm2/GameState.h"
#include "mm2/events/EventTextView.h"
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

    void setPendingPortal(const EventPortalOffer &offer) { pending_portal_ = offer; pending_portal_active_ = true; }
    void clearPendingPortal() { pending_portal_active_ = false; }
    bool hasPendingPortal() const { return pending_portal_active_; }

private:
    void initParsed(GameStateView &gs);
    int poolSeek(uint8_t event_id) const;
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
    bool finishPendingPortal(GameStateView &gs, world::MapWorld &world, bool accepted);

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
    bool pending_portal_active_ = false;
    EventPortalOffer pending_portal_{};
};

}  // namespace mm2::events

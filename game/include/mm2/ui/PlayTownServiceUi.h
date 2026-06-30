#pragma once
// Interactive, multi-frame town-service menu backend (OP_0E temple/training/smith).
//
// This is the playable presentation+input layer for the ASM-canonical transaction
// engine (mm2::events::TownServiceTransactions / TownServiceMenu). CLAUDE.md allows a
// swappable UI backend (ITownServiceUi); the transaction LOGIC stays in the events
// layer and is unchanged here.
//
// Integration model (why this is not the blocking townSvcRun* driver):
//   The event VM runs the town-service selector synchronously while a tile event is
//   firing, but the game loop polls input once per frame and cannot block. So this
//   backend does NOT answer the blocking driver's choose/select calls inline; instead
//   the ITownServiceUi hooks (chooseTempleOption / chooseTrainingOption /
//   chooseSmithCategory) merely RECORD which service was triggered + the live
//   TownServiceContext, then return false so townSvcRun* exits immediately. The host
//   (GameSession) sees pending(), opens a modal overlay, and drives the real menu
//   across frames via handleKey()/render(), applying the faithful transaction leaves
//   (townSvcHeal / townSvcTrain / townSvcTempleDonate / townSvcSmithBuy) directly.
//
// This mirrors the original engine, which itself is a modal menu loop that takes over
// the screen with its own keyread loop (A4 vtable thunks) — the host just pumps it
// frame-by-frame instead of busy-waiting.

#include "mm2/events/TownServiceMenu.h"
#include "mm2/events/TownServiceTransactions.h"
#include "mm2/gfx/ScreenCompositor.h"

#include "mm2_items_codec.h"
#include "mm2_roster_codec.h"
#include "mm2_town_tables.h"

#include <cstdint>

namespace mm2::ui {

class PlayTownServiceUi : public mm2::events::ITownServiceUi {
public:
    /* ---- ITownServiceUi capture hooks (called by townSvcRun* during the event VM).
     * Record the requested service + context, return false so the blocking driver
     * exits; the interactive menu then runs as a multi-frame overlay. ---- */
    bool chooseTempleOption(const mm2::events::TownServiceContext &ctx,
                            mm2::events::TempleOption &out) override;
    bool chooseTrainingOption(const mm2::events::TownServiceContext &ctx,
                              mm2::events::TrainingOption &out, int &stat_id) override;
    bool chooseSmithCategory(const mm2::events::TownServiceContext &ctx, int &out_category) override;
    bool selectMember(const mm2::events::TownServiceContext &, int &) override { return false; }
    bool selectSmithItem(const mm2::events::TownServiceContext &, int,
                         const mm2::events::SmithItemView[MM2_SMITH_SLOTS], int &) override
    {
        return false;
    }

    /* ---- Multi-frame overlay lifecycle (owned + driven by the host). ---- */

    /** True when the event VM requested a service menu that has not been opened yet. */
    bool pending() const { return pending_; }

    /** Promote the captured request to an active, on-screen modal menu. */
    void begin();

    bool active() const { return active_; }
    void close();

    /** Advance the menu state machine for one frame's input. ascii is uppercased by
     *  the caller; escape backs out one level (and closes from the top menu). */
    void handleKey(char ascii, bool escape);

    /** Render the menu in the LOWER CONSOLE band only (rows ~16..23), leaving the
     *  3D view + party panel visible — faithful, non-fullscreen (doc 15 §4). */
    void render(gfx::ScreenCompositor &c) const;

private:
    enum class Kind : uint8_t { None, Temple, Training, Smith };
    enum class Phase : uint8_t { Menu, SmithItems, Member };

    void applyTempleAndReturn(int party_slot);
    void applyTrainingAndReturn(int party_slot);
    void applySmithBuyAndReturn(int party_slot);
    void buildSmithView();

    bool pending_ = false;
    bool active_ = false;
    Kind kind_ = Kind::None;
    Phase phase_ = Phase::Menu;
    mm2::events::TownServiceContext ctx_{};

    /* Pending selection carried from the option/category menu into member-select. */
    mm2::events::TempleOption temple_opt_ = mm2::events::TempleOption::Exit;
    int smith_category_ = 0;      /* smith: Mm2SmithCategory */
    int smith_slot_ = -1;         /* smith: chosen shop slot 0..5 */
    mm2::events::SmithItemView smith_view_[MM2_SMITH_SLOTS]{};

    char status_[96] = {};        /* last transaction feedback line */
};

}  // namespace mm2::ui

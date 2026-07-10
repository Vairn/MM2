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
                            mm2::events::TempleOption &out, int &out_spell_slot) override;
    bool chooseTrainingOption(const mm2::events::TownServiceContext &ctx,
                              mm2::events::TrainingOption &out, int &stat_id) override;
    bool chooseSmithCategory(const mm2::events::TownServiceContext &ctx, int &out_category) override;
    bool selectMember(const mm2::events::TownServiceContext &, int &) override { return false; }
    bool selectSmithItem(const mm2::events::TownServiceContext &, int,
                         const mm2::events::SmithItemView[MM2_SMITH_SLOTS], int &) override
    {
        return false;
    }
    bool chooseMageGuildSpell(const mm2::events::TownServiceContext &ctx,
                              const Mm2SpellShopSlot[MM2_MAGE_GUILD_SLOTS], int &) override;
    /* townSvcRunMageGuild checks the 0x1E410 party gate BEFORE ever calling
     * chooseMageGuildSpell, so this fires instead when no member qualifies.
     * Captures ctx (chooseMageGuildSpell never ran) and flags a "denied"
     * overlay instead of a menu. */
    void reportGuildNotMember(const mm2::events::TownServiceContext &ctx) override;

    /* Tavern (0x1D208): capture the context and menu data, return false so
     * townSvcRunTavern exits; the multi-frame overlay then takes over. */
    bool chooseTavernOption(const mm2::events::TownServiceContext &ctx,
                            const mm2::events::TavernMenuData &data,
                            mm2::events::TavernOption &out) override;

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
    enum class Kind : uint8_t { None, Temple, Training, Smith, MageGuild, Tavern };
    enum class Phase : uint8_t { Menu, SmithItems, Denied, TavernFood, TavernBoost, TavernRumor };
    enum class SmithMode : uint8_t { Buy, Sell, Identify };

    void applyTempleAndReturn(int party_slot);
    void applyTrainingAndReturn(int party_slot);
    void applySmithBuyAndReturn(int party_slot);
    void applySmithSellAndReturn(int party_slot);
    void applySmithIdentifyAndReturn(int party_slot);
    void applyTavernFeedingFrenzy();
    void applyTavernStatBoost(int slot);
    void applyTavernSpecialty(int food_idx);
    void applyTavernTip();
    void applyTavernRumor();
    void applyGuildBuyAndReturn(int party_slot);
    void showActiveMemberGold();
    void buildSmithView();
    void buildSmithBackpackView();
    void buildGuildStock();

    const char *serviceTitle() const;
    const char *selectPromptText() const;
    bool showGatherGoldLine() const;
    void drawLeftChrome(gfx::ScreenCompositor &c) const;
    void drawTrainingPrompt(gfx::ScreenCompositor &c) const;

    bool pending_ = false;
    bool active_ = false;
    Kind kind_ = Kind::None;
    Phase phase_ = Phase::Menu;
    mm2::events::TownServiceContext ctx_{};

    /* Last menu action context (temple spell slot, smith item slot, …). */
    mm2::events::TempleOption temple_opt_ = mm2::events::TempleOption::Exit;
    int temple_spell_slot_ = -1;  /* temple: chosen D/E/F spell slot 0..2 */
    Mm2SpellShopSlot temple_spell_stock_[MM2_TEMPLE_SPELL_SLOTS]{};
    int smith_category_ = 0;      /* smith: Mm2SmithCategory (buy mode) */
    SmithMode smith_mode_ = SmithMode::Buy;
    int smith_slot_ = -1;         /* smith: chosen shop slot 0..5 */
    bool smith_identify_pending_ = false; /* 0x1BBD6: dismiss identify text before next pick */
    mm2::events::SmithItemView smith_view_[MM2_SMITH_SLOTS]{};
    Mm2SpellShopSlot guild_stock_[MM2_MAGE_GUILD_SLOTS]{};
    int guild_slot_ = -1;         /* mage guild: chosen A-D spell slot 0..3 */
    bool guild_denied_ = false;   /* 0x1E410 whole-party membership gate failed */

    /* Tavern state */
    mm2::events::TavernMenuData tavern_data_{};
    mm2::events::TavernOption tavern_opt_ = mm2::events::TavernOption::Exit;
    int tavern_sub_sel_ = -1;   /* food or drink index chosen in sub-menu */
    int tavern_rumor_idx_ = 0;  /* E: cycles through rumor pool (A4-$594E) */
    int tavern_tip_idx_   = 0;  /* D: cycles through tip   pool (A4-$58AE) — separate from E */
    bool tavern_tipped_ = false; /* true when current TavernRumor phase was triggered by D */
    int active_member_ = 0;         /* A4-$5A3A shop member index; digits 1..8 or # */

    char status_[96] = {};        /* last transaction feedback line */
};

}  // namespace mm2::ui

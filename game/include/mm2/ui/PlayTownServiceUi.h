#pragma once
// OP_0E shop menus, one frame at a time. Capture hooks stash and return false
// so townSvcRun* exits; GameSession pumps handleKey()/render().

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
    /* Capture hooks: stash request, return false so townSvcRun* exits. */
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

    bool pending() const { return pending_; }
    void begin();

    bool active() const { return active_; }
    void close();

    /** ascii is uppercased by the caller; escape backs out (closes from the top). */
    void handleKey(char ascii, bool escape);

    /** Lower console band (rows ~16..23); 3D view + party stay up (doc 15 §4). */
    void render(gfx::ScreenCompositor &c) const;

    /** Temple hireling leaf @ 0x1E116 — paid A–F menu suppressed; heal text only. */
    bool templeHirelingHealView() const
    {
        return active_ && kind_ == Kind::Temple && phase_ == Phase::HirelingTemple;
    }

    /** ASM string @ 0x1E26A / 0x1E27B while hireling heal view is active; else "". */
    const char *templeHirelingHealMessage() const;

private:
    enum class Kind : uint8_t { None, Temple, Training, Smith, MageGuild, Tavern };
    enum class Phase : uint8_t {
        Menu,
        SmithItems,
        Denied,
        TavernFood,
        TavernBoost,
        /* D/E rumor/tip text, and 0x1C902 result hold after A/B/C (preset 7
         * + pair text + key_read, then the A-E menu redraws). */
        TavernRumor,
        HirelingTemple, /* free auto-heal leaf 0x1E116 — no A–F captions */
        /* 0x1D6C2 / 0x1C432: preset 7 + pair text + key_read, then the caller
         * menu redraws (temple/guild A-F, smith item list after buy/sell). */
        ShopResult,
        /* 0x20232: clear_rect_preset(7) wipes the trainee prompt; result text
         * holds until key_read @ 0x20554, then the hall redraws. */
        TrainingResult
    };
    enum class SmithMode : uint8_t { Buy, Sell, Identify };
    /* Right-panel copy after T @ 0x20232 (strings 0x2055C / 0x20582 / 0x205A1). */
    enum class TrainingFeedback : uint8_t { None, Gained, NeedGold, NotWell };

    void applyTempleAndReturn(int party_slot);
    void applyHirelingTempleAutoHeal(int party_slot);
    void enterTempleMemberView(int party_slot);
    void drawHirelingTempleHeal(gfx::ScreenCompositor &c) const;
    void applyTrainingAndReturn(int party_slot, bool to_max = false);
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
    void holdShopResult(Phase return_to);
    void buildSmithView();
    void buildSmithBackpackView();
    void buildGuildStock();

    const char *serviceTitle() const;
    const char *selectPromptText() const;
    bool showGatherGoldLine() const;
    void drawLeftChrome(gfx::ScreenCompositor &c) const;
    void drawTrainingPrompt(gfx::ScreenCompositor &c) const;
    void drawTrainingResult(gfx::ScreenCompositor &c) const;
    void dismissTrainingResult();

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
    Phase result_return_phase_ = Phase::Menu; /* ShopResult pops back to this phase */
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

    char status_[256] = {};       /* last transaction feedback line (identify is multi-line) */
    /* Hireling temple leaf message: "has been healed." or "  is healthy." (0x1E26A/0x1E27B). */
    char hireling_heal_msg_[24] = {};

    TrainingFeedback training_feedback_ = TrainingFeedback::None;
    uint16_t training_hp_gain_ = 0;
    bool training_gained_spells_ = false;
    /* Remake 'M' (train-to-max): cumulative summary instead of the ASM 1-level copy. */
    bool training_max_summary_ = false;
    uint8_t training_levels_gained_ = 0;
    uint8_t training_spell_levels_gained_ = 0;
};

}  // namespace mm2::ui

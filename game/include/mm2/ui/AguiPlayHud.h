#pragma once

#include "mm2/ui/AguiAtlas.h"
#include "mm2/ui/IPlayHud.h"

namespace mm2::ui {

/** A1200-oriented play HUD: portraits, icon rail, d-pad, protect gems. */
class AguiPlayHud final : public IPlayHud {
public:
    bool init(const char *data_dir) override;
    void shutdown() override;

    int viewOriginX() const override;
    int viewOriginY() const override;
    int viewW() const override;
    int viewH() const override;

    void drawChrome(gfx::ScreenCompositor &c) override;
    void drawChromeStatic(gfx::ScreenCompositor &c) override;
    void drawViewportDivider(gfx::ScreenCompositor &c) override;

    void drawStatusBar(gfx::ScreenCompositor &c, int day, int year, char facing_key,
                       bool protect_panel) override;
    void drawPartyPanel(gfx::ScreenCompositor &c, const gfx::PlayPartySlot slots[8]) override;
    void drawRightColumn(gfx::ScreenCompositor &c, gfx::PlayRightPanel panel,
                         const gfx::PlayProtectValues *protect) override;

    void drawCombatChrome(gfx::ScreenCompositor &c) override;
    void drawCombatLines(gfx::ScreenCompositor &c) override;
    void drawCombatViewportFrame(gfx::ScreenCompositor &c) override;
    void drawCombatViewportDivider(gfx::ScreenCompositor &c) override;
    void drawCombatRightColumn(gfx::ScreenCompositor &c, const gfx::CombatPanelView &view) override;
    void drawCombatOptionsBar(gfx::ScreenCompositor &c, const gfx::CombatPanelView &view) override;

    void drawModalBackdrop(gfx::ScreenCompositor &c) override;

    bool hitTest(int x, int y, char *out_cmd, int *out_special) const override;

private:
    void drawBevelRect(gfx::ScreenCompositor &c, int x, int y, int w, int h, bool inset) const;
    void drawProtectGems(gfx::ScreenCompositor &c, const gfx::PlayProtectValues *protect) const;
    void drawFaceGem(gfx::ScreenCompositor &c, char facing_key) const;
    void drawDpad(gfx::ScreenCompositor &c) const;
    void blitNamed(gfx::ScreenCompositor &c, const char *name, int dst_x, int dst_y) const;
    void drawIconButton(gfx::ScreenCompositor &c, int index, const char *label, const char *icon_name,
                        bool combat) const;

    AguiAtlas atlas_;
    bool atlas_ok_ = false;
    char last_facing_ = 'N';
    gfx::PlayProtectValues last_protect_{};
    bool combat_mode_ = false;
    char status_msg_[48]{};
};

}  // namespace mm2::ui

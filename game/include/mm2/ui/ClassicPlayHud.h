#pragma once

#include "mm2/ui/IPlayHud.h"

namespace mm2::ui {

class ClassicPlayHud final : public IPlayHud {
public:
    bool init(const char * /*data_dir*/) override { return true; }
    void shutdown() override {}

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
};

}  // namespace mm2::ui

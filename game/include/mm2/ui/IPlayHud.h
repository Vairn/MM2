#pragma once

#include "mm2/gfx/CombatPanel.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/gfx/ScreenCompositor.h"

namespace mm2::ui {

/** Play-screen HUD. Classic = PlayScreenChrome/CombatPanel; Agui = atlas. */
class IPlayHud {
public:
    virtual ~IPlayHud() = default;

    /** Load atlas / data under data_dir (Agui). Classic: no-op success. */
    virtual bool init(const char *data_dir) = 0;
    virtual void shutdown() = 0;

    /** 3D hood destination — must match View3D tables (208×120). */
    virtual int viewOriginX() const = 0;
    virtual int viewOriginY() const = 0;
    virtual int viewW() const = 0;
    virtual int viewH() const = 0;

    virtual void drawChrome(gfx::ScreenCompositor &c) = 0;
    virtual void drawChromeStatic(gfx::ScreenCompositor &c) = 0;
    virtual void drawViewportDivider(gfx::ScreenCompositor &c) = 0;

    virtual void drawStatusBar(gfx::ScreenCompositor &c, int day, int year, char facing_key,
                               bool protect_panel) = 0;
    virtual void drawPartyPanel(gfx::ScreenCompositor &c, const gfx::PlayPartySlot slots[8]) = 0;
    virtual void drawRightColumn(gfx::ScreenCompositor &c, gfx::PlayRightPanel panel,
                                 const gfx::PlayProtectValues *protect) = 0;

    virtual void drawCombatChrome(gfx::ScreenCompositor &c) = 0;
    virtual void drawCombatLines(gfx::ScreenCompositor &c) = 0;
    virtual void drawCombatViewportFrame(gfx::ScreenCompositor &c) = 0;
    virtual void drawCombatViewportDivider(gfx::ScreenCompositor &c) = 0;
    virtual void drawCombatRightColumn(gfx::ScreenCompositor &c, const gfx::CombatPanelView &view) = 0;
    virtual void drawCombatOptionsBar(gfx::ScreenCompositor &c, const gfx::CombatPanelView &view) = 0;

    /** Modal Quick Ref / sheet backdrop. */
    virtual void drawModalBackdrop(gfx::ScreenCompositor &c) = 0;

    /** Agui hit-test. out_cmd = uppercase letter; out_special 0=none, 1..6 move/wait/face. */
    virtual bool hitTest(int x, int y, char *out_cmd, int *out_special) const
    {
        (void)x;
        (void)y;
        if (out_cmd) {
            *out_cmd = 0;
        }
        if (out_special) {
            *out_special = 0;
        }
        return false;
    }
};

}  // namespace mm2::ui

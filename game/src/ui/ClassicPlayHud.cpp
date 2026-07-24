#include "mm2/ui/ClassicPlayHud.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/CombatPanel.h"
#include "mm2/gfx/PlayScreenChrome.h"

namespace mm2::ui {

int ClassicPlayHud::viewOriginX() const { return gfx::play_layout::kViewOriginX; }
int ClassicPlayHud::viewOriginY() const { return gfx::play_layout::kViewOriginY; }
int ClassicPlayHud::viewW() const { return gfx::play_layout::kViewW; }
int ClassicPlayHud::viewH() const { return gfx::play_layout::kViewH; }

void ClassicPlayHud::drawChrome(gfx::ScreenCompositor &c) { gfx::drawPlayScreenChrome(c); }

void ClassicPlayHud::drawChromeStatic(gfx::ScreenCompositor &c)
{
    gfx::drawPlayScreenChromeStatic(c);
}

void ClassicPlayHud::drawViewportDivider(gfx::ScreenCompositor &c)
{
    gfx::drawPlayViewportDivider(c);
}

void ClassicPlayHud::drawStatusBar(gfx::ScreenCompositor &c, int day, int year, char facing_key,
                                   bool protect_panel)
{
    gfx::drawPlayStatusBar(c, day, year, facing_key, protect_panel);
}

void ClassicPlayHud::drawPartyPanel(gfx::ScreenCompositor &c, const gfx::PlayPartySlot slots[8])
{
    gfx::drawPlayPartyPanel(c, slots);
}

void ClassicPlayHud::drawRightColumn(gfx::ScreenCompositor &c, gfx::PlayRightPanel panel,
                                     const gfx::PlayProtectValues *protect)
{
    gfx::drawPlayRightColumn(c, panel, protect);
}

void ClassicPlayHud::drawCombatChrome(gfx::ScreenCompositor &c) { gfx::drawCombatScreenChrome(c); }

void ClassicPlayHud::drawCombatLines(gfx::ScreenCompositor &c) { gfx::drawCombatScreenLines(c); }

void ClassicPlayHud::drawCombatViewportFrame(gfx::ScreenCompositor &c)
{
    gfx::drawCombatViewportFrame(c);
}

void ClassicPlayHud::drawCombatViewportDivider(gfx::ScreenCompositor &c)
{
    gfx::drawCombatViewportDivider(c);
}

void ClassicPlayHud::drawCombatRightColumn(gfx::ScreenCompositor &c, const gfx::CombatPanelView &view)
{
    gfx::drawCombatRightColumn(c, view);
}

void ClassicPlayHud::drawCombatOptionsBar(gfx::ScreenCompositor &c, const gfx::CombatPanelView &view)
{
    gfx::drawCombatOptionsBar(c, view);
}

void ClassicPlayHud::drawModalBackdrop(gfx::ScreenCompositor &c) { gfx::drawPlayModalBackdrop(c); }

}  // namespace mm2::ui

#include "mm2/ui/PlayHudFactory.h"

#include "mm2/ui/AguiPlayHud.h"
#include "mm2/ui/ClassicPlayHud.h"

namespace mm2::ui {

namespace {

ClassicPlayHud g_classic_hud;
AguiPlayHud g_agui_hud;

}  // namespace

std::unique_ptr<IPlayHud> createPlayHud(PlayHudKind kind)
{
    switch (kind) {
    case PlayHudKind::Agui:
        return std::unique_ptr<IPlayHud>(new AguiPlayHud());
    case PlayHudKind::Classic:
    default:
        return std::unique_ptr<IPlayHud>(new ClassicPlayHud());
    }
}

IPlayHud *acquirePlayHud(PlayHudKind kind)
{
    // File-scope (not function-local) statics: freestanding Amiga has no
    // __cxa_guard_acquire/release for thread-safe local static init.
    switch (kind) {
    case PlayHudKind::Agui:
        return &g_agui_hud;
    case PlayHudKind::Classic:
    default:
        return &g_classic_hud;
    }
}

}  // namespace mm2::ui

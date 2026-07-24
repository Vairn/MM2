#include "mm2/ui/PlayHudFactory.h"

#include "mm2/ui/AguiPlayHud.h"
#include "mm2/ui/ClassicPlayHud.h"

namespace mm2::ui {

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
    static ClassicPlayHud classic;
    static AguiPlayHud agui;
    switch (kind) {
    case PlayHudKind::Agui:
        return &agui;
    case PlayHudKind::Classic:
    default:
        return &classic;
    }
}

}  // namespace mm2::ui

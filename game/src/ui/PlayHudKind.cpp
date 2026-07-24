#include "mm2/ui/PlayHudKind.h"

#include <cstring>

namespace mm2::ui {

PlayHudKind playHudKindFromString(const char *value)
{
    if (!value || !*value) {
        return PlayHudKind::Classic;
    }
    if (std::strcmp(value, "agui") == 0 || std::strcmp(value, "aga") == 0 ||
        std::strcmp(value, "modern") == 0) {
        return PlayHudKind::Agui;
    }
    return PlayHudKind::Classic;
}

const char *playHudKindName(PlayHudKind kind)
{
    switch (kind) {
    case PlayHudKind::Agui:
        return "agui";
    case PlayHudKind::Classic:
    default:
        return "classic";
    }
}

}  // namespace mm2::ui

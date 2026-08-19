#pragma once

namespace mm2::ui {

enum class PlayHudKind {
    Classic, /**< glyph lattice (PlayScreenChrome) */
    Agui,    /**< portraits / icon rail */
};

PlayHudKind playHudKindFromString(const char *value);
const char *playHudKindName(PlayHudKind kind);

}  // namespace mm2::ui

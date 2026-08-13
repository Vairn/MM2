#pragma once

namespace mm2::ui {

/** Play-screen chrome backend (exploration / combat HUD). */
enum class PlayHudKind {
    Classic, /**< ASM-faithful glyph lattice (default for RE). */
    Agui,    /**< A1200-oriented portrait / icon HUD. */
};

PlayHudKind playHudKindFromString(const char *value);
const char *playHudKindName(PlayHudKind kind);

}  // namespace mm2::ui

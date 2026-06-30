#pragma once

#include "mm2/gfx/ScreenCompositor.h"

#include "mm2/GameState.h"

namespace mm2::gameplay {

/* Controls screen @ 0x13CCE (doc 43 §5, doc 25). */
class InGameControlsScreen {
public:
    void render(gfx::ScreenCompositor &c, const GameStateView &gs) const;
    void handleKey(char key, GameStateView &gs);
};

}  // namespace mm2::gameplay

#pragma once

#include "mm2/CppStdCompat.h"
#include "mm2/ui/IPlayHud.h"
#include "mm2/ui/PlayHudKind.h"

namespace mm2::ui {

std::unique_ptr<IPlayHud> createPlayHud(PlayHudKind kind);

/** Amiga: static storage, no heap. */
IPlayHud *acquirePlayHud(PlayHudKind kind);

}  // namespace mm2::ui

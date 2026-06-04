#pragma once

#include "mm2/CppStdCompat.h"

#include "mm2/ui/CharacterUiKind.h"
#include "mm2/ui/ICharacterUi.h"

namespace mm2::ui {

std::unique_ptr<ICharacterUi> createCharacterUi(CharacterUiKind kind);

}  // namespace mm2::ui

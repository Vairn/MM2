#pragma once

#include "mm2/CppStdCompat.h"

#include "mm2/ui/CharacterUiKind.h"
#include "mm2/ui/ICharacterUi.h"

namespace mm2::ui {

std::unique_ptr<ICharacterUi> createCharacterUi(CharacterUiKind kind);

#if MM2_HOST_AMIGA
/** Static singleton — no heap alloc on the 4K stack. */
ICharacterUi *acquireCharacterUi(CharacterUiKind kind);
#endif

}  // namespace mm2::ui

#include "mm2/ui/CharacterUiFactory.h"

#include "mm2/CppStdCompat.h"

namespace mm2::ui {

std::unique_ptr<ICharacterUi> makeStubCharacterUi();
std::unique_ptr<ICharacterUi> makeAmigaCharacterUi();

std::unique_ptr<ICharacterUi> createCharacterUi(CharacterUiKind kind)
{
    switch (kind) {
    case CharacterUiKind::Stub:
        return makeStubCharacterUi();
    case CharacterUiKind::AmigaClassic:
        return makeAmigaCharacterUi();
    }
    return makeAmigaCharacterUi();
}

}  // namespace mm2::ui

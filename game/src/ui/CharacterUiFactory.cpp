#include "mm2/ui/CharacterUiFactory.h"

#include "mm2/CppStdCompat.h"

namespace mm2::ui {

std::unique_ptr<ICharacterUi> makeStubCharacterUi();
std::unique_ptr<ICharacterUi> makeAmigaCharacterUi();

#if MM2_HOST_AMIGA
ICharacterUi *acquireStubCharacterUi();
ICharacterUi *acquireAmigaCharacterUi();
#endif

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

#if MM2_HOST_AMIGA
ICharacterUi *acquireCharacterUi(CharacterUiKind kind)
{
    switch (kind) {
    case CharacterUiKind::Stub:
        return acquireStubCharacterUi();
    case CharacterUiKind::AmigaClassic:
        return acquireAmigaCharacterUi();
    }
    return acquireAmigaCharacterUi();
}
#endif

}  // namespace mm2::ui

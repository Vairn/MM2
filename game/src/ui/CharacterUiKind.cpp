#include "mm2/ui/CharacterUiKind.h"

#include "mm2/CppStdCompat.h"

namespace mm2::ui {

CharacterUiKind characterUiKindFromString(const char *value)
{
    if (!value || !*value) {
        return CharacterUiKind::AmigaClassic;
    }
    if (std::strcmp(value, "stub") == 0 || std::strcmp(value, "text") == 0) {
        return CharacterUiKind::Stub;
    }
    if (std::strcmp(value, "classic") == 0 || std::strcmp(value, "amiga") == 0) {
        return CharacterUiKind::AmigaClassic;
    }
    return CharacterUiKind::AmigaClassic;
}

const char *characterUiKindName(CharacterUiKind kind)
{
    switch (kind) {
    case CharacterUiKind::Stub:
        return "stub";
    case CharacterUiKind::AmigaClassic:
        return "classic";
    }
    return "classic";
}

}  // namespace mm2::ui

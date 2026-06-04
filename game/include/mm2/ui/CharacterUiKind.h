#pragma once

namespace mm2::ui {

enum class CharacterUiKind {
    Stub,
    AmigaClassic,
};

CharacterUiKind characterUiKindFromString(const char *value);
const char *characterUiKindName(CharacterUiKind kind);

}  // namespace mm2::ui

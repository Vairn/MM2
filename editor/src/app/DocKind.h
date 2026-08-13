#pragma once

namespace mm2 {

enum class DocKind {
    None = -1,
    Items = 0,
    Monsters,
    Spells,
    Roster,
    Str,
    Map,
    Attrib,
    Events,
    Gfx32,
    GfxAnm,
    PcGfxCga,
    PcGfxEga,
    Count
};

inline const char* DocKindTitle(DocKind k) {
    switch (k) {
        case DocKind::Items: return "Items";
        case DocKind::Monsters: return "Monsters";
        case DocKind::Spells: return "Spells";
        case DocKind::Roster: return "Roster / Party";
        case DocKind::Str: return "Strings";
        case DocKind::Map: return "Maps";
        case DocKind::Attrib: return "Attrib";
        case DocKind::Events: return "Events";
        case DocKind::Gfx32: return "Graphics (.32)";
        case DocKind::GfxAnm: return "Animations (.anm)";
        case DocKind::PcGfxCga: return "PC Walls (CGA .4)";
        case DocKind::PcGfxEga: return "PC Walls (EGA .16)";
        default: return "(none)";
    }
}

inline const char* DocKindGroup(DocKind k) {
    switch (k) {
        case DocKind::Items:
        case DocKind::Monsters:
        case DocKind::Spells:
        case DocKind::Roster:
        case DocKind::Str:
            return "Game data";
        case DocKind::Map:
        case DocKind::Attrib:
        case DocKind::Events:
            return "World";
        case DocKind::Gfx32:
        case DocKind::GfxAnm:
        case DocKind::PcGfxCga:
        case DocKind::PcGfxEga:
            return "Graphics";
        default:
            return "";
    }
}

inline bool DocKindIsReadOnly(DocKind k) {
    switch (k) {
        case DocKind::Gfx32:
        case DocKind::GfxAnm:
        case DocKind::PcGfxCga:
        case DocKind::PcGfxEga:
            return true;
        default:
            return false;
    }
}

}  // namespace mm2

#pragma once

// In-game character sheet + Quick Ref overlays (doc 43 §6, §6.3; doc 39 §4).
//
// Entry: exploration digits 1–8 or Q (via 0x907A). Text-only sheet (doc 39 §4 title path);
// book.32 composite @ $39B4 is combat spell-book (V) only — not wired here yet.

#include "mm2/gfx/ScreenCompositor.h"

#include "mm2_image32_codec.h"
#include "mm2_items_codec.h"
#include "mm2_party_launch.h"

#include "mm2_roster_codec.h"

namespace mm2::gameplay {

enum class SheetKeyOutcome {
    None,
    Close,
    QuickRef,
    SwitchCharacter,
};

enum class SheetSubMode : uint8_t {
    Normal,
    EquipPickBackpack,
    RemovePickEquip,
    DropPickSlot,
    CastPicker,
    SpellBook,
    GatherPick,
    TradePickType,
    TradePickTarget,
    TradePickItemSlot,
};

enum class SheetTradeKind : uint8_t {
    None,
    Gold,
    Gems,
    Food,
    Items,
};

/** True while sheet sub-handler owns digit keys (doc 43 §6.1 — not 0x907A chain). */
inline bool sheetSubModeBlocksCharacterSwitch(SheetSubMode mode)
{
    switch (mode) {
    case SheetSubMode::Normal:
        return false;
    default:
        return true;
    }
}

struct SheetSession {
    int party_slot = 0;
    SheetSubMode sub_mode = SheetSubMode::Normal;
    SheetTradeKind trade_kind = SheetTradeKind::None;
    int trade_target_slot = -1; /* Item trade ($E492): target chosen before backpack-letter pick. */
    char status_line[48] = {};
};

class InGameCharacterSheet {
public:
    bool loadAssets(const char *data_dir);

    void renderSheet(gfx::ScreenCompositor &c, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                     int party_slot, const Mm2ItemsFile *items, const SheetSession *session) const;

    void renderQuickRef(gfx::ScreenCompositor &c, const Mm2RosterFile &roster,
                        const Mm2PartyLaunch &launch) const;

    /** Spell-book popup (grid @ 0x65fa inside window @ 0x6736). Overlays the
        character sheet with the retail 9×7 known-spell grid and cast prompt. */
    void renderSpellBook(gfx::ScreenCompositor &c, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                         int party_slot) const;

    /** Sheet sub-menu @ $8EA6 (C/D/E/G/R/S/T/U). Mutates roster on equip/remove/drop. */
    SheetKeyOutcome handleKey(char key, SheetSession &session, Mm2RosterFile &roster,
                              const Mm2PartyLaunch &launch, const Mm2ItemsFile *items);

private:
    mm2_image32_file book_{};
    bool has_book_ = false;
};

}  // namespace mm2::gameplay

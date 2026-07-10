#pragma once

// In-game character sheet + Quick Ref overlays (doc 43 §6, §6.3; doc 39 §4).
//
// Entry: exploration digits 1–8 or Q (via 0x907A). Text-only sheet (doc 39 §4 title path);
// book.32 composite @ LAB_4252 / $39B4 — in-game + combat character sheet backdrop.

#include "mm2/gfx/ScreenCompositor.h"

#include "mm2_image32_codec.h"
#include "mm2_gfx_sheet.h"
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
    /** Exploration cast @ 0x6E30: spell grid + level/number prompt (0x79EE). */
    CastPicker,
    /** View-only spell grid @ 0x675A / combat sheet 'V' — no cast prompts. */
    SpellBook,
    GatherPick,
    /** Share @ 0x7DCC: '1' gold (0x7BBE), '2' gems (0x7CB0), '3' food (0x7D3E). */
    SharePick,
    /** Use @ 0xE94A: '1'..'6' equip / 'A'..'F' backpack. */
    UsePick,
    TradePickType,
    TradePickTarget,
    TradePickItemSlot,
};

/** Cast level/number prompt phase inside CastPicker (0x79EE). */
enum class CastPromptPhase : uint8_t {
    Level,   /* " Spell Level: " — digit 1..spell_level */
    Number,  /* "Number: " — digit 1..spells-in-level */
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
    CastPromptPhase cast_phase = CastPromptPhase::Level;
    int cast_level = 0; /* 1..9 after level digit accepted */
    int cast_spell_flat = -1; /* 0..47 school-local index when picker completes */
    /** UsePick result: -1 none; 0..5 equip; 6..11 backpack (slot = v-6). */
    int pending_use_slot = -1;
    char status_line[48] = {};
};

class InGameCharacterSheet {
public:
    bool loadAssets(const char *data_dir);

    void renderSheet(gfx::ScreenCompositor &c, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                     int party_slot, const Mm2ItemsFile *items, const SheetSession *session,
                     bool combat_mode = false) const;

    void renderQuickRef(gfx::ScreenCompositor &c, const Mm2RosterFile &roster,
                        const Mm2PartyLaunch &launch) const;

    /** Spell-book popup (grid @ 0x6622 inside window @ 0x675A). View-only known-spell marks. */
    void renderSpellBook(gfx::ScreenCompositor &c, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                         int party_slot) const;

    /** Exploration cast UI: same grid as SpellBook plus 0x79EE level/number prompts. */
    void renderCastPicker(gfx::ScreenCompositor &c, const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                          int party_slot, const SheetSession &session) const;

    /** Sheet sub-menu @ $8EA6 (C/D/E/G/R/S/T/U). Mutates roster on equip/remove/drop. */
    SheetKeyOutcome handleKey(char key, SheetSession &session, Mm2RosterFile &roster,
                              const Mm2PartyLaunch &launch, const Mm2ItemsFile *items, bool combat_mode = false);

private:
    mm2_image32_file book_{};
    mm2_gfx_sheet book_pc_{};
    bool has_book_ = false;
    bool book_pc_mode_ = false;
};

}  // namespace mm2::gameplay

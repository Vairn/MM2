#pragma once

// Map platform KeyState → exploration codes (doc 43 §1–§2).
//
// Movement: cursor + keypad only → $F0..$F3 (no WASD / N-E-S-W letters).
// Commands: Ctrl-Q quit, Q quick ref, 1–8 view character, O/P panel toggle,
// B/C/D/E/M/R/S/U exploration handlers @ $1482.

#include "mm2/gameplay/Movement.h"

#include "mm2/platform/Platform.h"

namespace mm2::gameplay {

enum class PlaySessionAction {
    None,
    CtrlQuit,       /* $11 @ $12F4 — quit without save prompt */
    QuickRef,       /* Q @ $140A → 0x907A → $595C */
    ViewCharacter,  /* digit 1..N @ $1444 → 0x907A → $8C8A */
    PanelOptions,   /* O @ $13C8 when -$79B2==1 */
    PanelProtect,   /* P @ $13EA when -$79B2==0 */
    BashDoor,       /* B @ $1394 → 0x9B48 */
    Controls,       /* C @ $139C → 0x13CCE */
    DismissHireling, /* D @ $13A8 → 0x141F4 */
    ExchangeOrder,  /* E @ $13B0 → 0x20F58 */
    Automap,        /* M @ $13BC → 0x223A */
    Rest,           /* R @ $141C → 0x19E20 */
    Search,         /* S @ $1434 → $4800 */
    Unlock,         /* U @ $143C → 0x20CA2 */
};

/* Returns ExploreCode only for movement keys ($F0..$F3). */
bool pollExploreCode(const platform::KeyState &keys, ExploreCode *out_code);

/** Non-movement exploration dispatcher keys (doc 43 cascade @ $1482). */
bool pollPlaySessionAction(const platform::KeyState &keys, int party_count, PlaySessionAction *out_action,
                           int *out_party_slot);

}  // namespace mm2::gameplay

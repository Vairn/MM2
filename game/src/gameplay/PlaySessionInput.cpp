#include "mm2/gameplay/PlaySessionInput.h"

#include "mm2/CppStdCompat.h"

namespace mm2::gameplay {

bool pollExploreCode(const platform::KeyState &keys, ExploreCode *out_code)
{
    if (!out_code) {
        return false;
    }

    /* Doc 43 §1: cursor/keypad only — Left=$F0, Right=$F1, Up=$F2, Down=$F3. */
    if (keys.left) {
        *out_code = ExploreCode::TurnLeft;
        return true;
    }
    if (keys.right) {
        *out_code = ExploreCode::TurnRight;
        return true;
    }
    if (keys.up) {
        *out_code = ExploreCode::Forward;
        return true;
    }
    if (keys.down) {
        *out_code = ExploreCode::Back;
        return true;
    }

    return false;
}

bool pollPlaySessionAction(const platform::KeyState &keys, int party_count, PlaySessionAction *out_action,
                           int *out_party_slot)
{
    if (!out_action) {
        return false;
    }

    *out_action = PlaySessionAction::None;
    if (out_party_slot) {
        *out_party_slot = -1;
    }

    if (keys.ctrl && keys.key_q) {
        *out_action = PlaySessionAction::CtrlQuit;
        return true;
    }

    if (keys.key_q && !keys.ctrl) {
        *out_action = PlaySessionAction::QuickRef;
        return true;
    }

    const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));

    if (ch >= '1' && ch <= '8') {
        const int slot = ch - '1';
        if (slot < party_count) {
            *out_action = PlaySessionAction::ViewCharacter;
            if (out_party_slot) {
                *out_party_slot = slot;
            }
            return true;
        }
    }

    if (keys.key_o || ch == 'O') {
        *out_action = PlaySessionAction::PanelOptions;
        return true;
    }

    if (keys.key_p || ch == 'P') {
        *out_action = PlaySessionAction::PanelProtect;
        return true;
    }

    if (ch == 'B') {
        *out_action = PlaySessionAction::BashDoor;
        return true;
    }
    if (ch == 'C') {
        *out_action = PlaySessionAction::Controls;
        return true;
    }
    if (ch == 'D') {
        *out_action = PlaySessionAction::DismissHireling;
        return true;
    }
    if (ch == 'E') {
        *out_action = PlaySessionAction::ExchangeOrder;
        return true;
    }
    if (ch == 'M') {
        *out_action = PlaySessionAction::Automap;
        return true;
    }
    if (ch == 'R') {
        *out_action = PlaySessionAction::Rest;
        return true;
    }
    if (ch == 'S') {
        *out_action = PlaySessionAction::Search;
        return true;
    }
    if (ch == 'U') {
        *out_action = PlaySessionAction::Unlock;
        return true;
    }

    return false;
}

}  // namespace mm2::gameplay

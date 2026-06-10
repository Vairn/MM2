// Input routing unit test (doc 43 §1–§2): movement keys, Q vs Ctrl-Q, digits 1–8,
// O/P panel toggle, B/C/D/E/M/R/S/U exploration commands.

#include <cstdio>

#include "mm2/gameplay/PlaySessionInput.h"

namespace {

bool expect(bool cond, const char *msg, int &fails)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++fails;
        return false;
    }
    return true;
}

}  // namespace

int main()
{
    int fails = 0;
    mm2::platform::KeyState keys{};
    mm2::gameplay::ExploreCode code{};

    keys.up = false;
    keys.left = false;
    expect(!mm2::gameplay::pollExploreCode(keys, &code), "idle -> no movement code", fails);

    keys.right = true;
    expect(mm2::gameplay::pollExploreCode(keys, &code) && code == mm2::gameplay::ExploreCode::TurnRight,
           "cursor right -> $F1", fails);

    keys.right = false;
    keys.up = true;
    expect(mm2::gameplay::pollExploreCode(keys, &code) && code == mm2::gameplay::ExploreCode::Forward,
           "cursor up -> $F2", fails);

    mm2::gameplay::PlaySessionAction action{};
    int slot = -1;

    keys = {};
    keys.key_q = true;
    expect(mm2::gameplay::pollPlaySessionAction(keys, 4, &action, &slot) &&
               action == mm2::gameplay::PlaySessionAction::QuickRef,
           "plain Q -> Quick Ref (not quit)", fails);

    keys = {};
    keys.ctrl = true;
    keys.key_q = true;
    expect(mm2::gameplay::pollPlaySessionAction(keys, 4, &action, &slot) &&
               action == mm2::gameplay::PlaySessionAction::CtrlQuit,
           "Ctrl-Q -> quit prompt", fails);

    keys = {};
    keys.last_ascii = '3';
    expect(mm2::gameplay::pollPlaySessionAction(keys, 4, &action, &slot) &&
               action == mm2::gameplay::PlaySessionAction::ViewCharacter && slot == 2,
           "digit 3 -> party slot 2 (1-based key)", fails);

    keys = {};
    keys.last_ascii = '9';
    expect(!mm2::gameplay::pollPlaySessionAction(keys, 4, &action, &slot),
           "digit 9 ignored when party_count=4", fails);

    keys = {};
    keys.last_ascii = 'O';
    expect(mm2::gameplay::pollPlaySessionAction(keys, 4, &action, &slot) &&
               action == mm2::gameplay::PlaySessionAction::PanelOptions,
           "O -> options panel toggle", fails);

    keys = {};
    keys.last_ascii = 'C';
    expect(mm2::gameplay::pollPlaySessionAction(keys, 4, &action, &slot) &&
               action == mm2::gameplay::PlaySessionAction::Controls,
           "C -> Controls screen", fails);

    keys = {};
    keys.last_ascii = 'R';
    expect(mm2::gameplay::pollPlaySessionAction(keys, 4, &action, &slot) &&
               action == mm2::gameplay::PlaySessionAction::Rest,
           "R -> Rest", fails);

    keys = {};
    keys.last_ascii = 'M';
    expect(mm2::gameplay::pollPlaySessionAction(keys, 4, &action, &slot) &&
               action == mm2::gameplay::PlaySessionAction::Automap,
           "M -> Automap", fails);

    if (fails == 0) {
        std::printf("OK: playsession_input_test (11 checks)\n");
        return 0;
    }
    return 1;
}

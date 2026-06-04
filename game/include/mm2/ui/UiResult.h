#pragma once

namespace mm2::ui {

enum class UiResult {
    Continue,
    Done,
    Cancel,
    Quit,  // request to exit the game (party screen 'ESC')
};

}  // namespace mm2::ui

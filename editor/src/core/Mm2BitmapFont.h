#pragma once

#include "imgui.h"

namespace mm2 {

// Attempts to load the original Amiga MM2 8px bitmap font and install it in
// the current ImGui font atlas. Returns nullptr on failure.
ImFont* installMm2BitmapFont(ImGuiIO& io);

}  // namespace mm2


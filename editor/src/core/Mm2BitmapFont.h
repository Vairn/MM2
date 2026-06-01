#pragma once

#include "imgui.h"

namespace mm2 {

// Attempts to load the original Amiga MM2 8px bitmap font and install it in
// the current ImGui font atlas. Returns nullptr on failure.
ImFont* installMm2BitmapFont(ImGuiIO& io, float sizePixels = 16.0f);

// Returns last status text from installMm2BitmapFont() for on-screen debug UI.
const char* mm2BitmapFontDebugStatus();

}  // namespace mm2


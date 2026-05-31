#pragma once
// Minimal read-only hex dump widget for ImGui.

#include <cstddef>
#include <cstdint>

namespace mm2 {

// Draws a scrollable hex+ASCII dump of `data`/`size`. `baseAddr` is added to
// the displayed offsets (useful for record-relative views).
void DrawHexView(const char* id, const uint8_t* data, size_t size, size_t baseAddr = 0);

}  // namespace mm2

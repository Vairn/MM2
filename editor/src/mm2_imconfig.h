#pragma once
// MM2ED ImGui user configuration (wired via IMGUI_USER_CONFIG in CMakeLists.txt).
//
// Use 32-bit draw indices so large Workspace meshes (map grids, long string
// tables, gfx contact sheets) do not trip the default 16-bit index assert in
// ImGui::AddDrawListToDrawDataEx(). imgui_impl_opengl3 picks GL_UNSIGNED_INT
// from sizeof(ImDrawIdx).
#define ImDrawIdx unsigned int
// Also required by the imnodes event graph (large meshes overflow 16-bit indices).

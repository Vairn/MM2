#pragma once
// MM2ED ImGui user configuration (wired via IMGUI_USER_CONFIG in CMakeLists.txt).
//
// Use 32-bit draw indices. The event editor renders the whole imnodes node graph
// for a location into a single window draw list (imnodes merges all nodes via one
// ImDrawListSplitter, with no off-screen culling). Large locations such as
// Middlegate emit well over 65,536 vertices in that one draw list, which trips the
// default 16-bit index assert:
//   IM_ASSERT(draw_list->_VtxCurrentIdx < (1 << 16) ...)
// in ImGui::AddDrawListToDrawDataEx(). With 32-bit indices that assert is compiled
// out (it is guarded by `if (sizeof(ImDrawIdx) == 2)`) and imgui_impl_opengl3 picks
// GL_UNSIGNED_INT automatically from sizeof(ImDrawIdx), so large meshes render
// correctly regardless of GL version / ImGuiBackendFlags_RendererHasVtxOffset.
#define ImDrawIdx unsigned int

// MM2ED - Might & Magic II (Amiga) data editor.
// ImGui (docking) + GLFW + OpenGL3.

#include <cstdio>
#include <algorithm>
#include <filesystem>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imnodes.h"

#include <GLFW/glfw3.h>

#include "app/App.h"
#include "core/Mm2BitmapFont.h"
#include "eventlang/Decompile.h"
#include "eventlang/Encode.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static void glfwErrorCallback(int error, const char* desc) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, desc);
}

static void scaleImNodesStyle(float scale) {
    ImNodesStyle& s = ImNodes::GetStyle();
    s.GridSpacing *= scale;
    s.NodeCornerRounding *= scale;
    s.NodePadding = ImVec2(s.NodePadding.x * scale, s.NodePadding.y * scale);
    s.NodeBorderThickness *= scale;
    s.LinkThickness *= scale;
    s.LinkHoverDistance *= scale;
    s.PinCircleRadius *= scale;
    s.PinQuadSideLength *= scale;
    s.PinTriangleSideLength *= scale;
    s.PinLineThickness *= scale;
    s.PinHoverRadius *= scale;
    s.PinOffset *= scale;
    s.MiniMapPadding = ImVec2(s.MiniMapPadding.x * scale, s.MiniMapPadding.y * scale);
    s.MiniMapOffset = ImVec2(s.MiniMapOffset.x * scale, s.MiniMapOffset.y * scale);
}

static void applyRedBlackTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.93f, 0.93f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.43f, 0.43f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.01f, 0.01f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.01f, 0.01f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.02f, 0.02f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.45f, 0.10f, 0.10f, 0.85f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.50f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.65f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.45f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.01f, 0.01f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.42f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.58f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.70f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.92f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.78f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.92f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.42f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.62f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.78f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.35f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.55f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.70f, 0.13f, 0.13f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.45f, 0.10f, 0.10f, 0.85f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.63f, 0.13f, 0.13f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.80f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.45f, 0.10f, 0.10f, 0.45f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.70f, 0.13f, 0.13f, 0.78f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.85f, 0.18f, 0.18f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.03f, 0.03f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.60f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.50f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.14f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.30f, 0.05f, 0.05f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.85f, 0.18f, 0.18f, 0.55f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.05f, 0.01f, 0.01f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.93f, 0.23f, 0.23f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.93f, 0.23f, 0.23f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.24f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.56f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.34f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.08f, 0.01f, 0.01f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.12f, 0.02f, 0.02f, 0.35f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.82f, 0.17f, 0.17f, 0.42f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.95f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.86f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.86f, 0.20f, 0.20f, 0.72f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.04f, 0.04f, 0.40f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.04f, 0.04f, 0.46f);
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "--roundtrip-events") {
        const char* path = argv[2];
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "cannot open %s\n", path);
            return 1;
        }
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        mm2::eventlang::EventFileAst file;
        if (!mm2::eventlang::loadEventRecords(data.data(), data.size(), file)) {
            std::fprintf(stderr, "bad event.dat header\n");
            return 1;
        }
        int ok = 0, fail = 0;
        for (int i = 0; i < 71; ++i) {
            auto loc = mm2::eventlang::decompileLocation(file.rawRecords[i].data(),
                                                         file.rawRecords[i].size(), i);
            loc.modified = true;
            loc.rawBlob.clear();
            for (auto& sc : loc.scripts) sc.rawSegment.clear();
            auto rebuilt = mm2::eventlang::encodeLocation(loc);
            if (rebuilt == file.rawRecords[i])
                ++ok;
            else {
                ++fail;
                std::fprintf(stderr, "loc %d: mismatch orig=%zu rebuilt=%zu\n", i,
                             file.rawRecords[i].size(), rebuilt.size());
            }
        }
        std::printf("strict AST roundtrip: %d/%d byte-identical\n", ok, ok + fail);
        file.locations.clear();
        for (int i = 0; i < 71; ++i)
            file.locations.push_back(mm2::eventlang::decompileLocation(
                file.rawRecords[i].data(), file.rawRecords[i].size(), i));
        auto full = mm2::eventlang::encodeEventDat(file);
        if (full == data) {
            std::printf("full file: OK\n");
        } else {
            std::printf("full file: MISMATCH (%zu vs %zu)\n", full.size(), data.size());
            return 1;
        }
        return fail ? 1 : 0;
    }

    constexpr float kBaseUiFontPx = 16.0f;
    constexpr float kTargetUiFontPx = 32.0f;

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return 1;

    // OpenGL 3.2+ is required so imgui_impl_opengl3 enables
    // ImGuiBackendFlags_RendererHasVtxOffset (>64K verts per draw list).
    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    const float mainScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    const float fontUiScale = kTargetUiFontPx / kBaseUiFontPx;
    // Keep default window size reasonable on high-DPI while still giving larger-font UIs room.
    const float windowScale = mainScale * std::clamp(fontUiScale, 1.0f, 1.6f);
    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(1500.0f * windowScale), static_cast<int>(900.0f * windowScale),
        "MM2ED - Might & Magic II Editor", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    applyRedBlackTheme();

    ImGuiStyle& style = ImGui::GetStyle();
    const float layoutScale = mainScale * fontUiScale;
    style.ScaleAllSizes(layoutScale);
    // Keep bitmap font at authored pixel size (MM2 16px atlas), otherwise
    // DPI font scaling bakes a larger size and falls back to default glyphs.
    style.FontScaleDpi = 1.0f;
    style.WindowPadding = ImVec2(12.0f * layoutScale, 10.0f * layoutScale);
    style.FramePadding = ImVec2(8.0f * layoutScale, 4.0f * layoutScale);
    style.ItemSpacing = ImVec2(10.0f * layoutScale, 6.0f * layoutScale);
    style.ItemInnerSpacing = ImVec2(8.0f * layoutScale, 4.0f * layoutScale);
    style.CellPadding = ImVec2(6.0f * layoutScale, 4.0f * layoutScale);
    style.WindowMinSize = ImVec2(320.0f * layoutScale, 180.0f * layoutScale);
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
    io.ConfigDpiScaleFonts = false;
    io.ConfigDpiScaleViewports = true;
#endif

    ImNodes::CreateContext();
    ImNodes::StyleColorsDark();
    scaleImNodesStyle(layoutScale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);
    // Install MM2 font only after renderer backend init sets texture capability flags.
    std::fprintf(stderr, "[mm2-font] cwd: %s\n", std::filesystem::current_path().string().c_str());
    ImFont* mm2Font = mm2::installMm2BitmapFont(io, kTargetUiFontPx);
    std::fprintf(stderr, "[mm2-font] install result: %s\n", mm2Font ? "success" : "failed");
    // Dedicated monospace for script/hex panes (UI bitmap is too large + loose spacing).
    constexpr float kCodeFontPx = 20.0f;
    ImFont* codeFont = mm2::installMm2CodeFont(io, kCodeFontPx);
    std::fprintf(stderr, "[mm2-font] code font: %s\n", codeFont ? "success" : "failed");

    mm2::App app;
    bool showFontDebug = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Ctrl+S saves the active section.
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) app.saveActive();

        app.frame();
        if (showFontDebug) {
            ImGui::SetNextWindowBgAlpha(0.85f);
            ImGui::SetNextWindowPos(ImVec2(12.0f * mainScale, 12.0f * mainScale), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("MM2 Font Debug", &showFontDebug,
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) {
                ImGui::Text("%s", mm2::mm2BitmapFontDebugStatus());
                ImFont* cur = ImGui::GetFont();
                ImGui::Text("Current font: %s", cur ? cur->GetDebugName() : "<null>");
                ImGui::Text("Current size: %.1f", ImGui::GetFontSize());
            }
            ImGui::End();
        }

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.05f, 0.01f, 0.01f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

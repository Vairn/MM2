// MM2ED - Might & Magic II (Amiga) data editor.
// ImGui (docking) + GLFW + OpenGL3.

#include <cstdio>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imnodes.h"

#include <GLFW/glfw3.h>

#include "app/App.h"

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

int main(int, char**) {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return 1;

    // OpenGL 3.2+ is required so imgui_impl_opengl3 enables
    // ImGuiBackendFlags_RendererHasVtxOffset (>64K verts per draw list).
    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    const float mainScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(1500.0f * mainScale), static_cast<int>(900.0f * mainScale),
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

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(mainScale);
    style.FontScaleDpi = mainScale;
    style.WindowPadding = ImVec2(12.0f * mainScale, 10.0f * mainScale);
    style.FramePadding = ImVec2(8.0f * mainScale, 4.0f * mainScale);
    style.ItemSpacing = ImVec2(10.0f * mainScale, 6.0f * mainScale);
    style.ItemInnerSpacing = ImVec2(8.0f * mainScale, 4.0f * mainScale);
    style.CellPadding = ImVec2(6.0f * mainScale, 4.0f * mainScale);
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;
#endif

    ImNodes::CreateContext();
    ImNodes::StyleColorsDark();
    scaleImNodesStyle(mainScale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    mm2::App app;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Ctrl+S saves the active section.
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) app.saveActive();

        app.frame();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
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

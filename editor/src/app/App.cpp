#include "app/App.h"

#include <algorithm>
#include <cstdio>

#include "imgui.h"
#include "imgui_internal.h"
#include "portable-file-dialogs.h"
#include "sections/AttribSection.h"
#include "sections/EventSection.h"
#include "sections/GfxSection.h"
#include "sections/ItemsSection.h"
#include "sections/MapSection.h"
#include "sections/MonstersSection.h"
#include "sections/RosterSection.h"
#include "sections/SpellsSection.h"
#include "sections/StrSection.h"

namespace mm2 {

App::App() { registerSections(); }

void App::registerSections() {
    auto items = std::make_unique<ItemsSection>();
    items_ = items.get();
    sections_.push_back(std::move(items));
    sections_.push_back(std::make_unique<MonstersSection>());
    sections_.push_back(std::make_unique<RosterSection>());
    sections_.push_back(std::make_unique<MapSection>());
    sections_.push_back(std::make_unique<AttribSection>());
    sections_.push_back(std::make_unique<StrSection>());
    sections_.push_back(std::make_unique<SpellsSection>());
    sections_.push_back(std::make_unique<EventSection>());
    sections_.push_back(std::make_unique<GfxSection>("Graphics (.32)", ".32", /*isAnm=*/false));
    sections_.push_back(std::make_unique<GfxSection>("Animations (.anm)", ".anm", /*isAnm=*/true));
}

std::string App::itemName(int id) const {
    if (items_ && items_->loaded && id >= 0 && id < kItemsCount) {
        std::string nm = items_->file().records[id].nameStr();
        if (!nm.empty()) return nm;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "#%d", id);
    return buf;
}

void App::openDataFolder(const std::string& dir) {
    pendingDataDir_ = dir;
    pendingOpen_ = true;
}

void App::openDataFolderNow(const std::string& dir) {
    state_.dataDir = dir;
    int ok = 0, total = 0;
    for (auto& s : sections_) {
        ++total;
        if (s->load(dir)) ++ok;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "Loaded %d/%d sections from %s", ok, total, dir.c_str());
    setStatus(buf);
}

void App::saveActive() {
    if (state_.dataDir.empty() || sections_.empty()) return;
    Section* s = sections_[active_].get();
    if (!s->loaded || s->fileName()[0] == '\0') return;
    if (s->save(state_.dataDir))
        setStatus(std::string("Saved ") + s->fileName());
    else
        setStatus(std::string("FAILED to save ") + s->fileName());
}

void App::saveAll() {
    if (state_.dataDir.empty()) return;
    int ok = 0, total = 0;
    for (auto& s : sections_) {
        if (!s->loaded || s->fileName()[0] == '\0') continue;
        ++total;
        if (s->save(state_.dataDir)) ++ok;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "Saved %d/%d loaded files.", ok, total);
    setStatus(buf);
}

void App::drawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Data Folder...")) {
                auto sel = pfd::select_folder("Select MM2 data folder").result();
                if (!sel.empty()) openDataFolder(sel);
            }
            bool hasDir = !state_.dataDir.empty();
            if (ImGui::MenuItem("Save Current", "Ctrl+S", false, hasDir)) saveActive();
            if (ImGui::MenuItem("Save All", nullptr, false, hasDir)) saveAll();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                // Handled by main loop checking window close; just request via ImGui.
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void App::drawBrowser() {
    ImGui::Begin("Data Files");
    if (state_.dataDir.empty()) {
        ImGui::TextWrapped("No data folder open. Use File > Open Data Folder...");
    } else {
        ImGui::TextDisabled("%s", state_.dataDir.c_str());
    }
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(sections_.size()); ++i) {
        Section* s = sections_[i].get();
        char label[96];
        const char* mark = s->dirty ? "* " : (s->loaded ? "  " : "x ");
        snprintf(label, sizeof(label), "%s%s", mark, s->title());
        if (ImGui::Selectable(label, active_ == i)) active_ = i;
        if (ImGui::IsItemHovered() && s->fileName()[0])
            ImGui::SetTooltip("%s%s", s->fileName(), s->loaded ? "" : " (not loaded)");
    }
    ImGui::Separator();
    ImGui::TextDisabled("* unsaved   x missing");
    ImGui::End();
}

void App::drawActivePanel() {
    ImGui::Begin("Editor");
    if (sections_.empty()) {
        ImGui::End();
        return;
    }
    Section* s = sections_[active_].get();
    ImGui::Text("%s", s->title());
    if (s->fileName()[0]) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)%s", s->fileName(), s->dirty ? "  *modified" : "");
    }
    ImGui::Separator();
    ImGui::BeginChild("##section_body", ImVec2(0, 0), ImGuiChildFlags_None);
    s->draw(*this);
    ImGui::EndChild();
    ImGui::End();
}

void App::frame() {
    for (auto& s : sections_) s->flushPending();

    // Fullscreen dockspace host.
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    drawMenuBar();

    ImGuiID dockId = ImGui::GetID("MM2EDDockSpace");
    static bool firstLayout = true;
    if (firstLayout && ImGui::DockBuilderGetNode(dockId) == nullptr) {
        firstLayout = false;
        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);
        ImGuiID left, right;
        const float fontScale = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
        const float leftRatio = std::clamp(0.18f + (fontScale - 1.0f) * 0.08f, 0.18f, 0.33f);
        left = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, leftRatio, nullptr, &right);
        ImGui::DockBuilderDockWindow("Data Files", left);
        ImGui::DockBuilderDockWindow("Editor", right);
        ImGui::DockBuilderFinish(dockId);
    }
    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();

    drawBrowser();
    drawActivePanel();

    // Status bar.
    if (ImGui::BeginViewportSideBar("##status", vp, ImGuiDir_Down, ImGui::GetFrameHeight(),
                                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::Text("%s", state_.status.c_str());
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();

    if (pendingOpen_) {
        openDataFolderNow(pendingDataDir_);
        pendingOpen_ = false;
    }
}

}  // namespace mm2

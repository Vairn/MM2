#include "app/App.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "imgui.h"
#include "imgui_internal.h"
#include "portable-file-dialogs.h"

#include "app/EditorPrefs.h"
#include "core/ItemsFile.h"
#include "core/PcGfx.h"
#include "sections/AttribSection.h"
#include "sections/EventSection.h"
#include "sections/GfxSection.h"
#include "sections/ItemsSection.h"
#include "sections/MapSection.h"
#include "sections/MonstersSection.h"
#include "sections/PcGfxSection.h"
#include "sections/RosterSection.h"
#include "sections/SpellsSection.h"
#include "sections/StrSection.h"
#include "widgets/UiLayout.h"
#include "widgets/UiTheme.h"

namespace mm2 {

App::App() { registerSections(); }

void App::registerSections() {
    auto items = std::make_unique<ItemsSection>();
    items_ = items.get();
    sections_.push_back(std::move(items));
    sections_.push_back(std::make_unique<MonstersSection>());
    sections_.push_back(std::make_unique<SpellsSection>());
    sections_.push_back(std::make_unique<RosterSection>());
    sections_.push_back(std::make_unique<StrSection>());
    auto maps = std::make_unique<MapSection>();
    maps_ = maps.get();
    sections_.push_back(std::move(maps));
    sections_.push_back(std::make_unique<AttribSection>());
    auto events = std::make_unique<EventSection>();
    events_ = events.get();
    events_->setApp(this);
    sections_.push_back(std::move(events));
    sections_.push_back(std::make_unique<GfxSection>("Graphics (.32)", ".32", /*isAnm=*/false));
    sections_.push_back(std::make_unique<GfxSection>("Animations (.anm)", ".anm", /*isAnm=*/true));
    sections_.push_back(std::make_unique<PcGfxSection>("PC Walls (CGA .4)", ".4"));
    sections_.push_back(std::make_unique<PcGfxSection>("PC Walls (EGA .16)", ".16"));
}

int App::sectionIndex(DocKind kind) const {
    for (int i = 0; i < static_cast<int>(sections_.size()); ++i) {
        if (sections_[i]->docKind() == kind) return i;
    }
    return -1;
}

Section* App::sectionFor(DocKind kind) const {
    const int i = sectionIndex(kind);
    return i >= 0 ? sections_[i].get() : nullptr;
}

int App::findOpenTab(DocKind kind) const {
    for (int i = 0; i < static_cast<int>(openTabs_.size()); ++i) {
        if (openTabs_[i] == kind) return i;
    }
    return -1;
}

Section* App::activeSection() const {
    if (activeTab_ < 0 || activeTab_ >= static_cast<int>(openTabs_.size())) return nullptr;
    return sectionFor(openTabs_[activeTab_]);
}

DocKind App::activeDocKind() const {
    if (activeTab_ < 0 || activeTab_ >= static_cast<int>(openTabs_.size())) return DocKind::None;
    return openTabs_[activeTab_];
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

std::string App::monsterName(int id) const {
    auto* m = sectionFor(DocKind::Monsters);
    if (m && m->loaded && id >= 0) {
        auto* mf = dynamic_cast<MonstersSection*>(m);
        if (mf) {
            const auto& recs = mf->file().records;
            if (id < static_cast<int>(recs.size())) {
                std::string nm = recs[id].nameStr();
                if (!nm.empty()) return nm;
            }
        }
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
    state_.pcDataDir = pcFindAssetsDir(dir);
    openTabs_.clear();
    activeTab_ = -1;
    selection_.Clear();

    int ok = 0, total = 0;
    for (auto& s : sections_) {
        ++total;
        if (s->load(dir)) ++ok;
    }
    RememberRecentFolder(dir);
    char buf[256];
    snprintf(buf, sizeof(buf), "Loaded %d/%d documents from %s", ok, total, dir.c_str());
    setStatus(buf);
}

void App::openDocument(DocKind kind) {
    if (kind == DocKind::None) return;
    int tab = findOpenTab(kind);
    if (tab < 0) {
        openTabs_.push_back(kind);
        tab = static_cast<int>(openTabs_.size()) - 1;
    }
    activeTab_ = tab;
    if (selection_.doc != kind) {
        selection_.Clear();
        selection_.doc = kind;
    }
}

void App::openEventsLocation(int locId) {
    openDocument(DocKind::Events);
    if (events_) events_->focusIndex(locId);
    selection_.Select(DocKind::Events, EditorSelection::Kind::EventLoc, locId);
}

void App::openMapTile(int screen, int tileY, int tileX) {
    openDocument(DocKind::Map);
    if (maps_) maps_->focusTile(screen, tileY, tileX);
    selection_.Select(DocKind::Map, EditorSelection::Kind::MapScreen, screen);
}

void App::openDocumentFocused(DocKind kind, int index) {
    openDocument(kind);
    if (index < 0) return;
    if (Section* s = sectionFor(kind)) s->focusIndex(index);
    if (kind == DocKind::Items)
        selection_.Select(kind, EditorSelection::Kind::Item, index);
    else if (kind == DocKind::Monsters)
        selection_.Select(kind, EditorSelection::Kind::Monster, index);
    else if (kind == DocKind::Map)
        selection_.Select(kind, EditorSelection::Kind::MapScreen, index);
}

bool App::confirmDiscard(Section* s, const char* action) const {
    if (!s) return true;
    if (!s->dirty && !s->hasUnsavedBuffer()) return true;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "%s has unsaved changes%s.\n\nDiscard and %s?", s->title(),
             s->hasUnsavedBuffer() ? " (including edit buffer)" : "", action);
    auto r = pfd::message("Unsaved changes", msg, pfd::choice::yes_no, pfd::icon::warning).result();
    return r == pfd::button::yes;
}

void App::closeActiveTab(bool force) {
    if (activeTab_ < 0 || activeTab_ >= static_cast<int>(openTabs_.size())) return;
    Section* s = activeSection();
    if (!force && !confirmDiscard(s, "close")) return;
    openTabs_.erase(openTabs_.begin() + activeTab_);
    if (openTabs_.empty()) {
        activeTab_ = -1;
        selection_.Clear();
    } else {
        if (activeTab_ >= static_cast<int>(openTabs_.size()))
            activeTab_ = static_cast<int>(openTabs_.size()) - 1;
        selection_.doc = openTabs_[activeTab_];
        if (selection_.kind != EditorSelection::Kind::None && selection_.doc != openTabs_[activeTab_])
            selection_.Clear();
    }
}

void App::saveActive() {
    Section* s = activeSection();
    if (!s || state_.dataDir.empty()) return;
    if (s->isReadOnly() || s->fileName()[0] == '\0') {
        setStatus(std::string(s->title()) + " is read-only (viewer)");
        return;
    }
    if (!s->loaded) return;
    if (s->hasUnsavedBuffer()) {
        setStatus("Compile script changes before saving event.dat (or lose buffer on save of compiled data)");
    }
    if (s->save(state_.dataDir))
        setStatus(std::string("Saved ") + s->fileName());
    else
        setStatus(std::string("FAILED to save ") + s->fileName());
}

void App::saveAll() {
    if (state_.dataDir.empty()) return;
    int ok = 0, total = 0;
    for (auto& s : sections_) {
        if (!s->loaded || s->isReadOnly() || s->fileName()[0] == '\0') continue;
        ++total;
        if (s->save(state_.dataDir)) ++ok;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "Saved %d/%d loaded files.", ok, total);
    setStatus(buf);
}

void App::drawRecentFoldersMenu() {
    auto recent = LoadRecentFolders();
    if (ImGui::BeginMenu("Open Recent", !recent.empty())) {
        for (const auto& path : recent) {
            const std::string label = RecentFolderLabel(path);
            if (ImGui::MenuItem(label.c_str())) openDataFolder(path);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Recent")) ClearRecentFolders();
        ImGui::EndMenu();
    }
}

void App::drawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Data Folder...")) {
                auto sel = pfd::select_folder("Select MM2 data folder").result();
                if (!sel.empty()) openDataFolder(sel);
            }
            drawRecentFoldersMenu();
            ImGui::Separator();
            const bool hasDir = !state_.dataDir.empty();
            Section* active = activeSection();
            const bool canSave = hasDir && active && active->loaded && !active->isReadOnly() &&
                                 active->fileName()[0] != '\0';
            if (ImGui::MenuItem("Save", "Ctrl+S", false, canSave)) saveActive();
            if (ImGui::MenuItem("Save All", nullptr, false, hasDir)) saveAll();
            ImGui::Separator();
            if (ImGui::MenuItem("Close Tab", nullptr, false, activeTab_ >= 0)) closeActiveTab();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) requestQuit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Project Tree", nullptr, &showTree_);
            ImGui::MenuItem("Properties", nullptr, &showProps_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Run")) {
            const char* runCmd = std::getenv("MM2ED_RUN");
            const bool hasDir = !state_.dataDir.empty();
            if (ImGui::MenuItem("Launch (MM2ED_RUN)…", "F5", false, hasDir && runCmd && runCmd[0])) {
#ifdef _WIN32
                std::string cmd = std::string("\"") + runCmd + "\" \"" + state_.dataDir + "\"";
                STARTUPINFOA si{};
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi{};
                std::vector<char> buf(cmd.begin(), cmd.end());
                buf.push_back('\0');
                if (CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr,
                                   state_.dataDir.c_str(), &si, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    setStatus(std::string("Launched ") + runCmd);
                } else {
                    setStatus("FAILED to launch MM2ED_RUN");
                }
#else
                std::string cmd = std::string("\"") + runCmd + "\" \"" + state_.dataDir + "\" &";
                if (std::system(cmd.c_str()) == 0)
                    setStatus(std::string("Launched ") + runCmd);
                else
                    setStatus("FAILED to launch MM2ED_RUN");
#endif
            }
            if (!runCmd || !runCmd[0]) {
                ImGui::TextDisabled("Set MM2ED_RUN to a host/exe path");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void App::drawProjectTree() {
    if (!showTree_) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::Em(0.6f), ui::Em(0.5f)));
    ImGui::Begin("Project Tree");

    if (state_.dataDir.empty()) {
        ui::EmptyState("No project open", "File → Open Data Folder…\n(folder with items.dat / map.dat / …)");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ui::Muted());
    ImGui::TextWrapped("%s", state_.dataDir.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* groups[] = {"Game data", "World", "Graphics"};
    for (const char* group : groups) {
        if (!ImGui::TreeNodeEx(group, ImGuiTreeNodeFlags_DefaultOpen)) continue;
        for (auto& s : sections_) {
            if (std::strcmp(DocKindGroup(s->docKind()), group) != 0) continue;
            const bool missing = !s->loaded && s->fileName()[0] != '\0';
            const bool dirty = s->dirty || s->hasUnsavedBuffer();
            const bool open = findOpenTab(s->docKind()) >= 0;
            const bool active = activeDocKind() == s->docKind();

            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Header, ui::AccentSoft());
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ui::AccentSoft());
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ui::Accent());
            } else if (missing) {
                ImGui::PushStyleColor(ImGuiCol_Text, ui::Muted());
            } else if (dirty) {
                ImGui::PushStyleColor(ImGuiCol_Text, ui::Warn());
            }

            char label[96];
            const char* mark = dirty ? "* " : (missing ? "o " : (open ? "· " : "  "));
            snprintf(label, sizeof(label), "%s%s", mark, s->title());
            if (ImGui::Selectable(label, active, ImGuiSelectableFlags_SpanAvailWidth))
                openDocument(s->docKind());

            if (active)
                ImGui::PopStyleColor(3);
            else if (missing || dirty)
                ImGui::PopStyleColor();

            if (ImGui::IsItemHovered()) {
                if (s->fileName()[0])
                    ImGui::SetTooltip("%s%s%s", s->fileName(),
                                      s->loaded ? "" : " (not loaded)",
                                      s->isReadOnly() ? " · read-only" : "");
                else if (s->isReadOnly())
                    ImGui::SetTooltip("Viewer (read-only)");
            }
        }
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ui::TextMuted("* unsaved   o missing   · open");
    ImGui::End();
    ImGui::PopStyleVar();
}

void App::drawWorkspace() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::Em(0.75f), ui::Em(0.55f)));
    ImGui::Begin("Workspace");

    if (state_.dataDir.empty()) {
        ui::EmptyState("Open a data folder", "File → Open Data Folder… to load MM2 .dat files");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    if (openTabs_.empty()) {
        ui::EmptyState("No document open", "Select an entry in the Project Tree");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    if (ImGui::BeginTabBar("##OpenDocs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs |
                                             ImGuiTabBarFlags_FittingPolicyScroll)) {
        int closeIdx = -1;
        int clickedIdx = -1;
        for (int i = 0; i < static_cast<int>(openTabs_.size()); ++i) {
            Section* s = sectionFor(openTabs_[i]);
            if (!s) continue;
            char label[96];
            const bool dirty = s->dirty || s->hasUnsavedBuffer();
            snprintf(label, sizeof(label), "%s%s###tab%d", s->title(), dirty ? " *" : "",
                     static_cast<int>(openTabs_[i]));
            bool open = true;
            ImGuiTabItemFlags flags = 0;
            if (i == activeTab_) flags |= ImGuiTabItemFlags_SetSelected;
            const bool shown = ImGui::BeginTabItem(label, &open, flags);
            if (shown && ImGui::IsItemClicked()) clickedIdx = i;

            // Draw content only for the active tab. Driving the body (and thus
            // activeTab_) off BeginTabItem's returned "shown" state is racy:
            // ImGui's tab-bar selection lags one frame behind activeTab_, so on
            // the frame after a Project Tree click it reports the old tab as
            // shown and would snap us back to the first opened file.
            if (shown && i == activeTab_) {
                char fileSub[96] = {};
                if (s->fileName()[0]) snprintf(fileSub, sizeof(fileSub), "%s", s->fileName());
                const ImVec4 chipCol = ui::Warn();
                const char* chip = nullptr;
                if (s->isReadOnly())
                    chip = "READ-ONLY";
                else if (s->hasUnsavedBuffer())
                    chip = "BUFFER";
                else if (s->dirty)
                    chip = "MODIFIED";
                ui::PanelHeader(s->title(), fileSub[0] ? fileSub : nullptr, chip,
                                chip ? &chipCol : nullptr);

                ImGui::BeginChild("##workspace_body", ImVec2(0, 0), ImGuiChildFlags_None);
                s->drawWorkspace(*this, selection_);
                ImGui::EndChild();
            }
            if (shown) ImGui::EndTabItem();
            if (!open) closeIdx = i;
        }
        ImGui::EndTabBar();

        if (clickedIdx >= 0) activeTab_ = clickedIdx;

        if (closeIdx >= 0) {
            activeTab_ = closeIdx;
            closeActiveTab();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void App::drawProperties() {
    if (!showProps_) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::Em(0.65f), ui::Em(0.5f)));
    ImGui::Begin("Properties");

    Section* s = activeSection();
    if (!s) {
        ui::EmptyState("Nothing selected", "Open a document and pick a row / entity");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    s->drawProperties(*this, selection_);
    ImGui::End();
    ImGui::PopStyleVar();
}

void App::drawStatusBar() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float statusH = ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y;
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.08f, 0.04f, 0.04f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.10f, 0.10f, 0.35f));
    if (ImGui::BeginViewportSideBar("##status", vp, ImGuiDir_Down, statusH,
                                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ui::Muted());
            ImGui::TextUnformatted(state_.status.empty() ? "Ready" : state_.status.c_str());

            int dirtyCount = 0;
            for (auto& sec : sections_) {
                if (sec->dirty || sec->hasUnsavedBuffer()) ++dirtyCount;
            }
            if (dirtyCount > 0) {
                ImGui::SameLine(0, ui::Em(1.2f));
                ImGui::PushStyleColor(ImGuiCol_Text, ui::Warn());
                ImGui::Text("%d unsaved", dirtyCount);
                ImGui::PopStyleColor();
            }

            Section* active = activeSection();
            if (active) {
                if (const char* buf = active->bufferStatus()) {
                    ImGui::SameLine(0, ui::Em(1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ui::Warn());
                    ImGui::TextUnformatted(buf);
                    ImGui::PopStyleColor();
                }
                if (!state_.dataDir.empty() && active->fileName()[0]) {
                    ImGui::SameLine(0, ui::Em(1.0f));
                    ImGui::TextDisabled("|  %s", active->fileName());
                }
            }
            ImGui::PopStyleColor();
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

void App::frame() {
    for (auto& s : sections_) s->flushPending();

    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        const char* runCmd = std::getenv("MM2ED_RUN");
        if (runCmd && runCmd[0] && !state_.dataDir.empty()) {
#ifdef _WIN32
            std::string cmd = std::string("\"") + runCmd + "\" \"" + state_.dataDir + "\"";
            STARTUPINFOA si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            std::vector<char> buf(cmd.begin(), cmd.end());
            buf.push_back('\0');
            if (CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr,
                               state_.dataDir.c_str(), &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                setStatus(std::string("Launched ") + runCmd);
            }
#else
            std::string cmd = std::string("\"") + runCmd + "\" \"" + state_.dataDir + "\" &";
            std::system(cmd.c_str());
#endif
        }
    }

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
    if (firstLayout_ && ImGui::DockBuilderGetNode(dockId) == nullptr) {
        firstLayout_ = false;
        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

        ImGuiID left, center, right;
        const float fontScale = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
        const float leftRatio = std::clamp(0.16f + (fontScale - 1.0f) * 0.06f, 0.14f, 0.28f);
        const float rightRatio = std::clamp(0.18f + (fontScale - 1.0f) * 0.03f, 0.16f, 0.24f);

        left = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, leftRatio, nullptr, &center);
        right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, rightRatio / (1.0f - leftRatio),
                                           nullptr, &center);
        ImGui::DockBuilderDockWindow("Project Tree", left);
        ImGui::DockBuilderDockWindow("Workspace", center);
        ImGui::DockBuilderDockWindow("Properties", right);
        ImGui::DockBuilderFinish(dockId);
    }
    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();

    drawProjectTree();
    drawWorkspace();
    drawProperties();
    drawStatusBar();

    if (pendingOpen_) {
        openDataFolderNow(pendingDataDir_);
        pendingOpen_ = false;
    }
}

}  // namespace mm2

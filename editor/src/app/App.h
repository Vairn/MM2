#pragma once
#include <memory>
#include <string>
#include <vector>

#include "app/AppState.h"
#include "app/DocKind.h"
#include "app/EditorSelection.h"
#include "app/Section.h"

struct GLFWwindow;

namespace mm2 {

class ItemsSection;
class EventSection;
class MapSection;

class App {
public:
    App();

    // Called once per frame between ImGui::NewFrame and ImGui::Render.
    void frame();

    AppState& state() { return state_; }
    EditorSelection& selection() { return selection_; }

    // Cross-section helper: resolve an item id (0..255) to a display name.
    std::string itemName(int id) const;
    // Cross-section helper: resolve a monster id (0..kMonstersCount) to a name.
    std::string monsterName(int id) const;

    void openDataFolder(const std::string& dir);
    void saveActive();
    void saveAll();

    // Open / focus a document tab by kind.
    void openDocument(DocKind kind);
    // Open a document and jump to a specific record/index within it.
    void openDocumentFocused(DocKind kind, int index);
    void closeActiveTab(bool force = false);
    Section* sectionFor(DocKind kind) const;
    Section* activeSection() const;
    DocKind activeDocKind() const;

    // Cross-doc jump: open Events and select location `locId`.
    void openEventsLocation(int locId);
    /** Open Maps on `screen` and select tile (y,x). */
    void openMapTile(int screen, int tileY, int tileX);

    bool quitRequested() const { return quitRequested_; }
    void requestQuit() { quitRequested_ = true; }

private:
    void registerSections();
    void drawMenuBar();
    void drawProjectTree();
    void drawWorkspace();
    void drawProperties();
    void drawStatusBar();
    void drawRecentFoldersMenu();
    void openDataFolderNow(const std::string& dir);
    void setStatus(const std::string& s) { state_.status = s; }
    int sectionIndex(DocKind kind) const;
    int findOpenTab(DocKind kind) const;
    bool confirmDiscard(Section* s, const char* action) const;

    AppState state_;
    EditorSelection selection_;
    std::vector<std::unique_ptr<Section>> sections_;
    std::vector<DocKind> openTabs_;
    int activeTab_ = -1;  // index into openTabs_
    ItemsSection* items_ = nullptr;
    EventSection* events_ = nullptr;
    MapSection* maps_ = nullptr;
    std::string pendingDataDir_;
    bool pendingOpen_ = false;
    bool quitRequested_ = false;
    bool showTree_ = true;
    bool showProps_ = true;
    bool firstLayout_ = true;
};

}  // namespace mm2

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "app/AppState.h"
#include "app/Section.h"

struct GLFWwindow;

namespace mm2 {

class ItemsSection;

class App {
public:
    App();

    // Called once per frame between ImGui::NewFrame and ImGui::Render.
    void frame();

    AppState& state() { return state_; }

    // Cross-section helper: resolve an item id (0..255) to a display name.
    // Falls back to "#<id>" when items.dat is not loaded.
    std::string itemName(int id) const;

    void openDataFolder(const std::string& dir);
    void saveActive();
    void saveAll();

private:
    void registerSections();
    void drawMenuBar();
    void drawBrowser();
    void drawActivePanel();
    void setStatus(const std::string& s) { state_.status = s; }

    AppState state_;
    std::vector<std::unique_ptr<Section>> sections_;
    int active_ = 0;
    ItemsSection* items_ = nullptr;  // non-owning; for cross-references
};

}  // namespace mm2

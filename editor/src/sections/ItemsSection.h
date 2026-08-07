#pragma once
#include "app/Section.h"
#include "core/ItemsFile.h"
#include "widgets/UiLayout.h"

namespace mm2 {

class ItemsSection : public Section {
public:
    DocKind docKind() const override { return DocKind::Items; }
    const char* fileName() const override { return "items.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;
    void focusIndex(int index) override { selected_ = index; }

    const ItemsFile& file() const { return file_; }

private:
    void drawItemDetail();
    ItemsFile file_;
    int selected_ = 0;
    ui::MasterDetail layout_;
};

}  // namespace mm2

#pragma once
#include <string>

#include "app/Section.h"
#include "core/SpellsFile.h"
#include "widgets/UiLayout.h"

namespace mm2 {

class SpellsSection : public Section {
public:
    DocKind docKind() const override { return DocKind::Spells; }
    const char* fileName() const override { return "spells.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;

private:
    void drawSpellDetail();
    SpellsFile file_;
    int selected_ = 0;
    ui::MasterDetail layout_;
};

}  // namespace mm2

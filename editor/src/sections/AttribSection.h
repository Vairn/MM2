#pragma once
#include "app/Section.h"
#include "core/AttribFile.h"

namespace mm2 {

class AttribSection : public Section {
public:
    DocKind docKind() const override { return DocKind::Attrib; }
    const char* title() const override { return "Map Attributes"; }
    const char* fileName() const override { return "attrib.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;
    void focusIndex(int index) override;

private:
    AttribFile file_;
    int screen_ = 0;
};

}  // namespace mm2

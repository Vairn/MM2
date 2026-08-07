#pragma once
#include <string>
#include <vector>

#include "app/Section.h"
#include "core/StrFile.h"

namespace mm2 {

class StrSection : public Section {
public:
    DocKind docKind() const override { return DocKind::Str; }
    const char* fileName() const override { return "str.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;

private:
    void rebuildLines();
    void syncTextFromLines();

    StrFile file_;
    std::vector<std::string> lines_;
    bool linesBuilt_ = false;
    int selectedLine_ = -1;
};

}  // namespace mm2

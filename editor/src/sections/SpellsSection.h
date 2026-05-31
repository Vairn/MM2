#pragma once
#include <string>

#include "app/Section.h"
#include "core/SpellsFile.h"

namespace mm2 {

class SpellsSection : public Section {
public:
    const char* title() const override { return "Spells"; }
    const char* fileName() const override { return "spells.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;

private:
    SpellsFile file_;
    int selected_ = 0;
};

}  // namespace mm2

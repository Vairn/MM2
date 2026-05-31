#pragma once
#include <string>
#include <vector>

#include "app/Section.h"
#include "core/StrFile.h"

namespace mm2 {

class StrSection : public Section {
public:
    const char* title() const override { return "Strings"; }
    const char* fileName() const override { return "str.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;

private:
    void rebuildLines();          // split file_.text on '\n' into lines_
    void syncTextFromLines();     // rejoin lines_ back into file_.text

    StrFile file_;
    std::vector<std::string> lines_;
    bool linesBuilt_ = false;
};

}  // namespace mm2

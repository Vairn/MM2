#pragma once
#include "app/Section.h"
#include "core/ItemsFile.h"

namespace mm2 {

class ItemsSection : public Section {
public:
    const char* title() const override { return "Items"; }
    const char* fileName() const override { return "items.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;

    const ItemsFile& file() const { return file_; }

private:
    ItemsFile file_;
    int selected_ = 0;
};

}  // namespace mm2

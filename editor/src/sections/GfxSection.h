#pragma once
// Graphics viewer for the planar image-chunk formats. One instance handles a
// file extension (".32" tilesets or ".anm" animations); it scans the data
// folder, decodes the selected file, and previews frames + palette.

#include <string>
#include <vector>

#include "app/Section.h"
#include "core/Gfx.h"

namespace mm2 {

class GfxSection : public Section {
public:
    GfxSection(const char* title, const char* ext, bool isAnm)
        : title_(title), ext_(ext), isAnm_(isAnm) {}
    ~GfxSection() override;

    const char* title() const override { return title_; }
    const char* fileName() const override { return ""; }  // scans many files
    bool load(const std::string& dataDir) override;       // scans the folder
    bool save(const std::string& dataDir) override { (void)dataDir; return false; }
    void draw(App& app) override;

private:
    void selectFile(int idx);
    void releaseTextures();

    const char* title_;
    const char* ext_;
    bool isAnm_;

    std::string dir_;
    std::vector<std::string> files_;
    int selectedFile_ = -1;
    int selectedFrame_ = 0;
    float zoom_ = 2.0f;

    GfxImage image_;
    std::vector<unsigned int> textures_;
};

}  // namespace mm2

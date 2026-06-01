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
    bool initTitlePreviewAssets();
    void updateTitlePlayback();
    void drawTitleTab();

    const char* title_;
    const char* ext_;
    bool isAnm_;

    std::string dir_;
    std::vector<std::string> files_;
    int selectedFile_ = -1;
    int selectedFrame_ = 0;
    float zoom_ = 2.0f;
    bool playing_ = false;
    bool loop_ = true;
    float speed_ = 1.0f;
    double lastTick_ = 0.0;
    float elapsed_ = 0.0f;
    int selectedSequence_ = 0;
    int sequenceStep_ = 0;
    bool useAnmPreludePlacement_ = false;
    int compositeCanvasW_ = 0;
    int compositeCanvasH_ = 0;
    int compositeCanvasMinX_ = 0;
    int compositeCanvasMinY_ = 0;
    unsigned int compositeTexture_ = 0;
    int compositeTextureFrame_ = -1;
    bool compositeTextureValid_ = false;
    std::vector<uint8_t> compositeRgba_;
    // Title compositor preview: intro.32 + introclips.32.
    bool titleReady_ = false;
    std::string titleStatus_;
    GfxImage titleIntro_;
    GfxImage titleClips_;
    unsigned int titleTexture_ = 0;
    int titleCanvasW_ = 0;
    int titleCanvasH_ = 0;
    std::vector<uint8_t> titleRgba_;
    int titleFrame_ = 0;
    bool titlePlaying_ = true;
    bool titleLoop_ = true;
    bool titleAnimateBase_ = false;
    float titleSpeed_ = 1.0f;
    double titleLastTick_ = 0.0;
    float titleElapsed_ = 0.0f;
    bool titleAsmReplay_ = true;
    int titleAsmTickCounter_ = 0;   // mirrors A4-$6478 gate in 0x26A10
    int titleAsmFrame_ = 0;         // mirrors A4-$647A frame index in 0x26A10
    int titleAsmX_ = 8;
    int titleAsmYTop_ = 8;
    int titleAsmYBottom_ = 216;
    bool titleAsmCanvasPrimed_ = false;

    GfxImage image_;
    std::vector<unsigned int> textures_;
};

}  // namespace mm2

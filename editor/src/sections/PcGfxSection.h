#pragma once
// PC DOS CGA (.4) / EGA (.16) sheets and MONSTERS.4/.16 atlas.
// Same scan/decode path as GfxSection; codec in core/PcGfx.h
// (tools/decode_pc_gfx.py, docs/54-pc-dos-graphics-formats.md).
// Assets live in AppState::pcDataDir, not the Amiga data folder.

#include <optional>
#include <string>
#include <vector>

#include "app/Section.h"
#include "core/PcGfx.h"

namespace mm2 {

enum class PcPlayMode { Flipbook, Sequence };

class PcGfxSection : public Section {
public:
    PcGfxSection(const char* title, const char* ext) : title_(title), ext_(ext) {}
    ~PcGfxSection() override;

    DocKind docKind() const override {
        return (ext_ && ext_[1] == '4') ? DocKind::PcGfxCga : DocKind::PcGfxEga;
    }
    const char* title() const override { return title_; }
    const char* fileName() const override { return ""; }  // scans many files
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override { (void)dataDir; return false; }
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;

private:
    void rescan(const std::string& dir);
    void selectFile(int idx);
    int indexOfStem(const char* stem) const;
    void syncWallsetFromFile();
    void releaseTextures();

    // Wall sheet helpers.
    void loadWallSheet();
    void buildWallTextures();

    // Monster atlas helpers.
    void loadAtlas();
    void selectPicture(int pictureId);
    void buildMonsterTextures();

    void drawWallDetail();
    void drawMonsterDetail();

    const char* title_;
    const char* ext_;      // ".4" or ".16"
    int bpp_;              // 2 or 4

    std::string dir_;
    std::string lastSyncedPcDir_;
    std::vector<std::string> files_;
    int selectedFile_ = -1;
    bool isMonsters_ = false;
    int cgaPalette_ = 1;  // 0 = green/red/brown, 1 = cyan/magenta/white (MM2 default)
    float zoom_ = 3.0f;

    // --- Wall sheet state ---
    PcWallSheet wall_;
    std::vector<unsigned int> wallTextures_;
    int wallFrame_ = 0;
    bool wallPlaying_ = false;
    bool wallLoop_ = true;
    float wallSpeed_ = 1.0f;
    double wallLastTick_ = 0.0;
    float wallElapsed_ = 0.0f;

    int selectedWallset_ = -1;
    PcMonstersAtlas atlas_;
    std::vector<int> availablePicIds_;
    int selectedPicId_ = -1;
    std::optional<PcMonsterPicture> pic_;
    std::vector<unsigned int> picRawTextures_;       // per raw frame (index-aligned with pic_->frames)
    std::vector<unsigned int> picComposedTextures_;  // per state 0..frameCount-1
    int monState_ = 0;
    bool monPlaying_ = true;
    bool monLoop_ = true;
    float monSpeed_ = 1.0f;
    double monLastTick_ = 0.0;
    float monElapsed_ = 0.0f;
    int monSequence_ = 0;
    int monSequenceStep_ = 0;
    PcPlayMode monPlayMode_ = PcPlayMode::Flipbook;
};

}  // namespace mm2

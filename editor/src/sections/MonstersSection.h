#pragma once
#include <optional>
#include <string>
#include <vector>

#include "app/Section.h"
#include "core/Gfx.h"
#include "core/MonstersFile.h"
#include "core/PcGfx.h"
#include "widgets/UiLayout.h"

namespace mm2 {

enum class PcSpriteMode { Amiga, Cga, Ega };

class MonstersSection : public Section {
public:
    ~MonstersSection() override;

    DocKind docKind() const override { return DocKind::Monsters; }
    const char* fileName() const override { return "monsters.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void drawWorkspace(App& app, EditorSelection& sel) override;
    void drawProperties(App& app, EditorSelection& sel) override;
    void focusIndex(int index) override { selected_ = index; }

    const MonstersFile& file() const { return file_; }

private:
    void loadSprite(uint8_t picture);
    void releaseTextures();
    void buildSpriteComposedTextures();
    void syncPcSprite(App& app, uint8_t picture);
    void releasePcTextures();
    void buildPcComposedTextures();
    void drawMonsterDetail(App& app);

    MonstersFile file_;
    int selected_ = 0;
    ui::MasterDetail layout_;

    std::string dir_;

    int spritePic_ = -1;
    int spriteFrame_ = 0;
    float spriteZoom_ = 2.0f;
    bool spritePlaying_ = true;
    bool spriteLoop_ = true;
    float spriteSpeed_ = 1.0f;
    double spriteLastTick_ = 0.0;
    float spriteElapsed_ = 0.0f;
    int spriteSequence_ = 0;
    int spriteSequenceStep_ = 0;
    AnmPlayMode spritePlayMode_ = AnmPlayMode::Flipbook;
    GfxImage sprite_;
    std::string spriteFile_;
    std::vector<unsigned int> textures_;
    std::vector<unsigned int> composedTextures_;
    int spriteCanvasW_ = 0;
    int spriteCanvasH_ = 0;
    int spriteCanvasMinX_ = 0;
    int spriteCanvasMinY_ = 0;
    GfxAnmCanvas spriteAnmCanvas_{};

    PcSpriteMode pcMode_ = PcSpriteMode::Amiga;
    std::string pcDir_;
    PcMonstersAtlas pcAtlasCga_;
    PcMonstersAtlas pcAtlasEga_;
    bool pcAtlasCgaLoaded_ = false;
    bool pcAtlasEgaLoaded_ = false;
    int pcCgaPalette_ = 1;

    int pcPicId_ = -1;
    PcSpriteMode pcPicVariant_ = PcSpriteMode::Amiga;
    std::optional<PcMonsterPicture> pcPic_;
    std::vector<unsigned int> pcComposedTextures_;
    int pcState_ = 0;
    bool pcPlaying_ = true;
    bool pcLoop_ = true;
    float pcSpeed_ = 1.0f;
    double pcLastTick_ = 0.0;
    float pcElapsed_ = 0.0f;
};

}  // namespace mm2

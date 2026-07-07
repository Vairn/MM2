#pragma once
#include <optional>
#include <string>
#include <vector>

#include "app/Section.h"
#include "core/Gfx.h"
#include "core/MonstersFile.h"
#include "core/PcGfx.h"

namespace mm2 {

// Sprite source for the preview pane: Amiga NN.anm (default), or the GOG PC
// CGA/EGA combat atlas (MONSTERS.4 / MONSTERS.16), keyed by the same picture id.
enum class PcSpriteMode { Amiga, Cga, Ega };

class MonstersSection : public Section {
public:
    ~MonstersSection() override;

    const char* title() const override { return "Monsters"; }
    const char* fileName() const override { return "monsters.dat"; }
    bool load(const std::string& dataDir) override;
    bool save(const std::string& dataDir) override;
    void draw(App& app) override;

private:
    // Decode the NN.anm sprite for a picture byte and upload its frames as
    // textures. NN = picture & 0x7F (the 0x80 bit is a separate flag).
    void loadSprite(uint8_t picture);
    void releaseTextures();
    void buildSpriteComposedTextures();

    // PC (CGA/EGA) preview: syncs the shared PC assets folder, lazily loads
    // the matching MONSTERS.4/.16 atlas, and decodes the picture for `picture`.
    void syncPcSprite(App& app, uint8_t picture);
    void releasePcTextures();
    void buildPcComposedTextures();

    MonstersFile file_;
    int selected_ = 0;

    std::string dir_;

    // Cached sprite for the currently shown picture byte.
    int spritePic_ = -1;     // picture byte currently decoded (-1 = none)
    int spriteFrame_ = 0;    // selected animation frame
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

    // --- PC (CGA/EGA) preview state ---
    PcSpriteMode pcMode_ = PcSpriteMode::Amiga;
    std::string pcDir_;
    PcMonstersAtlas pcAtlasCga_;
    PcMonstersAtlas pcAtlasEga_;
    bool pcAtlasCgaLoaded_ = false;  // load attempted (may still have failed)
    bool pcAtlasEgaLoaded_ = false;
    int pcCgaPalette_ = 1;  // 0 = green/red/brown, 1 = cyan/magenta/white (MM2 default)

    int pcPicId_ = -1;               // picture id currently decoded into pcPic_
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

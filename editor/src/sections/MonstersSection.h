#pragma once
#include <string>
#include <vector>

#include "app/Section.h"
#include "core/Gfx.h"
#include "core/MonstersFile.h"

namespace mm2 {

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
    void rebuildSpriteCompositeForFrame();

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
    GfxImage sprite_;
    std::string spriteFile_;
    std::vector<unsigned int> textures_;
    int spriteCanvasW_ = 0;
    int spriteCanvasH_ = 0;
    int spriteCanvasMinX_ = 0;
    int spriteCanvasMinY_ = 0;
    unsigned int spriteCompositeTex_ = 0;
    int spriteCompositeFrame_ = -1;
    std::vector<uint8_t> spriteCompositeRgba_;
};

}  // namespace mm2

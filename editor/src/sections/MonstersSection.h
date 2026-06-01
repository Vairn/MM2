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
};

}  // namespace mm2

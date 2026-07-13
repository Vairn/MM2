#include "sections/MonstersSection.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "app/App.h"
#include "imgui.h"
#include "widgets/HexView.h"
#include "widgets/Texture.h"
#include "widgets/UiLayout.h"
#include "widgets/UiTheme.h"

namespace fs = std::filesystem;

namespace mm2 {

namespace {

// Case-insensitive lookup of MONSTERS.4 / MONSTERS.16 within `dir` (GOG ships
// ALLCAPS filenames, but the host filesystem may be case-sensitive).
std::string findMonstersFile(const std::string& dir, const std::string& ext) {
    std::error_code ec;
    if (dir.empty() || !fs::is_directory(dir, ec)) return "";
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::string extLower = e.path().extension().string();
        std::transform(extLower.begin(), extLower.end(), extLower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extLower != ext) continue;
        if (pcIsMonstersFile(e.path().filename().string())) return e.path().string();
    }
    return "";
}

}  // namespace

MonstersSection::~MonstersSection() {
    releaseTextures();
    releasePcTextures();
}

void MonstersSection::releaseTextures() {
    for (unsigned int t : textures_) freeTexture(t);
    textures_.clear();
    for (unsigned int t : composedTextures_) freeTexture(t);
    composedTextures_.clear();
}

void MonstersSection::releasePcTextures() {
    for (unsigned int t : pcComposedTextures_) freeTexture(t);
    pcComposedTextures_.clear();
}

void MonstersSection::buildPcComposedTextures() {
    releasePcTextures();
    if (!pcPic_ || !pcPic_->ok) return;
    std::vector<uint8_t> rgba;
    pcComposedTextures_.reserve(pcPic_->frames.size());
    for (int i = 0; i < static_cast<int>(pcPic_->frames.size()); ++i) {
        pcCompositeMonsterFrame(*pcPic_, i, pcCgaPalette_, rgba);
        pcComposedTextures_.push_back(makeTextureRGBA(rgba.data(), kPcCombatCanvasW, kPcCombatCanvasH));
    }
}

void MonstersSection::syncPcSprite(App& app, uint8_t picture) {
    // Pull the shared PC assets folder (set by PcGfxSection or auto-detected
    // on folder open); reset cached atlases if it changed.
    if (pcDir_ != app.state().pcDataDir) {
        pcDir_ = app.state().pcDataDir;
        pcAtlasCgaLoaded_ = false;
        pcAtlasEgaLoaded_ = false;
        pcPicId_ = -1;
    }
    if (pcMode_ == PcSpriteMode::Amiga) return;

    const bool wantCga = (pcMode_ == PcSpriteMode::Cga);
    PcMonstersAtlas& atlas = wantCga ? pcAtlasCga_ : pcAtlasEga_;
    bool& atlasLoaded = wantCga ? pcAtlasCgaLoaded_ : pcAtlasEgaLoaded_;
    if (!atlasLoaded) {
        std::string path = findMonstersFile(pcDir_, wantCga ? ".4" : ".16");
        if (!path.empty()) pcLoadMonstersAtlas(path, atlas);
        atlasLoaded = true;
    }

    const int pictureId = picture & 0x7F;
    if (pcPicId_ == pictureId && pcPicVariant_ == pcMode_) return;  // already decoded
    pcPicId_ = pictureId;
    pcPicVariant_ = pcMode_;
    pcState_ = 0;
    pcElapsed_ = 0.0f;
    pcLastTick_ = ImGui::GetTime();
    pcPic_ = atlas.ok ? pcMonsterPictureForId(atlas, pictureId) : std::nullopt;
    buildPcComposedTextures();
    pcPlaying_ = pcPic_ && pcPic_->frames.size() > 1;
}

void MonstersSection::buildSpriteComposedTextures() {
    composedTextures_.clear();
    if (sprite_.frames.empty()) return;

    spriteAnmCanvas_ = gfxAnmCompositeCanvas(sprite_);
    if (!spriteAnmCanvas_.valid) return;

    spriteCanvasMinX_ = spriteAnmCanvas_.minX;
    spriteCanvasMinY_ = spriteAnmCanvas_.minY;
    spriteCanvasW_ = spriteAnmCanvas_.width;
    spriteCanvasH_ = spriteAnmCanvas_.height;

    std::vector<uint8_t> rgba;
    composedTextures_.reserve(sprite_.frames.size());
    for (int i = 0; i < static_cast<int>(sprite_.frames.size()); ++i) {
        if (!gfxAnmCompositeFrame(sprite_, i, rgba, &spriteAnmCanvas_)) {
            composedTextures_.push_back(0);
            continue;
        }
        composedTextures_.push_back(makeTextureRGBA(rgba.data(), spriteAnmCanvas_.width, spriteAnmCanvas_.height));
    }
}

bool MonstersSection::load(const std::string& dataDir) {
    dir_ = dataDir;
    loaded = file_.load(dataDir + "/" + fileName());
    dirty = false;
    releaseTextures();
    sprite_.clear();
    spritePic_ = -1;
    spriteCanvasW_ = spriteCanvasH_ = 0;
    spriteCanvasMinX_ = spriteCanvasMinY_ = 0;
    releasePcTextures();
    pcPic_.reset();
    pcPicId_ = -1;
    pcAtlasCgaLoaded_ = pcAtlasEgaLoaded_ = false;
    return loaded;
}

bool MonstersSection::save(const std::string& dataDir) {
    bool ok = file_.save(dataDir + "/" + fileName());
    if (ok) dirty = false;
    return ok;
}

void MonstersSection::loadSprite(uint8_t picture) {
    releaseTextures();
    sprite_.clear();
    spriteFrame_ = 0;
    spritePic_ = picture;
    spriteSequence_ = 0;
    spriteSequenceStep_ = 0;
    spriteElapsed_ = 0.0f;
    spriteLastTick_ = ImGui::GetTime();
    spriteCanvasW_ = spriteCanvasH_ = 0;
    spriteCanvasMinX_ = spriteCanvasMinY_ = 0;

    int idx = picture & 0x7F;
    char name[16];
    snprintf(name, sizeof(name), "%02d.anm", idx);
    spriteFile_ = name;

    if (idx <= 0) {
        sprite_.error = "no sprite (picture & 0x7F == 0)";
        return;
    }
    gfxLoad(dir_ + "/" + spriteFile_, /*isAnm=*/true, sprite_);
    if (sprite_.ok || !sprite_.frames.empty()) {
        textures_.reserve(sprite_.frames.size());
        for (auto& fr : sprite_.frames)
            textures_.push_back(makeTextureRGBA(fr.rgba.data(), fr.width, fr.height));
        buildSpriteComposedTextures();
        if (spritePlayMode_ == AnmPlayMode::Sequence && gfxAnmHasSequencePlayback(sprite_)) {
            int frame = gfxAnmSequenceFrameAt(sprite_, spriteSequence_, spriteSequenceStep_);
            if (frame >= 0) spriteFrame_ = frame;
        }
    }
}

void MonstersSection::draw(App& app) {
    if (!loaded) {
        ui::EmptyState("monsters.dat not loaded.");
        return;
    }

    if (!ui::BeginMasterList(layout_, "mon_list", "256 monsters")) {
        return;
    }
    for (int i = 0; i < kMonstersCount; ++i) {
        std::string nm = file_.records[i].nameStr();
        if (!ui::ListFilterPass(layout_, nm.c_str()) &&
            !ui::ListFilterPass(layout_, std::to_string(i).c_str()))
            continue;
        if (ui::ListRow(i, nm.c_str(), selected_ == i)) selected_ = i;
    }
    ui::EndMasterListBeginDetail(layout_, "mon_detail");

    MonsterRecord& r = file_.records[selected_];

    char nameBuf[kMonsterNameSize + 1];
    std::string nm = r.nameStr();
    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf));
    nameBuf[kMonsterNameSize] = '\0';

    ui::PanelHeader(nm.empty() ? "(blank monster)" : nm.c_str(),
                    (std::string("#") + std::to_string(selected_)).c_str());

    {
        ui::FormTable form("mon_name");
        if (form.begin()) {
            form.row("Name", [&] {
                ui::SetFieldStretch();
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                    r.setName(nameBuf);
                    dirty = true;
                }
            });
        }
    }

    uint8_t pic = r.raw[0x15];
    if (spritePic_ != pic) loadSprite(pic);

    if (ImGui::BeginTable("mon_body", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("sprite", ImGuiTableColumnFlags_WidthFixed, ui::Em(22.f));
        ImGui::TableSetupColumn("props", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ui::SectionBlock("Sprite");
        ImGui::TextDisabled("picture=%u  (file %u, flag 0x80=%s)", pic, pic & 0x7F,
                            (pic & 0x80) ? "set" : "clear");

        ui::SetFieldMed();
        const char* pcModeLabel = pcMode_ == PcSpriteMode::Amiga ? "Amiga (.anm)"
                                  : pcMode_ == PcSpriteMode::Cga  ? "PC CGA (.4)"
                                                                   : "PC EGA (.16)";
        if (ImGui::BeginCombo("Sprite source", pcModeLabel)) {
            if (ImGui::Selectable("Amiga (.anm)", pcMode_ == PcSpriteMode::Amiga)) pcMode_ = PcSpriteMode::Amiga;
            if (ImGui::Selectable("PC CGA (.4)", pcMode_ == PcSpriteMode::Cga)) pcMode_ = PcSpriteMode::Cga;
            if (ImGui::Selectable("PC EGA (.16)", pcMode_ == PcSpriteMode::Ega)) pcMode_ = PcSpriteMode::Ega;
            ImGui::EndCombo();
        }
        syncPcSprite(app, pic);

        if (pcMode_ != PcSpriteMode::Amiga) {
            if (!pcDir_.empty() && pcMode_ == PcSpriteMode::Cga) {
                ui::SetFieldShort();
                const char* palLabel = pcCgaPalette_ == 0 ? "green/red/brown" : "cyan/magenta/white";
                if (ImGui::BeginCombo("CGA palette", palLabel)) {
                    if (ImGui::Selectable("green/red/brown", pcCgaPalette_ == 0)) {
                        pcCgaPalette_ = 0;
                        buildPcComposedTextures();
                    }
                    if (ImGui::Selectable("cyan/magenta/white (MM2 default)", pcCgaPalette_ == 1)) {
                        pcCgaPalette_ = 1;
                        buildPcComposedTextures();
                    }
                    ImGui::EndCombo();
                }
            }
            if (pcDir_.empty()) {
                ImGui::TextDisabled("No PC assets folder set (open one from the PC Walls section).");
            } else if (!pcPic_ || !pcPic_->ok) {
                ImGui::TextColored(ui::Danger(), "No PC sprite for picture id %d in %s.", pic & 0x7F,
                                   pcMode_ == PcSpriteMode::Cga ? "MONSTERS.4" : "MONSTERS.16");
            } else {
                const int n = static_cast<int>(pcPic_->frames.size());
                ImGui::TextDisabled("%d raw frames  flags=0x%X", n, pcPic_->flags);
                ImGui::Checkbox("Play##pcmon", &pcPlaying_);
                ImGui::SameLine();
                ImGui::Checkbox("Loop##pcmon", &pcLoop_);
                ui::SetFieldShort();
                ImGui::SliderFloat("Speed##pcmon", &pcSpeed_, 0.1f, 4.0f, "%.2fx");
                ImGui::SliderInt("State##pcmon", &pcState_, 0, n > 0 ? n - 1 : 0);
                ImGui::SliderFloat("Zoom##pcmon", &spriteZoom_, 1.0f, 6.0f, "%.0fx");

                if (pcPlaying_ && n > 1) {
                    const double now = ImGui::GetTime();
                    const float dt = static_cast<float>((pcLastTick_ > 0.0) ? (now - pcLastTick_) : 0.0);
                    pcLastTick_ = now;
                    pcElapsed_ += (dt > 0.0f) ? dt : 0.0f;
                    const float frameDur = 0.125f / ((pcSpeed_ > 0.0f) ? pcSpeed_ : 1.0f);
                    while (pcElapsed_ >= frameDur) {
                        pcElapsed_ -= frameDur;
                        ++pcState_;
                        if (pcState_ >= n) {
                            if (pcLoop_)
                                pcState_ = 0;
                            else {
                                pcState_ = n - 1;
                                pcPlaying_ = false;
                                break;
                            }
                        }
                    }
                } else {
                    pcLastTick_ = ImGui::GetTime();
                }
                if (pcState_ < 0) pcState_ = 0;
                if (pcState_ >= n) pcState_ = (n > 0) ? (n - 1) : 0;

                if (pcState_ >= 0 && pcState_ < static_cast<int>(pcComposedTextures_.size())) {
                    unsigned int tex = pcComposedTextures_[static_cast<size_t>(pcState_)];
                    if (tex)
                        ImGui::Image(static_cast<ImTextureID>(tex), ImVec2(kPcCombatCanvasW * spriteZoom_,
                                                                           kPcCombatCanvasH * spriteZoom_));
                    ImGui::TextDisabled("state %d/%d  (%dx%d canvas, %dbpp)", pcState_, n - 1, kPcCombatCanvasW,
                                        kPcCombatCanvasH, pcPic_->bpp);
                }
            }
        }

        ImGui::Text("%s", spriteFile_.c_str());
        if (pcMode_ != PcSpriteMode::Amiga) ImGui::TextDisabled("(Amiga sprite decoded below for comparison)");
        if (sprite_.ok || !sprite_.frames.empty()) {
            int n = static_cast<int>(sprite_.frames.size());
            ImGui::TextDisabled("%d frames  depth=%d", n, sprite_.depth);
            ImGui::Checkbox("Play##sprite", &spritePlaying_);
            ImGui::SameLine();
            ImGui::Checkbox("Loop##sprite", &spriteLoop_);
            ui::SetFieldShort();
            ImGui::SliderFloat("Speed##sprite", &spriteSpeed_, 0.1f, 4.0f, "%.2fx");
            ui::SetFieldMed();
            const char* spriteModeLabel =
                (spritePlayMode_ == AnmPlayMode::Flipbook) ? "Flipbook" : "Sequence";
            if (ImGui::BeginCombo("Play mode##sprite", spriteModeLabel)) {
                if (ImGui::Selectable("Flipbook (all composed frames)", spritePlayMode_ == AnmPlayMode::Flipbook)) {
                    spritePlayMode_ = AnmPlayMode::Flipbook;
                    spriteSequenceStep_ = 0;
                    spriteElapsed_ = 0.0f;
                }
                if (ImGui::Selectable("Sequence (game stream)", spritePlayMode_ == AnmPlayMode::Sequence)) {
                    spritePlayMode_ = AnmPlayMode::Sequence;
                    spriteSequenceStep_ = 0;
                    spriteElapsed_ = 0.0f;
                    int frame = gfxAnmSequenceFrameAt(sprite_, spriteSequence_, spriteSequenceStep_);
                    if (frame >= 0) spriteFrame_ = frame;
                }
                ImGui::EndCombo();
            }
            ImGui::SliderInt("Frame##sprite", &spriteFrame_, 0, n > 0 ? n - 1 : 0);
            ImGui::SliderFloat("Zoom##sprite", &spriteZoom_, 1.0f, 6.0f, "%.0fx");

            if (spritePlaying_ && n > 1) {
                const double now = ImGui::GetTime();
                const float dt = static_cast<float>((spriteLastTick_ > 0.0) ? (now - spriteLastTick_) : 0.0);
                spriteLastTick_ = now;
                spriteElapsed_ += (dt > 0.0f) ? dt : 0.0f;

                const bool useSequence =
                    spritePlayMode_ == AnmPlayMode::Sequence && gfxAnmHasSequencePlayback(sprite_);
                if (useSequence) {
                    if (spriteSequence_ < 0 || spriteSequence_ >= static_cast<int>(sprite_.sequences.size()))
                        spriteSequence_ = 0;
                    const auto& seq = sprite_.sequences[spriteSequence_];
                    const int pairCount = static_cast<int>(seq.size() / 2);
                    if (pairCount > 0) {
                        while (spriteElapsed_ >= gfxAnmSequenceStepDurationSec(sprite_, spriteSequence_,
                                                                                 spriteSequenceStep_, spriteSpeed_)) {
                            spriteElapsed_ -= gfxAnmSequenceStepDurationSec(sprite_, spriteSequence_,
                                                                           spriteSequenceStep_, spriteSpeed_);
                            ++spriteSequenceStep_;
                            if (spriteSequenceStep_ >= pairCount) {
                                if (spriteLoop_)
                                    spriteSequenceStep_ = 0;
                                else {
                                    spriteSequenceStep_ = pairCount - 1;
                                    spritePlaying_ = false;
                                    break;
                                }
                            }
                            int frame = gfxAnmSequenceFrameAt(sprite_, spriteSequence_, spriteSequenceStep_);
                            if (frame >= 0 && frame < n) spriteFrame_ = frame;
                        }
                    }
                } else {
                    const float frameDur = 0.125f / ((spriteSpeed_ > 0.0f) ? spriteSpeed_ : 1.0f);
                    while (spriteElapsed_ >= frameDur) {
                        spriteElapsed_ -= frameDur;
                        ++spriteFrame_;
                        if (spriteFrame_ >= n) {
                            if (spriteLoop_)
                                spriteFrame_ = 0;
                            else {
                                spriteFrame_ = n - 1;
                                spritePlaying_ = false;
                                break;
                            }
                        }
                    }
                }
            } else {
                spriteLastTick_ = ImGui::GetTime();
            }

            if (spriteFrame_ < 0) spriteFrame_ = 0;
            if (spriteFrame_ >= n) spriteFrame_ = (n > 0) ? (n - 1) : 0;

            if (!sprite_.sequences.empty() && spritePlayMode_ == AnmPlayMode::Sequence) {
                if (spriteSequence_ < 0 || spriteSequence_ >= static_cast<int>(sprite_.sequences.size()))
                    spriteSequence_ = 0;
                std::string seqLabel = "Sequence " + std::to_string(spriteSequence_);
                if (ImGui::BeginCombo("Sequence##sprite", seqLabel.c_str())) {
                    for (int i = 0; i < static_cast<int>(sprite_.sequences.size()); ++i) {
                        std::string label =
                            "Sequence " + std::to_string(i) + " (" +
                            std::to_string(static_cast<int>(sprite_.sequences[i].size() / 2)) + " steps)";
                        bool sel = (i == spriteSequence_);
                        if (ImGui::Selectable(label.c_str(), sel)) {
                            spriteSequence_ = i;
                            spriteSequenceStep_ = 0;
                            spriteElapsed_ = 0.0f;
                            int frame = gfxAnmSequenceFrameAt(sprite_, spriteSequence_, spriteSequenceStep_);
                            if (frame >= 0 && frame < n) spriteFrame_ = frame;
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                const int frame = gfxAnmSequenceFrameAt(sprite_, spriteSequence_, spriteSequenceStep_);
                const int delay = (spriteSequence_ >= 0 &&
                                   spriteSequence_ < static_cast<int>(sprite_.sequences.size()) &&
                                   static_cast<size_t>(spriteSequenceStep_) * 2 + 1 <
                                       sprite_.sequences[spriteSequence_].size())
                                      ? static_cast<int>(
                                            sprite_.sequences[spriteSequence_][static_cast<size_t>(spriteSequenceStep_) * 2 + 1])
                                      : 0;
                ImGui::TextDisabled("Seq step=%d frame=%d delay=%d", spriteSequenceStep_, frame, delay);
            }

            if (spriteFrame_ >= 0 && spriteFrame_ < n) {
                const GfxFrame& fr = sprite_.frames[spriteFrame_];
                unsigned int tex = textures_[spriteFrame_];
                int drawW = fr.width;
                int drawH = fr.height;
                if (spriteAnmCanvas_.valid && spriteFrame_ >= 0 &&
                    spriteFrame_ < static_cast<int>(composedTextures_.size()) &&
                    composedTextures_[spriteFrame_]) {
                    tex = composedTextures_[spriteFrame_];
                    drawW = spriteCanvasW_;
                    drawH = spriteCanvasH_;
                }
                if (tex) {
                    ImVec2 sz(drawW * spriteZoom_, drawH * spriteZoom_);
                    ImGui::Image(static_cast<ImTextureID>(tex), sz);
                }
                ImGui::TextDisabled("composed %dx%d  raw patch %dx%d  flags=0x%X", drawW, drawH, fr.width, fr.height,
                                    fr.flags);
                const int preIdx = spriteFrame_ - 1;
                if (preIdx >= 0 && preIdx < static_cast<int>(sprite_.preludeEntries.size())) {
                    const GfxAnimPreludeEntry& pe = sprite_.preludeEntries[preIdx];
                    if (pe.used) {
                        ImGui::TextDisabled("Prelude[%d] for frame %d: x_off=%d y_off=%d w=%d h=%d", preIdx,
                                            spriteFrame_, pe.xOffset, pe.yOffset, pe.width, pe.height);
                    }
                }
            }
        } else {
            ImGui::TextColored(ui::Danger(), "No sprite: %s",
                               sprite_.error.empty() ? "decode failed" : sprite_.error.c_str());
        }

        ImGui::TableNextColumn();
        auto effectCombo = [&](const char* label, uint8_t cur, const char* const* names,
                               int count, void (MonsterRecord::*setter)(uint8_t)) {
            ui::SetFieldWide();
            const char* curName = cur < count ? names[cur] : "?";
            if (ImGui::BeginCombo(label, curName)) {
                for (int e = 0; e < count; ++e) {
                    bool sel = (e == cur);
                    char item[48];
                    snprintf(item, sizeof(item), "%2d  %s", e, names[e]);
                    if (ImGui::Selectable(item, sel)) {
                        (r.*setter)(static_cast<uint8_t>(e));
                        dirty = true;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        };

        ui::SectionBlock("Abilities", "ASM-confirmed");
        ImGui::TextDisabled("Single-target attack (Sabil 0x%02X)", r.sabil());
        effectCombo("Effect##single", r.singleEffect(), kAbilityNames, kAbilityCount,
                    &MonsterRecord::setSingleEffect);
        bool undead = r.isUndead();
        if (ImGui::Checkbox("Undead", &undead)) {
            r.setUndead(undead);
            dirty = true;
        }
        ImGui::SameLine();
        bool archer = r.isArcher();
        if (ImGui::Checkbox("Archer", &archer)) {
            r.setArcher(archer);
            dirty = true;
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Group attack (Pabil 0x%02X)", r.pabil());
        effectCombo("Verb##party", r.partyVerb(), kPartyVerbNames, kPartyVerbCount,
                    &MonsterRecord::setPartyVerb);
        {
            ui::SetFieldShort();
            int chance = r.partyChance();
            if (ImGui::SliderInt("Use-chance tier##party", &chance, 0, 7)) {
                r.setPartyChance(static_cast<uint8_t>(chance));
                dirty = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Pabil bits 5-7. Indexes a frequency table at\n"
                                  "asm 0x10002 - higher = used more often.");
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Other behavior (Oabil 0x%02X)", r.oabil());
        ImGui::Text("Adds friends: %u reinforcements", r.addFriends());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Oabil low nibble +1 (x10 if bit4). Count of monsters\n"
                              "summoned by the \"adds friends!\" action (asm 0x11F0A).");
        ImGui::Text("Flee tier: %u", r.fleeTier());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Oabil bits 5-6. Chance the monster \"runs\" / flees\n"
                              "(asm 0x10DFC).");
        bool mult = r.multiplies();
        if (ImGui::Checkbox("Multiplies / breeds", &mult)) {
            r.setMultiplies(mult);
            dirty = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Oabil bit7. Monster duplicates itself in combat\n"
                              "(asm 0x100B0).");

        ui::SectionBlock("Named fields", "semantics partial");
        ImGui::Text("Derived HP: %u", r.hpValue());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("ASM 0x4C8E: ((hp_code & 0x3F)+1) * {1,10,100,1000}[(hp_code>>6)&3]");
        ImGui::Text("Derived XP: %u", r.xpValue());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("ASM 0x4C8E: ((xp_code & 0x1F)+1) * {1,10,100,1000}[(xp_code>>5)&3], x1000 if bit7");
        ImGui::Text("Treasure flags: item_drop=%u  gems=%s  gold_tier=%u",
                    r.itemDropLevel(), r.dropsGems() ? "yes" : "no", r.goldTier());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("ASM 0x10B74 reward decode of byte 0x10:\n"
                              "bits0-1=item drop level, bit2=gems flag, bits3-4=gold tier");
        struct Field {
            const char* label;
            int off;
        };
        const Field fields[] = {
            {"HP code", 0x0E},   {"XP code", 0x0F},  {"Treasure", 0x10},
            {"Pabil", 0x11},     {"Sabil", 0x12},    {"Oabil", 0x13},
            {"Speed", 0x14},     {"Picture", 0x15},  {"Armor class", 0x16},
            {"Damage", 0x17},    {"Speed2", 0x18},   {"Magic resist", 0x19},
        };
        {
            ui::FormGrid grid("mon_fields");
            if (grid.begin()) {
                for (int i = 0; i < static_cast<int>(sizeof(fields) / sizeof(fields[0])); i += 2) {
                    const Field& f0 = fields[i];
                    const Field& f1 = fields[i + 1];
                    grid.row2(
                        f0.label,
                        [&] {
                            ui::SetFieldStretch();
                            int v = r.raw[f0.off];
                            if (ImGui::InputInt(("##" + std::to_string(f0.off)).c_str(), &v)) {
                                r.raw[f0.off] = static_cast<uint8_t>(v & 0xFF);
                                dirty = true;
                            }
                        },
                        f1.label,
                        [&] {
                            ui::SetFieldStretch();
                            int v = r.raw[f1.off];
                            if (ImGui::InputInt(("##" + std::to_string(f1.off)).c_str(), &v)) {
                                r.raw[f1.off] = static_cast<uint8_t>(v & 0xFF);
                                dirty = true;
                            }
                        });
                }
            }
        }

        ImGui::EndTable();
    }

    if (ui::BeginHexBlock("Raw record")) {
        ImGui::TextDisabled("Record %d @ 0x%04X (%d bytes)", selected_,
                            selected_ * kMonsterRecordSize, kMonsterRecordSize);
        DrawHexView("mon_hex", r.raw.data(), kMonsterRecordSize, selected_ * kMonsterRecordSize);
        ui::EndHexBlock();
    }

    ui::EndMasterDetail();
}

}  // namespace mm2

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

bool byteField(const char* id, uint8_t* byte, bool* dirty) {
    int v = *byte;
    ui::SetFieldStretch();
    if (ImGui::InputInt(id, &v, 1, 5)) {
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        *byte = static_cast<uint8_t>(v);
        *dirty = true;
        return true;
    }
    return false;
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
        pcComposedTextures_.push_back(
            makeTextureRGBA(rgba.data(), kPcCombatCanvasW, kPcCombatCanvasH));
    }
}

void MonstersSection::syncPcSprite(App& app, uint8_t picture) {
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
    if (pcPicId_ == pictureId && pcPicVariant_ == pcMode_) return;
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
        composedTextures_.push_back(
            makeTextureRGBA(rgba.data(), spriteAnmCanvas_.width, spriteAnmCanvas_.height));
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
        sprite_.error = "no sprite (picture id 0)";
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

void MonstersSection::drawWorkspace(App& app, EditorSelection& sel) {
    if (!loaded) {
        ui::EmptyState("monsters.dat not loaded", "Open a folder containing monsters.dat");
        return;
    }
    if (sel.doc == DocKind::Monsters && sel.kind == EditorSelection::Kind::Monster &&
        sel.index >= 0 && sel.index < kMonstersCount)
        selected_ = sel.index;
    if (selected_ < 0 || selected_ >= kMonstersCount) selected_ = 0;

    if (!ui::BeginMasterList(layout_, "mon_list", "Monsters")) return;
    for (int i = 0; i < kMonstersCount; ++i) {
        std::string nm = file_.records[i].nameStr();
        char hay[48];
        snprintf(hay, sizeof(hay), "%d %s", i, nm.c_str());
        if (!ui::ListFilterPass(layout_, hay)) continue;
        if (ui::ListRow(i, nm.c_str(), selected_ == i)) {
            selected_ = i;
            sel.Select(DocKind::Monsters, EditorSelection::Kind::Monster, i);
        }
    }
    ui::EndMasterListBeginDetail(layout_, "mon_detail");
    drawMonsterDetail(app);
    ui::EndMasterDetail();
}

void MonstersSection::drawProperties(App& app, EditorSelection& sel) {
    (void)app;
    if (!loaded) {
        ui::EmptyState("Not loaded", "monsters.dat missing from the data folder");
        return;
    }
    if (sel.doc == DocKind::Monsters && sel.kind == EditorSelection::Kind::Monster &&
        sel.index >= 0 && sel.index < kMonstersCount)
        selected_ = sel.index;
    if (selected_ < 0 || selected_ >= kMonstersCount) selected_ = 0;
    if (sel.kind == EditorSelection::Kind::None || sel.doc != DocKind::Monsters)
        sel.Select(DocKind::Monsters, EditorSelection::Kind::Monster, selected_);

    const MonsterRecord& r = file_.records[selected_];
    std::string nmStr = r.nameStr();
    const char* nm = nmStr.empty() ? "(blank)" : nmStr.c_str();
    char sub[48];
    snprintf(sub, sizeof(sub), "#%d · pic %d", selected_, r.picture() & 0x7F);
    ui::PanelHeader(nm, sub);

    ui::SectionBlock("Combat");
    {
        ui::FormTable form("mon_prop_combat", ui::Em(7.f));
        if (form.begin()) {
            form.row("HP", [&] { ImGui::Text("%u", r.hpValue()); });
            form.row("XP", [&] { ImGui::Text("%u", r.xpValue()); });
            form.row("AC", [&] { ImGui::Text("%u", r.ac()); });
            form.row("Damage", [&] { ImGui::Text("%u", r.damage()); });
            form.row("Speed", [&] { ImGui::Text("%u", r.speed()); });
            form.row("Magic resist", [&] { ImGui::Text("%u", r.magicResist()); });
        }
    }

    ui::SectionBlock("Traits");
    {
        ui::FormTable form("mon_prop_traits", ui::Em(7.f));
        if (form.begin()) {
            form.row("Attack", [&] {
                ImGui::TextUnformatted(abilityName(r.singleEffect()));
            });
            form.row("Group", [&] {
                ImGui::TextUnformatted(partyVerbName(r.partyVerb()));
            });
            form.row("Flags", [&] {
                std::string flags;
                if (r.isUndead()) flags += "undead";
                if (r.isArcher()) {
                    if (!flags.empty()) flags += ", ";
                    flags += "archer";
                }
                if (r.multiplies()) {
                    if (!flags.empty()) flags += ", ";
                    flags += "breeds";
                }
                if (flags.empty()) flags = "—";
                ImGui::TextUnformatted(flags.c_str());
            });
            form.row("Loot", [&] {
                ImGui::Text("drop %u · gems %s · gold %u", r.itemDropLevel(),
                            r.dropsGems() ? "yes" : "no", r.goldTier());
            });
        }
    }
}

void MonstersSection::drawMonsterDetail(App& app) {
    MonsterRecord& r = file_.records[selected_];

    char nameBuf[kMonsterNameSize + 1];
    std::string nm = r.nameStr();
    std::strncpy(nameBuf, nm.c_str(), sizeof(nameBuf));
    nameBuf[kMonsterNameSize] = '\0';

    char sub[96];
    snprintf(sub, sizeof(sub), "#%d · HP %u · XP %u · AC %u", selected_, r.hpValue(), r.xpValue(),
             r.ac());
    ui::PanelHeader(nm.empty() ? "(blank monster)" : nm.c_str(), sub);

    uint8_t pic = r.picture();
    if (spritePic_ != pic) loadSprite(pic);
    syncPcSprite(app, pic);

    // —— Top: sprite preview | identity + combat ——
    if (ImGui::BeginTable("mon_top", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("sprite", ImGuiTableColumnFlags_WidthFixed, ui::Em(20.f));
        ImGui::TableSetupColumn("stats", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        // LEFT: sprite
        ImGui::TableNextColumn();
        ui::SectionBlock("Sprite");
        {
            ui::FormTable form("mon_sprite_src", ui::Em(5.f));
            if (form.begin()) {
                form.row("Source", [&] {
                    ui::SetFieldStretch();
                    const char* pcModeLabel = pcMode_ == PcSpriteMode::Amiga ? "Amiga (.anm)"
                                              : pcMode_ == PcSpriteMode::Cga  ? "PC CGA (.4)"
                                                                               : "PC EGA (.16)";
                    if (ImGui::BeginCombo("##src", pcModeLabel)) {
                        if (ImGui::Selectable("Amiga (.anm)", pcMode_ == PcSpriteMode::Amiga))
                            pcMode_ = PcSpriteMode::Amiga;
                        if (ImGui::Selectable("PC CGA (.4)", pcMode_ == PcSpriteMode::Cga))
                            pcMode_ = PcSpriteMode::Cga;
                        if (ImGui::Selectable("PC EGA (.16)", pcMode_ == PcSpriteMode::Ega))
                            pcMode_ = PcSpriteMode::Ega;
                        ImGui::EndCombo();
                    }
                });
                form.row("Picture", [&] {
                    ui::SetFieldShort();
                    int p = pic;
                    if (ImGui::InputInt("##pic", &p, 1, 5)) {
                        if (p < 0) p = 0;
                        if (p > 255) p = 255;
                        r.raw[0x15] = static_cast<uint8_t>(p);
                        dirty = true;
                        pic = r.picture();
                        loadSprite(pic);
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", spriteFile_.c_str());
                });
            }
        }

        // Preview image
        bool drewPreview = false;
        if (pcMode_ != PcSpriteMode::Amiga) {
            if (pcDir_.empty()) {
                ImGui::TextDisabled("Set a PC assets folder from PC Walls.");
            } else if (!pcPic_ || !pcPic_->ok) {
                ImGui::TextColored(ui::Danger(), "No PC sprite for id %d", pic & 0x7F);
            } else {
                const int n = static_cast<int>(pcPic_->frames.size());
                if (pcPlaying_ && n > 1) {
                    const double now = ImGui::GetTime();
                    const float dt =
                        static_cast<float>((pcLastTick_ > 0.0) ? (now - pcLastTick_) : 0.0);
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
                    if (tex) {
                        ImGui::Image(static_cast<ImTextureID>(tex),
                                     ImVec2(kPcCombatCanvasW * spriteZoom_,
                                            kPcCombatCanvasH * spriteZoom_));
                        drewPreview = true;
                    }
                }
                ImGui::TextDisabled("%d frames", n);
            }
        } else if (sprite_.ok || !sprite_.frames.empty()) {
            const int n = static_cast<int>(sprite_.frames.size());
            if (spritePlaying_ && n > 1) {
                const double now = ImGui::GetTime();
                const float dt =
                    static_cast<float>((spriteLastTick_ > 0.0) ? (now - spriteLastTick_) : 0.0);
                spriteLastTick_ = now;
                spriteElapsed_ += (dt > 0.0f) ? dt : 0.0f;
                const bool useSequence =
                    spritePlayMode_ == AnmPlayMode::Sequence && gfxAnmHasSequencePlayback(sprite_);
                if (useSequence) {
                    if (spriteSequence_ < 0 ||
                        spriteSequence_ >= static_cast<int>(sprite_.sequences.size()))
                        spriteSequence_ = 0;
                    const auto& seq = sprite_.sequences[static_cast<size_t>(spriteSequence_)];
                    const int pairCount = static_cast<int>(seq.size() / 2);
                    if (pairCount > 0) {
                        while (spriteElapsed_ >=
                               gfxAnmSequenceStepDurationSec(sprite_, spriteSequence_,
                                                             spriteSequenceStep_, spriteSpeed_)) {
                            spriteElapsed_ -= gfxAnmSequenceStepDurationSec(
                                sprite_, spriteSequence_, spriteSequenceStep_, spriteSpeed_);
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
                            int frame =
                                gfxAnmSequenceFrameAt(sprite_, spriteSequence_, spriteSequenceStep_);
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

            if (spriteFrame_ >= 0 && spriteFrame_ < n) {
                const GfxFrame& fr = sprite_.frames[static_cast<size_t>(spriteFrame_)];
                unsigned int tex = textures_[static_cast<size_t>(spriteFrame_)];
                int drawW = fr.width;
                int drawH = fr.height;
                if (spriteAnmCanvas_.valid && spriteFrame_ < static_cast<int>(composedTextures_.size()) &&
                    composedTextures_[static_cast<size_t>(spriteFrame_)]) {
                    tex = composedTextures_[static_cast<size_t>(spriteFrame_)];
                    drawW = spriteCanvasW_;
                    drawH = spriteCanvasH_;
                }
                if (tex) {
                    ImGui::Image(static_cast<ImTextureID>(tex),
                                 ImVec2(drawW * spriteZoom_, drawH * spriteZoom_));
                    drewPreview = true;
                }
                ImGui::TextDisabled("%d frames · frame %d", n, spriteFrame_);
            }
        } else {
            ImGui::TextColored(ui::Danger(), "%s",
                               sprite_.error.empty() ? "No sprite" : sprite_.error.c_str());
        }
        (void)drewPreview;

        ImGui::Checkbox("Play", pcMode_ == PcSpriteMode::Amiga ? &spritePlaying_ : &pcPlaying_);
        ImGui::SameLine();
        ImGui::Checkbox("Loop", pcMode_ == PcSpriteMode::Amiga ? &spriteLoop_ : &pcLoop_);
        ui::SetFieldShort();
        ImGui::SliderFloat("Zoom", &spriteZoom_, 1.0f, 6.0f, "%.0fx");

        // RIGHT: identity + combat
        ImGui::TableNextColumn();
        ui::SectionBlock("Identity");
        {
            ui::FormTable form("mon_name", ui::Em(6.f));
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

        ui::SectionBlock("Combat");
        {
            ui::FormGrid grid("mon_combat", ui::Em(6.f));
            if (grid.begin()) {
                grid.row2(
                    "HP",
                    [&] {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("%u", r.hpValue());
                        ImGui::SameLine();
                        ImGui::TextDisabled("code");
                        ImGui::SameLine();
                        byteField("##hp", &r.raw[0x0E], &dirty);
                    },
                    "XP",
                    [&] {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("%u", r.xpValue());
                        ImGui::SameLine();
                        ImGui::TextDisabled("code");
                        ImGui::SameLine();
                        byteField("##xp", &r.raw[0x0F], &dirty);
                    });
                grid.row2(
                    "AC", [&] { byteField("##ac", &r.raw[0x16], &dirty); }, "Damage",
                    [&] { byteField("##dmg", &r.raw[0x17], &dirty); });
                grid.row2(
                    "Speed", [&] { byteField("##spd", &r.raw[0x14], &dirty); }, "Speed 2",
                    [&] { byteField("##spd2", &r.raw[0x18], &dirty); });
                grid.row1("Magic resist", [&] { byteField("##mr", &r.raw[0x19], &dirty); });
            }
        }

        ImGui::EndTable();
    }

    // —— Attacks ——
    ui::SectionBlock("Attacks");
    {
        auto effectCombo = [&](const char* id, uint8_t cur, const char* const* names, int count,
                               void (MonsterRecord::*setter)(uint8_t)) {
            ui::SetFieldStretch();
            const char* curName = cur < count ? names[cur] : "?";
            if (ImGui::BeginCombo(id, curName)) {
                for (int e = 0; e < count; ++e) {
                    bool sel = (e == cur);
                    char item[64];
                    snprintf(item, sizeof(item), "%s", names[e]);
                    if (ImGui::Selectable(item, sel)) {
                        (r.*setter)(static_cast<uint8_t>(e));
                        dirty = true;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        };

        ui::FormTable form("mon_atk", ui::Em(8.f));
        if (form.begin()) {
            form.row("Single target", [&] {
                effectCombo("##single", r.singleEffect(), kAbilityNames, kAbilityCount,
                            &MonsterRecord::setSingleEffect);
            });
            form.row("Flags", [&] {
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
            });
            form.row("Group attack", [&] {
                effectCombo("##party", r.partyVerb(), kPartyVerbNames, kPartyVerbCount,
                            &MonsterRecord::setPartyVerb);
            });
            form.row("Group chance", [&] {
                ui::SetFieldMed();
                int chance = r.partyChance();
                if (ImGui::SliderInt("##chance", &chance, 0, 7)) {
                    r.setPartyChance(static_cast<uint8_t>(chance));
                    dirty = true;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("How often the group attack is used (0–7).");
            });
        }
    }

    // —— Behavior ——
    ui::SectionBlock("Behavior");
    {
        ui::FormTable form("mon_beh", ui::Em(8.f));
        if (form.begin()) {
            form.row("Reinforcements", [&] {
                ImGui::Text("%u", r.addFriends());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Monsters summoned by \"adds friends\".");
            });
            form.row("Flee tier", [&] {
                ImGui::Text("%u", r.fleeTier());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance the monster flees (0–3).");
            });
            form.row("Breeds", [&] {
                bool mult = r.multiplies();
                if (ImGui::Checkbox("##mult", &mult)) {
                    r.setMultiplies(mult);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("duplicates itself in combat");
            });
        }
    }

    // —— Loot ——
    ui::SectionBlock("Loot");
    {
        ui::FormGrid grid("mon_loot", ui::Em(7.f));
        if (grid.begin()) {
            grid.row2(
                "Item drop",
                [&] {
                    ui::SetFieldShort();
                    int drop = r.itemDropLevel();
                    if (ImGui::InputInt("##drop", &drop, 1, 1)) {
                        if (drop < 0) drop = 0;
                        if (drop > 3) drop = 3;
                        r.raw[0x10] = static_cast<uint8_t>((r.raw[0x10] & ~0x03) | (drop & 0x03));
                        dirty = true;
                    }
                },
                "Gold tier",
                [&] {
                    ui::SetFieldShort();
                    int gold = r.goldTier();
                    if (ImGui::InputInt("##goldt", &gold, 1, 1)) {
                        if (gold < 0) gold = 0;
                        if (gold > 3) gold = 3;
                        r.raw[0x10] =
                            static_cast<uint8_t>((r.raw[0x10] & ~0x18) | ((gold & 0x03) << 3));
                        dirty = true;
                    }
                });
            grid.row1("Gems", [&] {
                bool gems = r.dropsGems();
                if (ImGui::Checkbox("##gems", &gems)) {
                    r.raw[0x10] = gems ? (r.raw[0x10] | 0x04) : (r.raw[0x10] & ~0x04);
                    dirty = true;
                }
            });
        }
    }

    // —— Animation advanced (collapsed) ——
    if (ImGui::CollapsingHeader("Animation controls")) {
        if (pcMode_ != PcSpriteMode::Amiga && pcMode_ == PcSpriteMode::Cga && !pcDir_.empty()) {
            ui::SetFieldMed();
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
        if (pcMode_ == PcSpriteMode::Amiga) {
            ui::SetFieldShort();
            ImGui::SliderFloat("Speed##sprite", &spriteSpeed_, 0.1f, 4.0f, "%.2fx");
            const char* modeLabel =
                (spritePlayMode_ == AnmPlayMode::Flipbook) ? "Flipbook" : "Sequence";
            ui::SetFieldMed();
            if (ImGui::BeginCombo("Play mode##sprite", modeLabel)) {
                if (ImGui::Selectable("Flipbook", spritePlayMode_ == AnmPlayMode::Flipbook)) {
                    spritePlayMode_ = AnmPlayMode::Flipbook;
                    spriteSequenceStep_ = 0;
                    spriteElapsed_ = 0.0f;
                }
                if (ImGui::Selectable("Sequence", spritePlayMode_ == AnmPlayMode::Sequence)) {
                    spritePlayMode_ = AnmPlayMode::Sequence;
                    spriteSequenceStep_ = 0;
                    spriteElapsed_ = 0.0f;
                    int frame = gfxAnmSequenceFrameAt(sprite_, spriteSequence_, spriteSequenceStep_);
                    if (frame >= 0) spriteFrame_ = frame;
                }
                ImGui::EndCombo();
            }
            const int n = static_cast<int>(sprite_.frames.size());
            ImGui::SliderInt("Frame##sprite", &spriteFrame_, 0, n > 0 ? n - 1 : 0);
            if (!sprite_.sequences.empty() && spritePlayMode_ == AnmPlayMode::Sequence) {
                if (spriteSequence_ < 0 ||
                    spriteSequence_ >= static_cast<int>(sprite_.sequences.size()))
                    spriteSequence_ = 0;
                std::string seqLabel = "Sequence " + std::to_string(spriteSequence_);
                if (ImGui::BeginCombo("Sequence##sprite", seqLabel.c_str())) {
                    for (int i = 0; i < static_cast<int>(sprite_.sequences.size()); ++i) {
                        std::string label =
                            "Sequence " + std::to_string(i) + " (" +
                            std::to_string(static_cast<int>(sprite_.sequences[static_cast<size_t>(i)].size() /
                                                           2)) +
                            " steps)";
                        bool sel = (i == spriteSequence_);
                        if (ImGui::Selectable(label.c_str(), sel)) {
                            spriteSequence_ = i;
                            spriteSequenceStep_ = 0;
                            spriteElapsed_ = 0.0f;
                            int frame =
                                gfxAnmSequenceFrameAt(sprite_, spriteSequence_, spriteSequenceStep_);
                            if (frame >= 0 && frame < n) spriteFrame_ = frame;
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        } else {
            ui::SetFieldShort();
            ImGui::SliderFloat("Speed##pcmon", &pcSpeed_, 0.1f, 4.0f, "%.2fx");
            const int n = pcPic_ ? static_cast<int>(pcPic_->frames.size()) : 0;
            ImGui::SliderInt("State##pcmon", &pcState_, 0, n > 0 ? n - 1 : 0);
        }
    }

    if (ui::BeginHexBlock("Raw record")) {
        ImGui::TextDisabled("Record %d · %d bytes", selected_, kMonsterRecordSize);
        DrawHexView("mon_hex", r.raw.data(), kMonsterRecordSize, selected_ * kMonsterRecordSize);
        ui::EndHexBlock();
    }
}

}  // namespace mm2

#include "sections/PcGfxSection.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

#include "app/App.h"
#include "imgui.h"
#include "portable-file-dialogs.h"
#include "widgets/Texture.h"
#include "widgets/UiLayout.h"
#include "widgets/UiTheme.h"

namespace fs = std::filesystem;

namespace {

constexpr float kWallThumbMaxPx = 128.0f;

ImVec2 wallFrameDisplaySize(int w, int h, float zoom, float maxPx = 0.0f) {
    float dw = w * zoom;
    float dh = h * zoom;
    if (maxPx > 0.0f) {
        const float lim = std::max(static_cast<float>(w), static_cast<float>(h)) * zoom;
        if (lim > maxPx) {
            const float scale = maxPx / lim;
            dw *= scale;
            dh *= scale;
        }
    }
    return ImVec2(dw, dh);
}

ImVec2 wallFrameFitSize(int w, int h, float zoom) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float dw = w * zoom;
    float dh = h * zoom;
    if (avail.x > 32.0f && dw > avail.x) {
        const float s = avail.x / dw;
        dw *= s;
        dh *= s;
    }
    if (avail.y > 32.0f && dh > avail.y) {
        const float s = avail.y / dh;
        dw *= s;
        dh *= s;
    }
    return ImVec2(dw, dh);
}

const char* wallOffsetKind(const mm2::PcWallSheet& sheet) {
    if (sheet.packedU32) return "packed u32";
    if (sheet.offsetsAreU32) return "u32";
    if (sheet.groupedU16) return "grouped u16";
    return "u16";
}

const char* wallFrameHint(const mm2::PcWallFrame& fr) {
    if (fr.width >= 300 && fr.height >= 180) return "title screen";
    return "";
}

struct PcWallsetDef {
    const char* label;
    const char* walls;
    const char* floor;
    const char* torch;
    const char* border;
};

constexpr PcWallsetDef kPcWallsets[] = {
    {"Town", "TOWN", "TOWNF", "TOWNT", "TOWNB"},
    {"Cavern", "CAVE", "CAVEF", "CAVET", "CAVEB"},
    {"Castle", "CASTLE", "CASTLEF", "CASTLET", "CASTLEB"},
    {"Desert (outdoor)", "DESERT", nullptr, nullptr, nullptr},
    {"Ocean (outdoor)", "OCEAN", nullptr, nullptr, nullptr},
    {"Swamp (outdoor)", "SWAMP", nullptr, nullptr, nullptr},
    {"Tundra (outdoor)", "TUNDRA", nullptr, nullptr, nullptr},
    {"Outdoor horizon 1", "OUTDOOR1", "OUTF", nullptr, "OUTB"},
    {"Outdoor horizon 2", "OUTDOOR2", "OUTF", nullptr, "OUTB"},
    {"Outdoor horizon 3", "OUTDOOR3", "OUTF", nullptr, "OUTB"},
    {"UI / misc", "MASTER", "SKY", "THROW", "BOOK"},
};

}  // namespace

namespace mm2 {

PcGfxSection::~PcGfxSection() { releaseTextures(); }

void PcGfxSection::releaseTextures() {
    for (unsigned int t : wallTextures_) freeTexture(t);
    wallTextures_.clear();
    for (unsigned int t : picRawTextures_) freeTexture(t);
    picRawTextures_.clear();
    for (unsigned int t : picComposedTextures_) freeTexture(t);
    picComposedTextures_.clear();
}

bool PcGfxSection::load(const std::string& dataDir) {
    // dataDir is the Amiga data folder. GOG PC assets usually live in a
    // separate install folder — auto-detect it alongside/under dataDir; the
    // user can also pick one explicitly via the button in draw().
    bpp_ = pcBppForExt(ext_);
    if (dir_.empty()) {
        std::string found = pcFindAssetsDir(dataDir);
        if (!found.empty()) rescan(found);
    }
    return loaded;
}

void PcGfxSection::rescan(const std::string& dir) {
    dir_ = dir;
    files_.clear();
    selectedFile_ = -1;
    isMonsters_ = false;
    releaseTextures();
    wall_.clear();
    atlas_.clear();
    pic_.reset();
    availablePicIds_.clear();
    selectedPicId_ = -1;

    std::error_code ec;
    if (!dir_.empty() && fs::is_directory(dir_, ec)) {
        std::string want = ext_;
        for (auto& e : fs::directory_iterator(dir_, ec)) {
            if (!e.is_regular_file()) continue;
            std::string lower = e.path().extension().string();
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower == want) files_.push_back(e.path().filename().string());
        }
    }
    std::sort(files_.begin(), files_.end());
    loaded = !files_.empty();
}

void PcGfxSection::selectFile(int idx) {
    if (idx < 0 || idx >= static_cast<int>(files_.size())) return;
    selectedFile_ = idx;
    isMonsters_ = pcIsMonstersFile(files_[static_cast<size_t>(idx)]);
    releaseTextures();
    wall_.clear();
    atlas_.clear();
    pic_.reset();
    availablePicIds_.clear();
    selectedPicId_ = -1;

    if (isMonsters_) {
        selectedWallset_ = -1;
        loadAtlas();
    } else {
        syncWallsetFromFile();
        loadWallSheet();
    }
}

int PcGfxSection::indexOfStem(const char* stem) const {
    if (!stem || !stem[0]) return -1;
    std::string want = stem;
    want += ext_;
    for (int i = 0; i < static_cast<int>(files_.size()); ++i) {
        std::string upper = files_[static_cast<size_t>(i)];
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (upper == want) return i;
    }
    return -1;
}

void PcGfxSection::syncWallsetFromFile() {
    selectedWallset_ = -1;
    if (selectedFile_ < 0) return;
    std::string upper = files_[static_cast<size_t>(selectedFile_)];
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const std::string stem = upper.substr(0, upper.find('.'));
    for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(kPcWallsets)); ++i) {
        const PcWallsetDef& ws = kPcWallsets[static_cast<size_t>(i)];
        if (stem == ws.walls || (ws.floor && stem == ws.floor) || (ws.torch && stem == ws.torch) ||
            (ws.border && stem == ws.border)) {
            selectedWallset_ = i;
            return;
        }
    }
}

void PcGfxSection::loadWallSheet() {
    wallFrame_ = 0;
    wallElapsed_ = 0.0f;
    wallLastTick_ = ImGui::GetTime();
    pcLoadWallSheet(dir_ + "/" + files_[static_cast<size_t>(selectedFile_)], wall_);
    wallPlaying_ = wall_.frames.size() > 1;
    if (wall_.ok || !wall_.frames.empty()) {
        int maxDim = 0;
        for (const PcWallFrame& fr : wall_.frames)
            maxDim = std::max(maxDim, std::max(fr.width, fr.height));
        if (maxDim > 0) {
            // Fit the largest frame (~480px wide detail pane) without forcing tiny zoom on sheets
            // that are already small (cap at default 3x).
            zoom_ = std::clamp(480.0f / static_cast<float>(maxDim), 1.0f, 3.0f);
        }
        buildWallTextures();
    }
}

void PcGfxSection::buildWallTextures() {
    wallTextures_.reserve(wall_.frames.size());
    std::vector<uint8_t> rgba;
    for (const PcWallFrame& fr : wall_.frames) {
        pcDecodeWallFrameRGBA(fr, wall_.bpp, cgaPalette_, rgba);
        wallTextures_.push_back(makeTextureRGBA(rgba.data(), fr.width, fr.height));
    }
}

void PcGfxSection::loadAtlas() {
    pcLoadMonstersAtlas(dir_ + "/" + files_[static_cast<size_t>(selectedFile_)], atlas_);
    if (atlas_.ok) {
        availablePicIds_ = pcMonsterAvailablePictureIds(atlas_);
        if (!availablePicIds_.empty()) selectPicture(availablePicIds_.front());
    }
}

void PcGfxSection::selectPicture(int pictureId) {
    selectedPicId_ = pictureId;
    monState_ = 0;
    monSequence_ = 0;
    monSequenceStep_ = 0;
    monElapsed_ = 0.0f;
    monLastTick_ = ImGui::GetTime();
    for (unsigned int t : picRawTextures_) freeTexture(t);
    picRawTextures_.clear();
    for (unsigned int t : picComposedTextures_) freeTexture(t);
    picComposedTextures_.clear();

    pic_ = pcMonsterPictureForId(atlas_, pictureId);
    if (pic_ && pic_->ok) {
        buildMonsterTextures();
        monPlaying_ = pic_->frames.size() > 1;
    } else {
        monPlaying_ = false;
    }
}

void PcGfxSection::buildMonsterTextures() {
    if (!pic_) return;
    std::vector<uint8_t> rgba;
    picRawTextures_.reserve(pic_->frames.size());
    for (const PcMonsterFrame& fr : pic_->frames) {
        pcDecodeMonsterFrameRGBA(fr, pic_->bpp, cgaPalette_, rgba);
        picRawTextures_.push_back(makeTextureRGBA(rgba.data(), fr.width, fr.height));
    }
    picComposedTextures_.reserve(pic_->frames.size());
    for (int i = 0; i < static_cast<int>(pic_->frames.size()); ++i) {
        pcCompositeMonsterFrame(*pic_, i, cgaPalette_, rgba);
        picComposedTextures_.push_back(makeTextureRGBA(rgba.data(), kPcCombatCanvasW, kPcCombatCanvasH));
    }
}

void PcGfxSection::draw(App& app) {
    // Two-way sync with the shared PC assets directory (App auto-detects it
    // on folder open; the button below lets the user override it).
    if (dir_.empty() && !app.state().pcDataDir.empty() && app.state().pcDataDir != lastSyncedPcDir_) {
        rescan(app.state().pcDataDir);
    }
    if (!dir_.empty() && app.state().pcDataDir != dir_) app.state().pcDataDir = dir_;
    lastSyncedPcDir_ = app.state().pcDataDir;

    ImGui::TextDisabled("%s", dir_.empty() ? "(no PC assets folder)" : dir_.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Open PC assets folder...")) {
        auto sel = pfd::select_folder("Select GOG Might and Magic II folder (ALLCAPS.4 / .16)").result();
        if (!sel.empty()) {
            rescan(sel);
            app.state().pcDataDir = sel;
        }
    }
    ImGui::Separator();

    if (dir_.empty() || files_.empty()) {
        ImGui::TextDisabled("No %s files found. Point this at a GOG Might and Magic II install folder.", ext_);
        return;
    }

    ui::BeginListPanel("pcgfx_files");
    if (selectedFile_ < 0 || !isMonsters_) {
        const char* wallsetLabel =
            (selectedWallset_ >= 0) ? kPcWallsets[static_cast<size_t>(selectedWallset_)].label : "(custom file)";
        ui::SetFieldMed();
        if (ImGui::BeginCombo("Wallset", wallsetLabel)) {
            for (int wi = 0; wi < static_cast<int>(IM_ARRAYSIZE(kPcWallsets)); ++wi) {
                const bool sel = (wi == selectedWallset_);
                if (ImGui::Selectable(kPcWallsets[static_cast<size_t>(wi)].label, sel)) {
                    selectedWallset_ = wi;
                    const PcWallsetDef& ws = kPcWallsets[static_cast<size_t>(wi)];
                    const int idx = indexOfStem(ws.walls);
                    if (idx >= 0) selectFile(idx);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (selectedWallset_ >= 0) {
            const PcWallsetDef& ws = kPcWallsets[static_cast<size_t>(selectedWallset_)];
            if (ws.walls && ImGui::SmallButton("Walls")) {
                const int idx = indexOfStem(ws.walls);
                if (idx >= 0) selectFile(idx);
            }
            ImGui::SameLine();
            if (ws.floor && ImGui::SmallButton("Floor")) {
                const int idx = indexOfStem(ws.floor);
                if (idx >= 0) selectFile(idx);
            }
            ImGui::SameLine();
            if (ws.torch && ImGui::SmallButton("Torch")) {
                const int idx = indexOfStem(ws.torch);
                if (idx >= 0) selectFile(idx);
            }
            ImGui::SameLine();
            if (ws.border && ImGui::SmallButton("Border")) {
                const int idx = indexOfStem(ws.border);
                if (idx >= 0) selectFile(idx);
            }
        }
        ImGui::Separator();
    }
    for (int i = 0; i < static_cast<int>(files_.size()); ++i) {
        if (ImGui::Selectable(files_[static_cast<size_t>(i)].c_str(), selectedFile_ == i)) selectFile(i);
    }
    ui::ListPanelNextDetail("pcgfx_view");
    if (selectedFile_ < 0) {
        ImGui::TextDisabled("Select a file.");
        ui::EndDetailPanel();
        return;
    }

    ImGui::Text("%s", files_[static_cast<size_t>(selectedFile_)].c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s, %dbpp)", isMonsters_ ? "combat atlas" : "wall sheet", bpp_);

    ui::SetFieldShort();
    if (bpp_ == 2) {
        const char* palLabel = cgaPalette_ == 0 ? "Palette 0 (green/red/brown)" : "Palette 1 (cyan/magenta/white)";
        if (ImGui::BeginCombo("CGA palette", palLabel)) {
            if (ImGui::Selectable("Palette 0 (green/red/brown)", cgaPalette_ == 0)) {
                cgaPalette_ = 0;
                releaseTextures();
                if (isMonsters_) buildMonsterTextures();
                else buildWallTextures();
            }
            if (ImGui::Selectable("Palette 1 (cyan/magenta/white) - MM2 default", cgaPalette_ == 1)) {
                cgaPalette_ = 1;
                releaseTextures();
                if (isMonsters_) buildMonsterTextures();
                else buildWallTextures();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
    }
    ui::SetFieldMed();
    ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 8.0f, "%.0fx");

    if (isMonsters_) drawMonsterDetail();
    else drawWallDetail();

    ui::EndDetailPanel();
}

void PcGfxSection::drawWallDetail() {
    if (!wall_.ok && wall_.frames.empty()) {
        ImGui::TextColored(ui::Danger(), "Decode failed: %s", wall_.error.c_str());
        return;
    }
    ImGui::TextDisabled("%d frames  header=%d  offsets=%s  compressed=%zuB decompressed=%zu/%uB%s",
                        wall_.frameCount, wall_.tableSlotCount, wallOffsetKind(wall_), wall_.compressedBytes,
                        wall_.decompressed.size(), wall_.uncompressedSize,
                        wall_.error.empty() ? "" : "  (partial)");

    const int n = static_cast<int>(wall_.frames.size());
    ImGui::Checkbox("Play", &wallPlaying_);
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &wallLoop_);
    ui::SetFieldShort();
    ImGui::SliderFloat("Speed", &wallSpeed_, 0.1f, 4.0f, "%.2fx");

    if (wallPlaying_ && n > 1) {
        const double now = ImGui::GetTime();
        const float dt = static_cast<float>((wallLastTick_ > 0.0) ? (now - wallLastTick_) : 0.0);
        wallLastTick_ = now;
        wallElapsed_ += (dt > 0.0f) ? dt : 0.0f;
        const float frameDur = 0.125f / ((wallSpeed_ > 0.0f) ? wallSpeed_ : 1.0f);
        while (wallElapsed_ >= frameDur) {
            wallElapsed_ -= frameDur;
            ++wallFrame_;
            if (wallFrame_ >= n) {
                if (wallLoop_) wallFrame_ = 0;
                else {
                    wallFrame_ = n - 1;
                    wallPlaying_ = false;
                    break;
                }
            }
        }
    } else {
        wallLastTick_ = ImGui::GetTime();
    }

    if (ImGui::BeginTabBar("pcgfx_wall_tabs")) {
        if (ImGui::BeginTabItem("Frame")) {
            ImGui::SetNextItemWidth(160);
            ImGui::SliderInt("Frame##wall", &wallFrame_, 0, n > 0 ? n - 1 : 0);
            if (wallFrame_ < 0) wallFrame_ = 0;
            if (wallFrame_ >= n) wallFrame_ = (n > 0) ? (n - 1) : 0;
            if (wallFrame_ >= 0 && wallFrame_ < n) {
                const PcWallFrame& fr = wall_.frames[static_cast<size_t>(wallFrame_)];
                unsigned int tex = wallTextures_[static_cast<size_t>(wallFrame_)];
                if (tex) {
                    ImVec2 sz = wallFrameFitSize(fr.width, fr.height, zoom_);
                    ImGui::Image(static_cast<ImTextureID>(tex), sz);
                }
                const char* hint = wallFrameHint(fr);
                if (hint[0] != '\0')
                    ImGui::Text("frame %d  slot %d  @0x%zX  %dx%d  (%s)", fr.index, fr.tableSlot, fr.offset,
                                fr.width, fr.height, hint);
                else
                    ImGui::Text("frame %d  slot %d  @0x%zX  %dx%d", fr.index, fr.tableSlot, fr.offset, fr.width,
                                fr.height);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("All frames")) {
            float avail = ImGui::GetContentRegionAvail().x;
            float x = 0;
            for (int i = 0; i < n; ++i) {
                const PcWallFrame& fr = wall_.frames[static_cast<size_t>(i)];
                unsigned int tex = wallTextures_[static_cast<size_t>(i)];
                if (!tex) continue;
                ImVec2 sz = wallFrameDisplaySize(fr.width, fr.height, zoom_, kWallThumbMaxPx);
                const bool current = (i == wallFrame_);
                ui::SelectedFrameScope sel(current);
                ImGui::BeginGroup();
                if (ImGui::ImageButton(("wf" + std::to_string(i)).c_str(), static_cast<ImTextureID>(tex), sz))
                    wallFrame_ = i;
                const char* hint = wallFrameHint(fr);
                if (hint[0] != '\0')
                    ImGui::TextDisabled("%d  %dx%d  %s", fr.index, fr.width, fr.height, hint);
                else
                    ImGui::TextDisabled("%d  %dx%d", fr.index, fr.width, fr.height);
                ImGui::EndGroup();
                x += sz.x + 12;
                if (x + sz.x < avail) ImGui::SameLine();
                else x = 0;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void PcGfxSection::drawMonsterDetail() {
    if (!atlas_.ok) {
        ImGui::TextColored(ui::Danger(), "Decode failed: %s", atlas_.error.c_str());
        return;
    }
    ImGui::TextDisabled("header=%zuB (%zu slots)  %zu picture ids resolved", atlas_.headerBytes,
                        atlas_.headerBytes / 4, availablePicIds_.size());

    ui::SetFieldMed();
    std::string picLabel = (selectedPicId_ >= 0) ? ("Picture " + std::to_string(selectedPicId_)) : "(none)";
    if (ImGui::BeginCombo("Picture id", picLabel.c_str())) {
        for (int id : availablePicIds_) {
            char label[32];
            snprintf(label, sizeof(label), "Picture %d", id);
            bool sel = (id == selectedPicId_);
            if (ImGui::Selectable(label, sel)) selectPicture(id);
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(same picture id as NN.anm in monsters.dat byte 0x15 & 0x7F)");

    if (!pic_ || !pic_->ok) {
        ImGui::TextColored(ui::Danger(), "No decodable picture for this id.");
        return;
    }

    const int n = static_cast<int>(pic_->frames.size());
    ImGui::TextDisabled("%d raw frames  flags=0x%X  blob@0x%zX", n, pic_->flags, pic_->blobOffset);

    ImGui::Checkbox("Play##mon", &monPlaying_);
    ImGui::SameLine();
    ImGui::Checkbox("Loop##mon", &monLoop_);
    ui::SetFieldShort();
    ImGui::SliderFloat("Speed##mon", &monSpeed_, 0.1f, 4.0f, "%.2fx");
    ui::SetFieldMed();
    const char* modeLabel = (monPlayMode_ == PcPlayMode::Flipbook) ? "Flipbook" : "Sequence";
    if (ImGui::BeginCombo("Play mode##mon", modeLabel)) {
        if (ImGui::Selectable("Flipbook (all composed states)", monPlayMode_ == PcPlayMode::Flipbook)) {
            monPlayMode_ = PcPlayMode::Flipbook;
            monSequenceStep_ = 0;
            monElapsed_ = 0.0f;
        }
        if (ImGui::Selectable("Sequence (game script)", monPlayMode_ == PcPlayMode::Sequence)) {
            monPlayMode_ = PcPlayMode::Sequence;
            monSequenceStep_ = 0;
            monElapsed_ = 0.0f;
            int frame = pcMonsterSequenceFrameAt(*pic_, monSequence_, monSequenceStep_);
            if (frame >= 0) monState_ = frame;
        }
        ImGui::EndCombo();
    }

    if (monPlaying_ && n > 1) {
        const double now = ImGui::GetTime();
        const float dt = static_cast<float>((monLastTick_ > 0.0) ? (now - monLastTick_) : 0.0);
        monLastTick_ = now;
        monElapsed_ += (dt > 0.0f) ? dt : 0.0f;

        const bool useSequence = monPlayMode_ == PcPlayMode::Sequence && pcMonsterHasSequencePlayback(*pic_);
        if (useSequence) {
            if (monSequence_ < 0 || monSequence_ >= static_cast<int>(pic_->scripts.size())) monSequence_ = 0;
            const auto& seq = pic_->scripts[static_cast<size_t>(monSequence_)];
            const int stepCount = static_cast<int>(seq.size());
            if (stepCount > 0) {
                while (monElapsed_ >= pcMonsterSequenceStepDurationSec(*pic_, monSequence_, monSequenceStep_, monSpeed_)) {
                    monElapsed_ -= pcMonsterSequenceStepDurationSec(*pic_, monSequence_, monSequenceStep_, monSpeed_);
                    ++monSequenceStep_;
                    if (monSequenceStep_ >= stepCount) {
                        if (monLoop_) monSequenceStep_ = 0;
                        else {
                            monSequenceStep_ = stepCount - 1;
                            monPlaying_ = false;
                            break;
                        }
                    }
                    int frame = pcMonsterSequenceFrameAt(*pic_, monSequence_, monSequenceStep_);
                    if (frame >= 0 && frame < n) monState_ = frame;
                }
            }
        } else {
            const float frameDur = 0.125f / ((monSpeed_ > 0.0f) ? monSpeed_ : 1.0f);
            while (monElapsed_ >= frameDur) {
                monElapsed_ -= frameDur;
                ++monState_;
                if (monState_ >= n) {
                    if (monLoop_) monState_ = 0;
                    else {
                        monState_ = n - 1;
                        monPlaying_ = false;
                        break;
                    }
                }
            }
        }
    } else {
        monLastTick_ = ImGui::GetTime();
    }
    if (monState_ < 0) monState_ = 0;
    if (monState_ >= n) monState_ = (n > 0) ? (n - 1) : 0;

    if (!pic_->scripts.empty() && monPlayMode_ == PcPlayMode::Sequence) {
        if (monSequence_ < 0 || monSequence_ >= static_cast<int>(pic_->scripts.size())) monSequence_ = 0;
        std::string seqLabel = "Sequence " + std::to_string(monSequence_);
        if (ImGui::BeginCombo("Sequence##mon", seqLabel.c_str())) {
            for (int i = 0; i < static_cast<int>(pic_->scripts.size()); ++i) {
                std::string label = "Sequence " + std::to_string(i) + " (" +
                                    std::to_string(pic_->scripts[static_cast<size_t>(i)].size()) + " steps)";
                bool sel = (i == monSequence_);
                if (ImGui::Selectable(label.c_str(), sel)) {
                    monSequence_ = i;
                    monSequenceStep_ = 0;
                    monElapsed_ = 0.0f;
                    int frame = pcMonsterSequenceFrameAt(*pic_, monSequence_, monSequenceStep_);
                    if (frame >= 0 && frame < n) monState_ = frame;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    if (ImGui::BeginTabBar("pcgfx_mon_tabs")) {
        if (ImGui::BeginTabItem("Composed")) {
            ImGui::SetNextItemWidth(160);
            ImGui::SliderInt("State##mon", &monState_, 0, n > 0 ? n - 1 : 0);
            if (monState_ >= 0 && monState_ < static_cast<int>(picComposedTextures_.size())) {
                unsigned int tex = picComposedTextures_[static_cast<size_t>(monState_)];
                if (tex)
                    ImGui::Image(static_cast<ImTextureID>(tex),
                                 ImVec2(kPcCombatCanvasW * zoom_, kPcCombatCanvasH * zoom_));
                ImGui::TextDisabled("state %d / %d  (%dx%d canvas)", monState_, n - 1, kPcCombatCanvasW,
                                    kPcCombatCanvasH);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Raw frames")) {
            ImGui::TextDisabled("Frame 0 = base sprite; frame N>0 = delta patch blitted at (x,y) after clearing.");
            float avail = ImGui::GetContentRegionAvail().x;
            float x = 0;
            for (int i = 0; i < n; ++i) {
                const PcMonsterFrame& fr = pic_->frames[static_cast<size_t>(i)];
                unsigned int tex = picRawTextures_[static_cast<size_t>(i)];
                if (!tex) continue;
                ImVec2 sz(fr.width * zoom_, fr.height * zoom_);
                ImGui::BeginGroup();
                if (ImGui::ImageButton(("mf" + std::to_string(i)).c_str(), static_cast<ImTextureID>(tex), sz))
                    monState_ = fr.frameIndex;
                ImGui::TextDisabled("f%d @(%d,%d) %dx%d", fr.frameIndex, fr.x, fr.y, fr.width, fr.height);
                ImGui::EndGroup();
                x += sz.x + 12;
                if (x + sz.x < avail) ImGui::SameLine();
                else x = 0;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Scripts")) {
            if (pic_->scripts.empty()) {
                ImGui::TextDisabled("No animation scripts in this blob.");
            }
            for (int i = 0; i < static_cast<int>(pic_->scripts.size()); ++i) {
                const auto& seq = pic_->scripts[static_cast<size_t>(i)];
                std::string line = "Sequence " + std::to_string(i) + ": ";
                for (const auto& step : seq) {
                    line += "(" + std::to_string(step.first) + "," + std::to_string(step.second) + ") ";
                }
                ImGui::TextWrapped("%s", line.c_str());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

}  // namespace mm2

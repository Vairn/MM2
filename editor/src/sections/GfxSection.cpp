#include "sections/GfxSection.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>

#include "imgui.h"
#include "widgets/Texture.h"
#include "widgets/UiLayout.h"
#include "widgets/UiTheme.h"

namespace fs = std::filesystem;

namespace mm2 {
namespace {

void clearRgba(std::vector<uint8_t>& rgba) { std::fill(rgba.begin(), rgba.end(), 0); }

void blitOpaque(const GfxFrame& fr, int dstX, int dstY, int canvasW, int canvasH, std::vector<uint8_t>& rgba) {
    if (canvasW <= 0 || canvasH <= 0) return;
    for (int y = 0; y < fr.height; ++y) {
        const int oy = dstY + y;
        if (oy < 0 || oy >= canvasH) continue;
        for (int x = 0; x < fr.width; ++x) {
            const int ox = dstX + x;
            if (ox < 0 || ox >= canvasW) continue;
            const uint8_t* src = &fr.rgba[(static_cast<size_t>(y) * fr.width + x) * 4];
            if (src[3] == 0) continue;
            uint8_t* dst = &rgba[(static_cast<size_t>(oy) * canvasW + ox) * 4];
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        }
    }
}

void clearRectRgba(int dstX, int dstY, int rectW, int rectH, int canvasW, int canvasH, std::vector<uint8_t>& rgba) {
    if (rectW <= 0 || rectH <= 0) return;
    for (int y = 0; y < rectH; ++y) {
        const int oy = dstY + y;
        if (oy < 0 || oy >= canvasH) continue;
        for (int x = 0; x < rectW; ++x) {
            const int ox = dstX + x;
            if (ox < 0 || ox >= canvasW) continue;
            uint8_t* dst = &rgba[(static_cast<size_t>(oy) * canvasW + ox) * 4];
            dst[0] = dst[1] = dst[2] = dst[3] = 0;
        }
    }
}

std::string toLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}  // namespace

GfxSection::~GfxSection() { releaseTextures(); }

void GfxSection::releaseTextures() {
    for (unsigned int t : textures_) freeTexture(t);
    textures_.clear();
    for (unsigned int t : composedTextures_) freeTexture(t);
    composedTextures_.clear();
    if (titleTexture_) freeTexture(titleTexture_);
    titleTexture_ = 0;
    titleRgba_.clear();
    anmCanvas_ = {};
}

void GfxSection::buildComposedTextures() {
    composedTextures_.clear();
    if (!isAnm_ || image_.frames.empty()) return;

    anmCanvas_ = gfxAnmCompositeCanvas(image_);
    if (!anmCanvas_.valid) return;

    std::vector<uint8_t> rgba;
    composedTextures_.reserve(image_.frames.size());
    for (int i = 0; i < static_cast<int>(image_.frames.size()); ++i) {
        if (!gfxAnmCompositeFrame(image_, i, rgba, &anmCanvas_)) {
            composedTextures_.push_back(0);
            continue;
        }
        composedTextures_.push_back(makeTextureRGBA(rgba.data(), anmCanvas_.width, anmCanvas_.height));
    }
}

unsigned int GfxSection::composedTextureForFrame(int frameIdx) const {
    if (frameIdx < 0 || frameIdx >= static_cast<int>(composedTextures_.size())) return 0;
    return composedTextures_[static_cast<size_t>(frameIdx)];
}

bool GfxSection::load(const std::string& dataDir) {
    dir_ = dataDir;
    files_.clear();
    selectedFile_ = -1;
    selectedFrame_ = 0;
    anmPlayMode_ = AnmPlayMode::Flipbook;
    titleReady_ = false;
    titleStatus_.clear();
    titleIntro_.clear();
    titleClips_.clear();
    titleCanvasW_ = titleCanvasH_ = 0;
    titleFrame_ = 0;
    titlePlaying_ = true;
    titleLoop_ = true;
    titleAnimateBase_ = false;
    titleSpeed_ = 1.0f;
    titleLastTick_ = 0.0;
    titleElapsed_ = 0.0f;
    titleAsmReplay_ = true;
    titleAsmTickCounter_ = 0;
    titleAsmFrame_ = 0;
    titleAsmX_ = 8;
    titleAsmYTop_ = 8;
    titleAsmYBottom_ = 216;
    titleAsmCanvasPrimed_ = false;
    releaseTextures();
    image_.clear();

    std::string want = ext_;  // e.g. ".32"
    std::error_code ec;
    if (fs::is_directory(dataDir, ec)) {
        for (auto& e : fs::directory_iterator(dataDir, ec)) {
            if (!e.is_regular_file()) continue;
            std::string name = e.path().filename().string();
            std::string lower = e.path().extension().string();
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lower == want) files_.push_back(name);
        }
    }
    std::sort(files_.begin(), files_.end());
    if (!isAnm_ && ext_ && std::string(ext_) == ".32") {
        initTitlePreviewAssets();
    }
    loaded = !files_.empty();
    return loaded;
}

void GfxSection::selectFile(int idx) {
    if (idx < 0 || idx >= static_cast<int>(files_.size())) return;
    selectedFile_ = idx;
    selectedFrame_ = 0;
    selectedSequence_ = 0;
    sequenceStep_ = 0;
    elapsed_ = 0.0f;
    lastTick_ = ImGui::GetTime();
    releaseTextures();
    gfxLoad(dir_ + "/" + files_[idx], isAnm_, image_);
    playing_ = image_.frameCount > 1;
    if (image_.ok || !image_.frames.empty()) {
        textures_.reserve(image_.frames.size());
        for (auto& fr : image_.frames)
            textures_.push_back(makeTextureRGBA(fr.rgba.data(), fr.width, fr.height));
    }
    if (isAnm_) {
        buildComposedTextures();
        if (anmPlayMode_ == AnmPlayMode::Sequence && gfxAnmHasSequencePlayback(image_)) {
            int frame = gfxAnmSequenceFrameAt(image_, selectedSequence_, sequenceStep_);
            if (frame >= 0) selectedFrame_ = frame;
        }
    }
}

bool GfxSection::initTitlePreviewAssets() {
    titleReady_ = false;
    titleStatus_.clear();
    titleIntro_.clear();
    titleClips_.clear();
    titleCanvasW_ = titleCanvasH_ = 0;
    titleFrame_ = 0;
    titleElapsed_ = 0.0f;
    titleLastTick_ = ImGui::GetTime();
    titleAsmTickCounter_ = 0;
    titleAsmFrame_ = 0;
    titleAsmCanvasPrimed_ = false;

    std::string introName;
    std::string clipsName;
    for (const std::string& f : files_) {
        std::string lower = toLowerCopy(f);
        if (lower == "intro.32")
            introName = f;
        else if (lower == "introclips.32")
            clipsName = f;
    }
    if (introName.empty() || clipsName.empty()) {
        titleStatus_ = "Missing intro.32 or introclips.32 in this folder.";
        return false;
    }

    const bool okIntro = gfxLoad(dir_ + "/" + introName, false, titleIntro_);
    const bool okClips = gfxLoad(dir_ + "/" + clipsName, false, titleClips_);
    if ((!okIntro && titleIntro_.frames.empty()) || (!okClips && titleClips_.frames.empty())) {
        titleStatus_ = "Failed to decode intro.32 or introclips.32.";
        return false;
    }
    if (titleIntro_.frames.empty() || titleClips_.frames.empty()) {
        titleStatus_ = "One title source has no frames.";
        return false;
    }

    int maxW = 0, maxH = 0;
    for (const GfxFrame& fr : titleIntro_.frames) {
        if (fr.width > maxW) maxW = fr.width;
        if (fr.height > maxH) maxH = fr.height;
    }
    for (const GfxFrame& fr : titleClips_.frames) {
        if (fr.width > maxW) maxW = fr.width;
        if (fr.height > maxH) maxH = fr.height;
    }
    if (maxW <= 0 || maxH <= 0) {
        titleStatus_ = "Invalid title frame sizes.";
        return false;
    }
    titleCanvasW_ = maxW;
    titleCanvasH_ = maxH;
    titleRgba_.assign(static_cast<size_t>(titleCanvasW_) * titleCanvasH_ * 4, 0);
    titleTexture_ = makeTextureRGBA(titleRgba_.data(), titleCanvasW_, titleCanvasH_);
    titleReady_ = (titleTexture_ != 0);
    if (!titleReady_) {
        titleStatus_ = "Could not allocate title preview texture.";
        return false;
    }

    titleStatus_ = "Using intro.32 + introclips.32 composite preview.";
    return true;
}

void GfxSection::updateTitlePlayback() {
    if (!titleReady_) return;
    const int introN = static_cast<int>(titleIntro_.frames.size());
    const int clipsN = static_cast<int>(titleClips_.frames.size());
    if (introN <= 0 || clipsN <= 0) return;
    const int total = (introN > clipsN) ? introN : clipsN;
    if (!titlePlaying_ || total <= 1) {
        titleLastTick_ = ImGui::GetTime();
        return;
    }
    const double now = ImGui::GetTime();
    const float dt = static_cast<float>((titleLastTick_ > 0.0) ? (now - titleLastTick_) : 0.0);
    titleLastTick_ = now;
    titleElapsed_ += (dt > 0.0f) ? dt : 0.0f;
    const float frameDur = 0.125f / ((titleSpeed_ > 0.0f) ? titleSpeed_ : 1.0f);  // 8 fps baseline
    while (titleElapsed_ >= frameDur) {
        titleElapsed_ -= frameDur;
        ++titleFrame_;
        if (titleFrame_ >= total) {
            if (titleLoop_)
                titleFrame_ = 0;
            else {
                titleFrame_ = total - 1;
                titlePlaying_ = false;
                break;
            }
        }
    }

    // ASM replay core from 0x26A10:
    // - increment gate counter (A4-$6478)
    // - when gate reaches 3, reset gate and advance frame (A4-$647A)
    if (titleAsmReplay_) {
        ++titleAsmTickCounter_;
        if (titleAsmTickCounter_ >= 3) {
            titleAsmTickCounter_ = 0;
            ++titleAsmFrame_;
            int mod = 5;
            if (!titleClips_.frames.empty() && static_cast<int>(titleClips_.frames.size()) < mod)
                mod = static_cast<int>(titleClips_.frames.size());
            if (mod < 1) mod = 1;
            if (titleAsmFrame_ >= mod) titleAsmFrame_ = 0;
        }
    }
}

void GfxSection::drawTitleTab() {
    if (!titleReady_) {
        ImGui::TextDisabled("%s", titleStatus_.empty() ? "Title composite not available." : titleStatus_.c_str());
        return;
    }

    updateTitlePlayback();
    const int introN = static_cast<int>(titleIntro_.frames.size());
    const int clipsN = static_cast<int>(titleClips_.frames.size());
    const int total = (introN > clipsN) ? introN : clipsN;
    if (titleFrame_ < 0) titleFrame_ = 0;
    if (titleFrame_ >= total) titleFrame_ = (total > 0) ? (total - 1) : 0;

    ImGui::TextDisabled("%s", titleStatus_.c_str());
    ImGui::Checkbox("Play title", &titlePlaying_);
    ImGui::SameLine();
    ImGui::Checkbox("Loop title", &titleLoop_);
    ImGui::SameLine();
    if (ImGui::Checkbox("Animate intro base", &titleAnimateBase_)) titleAsmCanvasPrimed_ = false;
    ImGui::SameLine();
    if (ImGui::Checkbox("ASM replay", &titleAsmReplay_)) titleAsmCanvasPrimed_ = false;
    ImGui::SliderFloat("Title speed", &titleSpeed_, 0.1f, 4.0f, "%.2fx");
    ImGui::SliderInt("Title frame", &titleFrame_, 0, total > 0 ? total - 1 : 0);
    if (titleAsmReplay_) {
        if (ImGui::SliderInt("ASM X", &titleAsmX_, -64, titleCanvasW_)) titleAsmCanvasPrimed_ = false;
        if (ImGui::SliderInt("ASM Y top", &titleAsmYTop_, -64, titleCanvasH_ + 64)) titleAsmCanvasPrimed_ = false;
        if (ImGui::SliderInt("ASM Y bottom", &titleAsmYBottom_, -64, titleCanvasH_ + 64))
            titleAsmCanvasPrimed_ = false;
    }

    const int introIdx = titleAnimateBase_ ? (titleFrame_ % introN) : 0;
    const int clipsIdx = titleFrame_ % clipsN;
    const GfxFrame& base = titleIntro_.frames[introIdx];
    const GfxFrame& over = titleClips_.frames[clipsIdx];

    if (titleAsmReplay_) {
        // ASM-like sprite loop: clear destination rect(s) from background first,
        // then draw only the current frame into those rects.
        if (!titleAsmCanvasPrimed_ || titleAnimateBase_) {
            clearRgba(titleRgba_);
            blitOpaque(base, 0, 0, titleCanvasW_, titleCanvasH_, titleRgba_);
            titleAsmCanvasPrimed_ = true;
        }
        int asmIdx = titleAsmFrame_;
        if (clipsN > 0) {
            int mod = (clipsN < 5) ? clipsN : 5;
            if (mod < 1) mod = 1;
            asmIdx %= mod;
        } else {
            asmIdx = 0;
        }
        const GfxFrame& asmFr = titleClips_.frames[asmIdx];
        clearRectRgba(titleAsmX_, titleAsmYTop_, asmFr.width, asmFr.height, titleCanvasW_, titleCanvasH_, titleRgba_);
        clearRectRgba(titleAsmX_, titleAsmYBottom_, asmFr.width, asmFr.height, titleCanvasW_, titleCanvasH_,
                      titleRgba_);
        blitOpaque(asmFr, titleAsmX_, titleAsmYTop_, titleCanvasW_, titleCanvasH_, titleRgba_);
        blitOpaque(asmFr, titleAsmX_, titleAsmYBottom_, titleCanvasW_, titleCanvasH_, titleRgba_);
        ImGui::TextDisabled("ASM gate=%d frame=%d (mod 5)", titleAsmTickCounter_, asmIdx);
    } else {
        clearRgba(titleRgba_);
        blitOpaque(base, 0, 0, titleCanvasW_, titleCanvasH_, titleRgba_);
        blitOpaque(over, 0, 0, titleCanvasW_, titleCanvasH_, titleRgba_);
        titleAsmCanvasPrimed_ = false;
    }
    uploadTextureRGBA(titleTexture_, titleRgba_.data(), titleCanvasW_, titleCanvasH_);

    ImGui::TextDisabled("intro=%d/%d clips=%d/%d canvas=%dx%d", introIdx, introN, clipsIdx, clipsN, titleCanvasW_,
                        titleCanvasH_);
    ImGui::Image(static_cast<ImTextureID>(titleTexture_), ImVec2(titleCanvasW_ * zoom_, titleCanvasH_ * zoom_));
}

void GfxSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ui::EmptyState(isAnm_ ? "No .anm files found in the data folder."
                              : "No .32 files found in the data folder.");
        return;
    }

    ui::BeginListPanel("gfx_files");
    for (int i = 0; i < static_cast<int>(files_.size()); ++i) {
        if (!ui::ListFilterPass(ui::LegacyMasterDetail("gfx_files"), files_[i].c_str())) continue;
        if (ImGui::Selectable(files_[i].c_str(), selectedFile_ == i)) selectFile(i);
    }
    ui::ListPanelNextDetail("gfx_view");
    if (selectedFile_ < 0) {
        ui::EmptyState("Select a file.");
        ui::EndDetailPanel();
        return;
    }

    ui::PanelHeader(files_[selectedFile_].c_str());
    if (!image_.ok && image_.frames.empty()) {
        ImGui::TextColored(ui::Danger(), "Decode failed: %s", image_.error.c_str());
        ui::EndDetailPanel();
        return;
    }
    ImGui::TextDisabled("frames=%d depth=%d  chunk@0x%zX%s", image_.frameCount, image_.depth,
                        image_.chunkOffset,
                        image_.error.empty() ? "" : "  (partial)");

    ui::SetFieldMed();
    ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 8.0f, "%.0fx");
    ImGui::Checkbox("Play", &playing_);
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &loop_);
    ImGui::SliderFloat("Speed", &speed_, 0.1f, 4.0f, "%.2fx");
    if (isAnm_) {
        const char* modeLabel = (anmPlayMode_ == AnmPlayMode::Flipbook) ? "Flipbook" : "Sequence";
        if (ImGui::BeginCombo("Play mode", modeLabel)) {
            if (ImGui::Selectable("Flipbook (all composed frames)", anmPlayMode_ == AnmPlayMode::Flipbook)) {
                anmPlayMode_ = AnmPlayMode::Flipbook;
                sequenceStep_ = 0;
                elapsed_ = 0.0f;
            }
            if (ImGui::Selectable("Sequence (game stream)", anmPlayMode_ == AnmPlayMode::Sequence)) {
                anmPlayMode_ = AnmPlayMode::Sequence;
                sequenceStep_ = 0;
                elapsed_ = 0.0f;
                int frame = gfxAnmSequenceFrameAt(image_, selectedSequence_, sequenceStep_);
                if (frame >= 0) selectedFrame_ = frame;
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Flipbook: composed frames 0..N-1.\nSequence: (frame,delay) stream from the file.");
    }

    const int n = static_cast<int>(image_.frames.size());
    if (playing_ && n > 1) {
        const double now = ImGui::GetTime();
        const float dt = static_cast<float>((lastTick_ > 0.0) ? (now - lastTick_) : 0.0);
        lastTick_ = now;
        elapsed_ += (dt > 0.0f) ? dt : 0.0f;

        const bool useSequence =
            isAnm_ && anmPlayMode_ == AnmPlayMode::Sequence && gfxAnmHasSequencePlayback(image_);
        if (useSequence) {
            if (selectedSequence_ < 0 || selectedSequence_ >= static_cast<int>(image_.sequences.size()))
                selectedSequence_ = 0;
            const auto& seq = image_.sequences[selectedSequence_];
            const int pairCount = static_cast<int>(seq.size() / 2);
            if (pairCount > 0) {
                while (elapsed_ >= gfxAnmSequenceStepDurationSec(image_, selectedSequence_, sequenceStep_, speed_)) {
                    elapsed_ -= gfxAnmSequenceStepDurationSec(image_, selectedSequence_, sequenceStep_, speed_);
                    ++sequenceStep_;
                    if (sequenceStep_ >= pairCount) {
                        if (loop_)
                            sequenceStep_ = 0;
                        else {
                            sequenceStep_ = pairCount - 1;
                            playing_ = false;
                            break;
                        }
                    }
                    int frame = gfxAnmSequenceFrameAt(image_, selectedSequence_, sequenceStep_);
                    if (frame >= 0 && frame < n) selectedFrame_ = frame;
                }
            }
        } else {
            const float frameDur = 0.125f / ((speed_ > 0.0f) ? speed_ : 1.0f);  // 8 fps baseline
            while (elapsed_ >= frameDur) {
                elapsed_ -= frameDur;
                ++selectedFrame_;
                if (selectedFrame_ >= n) {
                    if (loop_)
                        selectedFrame_ = 0;
                    else {
                        selectedFrame_ = n - 1;
                        playing_ = false;
                        break;
                    }
                }
            }
        }
    } else {
        lastTick_ = ImGui::GetTime();
    }

    if (ImGui::BeginTabBar("gfx_tabs")) {
        if (ImGui::BeginTabItem("Frames")) {
            ImGui::SetNextItemWidth(160);
            ImGui::SliderInt("Frame", &selectedFrame_, 0, n > 0 ? n - 1 : 0);
            if (selectedFrame_ < 0) selectedFrame_ = 0;
            if (selectedFrame_ >= n) selectedFrame_ = (n > 0) ? (n - 1) : 0;

            if (isAnm_ && !image_.sequences.empty() && anmPlayMode_ == AnmPlayMode::Sequence) {
                if (selectedSequence_ < 0 || selectedSequence_ >= static_cast<int>(image_.sequences.size()))
                    selectedSequence_ = 0;
                std::string seqLabel = "Sequence " + std::to_string(selectedSequence_);
                if (ImGui::BeginCombo("Sequence", seqLabel.c_str())) {
                    for (int i = 0; i < static_cast<int>(image_.sequences.size()); ++i) {
                        std::string label =
                            "Sequence " + std::to_string(i) + " (" +
                            std::to_string(static_cast<int>(image_.sequences[i].size() / 2)) + " steps)";
                        bool sel = (i == selectedSequence_);
                        if (ImGui::Selectable(label.c_str(), sel)) {
                            selectedSequence_ = i;
                            sequenceStep_ = 0;
                            elapsed_ = 0.0f;
                            int frame = gfxAnmSequenceFrameAt(image_, selectedSequence_, sequenceStep_);
                            if (frame >= 0 && frame < n) selectedFrame_ = frame;
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                const int frame = gfxAnmSequenceFrameAt(image_, selectedSequence_, sequenceStep_);
                const int delay = (selectedSequence_ >= 0 &&
                                   selectedSequence_ < static_cast<int>(image_.sequences.size()) &&
                                   static_cast<size_t>(sequenceStep_) * 2 + 1 <
                                       image_.sequences[selectedSequence_].size())
                                      ? static_cast<int>(image_.sequences[selectedSequence_]
                                                                [static_cast<size_t>(sequenceStep_) * 2 + 1])
                                      : 0;
                ImGui::TextDisabled("Seq step=%d frame=%d delay=%d", sequenceStep_, frame, delay);
            }

            if (selectedFrame_ >= 0 && selectedFrame_ < n) {
                const GfxFrame& fr = image_.frames[selectedFrame_];
                unsigned int tex = textures_[selectedFrame_];
                int drawW = fr.width;
                int drawH = fr.height;

                if (isAnm_ && anmCanvas_.valid) {
                    if (unsigned int ct = composedTextureForFrame(selectedFrame_)) {
                        tex = ct;
                        drawW = anmCanvas_.width;
                        drawH = anmCanvas_.height;
                    }
                }

                if (tex) {
                    ImVec2 sz(drawW * zoom_, drawH * zoom_);
                    ImGui::Image(static_cast<ImTextureID>(tex), sz);
                }
                if (isAnm_) {
                    ImGui::TextDisabled("composed %dx%d  raw patch %dx%d  flags=0x%X", drawW, drawH, fr.width,
                                        fr.height, fr.flags);
                    const int preIdx = selectedFrame_ - 1;
                    if (preIdx >= 0 && preIdx < static_cast<int>(image_.preludeEntries.size())) {
                        const GfxAnimPreludeEntry& pe = image_.preludeEntries[preIdx];
                        if (pe.used) {
                            ImGui::TextDisabled("Prelude[%d] for frame %d: x_off=%d y_off=%d w=%d h=%d", preIdx,
                                                selectedFrame_, pe.xOffset, pe.yOffset, pe.width, pe.height);
                        }
                    }
                } else {
                    ImGui::Text("%dx%d  flags=0x%X", fr.width, fr.height, fr.flags);
                }
            }
            ImGui::EndTabItem();
        }
        if (isAnm_ && ImGui::BeginTabItem("Composed frames")) {
            ImGui::TextDisabled(
                "Runtime draw states (base + patches). Use Play + Flipbook to animate all frames here.");
            if (!anmCanvas_.valid) {
                ImGui::TextDisabled("No TV prelude canvas for this file.");
            } else {
                ImGui::TextDisabled("Canvas %dx%d (origin %d,%d)", anmCanvas_.width, anmCanvas_.height,
                                    anmCanvas_.minX, anmCanvas_.minY);
                if (unsigned int ct = composedTextureForFrame(selectedFrame_)) {
                    ImVec2 sz(anmCanvas_.width * zoom_, anmCanvas_.height * zoom_);
                    ImGui::Image(static_cast<ImTextureID>(ct), sz);
                    ImGui::TextDisabled("Frame %d / %d", selectedFrame_, n > 0 ? n - 1 : 0);
                }
                ImGui::Separator();
                float avail = ImGui::GetContentRegionAvail().x;
                float x = 0;
                for (int i = 0; i < static_cast<int>(composedTextures_.size()); ++i) {
                    unsigned int tex = composedTextures_[i];
                    if (!tex) continue;
                    ImVec2 sz(anmCanvas_.width * zoom_, anmCanvas_.height * zoom_);
                    const bool current = (i == selectedFrame_);
                    ui::SelectedFrameScope sel(current);
                    ImGui::BeginGroup();
                    if (ImGui::ImageButton(("cf" + std::to_string(i)).c_str(), static_cast<ImTextureID>(tex), sz))
                        selectedFrame_ = i;
                    ImGui::TextDisabled("%d", i);
                    ImGui::EndGroup();
                    x += sz.x + 12;
                    if (x + sz.x < avail) ImGui::SameLine();
                    else x = 0;
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(isAnm_ ? "Raw patches" : "All frames")) {
            if (isAnm_)
                ImGui::TextDisabled(
                    "Decoded bitmaps before TV-prelude composition — not what the game blits for N>0.");
            float avail = ImGui::GetContentRegionAvail().x;
            float x = 0;
            for (int i = 0; i < static_cast<int>(image_.frames.size()); ++i) {
                const GfxFrame& fr = image_.frames[i];
                unsigned int tex = textures_[i];
                if (!tex) continue;
                ImVec2 sz(fr.width * zoom_, fr.height * zoom_);
                ui::SelectedFrameScope sel(i == selectedFrame_);
                ImGui::BeginGroup();
                if (ImGui::ImageButton(("f" + std::to_string(i)).c_str(),
                                       static_cast<ImTextureID>(tex), sz))
                    selectedFrame_ = i;
                ImGui::TextDisabled("%d", i);
                ImGui::EndGroup();
                x += sz.x + 12;
                if (x + sz.x < avail) ImGui::SameLine();
                else x = 0;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Palette")) {
            if (ImGui::BeginTable("gfx_palette", 8, ImGuiTableFlags_SizingFixedSame)) {
                for (int i = 0; i < kGfxPaletteColors; ++i) {
                    if (i % 8 == 0) ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImVec4 col(image_.palette[i][0] / 255.0f, image_.palette[i][1] / 255.0f,
                               image_.palette[i][2] / 255.0f, 1.0f);
                    ImGui::PushID(i);
                    ImGui::ColorButton("##c", col, ImGuiColorEditFlags_NoTooltip,
                                       ImVec2(ui::Em(1.75f), ui::Em(1.75f)));
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (!isAnm_ && ImGui::BeginTabItem("Title Preview")) {
            drawTitleTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ui::EndDetailPanel();
}

}  // namespace mm2

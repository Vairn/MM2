#include "sections/GfxSection.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>

#include "imgui.h"
#include "widgets/Texture.h"
#include "widgets/UiLayout.h"

namespace fs = std::filesystem;

namespace mm2 {

GfxSection::~GfxSection() { releaseTextures(); }

void GfxSection::releaseTextures() {
    for (unsigned int t : textures_) freeTexture(t);
    textures_.clear();
}

bool GfxSection::load(const std::string& dataDir) {
    dir_ = dataDir;
    files_.clear();
    selectedFile_ = -1;
    selectedFrame_ = 0;
    releaseTextures();
    image_ = GfxImage{};

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
    loaded = !files_.empty();
    return loaded;
}

void GfxSection::selectFile(int idx) {
    if (idx < 0 || idx >= static_cast<int>(files_.size())) return;
    selectedFile_ = idx;
    selectedFrame_ = 0;
    releaseTextures();
    gfxLoad(dir_ + "/" + files_[idx], isAnm_, image_);
    if (image_.ok || !image_.frames.empty()) {
        textures_.reserve(image_.frames.size());
        for (auto& fr : image_.frames)
            textures_.push_back(makeTextureRGBA(fr.rgba.data(), fr.width, fr.height));
    }
}

void GfxSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ImGui::TextDisabled("No %s files found in the data folder.", ext_);
        return;
    }

    ui::BeginListPanel("gfx_files");
    for (int i = 0; i < static_cast<int>(files_.size()); ++i) {
        if (ImGui::Selectable(files_[i].c_str(), selectedFile_ == i)) selectFile(i);
    }
    ui::ListPanelNextDetail("gfx_view");
    if (selectedFile_ < 0) {
        ImGui::TextDisabled("Select a file.");
        ui::EndDetailPanel();
        return;
    }

    ImGui::Text("%s", files_[selectedFile_].c_str());
    if (!image_.ok && image_.frames.empty()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "Decode failed: %s", image_.error.c_str());
        ui::EndDetailPanel();
        return;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("frames=%d depth=%d  chunk@0x%zX%s", image_.frameCount, image_.depth,
                        image_.chunkOffset,
                        image_.error.empty() ? "" : "  (partial)");

    ui::SetFieldMed();
    ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 8.0f, "%.0fx");

    if (ImGui::BeginTabBar("gfx_tabs")) {
        if (ImGui::BeginTabItem("Frames")) {
            int n = static_cast<int>(image_.frames.size());
            ImGui::SetNextItemWidth(160);
            ImGui::SliderInt("Frame", &selectedFrame_, 0, n > 0 ? n - 1 : 0);
            if (selectedFrame_ >= 0 && selectedFrame_ < n) {
                const GfxFrame& fr = image_.frames[selectedFrame_];
                ImGui::Text("%dx%d  flags=0x%X", fr.width, fr.height, fr.flags);
                unsigned int tex = textures_[selectedFrame_];
                if (tex) {
                    ImVec2 sz(fr.width * zoom_, fr.height * zoom_);
                    ImGui::Image(static_cast<ImTextureID>(tex), sz);
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("All frames")) {
            float avail = ImGui::GetContentRegionAvail().x;
            float x = 0;
            for (int i = 0; i < static_cast<int>(image_.frames.size()); ++i) {
                const GfxFrame& fr = image_.frames[i];
                unsigned int tex = textures_[i];
                if (!tex) continue;
                ImVec2 sz(fr.width * zoom_, fr.height * zoom_);
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
                    ImGui::ColorButton("##c", col, ImGuiColorEditFlags_NoTooltip, ImVec2(28, 28));
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ui::EndDetailPanel();
}

}  // namespace mm2

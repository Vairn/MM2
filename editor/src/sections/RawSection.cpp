#include "sections/RawSection.h"

#include "imgui.h"
#include "widgets/HexView.h"

namespace mm2 {

bool RawSection::load(const std::string& dataDir) {
    loaded = file_.load(std::string(dataDir) + "/" + fileName_);
    dirty = false;
    return loaded;
}

bool RawSection::save(const std::string& dataDir) {
    bool ok = file_.save(std::string(dataDir) + "/" + fileName_);
    if (ok) dirty = false;
    return ok;
}

void RawSection::draw(App& app) {
    (void)app;
    if (!loaded) {
        ImGui::TextDisabled("%s not loaded.", fileName_);
        return;
    }
    ImGui::TextDisabled("%zu bytes  -  layout not yet reverse-engineered.", file_.data.size());
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ImGui::TextWrapped("%s", note_);
    ImGui::PopTextWrapPos();

    ImGui::SeparatorText("Raw bytes");
    DrawHexView("raw_hex", file_.data.data(), file_.data.size(), 0);
}

}  // namespace mm2

#include "sections/eventgraph/EventGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

#include "app/App.h"
#include "core/AreaNames.h"
#include "eventlang/Semantics.h"
#include "imnodes.h"
#include "widgets/UiLayout.h"
#include "widgets/UiTheme.h"

namespace mm2 {
namespace {

constexpr float kNodeGapX = 36.f;     // gap between sequential nodes
constexpr float kBranchGapY = 24.f;   // extra gap below then-row for else (plus node h)
constexpr float kChainGapY = 56.f;    // vertical gap between script chains
constexpr float kTriggerGapX = 28.f;  // gap between trigger and header
constexpr float kTextWrap = 180.f;    // subtitle wrap / estimate
constexpr float kMinNodeW = 140.f;
constexpr float kMaxNodeW = 260.f;    // hard cap for layout advance (stops grow-loop)
constexpr float kInlineFieldW = 168.f;  // fixed editor width inside nodes
constexpr float kMaxNodeH = 220.f;

int pinIn(int nodeId) { return nodeId * 10 + 1; }
int pinOut(int nodeId) { return nodeId * 10 + 2; }
int pinElse(int nodeId) { return nodeId * 10 + 3; }

bool pathEqual(const StmtPath& a, const StmtPath& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].list != b[i].list || a[i].index != b[i].index) return false;
    }
    return true;
}

ImVec2 estimateNodeSize(const GraphNode& n) {
    const float padX = 18.f;
    const float padY = 10.f;
    const float titleBarExtra = 6.f;

    ImVec2 titleSz = ImGui::CalcTextSize(n.title.c_str());
    float w = titleSz.x;
    float h = titleSz.y + titleBarExtra;

    // Input pin row.
    h += 10.f;

    if (!n.subtitle.empty()) {
        ImVec2 subSz = ImGui::CalcTextSize(n.subtitle.c_str(), nullptr, false, kTextWrap);
        w = std::max(w, std::min(subSz.x, kTextWrap));
        h += subSz.y + 4.f;
    } else {
        w = std::max(w, 160.f);
        h += 8.f;
    }

    if (n.overlayLoc >= 0 && n.overlayEvent >= 0) h += ImGui::GetTextLineHeight() + 4.f;

    if (n.isIf)
        h += ImGui::GetTextLineHeight() * 2.f + 8.f;
    else
        h += 12.f;  // output pin dummy

    w = std::clamp(w + padX * 2.f, kMinNodeW, kMaxNodeW);
    h += padY * 2.f;
    return ImVec2(w, h);
}

float nodeAdvanceX(const GraphNode& n) {
    const float w = std::clamp(n.size.x, kMinNodeW, kMaxNodeW);
    return w + kNodeGapX;
}

void clampNodeSize(GraphNode& n) {
    n.size.x = std::clamp(n.size.x, kMinNodeW, kMaxNodeW);
    n.size.y = std::clamp(n.size.y, 40.f, kMaxNodeH);
}

std::string formatExpr(const eventlang::Expr& c, App* app);  // defined below

eventlang::Script* scriptForEvent(eventlang::Location& loc, int eventId) {
    for (auto& s : loc.scripts)
        if (s.eventId == eventId) return &s;
    return nullptr;
}

bool inlineItemCombo(const char* id, App& app, int* itemId) {
    char preview[64];
    std::snprintf(preview, sizeof(preview), "0x%02X %s", *itemId & 0xFF,
                  app.itemName(*itemId).c_str());
    bool changed = false;
    ImGui::SetNextItemWidth(kInlineFieldW);
    if (ImGui::BeginCombo(id, preview, ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < 256; ++i) {
            char label[72];
            std::snprintf(label, sizeof(label), "0x%02X %s", i, app.itemName(i).c_str());
            if (ImGui::Selectable(label, i == *itemId)) {
                *itemId = i;
                changed = true;
            }
            if (i == *itemId) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool inlineStringCombo(const char* id, eventlang::Location& loc, eventlang::Stmt& st) {
    std::string cur = st.getStr("string");
    if (cur.empty() && st.num.count("string"))
        cur = "s" + std::to_string(st.getNum("string"));
    bool changed = false;
    ImGui::SetNextItemWidth(kInlineFieldW);
    if (ImGui::BeginCombo(id, cur.empty() ? "(none)" : cur.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (const auto& sd : loc.strings) {
            const char* name = sd.name.empty() ? "?" : sd.name.c_str();
            char label[160];
            std::snprintf(label, sizeof(label), "%s  \"%.32s%s\"", name, sd.text.c_str(),
                          sd.text.size() > 32 ? "…" : "");
            if (ImGui::Selectable(label, sd.name == cur)) {
                st.str["string"] = sd.name;
                st.num.erase("string");
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

/** Compact field editors drawn inside a selected graph node. Returns true if mutated. */
bool drawInlineNodeEditor(App& app, eventlang::Location& loc, GraphNode& n) {
    if (n.category == GraphNodeCategory::Header || n.category == GraphNodeCategory::Trigger)
        return false;
    eventlang::Script* sc = scriptForEvent(loc, n.eventId);
    if (!sc) return false;
    eventlang::Stmt* st = EventGraph::resolveStmt(*sc, n.path);
    if (!st) return false;

    bool mutated = false;
    ImGui::PushID(n.id);
    // Fixed width — never derive from measured node size (that feedback-loops).
    ImGui::PushItemWidth(kInlineFieldW);

    if (st->kind == "say" || st->kind == "plain_text" || st->kind == "service_title") {
        if (st->kind == "say") {
            const char* variants[] = {"", "door", "block", "popup_a", "popup_b", "basic"};
            const char* labels[] = {"say", "door", "block", "popup_a", "popup_b", "basic"};
            int cur = 0;
            const std::string v = st->getStr("variant");
            for (int i = 0; i < 6; ++i)
                if (v == variants[i]) cur = i;
            if (ImGui::BeginCombo("##var", labels[cur])) {
                for (int i = 0; i < 6; ++i)
                    if (ImGui::Selectable(labels[i], i == cur)) {
                        st->str["variant"] = variants[i];
                        mutated = true;
                    }
                ImGui::EndCombo();
            }
        }
        if (inlineStringCombo("##str", loc, *st)) mutated = true;
    } else if (st->kind == "give_item") {
        int item = st->getNum("item");
        if (inlineItemCombo("##gi", app, &item)) {
            st->set("item", item);
            mutated = true;
        }
    } else if (st->kind == "if" || st->kind == "set_cond") {
        ImGui::TextDisabled("%s", formatExpr(st->cond, &app).c_str());
        if (st->cond.kind == "gold_at_least" || st->cond.kind == "gems_at_least") {
            int amt = st->cond.getNum("amount", st->cond.getNum("value"));
            if (ImGui::InputInt("##amt", &amt, 1, 10)) {
                if (amt < 0) amt = 0;
                st->cond.set("amount", amt);
                mutated = true;
            }
        } else if (st->cond.kind == "consume_item" || st->cond.kind == "has_item_id" ||
                   st->cond.kind == "has_item" || st->cond.kind == "party_has_item" ||
                   st->cond.kind == "give_item_ok") {
            int item = st->cond.getNum("item", st->cond.getNum("b"));
            if (inlineItemCombo("##ci", app, &item)) {
                st->cond.set("item", item);
                st->cond.set("b", item);
                mutated = true;
            }
        } else if (st->cond.kind == "yes_no") {
            bool m = st->cond.getNum("mode") != 0;
            if (ImGui::Checkbox("mode 1##yn", &m)) {
                st->cond.set("mode", m ? 1 : 0);
                mutated = true;
            }
        }
    } else if (st->kind == "ask_yes_no" || st->kind == "wait") {
        bool m = st->getNum("mode") != 0;
        if (ImGui::Checkbox("mode 1##wm", &m)) {
            st->set("mode", m ? 1 : 0);
            mutated = true;
        }
    } else if (st->kind == "go_to") {
        int screen = st->getNum("screen");
        int pos = st->getNum("pos");
        ImGui::SetNextItemWidth(70.f);
        if (ImGui::InputInt("##scr", &screen, 1, 1)) {
            st->set("screen", screen);
            mutated = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.f);
        if (ImGui::InputInt("##pos", &pos, 1, 1)) {
            st->set("pos", pos);
            mutated = true;
        }
        const char* an = areaNameRaw(screen);
        if (an && an[0]) ImGui::TextDisabled("%s", an);
    } else if (st->kind == "fight" || st->kind == "fight_b") {
        auto* list = &st->lists[st->kind == "fight_b" ? "data" : "monsters"];
        if (list->empty() && st->kind == "fight_b") list = &st->lists["monsters"];
        // Edit first few non-trailing slots compactly.
        int shown = 0;
        for (size_t i = 0; i < list->size() && shown < 4; ++i) {
            int mid = (*list)[i];
            if (mid == 0 && i + 1 < list->size() && (*list)[i + 1] == 0) break;
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(56.f);
            if (ImGui::InputInt("##m", &mid, 0, 0)) {
                if (mid < 0) mid = 0;
                if (mid > 255) mid = 255;
                (*list)[i] = mid;
                mutated = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%.18s", mid ? app.monsterName(mid).c_str() : "—");
            ImGui::PopID();
            ++shown;
        }
        if (st->kind == "fight") {
            auto fit = st->lists.find("flags");
            if (fit != st->lists.end() && fit->second.size() >= 2) {
                int ov = fit->second[0], lc = fit->second[1];
                ImGui::SetNextItemWidth(48.f);
                if (ImGui::InputInt("##ov", &ov, 0, 0)) {
                    fit->second[0] = ov & 0xFF;
                    mutated = true;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(48.f);
                if (ImGui::InputInt("##lc", &lc, 0, 0)) {
                    fit->second[1] = lc & 0xFF;
                    mutated = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("overflow");
            }
        }
    } else if (st->kind == "shop" || st->kind == "quest" || st->kind == "selector") {
        int sel = st->getNum("value");
        if (!sel) sel = st->getNum("selector");
        if (!sel && st->kind != "selector")
            sel = eventlang::selectorByShopOrQuest(st->kind, st->getStr("name"));
        if (ImGui::InputInt("##sel", &sel, 1, 1)) {
            st->set("value", sel & 0xFF);
            mutated = true;
        }
        ImGui::TextDisabled("%s", eventlang::formatSelectorSummary(sel).c_str());
    } else if (st->kind == "overlay") {
        int ol = st->getNum("loc", st->getNum("overlay"));
        int oe = st->getNum("event");
        ImGui::SetNextItemWidth(56.f);
        if (ImGui::InputInt("##ol", &ol, 1, 1)) {
            st->set("loc", ol);
            mutated = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(56.f);
        if (ImGui::InputInt("##oe", &oe, 1, 1)) {
            st->set("event", oe);
            mutated = true;
        }
    } else if (st->kind == "skip_if_true" || st->kind == "skip_if_false" ||
               st->kind == "skip_if_victory") {
        int nn = st->getNum("n");
        if (ImGui::InputInt("##skip", &nn, 1, 1)) {
            st->set("n", nn);
            mutated = true;
        }
    } else if (!n.subtitle.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + kTextWrap);
        ImGui::TextWrapped("%s", n.subtitle.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::PopItemWidth();
    ImGui::PopID();

    if (mutated) {
        describeStmt(*st, loc, &app, &n.title, &n.subtitle);
        if (st->kind == "selector" || st->kind == "shop" || st->kind == "quest") {
            int sel = st->getNum("value");
            if (!sel) sel = st->getNum("selector");
            if (!sel && st->kind != "selector")
                sel = eventlang::selectorByShopOrQuest(st->kind, st->getStr("name"));
            if (auto bin = eventlang::binExecSelector(sel)) {
                n.overlayLoc = bin->first;
                n.overlayEvent = bin->second;
            } else {
                n.overlayLoc = n.overlayEvent = -1;
            }
        } else if (st->kind == "overlay") {
            n.overlayLoc = st->getNum("loc", st->getNum("overlay"));
            n.overlayEvent = st->getNum("event");
        }
    }
    return mutated;
}

std::string resolveStringPreview(const eventlang::Location& loc, const std::string& ref,
                                 int maxLen = 48, bool withQuotes = true) {
    auto trimText = [&](std::string t) {
        // Collapse newlines / excess spaces for node labels.
        for (char& c : t) {
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        }
        std::string out;
        out.reserve(t.size());
        bool sp = false;
        for (char c : t) {
            if (c == ' ') {
                if (!sp) out.push_back(' ');
                sp = true;
            } else {
                out.push_back(c);
                sp = false;
            }
        }
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back() == ' ') out.pop_back();
        if (static_cast<int>(out.size()) > maxLen) out = out.substr(0, maxLen - 3) + "...";
        if (withQuotes) return "\"" + out + "\"";
        return out;
    };

    auto fromDef = [&](const eventlang::StringDef& sd) -> std::string {
        return trimText(sd.text);
    };

    if (ref.empty()) return {};
    for (const auto& sd : loc.strings) {
        if (sd.name == ref) return fromDef(sd);
    }
    if (!ref.empty() && ref[0] >= '0' && ref[0] <= '9') {
        const int idx = std::atoi(ref.c_str());
        for (const auto& sd : loc.strings) {
            if (sd.index == idx) return fromDef(sd);
        }
    }
    if (ref.size() >= 2 && (ref[0] == 's' || ref[0] == 'S')) {
        bool digits = true;
        for (size_t i = 1; i < ref.size(); ++i)
            if (ref[i] < '0' || ref[i] > '9') digits = false;
        if (digits) {
            const int idx = std::atoi(ref.c_str() + 1);
            for (const auto& sd : loc.strings) {
                if (sd.index == idx || sd.name == ref) return fromDef(sd);
            }
        }
    }
    return ref;
}

std::string stmtStringRef(const eventlang::Stmt& st) {
    std::string ref = st.getStr("string");
    if (ref.empty() && st.num.count("string")) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "s%d", st.getNum("string"));
        return buf;
    }
    return ref;
}

std::string itemLabel(App* app, int id) {
    char hex[16];
    std::snprintf(hex, sizeof(hex), "0x%02X", id & 0xFF);
    if (!app) return hex;
    std::string nm = app->itemName(id);
    if (nm.empty() || nm[0] == '#') return hex;
    return nm;
}

std::string monsterLabel(App* app, int id) {
    char hex[16];
    std::snprintf(hex, sizeof(hex), "#%d", id & 0xFF);
    if (!app) return hex;
    std::string nm = app->monsterName(id);
    if (nm.empty()) return hex;
    return nm;
}

/** Group non-zero monster slot ids into "N× Name" fragments (order preserved). */
std::string formatSpawnGroups(const std::vector<int>& slots, App* app, int maxGroups = 6) {
    struct Group {
        int id;
        int n;
    };
    std::vector<Group> groups;
    for (int id : slots) {
        if (id == 0) continue;
        if (!groups.empty() && groups.back().id == id)
            ++groups.back().n;
        else
            groups.push_back({id, 1});
    }
    if (groups.empty()) return "(empty)";

    std::string out;
    const int show = std::min(static_cast<int>(groups.size()), maxGroups);
    for (int i = 0; i < show; ++i) {
        if (!out.empty()) out += ", ";
        const auto& g = groups[static_cast<size_t>(i)];
        if (g.n > 1) out += std::to_string(g.n) + "× ";
        out += monsterLabel(app, g.id);
    }
    if (static_cast<int>(groups.size()) > maxGroups)
        out += " +" + std::to_string(static_cast<int>(groups.size()) - maxGroups) + " types";
    return out;
}

std::vector<int> fightMonsterSlots(const eventlang::Stmt& st) {
    if (st.kind == "fight") {
        auto it = st.lists.find("monsters");
        if (it != st.lists.end()) return it->second;
    } else if (st.kind == "fight_b") {
        auto it = st.lists.find("data");
        if (it != st.lists.end()) return it->second;
        it = st.lists.find("monsters");
        if (it != st.lists.end()) return it->second;
    }
    return {};
}

std::string formatExpr(const eventlang::Expr& c, App* app) {
    if (c.kind == "gold_at_least")
        return "gold ≥ " + std::to_string(c.getNum("amount"));
    if (c.kind == "gems_at_least" || c.kind == "code16")
        return "gems ≥ " + std::to_string(c.getNum("amount", c.getNum("value")));
    if (c.kind == "yes_no") return c.getNum("mode") ? "ask Yes/No (mode 1)" : "ask Yes/No";
    if (c.kind == "answer_eq") return "answer is \"" + c.getStr("text") + "\"";
    if (c.kind == "combat_victory") return "won the fight";
    if (c.kind == "prior_cond") return "previous condition";
    if (c.kind == "day_odd") return "odd day";
    if (c.kind == "day_even") return "even day";
    if (c.kind == "day_in")
        return "day " + std::to_string(c.getNum("lo")) + "–" + std::to_string(c.getNum("hi"));
    if (c.kind == "era_in")
        return "era " + std::to_string(c.getNum("lo")) + "–" + std::to_string(c.getNum("hi"));
    if (c.kind == "rng_roll") return "random ≤ " + std::to_string(c.getNum("max"));
    if (c.kind == "threshold") {
        char b[32];
        std::snprintf(b, sizeof(b), "threshold 0x%02X", c.getNum("value"));
        return b;
    }
    if (c.kind == "class_is") return "class is " + c.getStr("class");
    if (c.kind == "consume_item" || c.kind == "has_item_id" || c.kind == "has_item" ||
        c.kind == "give_item_ok" || c.kind == "party_has_item") {
        const int id = c.getNum("item", c.getNum("b"));
        const std::string nm = itemLabel(app, id);
        if (c.kind == "consume_item") return "use/consume " + nm;
        if (c.kind == "give_item_ok") return "can give " + nm;
        return "has " + nm;
    }
    if (c.kind == "party_bits") {
        char b[48];
        std::snprintf(b, sizeof(b), "party bits m%d f%d mask 0x%02X", c.getNum("member"),
                      c.getNum("field"), c.getNum("mask"));
        return b;
    }
    if (c.kind == "member_attr") {
        char b[40];
        std::snprintf(b, sizeof(b), "member attr 0x%02X / 0x%02X", c.getNum("arg1"),
                      c.getNum("arg2"));
        return b;
    }
    if (c.kind == "load_var8") {
        return "var[" + std::to_string(c.getNum("group")) + "][" +
               std::to_string(c.getNum("index")) + "]";
    }
    if (c.kind == "count_title_nibble") {
        char b[32];
        std::snprintf(b, sizeof(b), "title nibble 0x%02X", c.getNum("id"));
        return b;
    }
    if (c.kind == "party_effect_ok") return "party effect ok";
    if (c.kind == "raw_op") {
        char b[24];
        std::snprintf(b, sizeof(b), "raw op 0x%02X", c.getNum("op"));
        return b;
    }
    if (c.kind == "unknown") {
        const std::string t = c.getStr("text");
        return t.empty() ? "unknown" : t;
    }
    if (c.kind.empty()) return "?";
    return c.kind;
}

void applyEditorMouseWheelZoom() {
    if (!ImNodes::IsEditorHovered()) return;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel == 0.0f) return;
    constexpr float kStep = 0.1f;
    constexpr float kMinZoom = 0.25f;
    constexpr float kMaxZoom = 2.5f;
    const float zoom =
        std::clamp(ImNodes::EditorContextGetZoom() + wheel * kStep, kMinZoom, kMaxZoom);
    ImNodes::EditorContextSetZoom(zoom, ImGui::GetMousePos());
}

}  // namespace

GraphNodeCategory categorizeStmt(const std::string& kind) {
    if (kind == "say" || kind == "service_title" || kind == "plain_text" || kind == "ask_yes_no" ||
        kind == "read_answer" || kind == "wait")
        return GraphNodeCategory::Dialogue;
    if (kind == "give_item" || kind == "set_cond" || kind == "set_party_bits" ||
        kind == "apply_party" || kind == "apply_party_masked" || kind == "party_effect" ||
        kind == "party_damage" || kind == "treasure" || kind == "or_member_field")
        return GraphNodeCategory::Economy;
    if (kind == "fight" || kind == "fight_b") return GraphNodeCategory::Combat;
    if (kind == "if" || kind == "skip_if_true" || kind == "skip_if_false" ||
        kind == "skip_if_victory" || kind == "end" || kind == "abort" || kind == "clear_tile_event")
        return GraphNodeCategory::Control;
    if (kind == "go_to" || kind == "selector" || kind == "shop" || kind == "quest" ||
        kind == "set_tile" || kind == "overlay")
        return GraphNodeCategory::World;
    return GraphNodeCategory::Other;
}

ImU32 categoryTitleColor(GraphNodeCategory c) {
    switch (c) {
        case GraphNodeCategory::Dialogue: return IM_COL32(40, 110, 70, 255);
        case GraphNodeCategory::Economy: return IM_COL32(90, 55, 130, 255);
        case GraphNodeCategory::Combat: return IM_COL32(140, 70, 40, 255);
        case GraphNodeCategory::Control: return IM_COL32(130, 90, 30, 255);
        case GraphNodeCategory::World: return IM_COL32(40, 70, 130, 255);
        case GraphNodeCategory::Trigger: return IM_COL32(50, 55, 40, 255);
        case GraphNodeCategory::Header: return IM_COL32(70, 40, 120, 255);
        default: return IM_COL32(70, 30, 30, 255);
    }
}

ImU32 categoryBgColor(GraphNodeCategory c) {
    switch (c) {
        case GraphNodeCategory::Dialogue: return IM_COL32(18, 40, 28, 255);
        case GraphNodeCategory::Economy: return IM_COL32(32, 20, 45, 255);
        case GraphNodeCategory::Combat: return IM_COL32(45, 24, 16, 255);
        case GraphNodeCategory::Control: return IM_COL32(42, 32, 14, 255);
        case GraphNodeCategory::World: return IM_COL32(16, 28, 48, 255);
        case GraphNodeCategory::Trigger: return IM_COL32(20, 26, 16, 255);
        case GraphNodeCategory::Header: return IM_COL32(26, 16, 42, 255);
        default: return IM_COL32(28, 12, 12, 255);
    }
}

void describeFightEncounter(const eventlang::Stmt& st, App* app, std::string* titleOut,
                            std::string* detailOut) {
    const bool randomPool = (st.kind == "fight_b");
    const std::vector<int> slots = fightMonsterSlots(st);

    int filled = 0;
    for (int id : slots)
        if (id != 0) ++filled;

    std::string title = randomPool ? "Random pool" : "Fixed fight";
    std::string detail = formatSpawnGroups(slots, app);

    if (!randomPool) {
        // OP_12 tail: flags[0] = overflow_type, flags[1] = live_count.
        auto fit = st.lists.find("flags");
        if (fit != st.lists.end() && fit->second.size() >= 2) {
            const int overflowType = fit->second[0];
            const int liveCount = fit->second[1];
            if (overflowType != 0 && liveCount > 0) {
                detail += " + ";
                detail += std::to_string(liveCount);
                detail += "× ";
                detail += monsterLabel(app, overflowType);
                detail += " (overflow)";
            }
        }
        if (filled == 0 && detail == "(empty)") detail = "no monsters";
    } else {
        if (filled == 0)
            detail = "empty pool (seeded random)";
        else
            detail = "pool: " + detail;
    }

    if (titleOut) *titleOut = title;
    if (detailOut) *detailOut = detail;
}

void describeStmt(const eventlang::Stmt& st, const eventlang::Location& loc, App* app,
                  std::string* titleOut, std::string* subtitleOut) {
    std::string title = st.kind.empty() ? "?" : st.kind;
    std::string sub;

    if (st.kind == "say") {
        const std::string v = st.getStr("variant");
        if (v == "door")
            title = "Door sign";
        else if (v == "block")
            title = "Blocked text";
        else if (v == "popup_a" || v == "popup_b")
            title = "Popup";
        else if (v == "basic")
            title = "Say (basic)";
        else
            title = "Say";
        sub = resolveStringPreview(loc, stmtStringRef(st), 56);
    } else if (st.kind == "plain_text") {
        title = "Plain text";
        sub = resolveStringPreview(loc, stmtStringRef(st), 56);
    } else if (st.kind == "service_title") {
        title = "Service title";
        sub = resolveStringPreview(loc, stmtStringRef(st), 40);
        if (sub.empty()) {
            char b[48];
            std::snprintf(b, sizeof(b), "sign 0x%02X", st.getNum("sign", st.getNum("string")));
            sub = b;
        }
    } else if (st.kind == "give_item") {
        title = "Give item";
        sub = itemLabel(app, st.getNum("item"));
    } else if (st.kind == "set_cond") {
        title = "Set condition";
        sub = formatExpr(st.cond, app);
    } else if (st.kind == "if") {
        title = "If";
        sub = formatExpr(st.cond, app);
    } else if (st.kind == "ask_yes_no") {
        title = "Ask Yes / No";
        sub = st.getNum("mode") ? "mode 1" : "";
    } else if (st.kind == "read_answer") {
        title = "Read answer";
    } else if (st.kind == "skip_if_true") {
        title = "Skip if true";
        sub = std::to_string(st.getNum("n")) + " tokens";
    } else if (st.kind == "skip_if_false") {
        title = "Skip if false";
        sub = std::to_string(st.getNum("n")) + " tokens";
    } else if (st.kind == "skip_if_victory") {
        title = "Skip if victory";
        sub = std::to_string(st.getNum("n")) + " tokens";
    } else if (st.kind == "go_to") {
        title = "Go to map";
        const int screen = st.getNum("screen");
        const char* an = areaNameRaw(screen);
        char buf[72];
        if (an && an[0])
            std::snprintf(buf, sizeof(buf), "%s  (#%d)", an, screen);
        else
            std::snprintf(buf, sizeof(buf), "screen #%d", screen);
        sub = buf;
    } else if (st.kind == "fight" || st.kind == "fight_b") {
        describeFightEncounter(st, app, &title, &sub);
    } else if (st.kind == "wait") {
        title = "Wait for space";
        if (st.getNum("mode")) sub = "mode 1";
    } else if (st.kind == "end") {
        title = "End script";
    } else if (st.kind == "abort") {
        title = "Abort";
    } else if (st.kind == "clear_tile_event") {
        title = "Clear tile event";
    } else if (st.kind == "set_tile") {
        title = "Set tile";
        char b[48];
        std::snprintf(b, sizeof(b), "(%d,%d) → 0x%02X", st.getNum("y", st.getNum("row")),
                      st.getNum("x", st.getNum("col")), st.getNum("tile", st.getNum("value")));
        sub = b;
    } else if (st.kind == "treasure") {
        title = "Treasure";
        char b[48];
        std::snprintf(b, sizeof(b), "code 0x%02X", st.getNum("code", st.getNum("value")));
        sub = b;
    } else if (st.kind == "party_damage") {
        title = "Party damage";
        sub = std::to_string(st.getNum("amount", st.getNum("value")));
    } else if (st.kind == "party_effect" || st.kind == "apply_party" ||
               st.kind == "apply_party_masked") {
        title = "Party effect";
    } else if (st.kind == "set_party_bits" || st.kind == "or_member_field") {
        title = "Set party bits";
    } else if (st.kind == "selector" || st.kind == "shop" || st.kind == "quest" ||
               st.kind == "overlay") {
        int sel = st.getNum("value");
        if (!sel) sel = st.getNum("selector");
        if (!sel && st.kind != "selector" && st.kind != "overlay")
            sel = eventlang::selectorByShopOrQuest(st.kind, st.getStr("name"));
        if (st.kind == "shop")
            title = "Open shop";
        else if (st.kind == "quest")
            title = "Quest";
        else if (st.kind == "overlay")
            title = "Overlay jump";
        else
            title = "Selector";
        if (st.kind == "overlay") {
            char b[48];
            std::snprintf(b, sizeof(b), "loc %d · event_%02d",
                          st.getNum("loc", st.getNum("overlay")), st.getNum("event"));
            sub = b;
        } else {
            sub = eventlang::formatSelectorSummary(sel);
            if (!st.getStr("name").empty() && st.kind != "selector")
                sub = st.getStr("name") + " · " + sub;
        }
    } else if (st.kind == "raw_op") {
        char b[32];
        std::snprintf(b, sizeof(b), "Raw op 0x%02X", st.getNum("op"));
        title = b;
    } else if (st.kind == "unlifted") {
        title = "Unlifted block";
        sub = std::to_string(st.body.size()) + " ops";
    } else {
        title = st.kind.empty() ? "?" : st.kind;
    }

    if (titleOut) *titleOut = title;
    if (subtitleOut) *subtitleOut = sub;
}

void summarizeScript(const eventlang::Script& sc, const eventlang::Location& loc, App* app,
                     std::string* titleOut, std::string* subtitleOut) {
    char idBuf[24];
    std::snprintf(idBuf, sizeof(idBuf), "event_%02d", sc.eventId);

    int opCount = 0;
    std::string bestTitle;
    std::string bestKind;
    std::string firstKinds;

    std::function<void(const std::vector<eventlang::Stmt>&, int)> walk;
    walk = [&](const std::vector<eventlang::Stmt>& stmts, int depth) {
        for (const auto& st : stmts) {
            ++opCount;
            if (depth == 0) {
                if (!firstKinds.empty()) firstKinds += " → ";
                std::string t, s;
                describeStmt(st, loc, app, &t, &s);
                firstKinds += t;
                if (firstKinds.size() > 56) {
                    firstKinds = firstKinds.substr(0, 53) + "...";
                }
            }

            // Prefer dialogue / world destinations as the header title.
            if (bestTitle.empty()) {
                if (st.kind == "say" || st.kind == "plain_text" || st.kind == "service_title") {
                    bestTitle = resolveStringPreview(loc, stmtStringRef(st), 36, false);
                    bestKind = (st.kind == "say")
                                   ? (st.getStr("variant").empty() ? "say" : st.getStr("variant"))
                                   : st.kind;
                } else if (st.kind == "shop" || st.kind == "quest" || st.kind == "selector") {
                    int sel = st.getNum("value");
                    if (!sel) sel = st.getNum("selector");
                    if (!sel && st.kind != "selector")
                        sel = eventlang::selectorByShopOrQuest(st.kind, st.getStr("name"));
                    bestTitle = eventlang::formatSelectorSummary(sel);
                    if (!st.getStr("name").empty()) bestTitle = st.getStr("name");
                    bestKind = st.kind;
                } else if (st.kind == "overlay") {
                    char b[48];
                    std::snprintf(b, sizeof(b), "→ loc %d event_%02d",
                                  st.getNum("loc", st.getNum("overlay")), st.getNum("event"));
                    bestTitle = b;
                    bestKind = "overlay";
                } else if (st.kind == "go_to") {
                    const int screen = st.getNum("screen");
                    const char* an = areaNameRaw(screen);
                    bestTitle = (an && an[0]) ? an : ("screen #" + std::to_string(screen));
                    bestKind = "travel";
                } else if (st.kind == "fight" || st.kind == "fight_b") {
                    std::string t, s;
                    describeFightEncounter(st, app, &t, &s);
                    bestTitle = s.empty() ? t : s;
                    bestKind = (st.kind == "fight_b") ? "random fight" : "fixed fight";
                } else if (st.kind == "give_item") {
                    bestTitle = "Give " + itemLabel(app, st.getNum("item"));
                    bestKind = "reward";
                } else if (st.kind == "if") {
                    // Peek into then-body for a better label before settling on the condition.
                    walk(st.thenBody, depth + 1);
                    if (bestTitle.empty()) {
                        bestTitle = "If " + formatExpr(st.cond, app);
                        bestKind = "branch";
                    }
                    walk(st.elseBody, depth + 1);
                    continue;
                }
            }

            if (st.kind == "if") {
                walk(st.thenBody, depth + 1);
                walk(st.elseBody, depth + 1);
            } else if (st.kind == "unlifted") {
                walk(st.body, depth + 1);
            }
        }
    };
    walk(sc.body, 0);

    // Triggers that fire this script — append as context.
    std::string trigHint;
    int trigN = 0;
    for (const auto& t : loc.triggers) {
        if (t.eventId != sc.eventId) continue;
        ++trigN;
        if (trigHint.empty()) {
            char b[48];
            std::snprintf(b, sizeof(b), "(%d,%d) %s", t.y, t.x, eventlang::triggerCondName(t.cond));
            trigHint = b;
        }
    }
    if (trigN > 1) trigHint += " +" + std::to_string(trigN - 1);

    std::string title;
    std::string sub;
    if (!bestTitle.empty()) {
        title = bestTitle;
        sub = idBuf;
        if (!bestKind.empty()) {
            sub += " · ";
            sub += bestKind;
        }
        if (opCount > 0) {
            char nbuf[24];
            std::snprintf(nbuf, sizeof(nbuf), " · %d op%s", opCount, opCount == 1 ? "" : "s");
            sub += nbuf;
        }
        if (!trigHint.empty()) {
            sub += " · ";
            sub += trigHint;
        }
    } else if (opCount == 0) {
        title = idBuf;
        sub = "empty script";
        if (!trigHint.empty()) sub += " · " + trigHint;
    } else {
        title = idBuf;
        sub = firstKinds.empty() ? (std::to_string(opCount) + " opcodes") : firstKinds;
        if (!trigHint.empty()) {
            sub += " · ";
            sub += trigHint;
        }
    }

    if (titleOut) *titleOut = title;
    if (subtitleOut) *subtitleOut = sub;
}

void EventGraph::clear() {
    nodes.clear();
    links.clear();
    selectedNodeId = -1;
    builtForEvent = -1;
    builtForLoc = -1;
    lastFocusedEvent = -1;
    layoutDirty = true;
    layoutRefinePasses = 0;
    nextLinkId = 1;
}

eventlang::Stmt* EventGraph::resolveStmt(eventlang::Script& sc, const StmtPath& path) {
    if (path.empty()) return nullptr;
    eventlang::Stmt* cur = nullptr;
    std::vector<eventlang::Stmt>* list = &sc.body;
    for (const StmtPathElem& e : path) {
        if (e.list == StmtListKind::Then) {
            if (!cur || cur->kind != "if") return nullptr;
            list = &cur->thenBody;
        } else if (e.list == StmtListKind::Else) {
            if (!cur || cur->kind != "if") return nullptr;
            list = &cur->elseBody;
        } else {
            if (cur != nullptr) return nullptr;
            list = &sc.body;
        }
        if (!list || e.index < 0 || e.index >= static_cast<int>(list->size())) return nullptr;
        cur = &(*list)[e.index];
    }
    return cur;
}

const eventlang::Stmt* EventGraph::resolveStmt(const eventlang::Script& sc, const StmtPath& path) {
    return resolveStmt(const_cast<eventlang::Script&>(sc), path);
}

bool EventGraph::insertAfter(eventlang::Script& sc, const StmtPath& path,
                             const eventlang::Stmt& stub) {
    if (path.empty()) return false;
    StmtPath parentPath(path.begin(), path.end() - 1);
    const StmtPathElem last = path.back();

    std::vector<eventlang::Stmt>* list = nullptr;
    if (parentPath.empty()) {
        list = &sc.body;
    } else {
        eventlang::Stmt* parent = resolveStmt(sc, parentPath);
        if (!parent || parent->kind != "if") return false;
        if (last.list == StmtListKind::Then)
            list = &parent->thenBody;
        else if (last.list == StmtListKind::Else)
            list = &parent->elseBody;
        else
            return false;
    }
    if (!list || last.index < 0 || last.index >= static_cast<int>(list->size())) return false;
    list->insert(list->begin() + last.index + 1, stub);
    return true;
}

bool EventGraph::deleteAt(eventlang::Script& sc, const StmtPath& path) {
    if (path.empty()) return false;
    StmtPath parentPath(path.begin(), path.end() - 1);
    const StmtPathElem last = path.back();

    std::vector<eventlang::Stmt>* list = nullptr;
    if (parentPath.empty()) {
        list = &sc.body;
    } else {
        eventlang::Stmt* parent = resolveStmt(sc, parentPath);
        if (!parent || parent->kind != "if") return false;
        if (last.list == StmtListKind::Then)
            list = &parent->thenBody;
        else if (last.list == StmtListKind::Else)
            list = &parent->elseBody;
        else
            return false;
    }
    if (!list || last.index < 0 || last.index >= static_cast<int>(list->size())) return false;
    list->erase(list->begin() + last.index);
    return true;
}

bool EventGraph::appendStmt(eventlang::Script& sc, const eventlang::Stmt& stub) {
    sc.body.push_back(stub);
    return true;
}

eventlang::Stmt EventGraph::makeStub(const char* kind) {
    using eventlang::Expr;
    using eventlang::Stmt;
    if (!kind) kind = "end";
    if (std::strcmp(kind, "say") == 0)
        return Stmt::make("say").set("variant", "door").set("string", "s0");
    if (std::strcmp(kind, "wait") == 0)
        return Stmt::make("wait").set("kind", "space").set("mode", 0);
    if (std::strcmp(kind, "end") == 0) return Stmt::make("end");
    if (std::strcmp(kind, "ask_yes_no") == 0) return Stmt::make("ask_yes_no").set("mode", 0);
    if (std::strcmp(kind, "give_item") == 0) return Stmt::make("give_item").set("item", 1);
    if (std::strcmp(kind, "clear_tile_event") == 0) return Stmt::make("clear_tile_event");
    if (std::strcmp(kind, "go_to") == 0) return Stmt::make("go_to").set("screen", 0).set("pos", 0);
    if (std::strcmp(kind, "shop") == 0) return Stmt::make("shop").set("name", "inn");
    if (std::strcmp(kind, "selector") == 0) return Stmt::make("selector").set("value", 1);
    if (std::strcmp(kind, "fight") == 0) {
        Stmt s = Stmt::make("fight");
        s.setList("monsters", {1, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        s.setList("flags", {0, 0});
        return s;
    }
    if (std::strcmp(kind, "fight_b") == 0) {
        Stmt s = Stmt::make("fight_b");
        s.setList("data", {1, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        return s;
    }
    if (std::strcmp(kind, "if") == 0) {
        Stmt s = Stmt::make("if");
        s.cond = Expr::make("yes_no").set("mode", 0);
        s.thenBody.push_back(Stmt::make("end"));
        return s;
    }
    if (std::strcmp(kind, "set_cond") == 0) {
        Stmt s = Stmt::make("set_cond");
        s.cond = Expr::make("gold_at_least").set("amount", 100);
        return s;
    }
    return Stmt::make(kind);
}

const GraphNode* EventGraph::findNode(int id) const {
    for (const auto& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

GraphNode* EventGraph::findNode(int id) {
    for (auto& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

const GraphNode* EventGraph::findNodeForEvent(int eventId) const {
    for (const auto& n : nodes)
        if (n.eventId == eventId && !n.isTrigger && n.path.size() == 1 &&
            n.path[0].index == 0)
            return &n;
    return nullptr;
}

const GraphNode* EventGraph::findEventHeader(int eventId) const {
    for (const auto& n : nodes)
        if (n.category == GraphNodeCategory::Header && n.eventId == eventId) return &n;
    return nullptr;
}

GraphNode* EventGraph::findNodeByPath(int eventId, const StmtPath& path) {
    for (auto& n : nodes) {
        if (n.eventId != eventId) continue;
        if (n.category == GraphNodeCategory::Header || n.category == GraphNodeCategory::Trigger)
            continue;
        if (pathEqual(n.path, path)) return &n;
    }
    return nullptr;
}

void EventGraph::rebuild(const eventlang::Location& loc, int locId, int focusEvent,
                         const std::vector<int>& scriptOrder, App* app) {
    const int prevSel = selectedNodeId;
    nodes.clear();
    links.clear();
    nextLinkId = 1;
    builtForLoc = locId;
    builtForEvent = focusEvent;
    lastFocusedEvent = focusEvent;
    layoutDirty = true;
    layoutRefinePasses = 0;

    // Gather the scripts we can build: all non-empty scripts in this location,
    // preferring focusEvent first but keeping scriptOrder order.
    struct Chain {
        int eventId;
        const eventlang::Script* sc;
    };
    std::vector<Chain> chains;
    for (int eid : scriptOrder) {
        const eventlang::Script* sc = nullptr;
        for (const auto& s : loc.scripts)
            if (s.eventId == eid) {
                sc = &s;
                break;
            }
        if (!sc) continue;
        if (sc->isPlainText || sc->body.empty()) continue;
        chains.push_back({eid, sc});
    }
    {
        bool has = false;
        for (auto& c : chains)
            if (c.eventId == focusEvent) has = true;
        if (!has && focusEvent >= 0) {
            for (const auto& s : loc.scripts) {
                if (s.eventId == focusEvent) {
                    chains.push_back({focusEvent, &s});
                    break;
                }
            }
        }
    }
    if (chains.empty()) {
        selectedNodeId = -1;
        return;
    }

    int nextId = 1;

    // Build nodes + links first (positions filled by reflow using estimated sizes).
    int chainIndex = 0;
    for (const auto& ch : chains) {
        const int eventId = ch.eventId;
        const eventlang::Script& sc = *ch.sc;

        GraphNode h;
        h.id = nextId++;
        h.category = GraphNodeCategory::Header;
        h.eventId = eventId;
        h.chainIndex = chainIndex;
        h.path = {};
        h.inPin = pinIn(h.id);
        h.outPin = pinOut(h.id);
        summarizeScript(sc, loc, app, &h.title, &h.subtitle);
        h.size = estimateNodeSize(h);
        nodes.push_back(h);
        const int headerId = h.id;

        std::function<int(const std::vector<eventlang::Stmt>&, const StmtPath&, StmtListKind)>
            bodyWalk;
        bodyWalk = [&](const std::vector<eventlang::Stmt>& stmts, const StmtPath& parentPath,
                       StmtListKind listKind) -> int {
            int first = -1;
            int prev = -1;
            for (int i = 0; i < static_cast<int>(stmts.size()); ++i) {
                const eventlang::Stmt& st = stmts[static_cast<size_t>(i)];
                GraphNode n;
                n.id = nextId++;
                n.path = parentPath;
                n.path.push_back(StmtPathElem{listKind, i});
                n.eventId = eventId;
                n.chainIndex = chainIndex;
                n.kind = st.kind;
                describeStmt(st, loc, app, &n.title, &n.subtitle);
                n.category = categorizeStmt(st.kind);
                n.inPin = pinIn(n.id);
                n.outPin = pinOut(n.id);
                n.isIf = (st.kind == "if");
                if (n.isIf) n.elseOutPin = pinElse(n.id);

                if (st.kind == "selector" || st.kind == "shop" || st.kind == "quest") {
                    int sel = st.getNum("value");
                    if (!sel) sel = st.getNum("selector");
                    if (!sel && st.kind != "selector")
                        sel = eventlang::selectorByShopOrQuest(st.kind, st.getStr("name"));
                    if (auto bin = eventlang::binExecSelector(sel)) {
                        n.overlayLoc = bin->first;
                        n.overlayEvent = bin->second;
                    }
                } else if (st.kind == "overlay") {
                    n.overlayLoc = st.getNum("loc", st.getNum("overlay"));
                    n.overlayEvent = st.getNum("event");
                }

                n.size = estimateNodeSize(n);

                if (prev >= 0) {
                    GraphLink lk;
                    lk.id = nextLinkId++;
                    lk.fromPin = pinOut(prev);
                    lk.toPin = n.inPin;
                    links.push_back(lk);
                }
                if (first < 0) first = n.id;
                const int thisBodyId = n.id;
                nodes.push_back(n);

                if (st.kind == "if") {
                    const int thenFirst = bodyWalk(st.thenBody, n.path, StmtListKind::Then);
                    const int elseFirst = bodyWalk(st.elseBody, n.path, StmtListKind::Else);
                    if (thenFirst >= 0) {
                        GraphLink lk;
                        lk.id = nextLinkId++;
                        lk.fromPin = pinOut(thisBodyId);
                        lk.toPin = pinIn(thenFirst);
                        links.push_back(lk);
                    }
                    if (elseFirst >= 0) {
                        GraphLink lk;
                        lk.id = nextLinkId++;
                        lk.fromPin = pinElse(thisBodyId);
                        lk.toPin = pinIn(elseFirst);
                        links.push_back(lk);
                    }
                }
                prev = thisBodyId;
            }
            return first;
        };

        const int firstBody = bodyWalk(sc.body, {}, StmtListKind::Body);
        if (firstBody >= 0) {
            GraphLink lk;
            lk.id = nextLinkId++;
            lk.fromPin = pinOut(headerId);
            lk.toPin = pinIn(firstBody);
            links.push_back(lk);
        }

        ++chainIndex;
    }

    // Triggers (left of header). Created after so headers exist for linking.
    std::unordered_map<int, int> triggerStack;
    for (const auto& t : loc.triggers) {
        GraphNode* hdr = nullptr;
        for (auto& n : nodes)
            if (n.category == GraphNodeCategory::Header && n.eventId == t.eventId) {
                hdr = &n;
                break;
            }
        if (!hdr) continue;
        const int stack = triggerStack[t.eventId]++;
        GraphNode tn;
        tn.id = nextId++;
        tn.category = GraphNodeCategory::Trigger;
        tn.eventId = t.eventId;
        tn.chainIndex = hdr->chainIndex;
        tn.isTrigger = true;
        tn.path = {};
        tn.outPin = pinOut(tn.id);
        tn.inPin = pinIn(tn.id);
        char lbl[80];
        std::snprintf(lbl, sizeof(lbl), "Tile (%d,%d)", t.y, t.x);
        tn.title = lbl;
        {
            char sub[96];
            std::snprintf(sub, sizeof(sub), "%s → event_%02d", eventlang::triggerCondName(t.cond),
                          t.eventId);
            tn.subtitle = sub;
        }
        // Encode stack index in path so reflow can order them (index only).
        tn.path.push_back(StmtPathElem{StmtListKind::Body, stack});
        tn.size = estimateNodeSize(tn);
        nodes.push_back(tn);
        GraphLink lk;
        lk.id = nextLinkId++;
        lk.fromPin = pinOut(tn.id);
        lk.toPin = hdr->inPin;
        links.push_back(lk);
    }

    reflow(loc);

    // Restore selection if still valid.
    selectedNodeId = findNode(prevSel) ? prevSel : -1;
    if (selectedNodeId < 0) {
        const GraphNode* hdr = findEventHeader(focusEvent);
        selectedNodeId = hdr ? hdr->id : (nodes.empty() ? -1 : nodes.front().id);
    }
}

void EventGraph::reflow(const eventlang::Location& loc) {
    for (auto& n : nodes) clampNodeSize(n);

    // Discover chain order from headers.
    std::vector<int> chainEvents;
    for (const auto& n : nodes) {
        if (n.category != GraphNodeCategory::Header) continue;
        chainEvents.push_back(n.eventId);
    }

    float chainY = 60.f;
    for (size_t ci = 0; ci < chainEvents.size(); ++ci) {
        const int eventId = chainEvents[ci];
        GraphNode* hdr = nullptr;
        for (auto& n : nodes)
            if (n.category == GraphNodeCategory::Header && n.eventId == eventId) {
                hdr = &n;
                break;
            }
        if (!hdr) continue;

        // Triggers stacked to the left of the header (top → bottom).
        std::vector<GraphNode*> trigs;
        for (auto& n : nodes)
            if (n.category == GraphNodeCategory::Trigger && n.eventId == eventId) trigs.push_back(&n);
        std::sort(trigs.begin(), trigs.end(), [](const GraphNode* a, const GraphNode* b) {
            const int ia = a->path.empty() ? 0 : a->path[0].index;
            const int ib = b->path.empty() ? 0 : b->path[0].index;
            return ia < ib;
        });

        float trigW = 0.f;
        float trigH = 0.f;
        for (GraphNode* t : trigs) {
            trigW = std::max(trigW, std::min(t->size.x, kMaxNodeW));
            trigH += std::min(t->size.y, kMaxNodeH) + 8.f;
        }
        if (!trigs.empty()) trigH -= 8.f;

        float x = 60.f + (trigs.empty() ? 0.f : trigW + kTriggerGapX);
        const float y = chainY;

        float trigY = y;
        // Center trigger stack against header if shorter.
        if (trigH < hdr->size.y) trigY = y + (hdr->size.y - trigH) * 0.5f;
        for (GraphNode* t : trigs) {
            t->pos = ImVec2(60.f, trigY);
            trigY += t->size.y + 8.f;
        }

        hdr->pos = ImVec2(x, y);
        x += nodeAdvanceX(*hdr);

        // Find script body for branch walk.
        const eventlang::Script* sc = nullptr;
        for (const auto& s : loc.scripts)
            if (s.eventId == eventId) {
                sc = &s;
                break;
            }

        float maxBottom = y + hdr->size.y;
        for (GraphNode* t : trigs) maxBottom = std::max(maxBottom, t->pos.y + t->size.y);

        if (sc) {
            std::function<void(const std::vector<eventlang::Stmt>&, const StmtPath&, StmtListKind,
                               float&, float)>
                placeBody;
            placeBody = [&](const std::vector<eventlang::Stmt>& stmts, const StmtPath& parentPath,
                            StmtListKind listKind, float& bx, float by) {
                for (int i = 0; i < static_cast<int>(stmts.size()); ++i) {
                    StmtPath path = parentPath;
                    path.push_back(StmtPathElem{listKind, i});
                    GraphNode* n = findNodeByPath(eventId, path);
                    if (!n) continue;
                    n->pos = ImVec2(bx, by);
                    maxBottom = std::max(maxBottom, by + n->size.y);
                    bx += nodeAdvanceX(*n);

                    const eventlang::Stmt& st = stmts[static_cast<size_t>(i)];
                    if (st.kind == "if") {
                        float thenX = bx;
                        float elseX = bx;
                        // Else drops by the if-node height + gap so it clears the then row.
                        const float elseY = by + n->size.y + kBranchGapY;
                        placeBody(st.thenBody, path, StmtListKind::Then, thenX, by);
                        placeBody(st.elseBody, path, StmtListKind::Else, elseX, elseY);
                        bx = std::max(thenX, elseX);
                    }
                }
            };
            placeBody(sc->body, {}, StmtListKind::Body, x, y);
        }

        chainY = maxBottom + kChainGapY;
    }
}

void EventGraph::draw(App& app, eventlang::Location& loc, eventlang::Script* focusScript,
                      bool readOnly, bool allowMutate, int* focusedEvent, bool* selectionChanged,
                      bool* astMutated) {
    (void)focusScript;
    if (selectionChanged) *selectionChanged = false;
    if (astMutated) *astMutated = false;

    if (readOnly) {
        ui::PushDangerBanner("graph_ro");
        ui::TextWarn("Compile script changes to edit the graph");
        ui::EndBanner();
        ImGui::Spacing();
    }

    if (nodes.empty()) {
        ui::EmptyState("No scripts", "No non-empty scripts in this location — use New Event…");
        return;
    }

    applyEditorMouseWheelZoom();
    ImNodes::BeginNodeEditor();

    bool inlineMutated = false;
    for (auto& n : nodes) {
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, categoryTitleColor(n.category));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, categoryTitleColor(n.category));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, categoryTitleColor(n.category));
        ImNodes::PushColorStyle(ImNodesCol_NodeBackground, categoryBgColor(n.category));
        ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundHovered, categoryBgColor(n.category));
        ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundSelected, categoryBgColor(n.category));

        if (layoutDirty) ImNodes::SetNodeGridSpacePos(n.id, n.pos);

        ImNodes::BeginNode(n.id);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(n.title.c_str());
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(n.inPin);
        ImGui::Dummy(ImVec2(4, 4));
        ImNodes::EndInputAttribute();

        const bool selected = (n.id == selectedNodeId);
        const bool canInline =
            selected && !readOnly && allowMutate &&
            n.category != GraphNodeCategory::Header &&
            n.category != GraphNodeCategory::Trigger;

        if (canInline) {
            if (drawInlineNodeEditor(app, loc, n)) inlineMutated = true;
        } else if (!n.subtitle.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ui::Muted());
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + kTextWrap);
            ImGui::TextWrapped("%s", n.subtitle.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        } else {
            ImGui::Dummy(ImVec2(kTextWrap, 4));
        }

        // Cross-script / cross-location jump affordance.
        if (n.overlayLoc >= 0 && n.overlayEvent >= 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ui::Accent());
            char jl[64];
            std::snprintf(jl, sizeof(jl), "→ loc %d event_%02d", n.overlayLoc, n.overlayEvent);
            if (canInline) {
                // Small jump button on the node itself.
                if (ImGui::SmallButton(jl)) {
                    pendingAction = PendingAction::NavigateOverlay;
                    pendingLoc = n.overlayLoc;
                    pendingEvent = n.overlayEvent;
                }
            } else {
                ImGui::TextUnformatted(jl);
            }
            ImGui::PopStyleColor();
        }

        if (n.isIf) {
            ImNodes::BeginOutputAttribute(n.outPin);
            ImGui::TextDisabled("then");
            ImNodes::EndOutputAttribute();
            ImNodes::BeginOutputAttribute(n.elseOutPin);
            ImGui::TextDisabled("else");
            ImNodes::EndOutputAttribute();
        } else if (n.category != GraphNodeCategory::Header &&
                   n.category != GraphNodeCategory::Trigger) {
            ImNodes::BeginOutputAttribute(n.outPin);
            ImGui::Dummy(ImVec2(4, 4));
            ImNodes::EndOutputAttribute();
        } else if (n.category == GraphNodeCategory::Header ||
                   n.category == GraphNodeCategory::Trigger) {
            ImNodes::BeginOutputAttribute(n.outPin);
            ImGui::Dummy(ImVec2(4, 4));
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();

        for (int i = 0; i < 6; ++i) ImNodes::PopColorStyle();
    }

    for (const auto& lk : links) ImNodes::Link(lk.id, lk.fromPin, lk.toPin);

    ImNodes::MiniMap(0.18f, ImNodesMiniMapLocation_BottomRight);
    ImNodes::EndNodeEditor();

    if (inlineMutated) {
        if (astMutated) *astMutated = true;
        layoutDirty = true;
        layoutRefinePasses = 0;
    }

    // Measure real node sizes (clamped) and reflow a couple times so gaps match content.
    // Never store unclamped ImNodes dimensions — inline widgets used to widen nodes forever.
    bool sizesChanged = false;
    for (auto& n : nodes) {
        const ImVec2 dim = ImNodes::GetNodeDimensions(n.id);
        if (dim.x < 1.f || dim.y < 1.f) continue;
        ImVec2 next{std::clamp(dim.x, kMinNodeW, kMaxNodeW),
                    std::clamp(dim.y, 40.f, kMaxNodeH)};
        if (std::fabs(next.x - n.size.x) > 1.5f || std::fabs(next.y - n.size.y) > 1.5f)
            sizesChanged = true;
        n.size = next;
    }
    if ((sizesChanged || inlineMutated) && layoutRefinePasses < 2) {
        reflow(loc);
        layoutDirty = true;
        ++layoutRefinePasses;
    } else if (!inlineMutated) {
        layoutDirty = false;
    }

    std::vector<int> selected;
    const int numSel = ImNodes::NumSelectedNodes();
    if (numSel > 0) {
        selected.resize(static_cast<size_t>(numSel));
        ImNodes::GetSelectedNodes(selected.data());
        if (!selected.empty() && selected[0] != selectedNodeId) {
            selectedNodeId = selected[0];
            if (selectionChanged) *selectionChanged = true;
        }
        const GraphNode* selNode = selected.empty() ? nullptr : findNode(selected[0]);
        if (selNode && selNode->eventId >= 0 && focusedEvent) {
            *focusedEvent = selNode->eventId;
            lastFocusedEvent = selNode->eventId;
        }
    } else if (focusedEvent) {
        *focusedEvent = lastFocusedEvent;
    }

    // Right-click menu (works with or without a selection — falls back to focused script).
    if (ImGui::BeginPopupContextWindow("graph_ctx")) {
        const GraphNode* n = selectedNodeId >= 0 ? findNode(selectedNodeId) : nullptr;
        int targetEvent = n ? n->eventId : lastFocusedEvent;
        eventlang::Script* sc = nullptr;
        if (targetEvent >= 0) {
            for (auto& s : loc.scripts)
                if (s.eventId == targetEvent) {
                    sc = &s;
                    break;
                }
        }

        auto tryInsert = [&](const char* kind, bool after) {
            if (!sc || !allowMutate || readOnly) return;
            eventlang::Stmt stub = makeStub(kind);
            bool ok = false;
            if (n && n->category != GraphNodeCategory::Header &&
                n->category != GraphNodeCategory::Trigger && after) {
                ok = insertAfter(*sc, n->path, stub);
            } else {
                ok = appendStmt(*sc, stub);
            }
            if (ok && astMutated) *astMutated = true;
        };

        if (n) {
            ImGui::TextUnformatted(n->title.c_str());
            if (!n->subtitle.empty()) ImGui::TextDisabled("%s", n->subtitle.c_str());
            ImGui::Separator();
        } else if (targetEvent >= 0) {
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "event_%02d", targetEvent);
            ImGui::TextDisabled("%s", lbl);
            ImGui::Separator();
        }

        const bool canMutate = !readOnly && allowMutate && sc != nullptr;
        if (canMutate && ImGui::BeginMenu("Add node")) {
            const bool after =
                n && n->category != GraphNodeCategory::Header &&
                n->category != GraphNodeCategory::Trigger;
            ImGui::TextDisabled(after ? "Insert after selection" : "Append to script");
            ImGui::Separator();
            if (ImGui::MenuItem("Door / say")) tryInsert("say", after);
            if (ImGui::MenuItem("Ask Yes/No")) tryInsert("ask_yes_no", after);
            if (ImGui::MenuItem("If branch")) tryInsert("if", after);
            if (ImGui::MenuItem("Give item")) tryInsert("give_item", after);
            if (ImGui::MenuItem("Set condition")) tryInsert("set_cond", after);
            if (ImGui::MenuItem("Fixed fight")) tryInsert("fight", after);
            if (ImGui::MenuItem("Random fight")) tryInsert("fight_b", after);
            if (ImGui::MenuItem("Open shop")) tryInsert("shop", after);
            if (ImGui::MenuItem("Selector")) tryInsert("selector", after);
            if (ImGui::MenuItem("Go to map")) tryInsert("go_to", after);
            if (ImGui::MenuItem("Wait for space")) tryInsert("wait", after);
            if (ImGui::MenuItem("Clear tile event")) tryInsert("clear_tile_event", after);
            if (ImGui::MenuItem("End script")) tryInsert("end", after);
            ImGui::EndMenu();
        }

        if (canMutate && n && n->category != GraphNodeCategory::Header &&
            n->category != GraphNodeCategory::Trigger) {
            if (ImGui::MenuItem("Delete node")) {
                if (deleteAt(*sc, n->path)) {
                    selectedNodeId = -1;
                    if (astMutated) *astMutated = true;
                    if (selectionChanged) *selectionChanged = true;
                }
            }
        }

        if (n) {
            ImGui::Separator();
            bool anyTrig = false;
            for (const auto& t : loc.triggers) {
                if (t.eventId != n->eventId) continue;
                anyTrig = true;
                char lbl[64];
                std::snprintf(lbl, sizeof(lbl), "Show on map (%d,%d)", t.y, t.x);
                if (ImGui::MenuItem(lbl)) {
                    pendingAction = PendingAction::ShowOnMap;
                    pendingTileY = t.y;
                    pendingTileX = t.x;
                    pendingLoc = loc.id;
                    pendingEvent = n->eventId;
                }
            }
            if (!anyTrig && n->category == GraphNodeCategory::Trigger) {
                if (ImGui::MenuItem("Show on map")) {
                    pendingAction = PendingAction::ShowOnMap;
                    pendingLoc = loc.id;
                    pendingEvent = n->eventId;
                    for (const auto& t : loc.triggers) {
                        if (t.eventId == n->eventId) {
                            pendingTileY = t.y;
                            pendingTileX = t.x;
                            break;
                        }
                    }
                }
            }

            if (n->overlayLoc >= 0 && n->overlayEvent >= 0) {
                char jl[72];
                std::snprintf(jl, sizeof(jl), "Go to loc %d event_%02d", n->overlayLoc,
                              n->overlayEvent);
                if (ImGui::MenuItem(jl)) {
                    pendingAction = PendingAction::NavigateOverlay;
                    pendingLoc = n->overlayLoc;
                    pendingEvent = n->overlayEvent;
                }
            }

            if (n->eventId >= 0 && n->eventId != lastFocusedEvent) {
                char jl[40];
                std::snprintf(jl, sizeof(jl), "Focus event_%02d", n->eventId);
                if (ImGui::MenuItem(jl)) {
                    pendingAction = PendingAction::JumpEvent;
                    pendingEvent = n->eventId;
                }
            }
        }

        if (readOnly) {
            ImGui::Separator();
            ImGui::TextDisabled("Compile script to enable editing");
        } else if (!allowMutate) {
            ImGui::Separator();
            ImGui::TextDisabled("Structured edits disabled for this record");
        }

        ImGui::EndPopup();
    }
}

}  // namespace mm2
#include "eventlang/DslEmit.h"

#include "core/EventOps.h"
#include "eventlang/Semantics.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>

namespace mm2::eventlang {
namespace {

std::string hex2(int v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "0x%02X", v & 0xFF);
    return buf;
}

std::string formatExpr(const Expr& expr) {
    const std::string& k = expr.kind;
    if (k == "class_field" || k == "member_attr") {
        // OP_2D @ 0x16DBA: keep packed args for roundtrip; annotate decode.
        const int a1 = expr.getNum("arg1", expr.getNum("field"));
        const int a2 = expr.getNum("arg2", expr.getNum("mask"));
        char buf[128];
        const char* which = (a1 & 0x80) ? "race" : (a1 & 0x40) ? "sex" : "class";
        const char* mode = (a1 & 0x20) ? "any" : "all";
        const int v1 = a1 & 0x0F;
        const int v2 = ((a1 & 0xE0) == 0) ? (a2 & 0x0F) : v1;
        if (v2 != v1)
            std::snprintf(buf, sizeof(buf),
                          "member_attr arg1=0x%02X arg2=0x%02X  # %s %s val=0x%X|0x%X", a1, a2,
                          which, mode, v1, v2);
        else
            std::snprintf(buf, sizeof(buf),
                          "member_attr arg1=0x%02X arg2=0x%02X  # %s %s val=0x%X", a1, a2, which,
                          mode, v1);
        return buf;
    }
    if (k == "class_is") return "class " + expr.getStr("class");
    if (k == "has_item_id" || k == "consume_item") {
        int probe = expr.getNum("probe", 0);
        char buf[64];
        if (probe)
            std::snprintf(buf, sizeof(buf), "consume_item 0x%02X probe=%d", expr.getNum("item"),
                          probe);
        else
            std::snprintf(buf, sizeof(buf), "consume_item 0x%02X", expr.getNum("item"));
        return buf;
    }
    if (k == "gold_at_least") return "gold >= " + std::to_string(expr.getNum("amount"));
    if (k == "gems_at_least" || k == "code16")
        return "gems >= " + std::to_string(expr.getNum("amount", expr.getNum("value")));
    if (k == "answer_eq") return std::string("answer == \"") + expr.getStr("text") + "\"";
    if (k == "rng_roll") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "rng_roll 1..%d", expr.getNum("max"));
        return buf;
    }
    if (k == "combat_victory") return "combat_victory";
    if (k == "prior_cond") return "prior_cond";
    if (k == "give_item_ok") {
        char buf[96];
        std::snprintf(buf, sizeof(buf),
                      "give_item_ok item=0x%02X member=%d charges=%d flags=0x%02X",
                      expr.getNum("item"), expr.getNum("member"), expr.getNum("charges"),
                      expr.getNum("flags"));
        return buf;
    }
    if (k == "party_effect_ok") {
        std::ostringstream oss;
        oss << "party_effect_ok mode=" << expr.getNum("mode") << " sel=" << hex2(expr.getNum("sel"));
        auto it = expr.lists.find("args");
        if (it != expr.lists.end() && it->second.size() > 1) {
            oss << " args";
            for (size_t i = 1; i < it->second.size(); ++i) oss << " " << hex2(it->second[i]);
        }
        return oss.str();
    }
    if (k == "era_in")
        return "era in " + std::to_string(expr.getNum("lo")) + ".." + std::to_string(expr.getNum("hi"));
    if (k == "day_in")
        return "day in " + std::to_string(expr.getNum("lo")) + ".." + std::to_string(expr.getNum("hi"));
    if (k == "day_odd") return "day odd";
    if (k == "day_even") return "day even";
    if (k == "monster_present" || k == "party_has_item") {
        char buf[64];
        const int item = expr.getNum("item", expr.getNum("b"));
        const int a = expr.getNum("a", 0);
        // OP_16 packs two args; arg1 is discarded by VM but must roundtrip.
        if (a)
            std::snprintf(buf, sizeof(buf), "party_has_item 0x%02X a=0x%02X", item, a);
        else
            std::snprintf(buf, sizeof(buf), "party_has_item 0x%02X", item);
        return buf;
    }
    if (k == "count_title_nibble") {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "count_title_nibble 0x%02X", expr.getNum("id"));
        return buf;
    }
    if (k == "yes_no") return expr.getNum("mode") ? "yes_no mode=1" : "yes_no";
    if (k == "threshold") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "cond >= 0x%02X", expr.getNum("value"));
        return buf;
    }
    if (k == "load_var8") {
        int idx = expr.getNum("index", 0);
        int group = expr.getNum("group");
        const char* gname = varGroupName(group);
        char buf[80];
        if (gname) {
            if (idx)
                std::snprintf(buf, sizeof(buf), "load_var8 %s index=0x%02X", gname, idx);
            else
                std::snprintf(buf, sizeof(buf), "load_var8 %s", gname);
        } else if (idx) {
            std::snprintf(buf, sizeof(buf), "load_var8 group=0x%02X index=0x%02X", group, idx);
        } else {
            std::snprintf(buf, sizeof(buf), "load_var8 group=0x%02X", group);
        }
        return buf;
    }
    if (k == "party_bits") {
        const int member = expr.getNum("member");
        const int field = expr.getNum("field");
        const int mask = expr.getNum("mask");
        const char* fname = partyFieldName(field);
        char buf[96];
        if (fname)
            std::snprintf(buf, sizeof(buf), "party_bits %s %s mask=0x%02X",
                          partyMemberToken(member).c_str(), fname, mask);
        else
            std::snprintf(buf, sizeof(buf), "party_bits %s field=0x%02X mask=0x%02X",
                          partyMemberToken(member).c_str(), field, mask);
        return buf;
    }
    if (k == "raw_op") {
        std::ostringstream oss;
        oss << "@op " << hex2(expr.getNum("op"));
        auto it = expr.lists.find("args");
        if (it != expr.lists.end())
            for (int x : it->second) oss << " " << hex2(x);
        return oss.str();
    }
    return "@cond " + k;
}

std::string sayVerb(const std::string& variant) {
    if (variant == "door") return "say_door";
    if (variant == "block") return "say_block";
    if (variant == "popup_a") return "say_popup_a";
    if (variant == "popup_b") return "say_popup_b";
    if (variant == "basic") return "say_basic";
    // OP_03 default / empty variant
    return "say";
}

std::string stringRefComment(const std::string& ref,
                             const std::unordered_map<std::string, std::string>& lookup) {
    auto it = lookup.find(ref);
    if (it == lookup.end()) return {};
    const std::string& text = it->second;
    if (text.empty() || text == "z" || text == "\n") return {};
    std::string one = text;
    for (char& c : one)
        if (c == '\n') c = '@';
    while (!one.empty() && (one.back() == ' ' || one.back() == '\t')) one.pop_back();
    if (one.size() > 56) one = one.substr(0, 53) + "...";
    for (char& c : one)
        if (c == '"') c = '\'';
    return std::string("  # \"") + one + "\"";
}

void formatStmt(const Stmt& stmt, int depth, const std::unordered_map<std::string, std::string>& lookup,
                std::vector<std::string>& lines, int locId, const EventFileAst* file);

std::string firstOverlayStringHint(const Location& oloc, int eventId) {
    if (eventId < 0 || eventId >= static_cast<int>(oloc.scripts.size())) return {};
    const auto& body = oloc.scripts[eventId].body;
    std::function<std::string(const std::vector<Stmt>&)> walk;
    walk = [&](const std::vector<Stmt>& stmts) -> std::string {
        for (const auto& s : stmts) {
            if (s.kind == "say") {
                std::string ref = s.getStr("string");
                if (ref.empty() && s.num.count("string")) ref = "s" + std::to_string(s.getNum("string"));
                auto it = std::find_if(oloc.strings.begin(), oloc.strings.end(),
                                       [&](const StringDef& d) { return d.name == ref; });
                if (it != oloc.strings.end() && !it->text.empty() && it->text != "z") {
                    std::string one = it->text;
                    for (char& c : one)
                        if (c == '\n') c = '@';
                    if (one.size() > 48) one = one.substr(0, 45) + "...";
                    for (char& c : one)
                        if (c == '"') c = '\'';
                    return one;
                }
            }
            if (s.kind == "if") {
                auto t = walk(s.thenBody);
                if (!t.empty()) return t;
                auto e = walk(s.elseBody);
                if (!e.empty()) return e;
            }
        }
        return {};
    };
    return walk(body);
}

void formatStmt(const Stmt& stmt, int depth, const std::unordered_map<std::string, std::string>& lookup,
                std::vector<std::string>& lines, int locId, const EventFileAst* file) {
    std::string pad(static_cast<size_t>(depth) * 2, ' ');
    const std::string& k = stmt.kind;

    if (k == "say") {
        std::string ref = stmt.getStr("string");
        if (ref.empty() && stmt.num.count("string")) ref = "s" + std::to_string(stmt.getNum("string"));
        lines.push_back(pad + sayVerb(stmt.getStr("variant")) + " " + ref +
                        stringRefComment(ref, lookup));
        return;
    }
    if (k == "service_title") {
        int sign = stmt.getNum("sign", stmt.getNum("string", 0));
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%sservice_title sign=0x%02X mode=%d", pad.c_str(), sign,
                      stmt.getNum("mode"));
        std::string line = buf;
        if (locId >= 0) {
            const int anm = mm2::serviceSignId(locId, static_cast<uint8_t>(sign));
            if (anm > 0) {
                char cmt[48];
                std::snprintf(cmt, sizeof(cmt), "  # signboard %d.anm", anm);
                line += cmt;
            } else {
                line += "  # signboard (env table)";
            }
        }
        lines.push_back(line);
        return;
    }
    if (k == "wait") {
        if (stmt.getNum("mode") || stmt.getStr("kind") == "key")
            lines.push_back(pad + "wait space mode=1  # OP_08 scripted SPACE");
        else
            lines.push_back(pad + "wait space");
        return;
    }
    if (k == "ask_yes_no") {
        lines.push_back(pad + (stmt.getNum("mode") ? "ask yes_no mode=1" : "ask yes_no"));
        return;
    }
    if (k == "set_cond") {
        lines.push_back(pad + "set_cond " + formatExpr(stmt.cond));
        return;
    }
    if (k == "skip_if_true") {
        lines.push_back(pad + "skip_if_true " + std::to_string(stmt.getNum("n")) +
                        "  # OP_10 skip when cond!=0");
        return;
    }
    if (k == "skip_if_false") {
        lines.push_back(pad + "skip_if_false " + std::to_string(stmt.getNum("n")) +
                        "  # OP_11 skip when cond==0");
        return;
    }
    if (k == "skip_if_victory") {
        lines.push_back(pad + "skip_if_victory " + std::to_string(stmt.getNum("n")) +
                        "  # OP_2B skip when combat victory");
        return;
    }
    if (k == "if") {
        lines.push_back(pad + "if " + formatExpr(stmt.cond) + ":");
        for (const auto& s : stmt.thenBody) formatStmt(s, depth + 1, lookup, lines, locId, file);
        if (!stmt.elseBody.empty()) {
            lines.push_back(pad + "else:");
            for (const auto& s : stmt.elseBody) formatStmt(s, depth + 1, lookup, lines, locId, file);
        }
        return;
    }
    if (k == "selector" || k == "shop" || k == "quest") {
        int sel = stmt.getNum("value");
        if (!sel) sel = stmt.getNum("selector");
        if (!sel && k != "selector")
            sel = selectorByShopOrQuest(k, stmt.getStr("name"));
        std::string line = pad + formatSelectorDsl(sel);
        if (file) {
            if (auto bin = binExecSelector(sel)) {
                if (bin->first >= 0 && bin->first < static_cast<int>(file->locations.size())) {
                    const auto& oloc = file->locations[bin->first];
                    auto hint = firstOverlayStringHint(oloc, bin->second);
                    if (!hint.empty()) {
                        // Replace generic bank tip with opening line of target script.
                        auto hash = line.find("  # ");
                        if (hash != std::string::npos) {
                            char hex[8];
                            std::snprintf(hex, sizeof(hex), "0x%02X", sel & 0xFF);
                            line = pad + "overlay " + std::to_string(bin->first) + " event_";
                            if (bin->second < 10) line += "0";
                            line += std::to_string(bin->second);
                            line += "  # selector ";
                            line += hex;
                            line += " — \"";
                            line += hint;
                            line += "\"";
                        }
                    }
                }
            }
        }
        lines.push_back(line);
        return;
    }
    if (k == "engine_call" || k == "play_sound") {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "play_sound %d", stmt.getNum("id", stmt.getNum("code")));
        lines.push_back(pad + buf);
        return;
    }
    if (k == "party_effect") {
        std::ostringstream oss;
        oss << pad << "party_effect mode=" << stmt.getNum("mode") << " sel=" << hex2(stmt.getNum("sel"));
        auto it = stmt.lists.find("args");
        if (it != stmt.lists.end() && it->second.size() > 1) {
            oss << " args";
            for (size_t i = 1; i < it->second.size(); ++i) oss << " " << hex2(it->second[i]);
            // Annotate field selector + LE24 amount (EventPartyEffects @ 0x1690E).
            if (it->second.size() >= 6) {
                const int field = it->second[1];
                const int width = it->second[2];
                const int amount = it->second[3] | (it->second[4] << 8) | (it->second[5] << 16);
                char tip[80];
                std::snprintf(tip, sizeof(tip), "  # field=0x%02X width=%d amount=%d", field, width,
                              amount);
                oss << tip;
            }
        }
        lines.push_back(oss.str());
        return;
    }
    if (k == "treasure") {
        std::ostringstream oss;
        oss << pad << "treasure";
        auto it = stmt.lists.find("data");
        if (it != stmt.lists.end())
            for (int x : it->second) oss << " " << hex2(x);
        lines.push_back(oss.str());
        return;
    }
    if (k == "add_counter") {
        lines.push_back(pad + "add_counter " + std::to_string(stmt.getNum("amount")));
        return;
    }
    if (k == "or_member_field") {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "or_member_field arg1=0x%02X arg2=0x%02X",
                      stmt.getNum("arg1"), stmt.getNum("arg2"));
        lines.push_back(pad + buf);
        return;
    }
    if (k == "party_damage") {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "party_damage member=0x%02X value=%d", stmt.getNum("member"),
                      stmt.getNum("value"));
        lines.push_back(pad + buf);
        return;
    }
    if (k == "audio_wait") {
        lines.push_back(pad + "audio_wait " + std::to_string(stmt.getNum("index")));
        return;
    }
    if (k == "go_to") {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "go_to screen %d pos 0x%02X", stmt.getNum("screen"),
                      stmt.getNum("pos"));
        lines.push_back(pad + buf);
        return;
    }
    if (k == "fight") {
        std::ostringstream oss;
        oss << pad << "fight monsters";
        auto mit = stmt.lists.find("monsters");
        if (mit != stmt.lists.end())
            for (int x : mit->second) oss << " " << hex2(x);
        oss << " flags";
        auto fit = stmt.lists.find("flags");
        if (fit != stmt.lists.end())
            for (int x : fit->second) oss << " " << hex2(x);
        lines.push_back(oss.str());
        return;
    }
    if (k == "fight_b") {
        std::ostringstream oss;
        oss << pad << "fight_b";
        auto it = stmt.lists.find("data");
        if (it != stmt.lists.end())
            for (int x : it->second) oss << " " << hex2(x);
        lines.push_back(oss.str());
        return;
    }
    if (k == "set_tile") {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "set_tile (%d,%d) 0x%02X 0x%02X", stmt.getNum("y"),
                      stmt.getNum("x"), stmt.getNum("a"), stmt.getNum("b"));
        lines.push_back(pad + buf);
        return;
    }
    if (k == "apply_party") {
        const int member = stmt.getNum("count", stmt.getNum("member"));
        const int field = stmt.getNum("op", stmt.getNum("field"));
        const int mask = stmt.getNum("val", stmt.getNum("mask"));
        const char* fname = partyFieldName(field);
        char buf[128];
        if (fname)
            std::snprintf(buf, sizeof(buf), "party_bits %s %s mask=0x%02X",
                          partyMemberToken(member).c_str(), fname, mask);
        else
            std::snprintf(buf, sizeof(buf), "party_bits %s field=0x%02X mask=0x%02X",
                          partyMemberToken(member).c_str(), field, mask);
        lines.push_back(pad + buf);
        return;
    }
    if (k == "apply_party_masked" || k == "set_party_bits") {
        const int member = stmt.getNum("count", stmt.getNum("member"));
        const int field = stmt.getNum("set", stmt.getNum("field"));
        const int andM = stmt.getNum("and");
        const int orM = stmt.getNum("or");
        const char* fname = partyFieldName(field);
        char buf[160];
        if (fname) {
            std::snprintf(buf, sizeof(buf), "set_party_bits %s %s and=0x%02X or=0x%02X",
                          partyMemberToken(member).c_str(), fname, andM, orM);
        } else {
            std::snprintf(buf, sizeof(buf),
                          "set_party_bits %s field=0x%02X and=0x%02X or=0x%02X",
                          partyMemberToken(member).c_str(), field, andM, orM);
        }
        std::string line = pad + buf;
        // Skill grant: and clears one nibble, or writes skill id into the other.
        if (field == 0x6D) {
            int skill = 0;
            if (andM == 0xF0) skill = orM & 0x0F;
            else if (andM == 0x0F) skill = (orM >> 4) & 0x0F;
            if (const char* sn = skillNibbleName(skill)) {
                line += "  # ";
                line += sn;
            }
        }
        lines.push_back(line);
        return;
    }
    if (k == "select_member") {
        lines.push_back(pad + (stmt.getNum("mode") ? "select_member mode=1" : "select_member"));
        return;
    }
    if (k == "give_item") {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "give_item item=0x%02X member=%d charges=%d flags=0x%02X",
                      stmt.getNum("item"), stmt.getNum("member"), stmt.getNum("charges"),
                      stmt.getNum("flags"));
        lines.push_back(pad + buf);
        return;
    }
    if (k == "clear_tile_event") {
        lines.push_back(pad + "clear_tile_event");
        return;
    }
    if (k == "clear_input" || k == "read_answer") {
        lines.push_back(pad + "read_answer");
        return;
    }
    if (k == "abort") {
        lines.push_back(pad + "abort");
        return;
    }
    if (k == "end") {
        lines.push_back(pad + "end");
        return;
    }
    if (k == "delay") {
        lines.push_back(pad + "delay " + std::to_string(stmt.getNum("ticks")));
        return;
    }
    if (k == "load_var8") {
        int idx = stmt.getNum("index", 0);
        int group = stmt.getNum("group");
        const char* gname = varGroupName(group);
        char buf[80];
        if (gname) {
            if (idx)
                std::snprintf(buf, sizeof(buf), "load_var8 %s index=0x%02X", gname, idx);
            else
                std::snprintf(buf, sizeof(buf), "load_var8 %s", gname);
        } else if (idx) {
            std::snprintf(buf, sizeof(buf), "load_var8 group=0x%02X index=0x%02X", group, idx);
        } else {
            std::snprintf(buf, sizeof(buf), "load_var8 group=0x%02X", group);
        }
        lines.push_back(pad + buf);
        return;
    }
    if (k == "store_var8") {
        int group = stmt.getNum("group");
        int value = stmt.getNum("value");
        const char* gname = varGroupName(group);
        char buf[80];
        if (gname)
            std::snprintf(buf, sizeof(buf), "store_var8 %s value=0x%02X", gname, value);
        else
            std::snprintf(buf, sizeof(buf), "store_var8 group=0x%02X value=0x%02X", group, value);
        lines.push_back(pad + buf);
        return;
    }
    if (k == "plain_text") {
        std::string t = stmt.getStr("text");
        for (char& c : t)
            if (c == '\n') c = '@';
        lines.push_back(pad + "| " + t + " |");
        return;
    }
    if (k == "unlifted") {
        lines.push_back(pad + "@unlifted {");
        for (const auto& s : stmt.body) formatStmt(s, depth + 1, lookup, lines, locId, file);
        lines.push_back(pad + "}");
        return;
    }
    if (k == "unreachable") {
        // Keep as real stmts so text/AST roundtrip still emits retail dead tails.
        lines.push_back(pad + "# unreachable (prior go_to/abort/end stops script):");
        for (const auto& s : stmt.body) formatStmt(s, depth, lookup, lines, locId, file);
        return;
    }
    if (k == "raw_op") {
        std::ostringstream oss;
        oss << pad << "@op " << hex2(stmt.getNum("op"));
        auto it = stmt.lists.find("args");
        if (it != stmt.lists.end())
            for (int x : it->second) oss << " " << hex2(x);
        lines.push_back(oss.str());
        return;
    }
    if (k == "set_quest_complete") {
        lines.push_back(pad + "set quest_complete");
        return;
    }
    if (k == "set_quest_flag") {
        lines.push_back(pad + "set quest_flag " + stmt.getStr("name") + " " +
                        stmt.getStr("value", "ok"));
        return;
    }
    lines.push_back(pad + "@" + k);
}

std::string formatTriggerCond(const Trigger& t) {
    if (t.cond == TriggerCond::Raw) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "when 0x%02X", t.condRaw);
        return buf;
    }
    return triggerCondName(t.cond);
}

}  // namespace

std::string emitLocation(const Location& loc, const std::string& areaComment,
                         const EventFileAst* file) {
    std::vector<std::string> lines;
    if (!areaComment.empty()) lines.push_back("# " + areaComment);
    lines.push_back("location " + std::to_string(loc.id));
    lines.push_back(std::string("record ") + recordKindName(loc.recordKind));

    if (loc.recordKind == RecordKind::CastleBlob && !loc.rawBlob.empty()) {
        lines.push_back("raw_record:");
        std::ostringstream hex;
        for (size_t i = 0; i < loc.rawBlob.size(); ++i) {
            if (i && (i % 24) == 0) {
                lines.push_back("  " + hex.str());
                hex.str("");
                hex.clear();
            }
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%02x", loc.rawBlob[i]);
            if (!hex.str().empty()) hex << ' ';
            hex << buf;
        }
        if (!hex.str().empty()) lines.push_back("  " + hex.str());
        std::ostringstream out;
        for (size_t i = 0; i < lines.size(); ++i) {
            out << lines[i] << '\n';
        }
        return out.str();
    }

    std::unordered_map<std::string, std::string> lookup;
    for (const auto& s : loc.strings) lookup[s.name] = s.text;

    if (!loc.strings.empty()) {
        lines.push_back("");
        lines.push_back("strings:");
        for (const auto& s : loc.strings) {
            // Prefer exact rawBytes when they differ from text→0x40 encoding (0x0A etc.).
            std::vector<uint8_t> expected;
            for (char c : s.text) expected.push_back(c == '\n' ? 0x40 : static_cast<uint8_t>(c));
            const bool useRaw = !s.rawBytes.empty() && s.rawBytes != expected;
            if (useRaw) {
                std::ostringstream hex;
                hex << "  " << s.name << ": raw";
                for (uint8_t b : s.rawBytes) {
                    char buf[4];
                    std::snprintf(buf, sizeof(buf), " %02x", b);
                    hex << buf;
                }
                // Human-readable hint (comment only).
                std::string shown = s.text;
                for (char& c : shown)
                    if (c == '\n') c = '@';
                if (shown.size() > 48) shown = shown.substr(0, 45) + "...";
                for (char& c : shown)
                    if (c == '"') c = '\'';
                if (!shown.empty()) hex << "  # \"" << shown << "\"";
                lines.push_back(hex.str());
                continue;
            }
            std::string shown = s.text;
            for (char& c : shown)
                if (c == '\n') c = '@';
            // Embedded quotes break naive """...""" delimiters — use raw hex.
            const bool hasQuote = s.text.find('"') != std::string::npos;
            if (hasQuote) {
                std::ostringstream hex;
                hex << "  " << s.name << ": raw";
                const auto& bytes = !s.rawBytes.empty() ? s.rawBytes : expected;
                for (uint8_t b : bytes) {
                    char buf[4];
                    std::snprintf(buf, sizeof(buf), " %02x", b);
                    hex << buf;
                }
                std::string hint = shown;
                if (hint.size() > 48) hint = hint.substr(0, 45) + "...";
                for (char& c : hint)
                    if (c == '"') c = '\'';
                if (!hint.empty()) hex << "  # \"" << hint << "\"";
                lines.push_back(hex.str());
                continue;
            }
            // Escape backslashes for parse symmetry (quotes already handled above).
            std::string esc;
            esc.reserve(shown.size() + 4);
            for (char c : shown) {
                if (c == '\\') esc.push_back('\\');
                esc.push_back(c);
            }
            if (esc.size() > 72) {
                lines.push_back("  " + s.name + ":");
                lines.push_back("    \"\"\"" + esc + "\"\"\"");
            } else {
                lines.push_back("  " + s.name + ": \"" + esc + "\"");
            }
        }
    }

    if (!loc.triggers.empty()) {
        lines.push_back("");
        std::set<std::tuple<int, int, int, int>> seen;
        for (const auto& t : loc.triggers) {
            auto key = std::make_tuple(t.y, t.x, static_cast<int>(t.condRaw), t.eventId);
            if (seen.count(key)) continue;
            seen.insert(key);
            lines.push_back("on tile (" + std::to_string(t.y) + ", " + std::to_string(t.x) + ") " +
                            formatTriggerCond(t) + " -> " + t.scriptName + "  @event " +
                            std::to_string(t.eventId));
        }
    }

    for (const auto& sc : loc.scripts) {
        if (sc.body.empty() && !sc.isPlainText) continue;
        lines.push_back("");
        lines.push_back("script " + sc.name + ":  @event " + std::to_string(sc.eventId));
        for (const auto& stmt : sc.body) formatStmt(stmt, 1, lookup, lines, loc.id, file);
    }

    std::ostringstream out;
    for (const auto& ln : lines) out << ln << '\n';
    return out.str();
}

std::string emitFile(const EventFileAst& file) {
    std::ostringstream out;
    for (const auto& loc : file.locations) out << emitLocation(loc, {}, &file) << '\n';
    return out.str();
}

}  // namespace mm2::eventlang

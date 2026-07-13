#include "eventlang/DslParse.h"

#include "eventlang/Semantics.h"

#include <cctype>
#include <cstdio>
#include <sstream>
#include <unordered_map>

namespace mm2::eventlang {
namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string codePart(const std::string& line) {
    auto pos = line.find("  #");
    std::string s = pos == std::string::npos ? line : line.substr(0, pos);
    return trim(s);
}

int indentLevel(const std::string& line) {
    size_t n = 0;
    while (n < line.size() && line[n] == ' ') ++n;
    return static_cast<int>(n / 2);
}

bool startsWith(const std::string& s, const char* pfx) {
    return s.compare(0, std::char_traits<char>::length(pfx), pfx) == 0;
}

int parseHex(const std::string& s, size_t pos = 0) {
    return static_cast<int>(std::stoul(s.substr(pos), nullptr, 16));
}

TriggerCond parseCond(const std::string& text, uint8_t& rawOut) {
    std::string t = text;
    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    t = trim(t);
    rawOut = 0;
    if (startsWith(t, "when 0x")) {
        rawOut = static_cast<uint8_t>(parseHex(t, 7));
        return TriggerCond::Raw;
    }
    if (t == "always") return TriggerCond::Always;
    if (t == "enter") return TriggerCond::Enter;
    if (t == "from north") return TriggerCond::FromNorth;
    if (t == "dir special") return TriggerCond::DirSpecial;
    if (t == "any_direction") return TriggerCond::AnyDirection;
    if (t == "facing ns") return TriggerCond::FacingNs;
    if (t == "enter special") return TriggerCond::EnterSpecial;
    return TriggerCond::Raw;
}

Expr parseExpr(const std::string& text) {
    std::string t = trim(text);
    if (startsWith(t, "class ")) return Expr::make("class_is").set("class", trim(t.substr(6)));
    if (startsWith(t, "has_item 0x") || startsWith(t, "has_item 0X") ||
        startsWith(t, "consume_item 0x") || startsWith(t, "consume_item 0X")) {
        size_t hx = t.find("0x");
        if (hx == std::string::npos) hx = t.find("0X");
        int item = parseHex(t, hx + 2);
        int probe = 0;
        size_t p = t.find("probe=");
        if (p != std::string::npos) probe = std::stoi(t.substr(p + 6));
        return Expr::make("consume_item").set("item", item).set("probe", probe);
    }
    if (startsWith(t, "gold >="))
        return Expr::make("gold_at_least").set("amount", std::stoi(trim(t.substr(7))));
    if (startsWith(t, "gems >="))
        return Expr::make("gems_at_least").set("amount", std::stoi(trim(t.substr(7))));
    if (startsWith(t, "code16")) {
        size_t hx = t.find("0x");
        return Expr::make("gems_at_least").set("amount", parseHex(t, hx + 2));
    }
    if (startsWith(t, "rng_roll ")) {
        size_t dots = t.find("..");
        int mx = dots != std::string::npos ? std::stoi(t.substr(dots + 2)) : std::stoi(trim(t.substr(9)));
        return Expr::make("rng_roll").set("max", mx);
    }
    if (t == "combat_victory") return Expr::make("combat_victory");
    if (startsWith(t, "member_attr ") || startsWith(t, "class_field ")) {
        size_t a1 = t.find("arg1=0x");
        size_t a2 = t.find("arg2=0x");
        if (a1 != std::string::npos && a2 != std::string::npos)
            return Expr::make("member_attr")
                .set("arg1", parseHex(t, a1 + 7))
                .set("arg2", parseHex(t, a2 + 7));
        size_t hx = t.find("0x");
        int field = parseHex(t, hx + 2);
        int mask = field;
        size_t m = t.find("mask=0x");
        if (m != std::string::npos) mask = parseHex(t, m + 7);
        return Expr::make("member_attr").set("arg1", field).set("arg2", mask);
    }
    if (startsWith(t, "answer == \"")) {
        size_t a = t.find('"');
        size_t b = t.find('"', a + 1);
        return Expr::make("answer_eq").set("text", t.substr(a + 1, b - a - 1));
    }
    if (startsWith(t, "era in ")) {
        size_t dots = t.find("..");
        int lo = std::stoi(t.substr(7, dots - 7));
        int hi = std::stoi(t.substr(dots + 2));
        return Expr::make("era_in").set("lo", lo).set("hi", hi);
    }
    if (t == "day odd") return Expr::make("day_odd");
    if (t == "day even") return Expr::make("day_even");
    if (startsWith(t, "day in ")) {
        size_t dots = t.find("..");
        int lo = std::stoi(t.substr(7, dots - 7));
        int hi = std::stoi(t.substr(dots + 2));
        return Expr::make("day_in").set("lo", lo).set("hi", hi);
    }
    if (startsWith(t, "monster_present ") || startsWith(t, "party_has_item ")) {
        size_t h1 = t.find("0x");
        size_t h2 = t.find("0x", h1 + 1);
        int item = h2 != std::string::npos ? parseHex(t, h2 + 2) : parseHex(t, h1 + 2);
        int a = h2 != std::string::npos ? parseHex(t, h1 + 2) : 0;
        return Expr::make("party_has_item").set("a", a).set("b", item).set("item", item);
    }
    if (startsWith(t, "count_title_nibble ")) {
        size_t hx = t.find("0x");
        return Expr::make("count_title_nibble").set("id", parseHex(t, hx + 2));
    }
    if (startsWith(t, "yes_no"))
        return Expr::make("yes_no").set("mode", t.find("mode=1") != std::string::npos ? 1 : 0);
    if (startsWith(t, "cond >=")) {
        size_t hx = t.find("0x");
        return Expr::make("threshold").set("value", parseHex(t, hx + 2));
    }
    if (startsWith(t, "load_var8 ")) {
        size_t g = t.find("group=0x");
        int group = parseHex(t, g + 8);
        int idx = 0;
        size_t ix = t.find("index=0x");
        if (ix != std::string::npos) idx = parseHex(t, ix + 8);
        return Expr::make("load_var8").set("group", group).set("index", idx);
    }
    if (startsWith(t, "party_bits ")) {
        // party_bits all skills mask=0x0F
        // party_bits selected field=0x6D mask=0x0F
        // party_bits member=3 quest_flags mask=0x01
        std::istringstream iss(t.substr(11));
        std::string memTok, fieldTok, maskTok;
        iss >> memTok >> fieldTok >> maskTok;
        int member = 0;
        if (memTok == "all") member = 0;
        else if (memTok == "selected") member = 9;
        else if (startsWith(memTok, "member=")) member = std::stoi(memTok.substr(7));
        else if (startsWith(memTok, "0x")) member = parseHex(memTok, 2);
        else member = std::stoi(memTok);
        int field = -1;
        if (startsWith(fieldTok, "field=0x") || startsWith(fieldTok, "field=0X"))
            field = parseHex(fieldTok, 8);
        else if (startsWith(fieldTok, "0x") || startsWith(fieldTok, "0X"))
            field = parseHex(fieldTok, 2);
        else
            field = partyFieldByName(fieldTok);
        if (field < 0) field = 0;
        int mask = 0;
        size_t mp = t.find("mask=0x");
        if (mp == std::string::npos) mp = t.find("mask=0X");
        if (mp != std::string::npos) mask = parseHex(t, mp + 7);
        return Expr::make("party_bits").set("member", member).set("field", field).set("mask", mask);
    }
    return Expr::make("unknown").set("text", t);
}

Stmt parseStmtLine(const std::string& line) {
    std::string s = codePart(line);
    if (s.empty() || s[0] == '#') return Stmt::make("");
    if (startsWith(s, "| ") && s.size() >= 4 && s.back() == '|') {
        std::string t = s.substr(2, s.size() - 4);
        for (char& c : t)
            if (c == '@') c = '\n';
        return Stmt::make("plain_text").set("text", t);
    }
    if (s == "wait space") return Stmt::make("wait").set("kind", "space").set("mode", 0);
    if (s == "wait space mode=1" || s == "wait key")
        return Stmt::make("wait").set("kind", "space").set("mode", 1);
    if (startsWith(s, "play_sound ") || startsWith(s, "@engine_call ")) {
        size_t hx = s.find("0x");
        int id = hx != std::string::npos ? parseHex(s, hx + 2) : std::stoi(trim(s.substr(s.find(' ') + 1)));
        return Stmt::make("play_sound").set("id", id);
    }
    if (startsWith(s, "party_effect ")) {
        int mode = 0;
        size_t mp = s.find("mode=");
        if (mp != std::string::npos) mode = std::stoi(s.substr(mp + 5));
        size_t sp = s.find("sel=");
        int sel = 0;
        if (sp != std::string::npos) {
            size_t hx = s.find("0x", sp);
            sel = hx != std::string::npos ? parseHex(s, hx + 2) : std::stoi(s.substr(sp + 4));
        }
        std::vector<int> args = {sel};
        size_t ap = s.find(" args ");
        if (ap != std::string::npos) {
            std::istringstream iss(s.substr(ap + 6));
            std::string tok;
            while (iss >> tok) {
                if (startsWith(tok, "0x") || startsWith(tok, "0X")) args.push_back(parseHex(tok, 2));
                else args.push_back(std::stoi(tok));
            }
        }
        while (static_cast<int>(args.size()) < 6) args.push_back(0);
        return Stmt::make("party_effect").set("mode", mode).set("sel", sel).setList("args", args);
    }
    if (startsWith(s, "treasure ")) {
        std::istringstream iss(s.substr(9));
        std::vector<int> data;
        std::string tok;
        while (iss >> tok) {
            if (startsWith(tok, "0x") || startsWith(tok, "0X")) data.push_back(parseHex(tok, 2));
            else data.push_back(std::stoi(tok));
        }
        return Stmt::make("treasure").setList("data", data);
    }
    if (startsWith(s, "add_counter "))
        return Stmt::make("add_counter").set("amount", std::stoi(trim(s.substr(12))));
    if (startsWith(s, "or_member_field ")) {
        size_t a1 = s.find("arg1=0x");
        size_t a2 = s.find("arg2=0x");
        return Stmt::make("or_member_field")
            .set("arg1", parseHex(s, a1 + 7))
            .set("arg2", parseHex(s, a2 + 7));
    }
    if (startsWith(s, "party_damage ")) {
        size_t mp = s.find("member=0x");
        size_t vp = s.find("value=");
        return Stmt::make("party_damage")
            .set("member", parseHex(s, mp + 9))
            .set("value", std::stoi(s.substr(vp + 6)));
    }
    if (startsWith(s, "audio_wait "))
        return Stmt::make("audio_wait").set("index", std::stoi(trim(s.substr(11))));
    if (s == "ask yes_no") return Stmt::make("ask_yes_no").set("mode", 0);
    if (s == "ask yes_no mode=1") return Stmt::make("ask_yes_no").set("mode", 1);
    if (s == "clear_input" || s == "read_answer") return Stmt::make("read_answer");
    if (s == "clear_tile_event") return Stmt::make("clear_tile_event");
    if (s == "abort") return Stmt::make("abort");
    if (s == "end") return Stmt::make("end");
    if (s == "select_member") return Stmt::make("select_member").set("mode", 0);
    if (s == "select_member mode=1") return Stmt::make("select_member").set("mode", 1);
    if (startsWith(s, "give_item ")) {
        auto grabHex = [&](const char* key) -> int {
            size_t p = s.find(key);
            if (p == std::string::npos) return 0;
            size_t hx = s.find("0x", p);
            if (hx == std::string::npos) hx = s.find("0X", p);
            return hx == std::string::npos ? 0 : parseHex(s, hx + 2);
        };
        auto grabInt = [&](const char* key) -> int {
            size_t p = s.find(key);
            if (p == std::string::npos) return 0;
            return std::stoi(s.substr(p + std::char_traits<char>::length(key)));
        };
        return Stmt::make("give_item")
            .set("item", grabHex("item="))
            .set("member", grabInt("member="))
            .set("charges", grabInt("charges="))
            .set("flags", grabHex("flags="));
    }
    if (startsWith(s, "set_party_bits ") || startsWith(s, "party_bits ")) {
        // Statement form of OP_15 (rare) or OP_18 set:
        // set_party_bits selected skills and=0xF0 or=0x03
        // party_bits all quest_flags mask=0x01   (OP_15 as stmt)
        const bool isSet = startsWith(s, "set_party_bits ");
        std::string rest = s.substr(isSet ? 15 : 11);
        std::istringstream iss(rest);
        std::string memTok, fieldTok;
        iss >> memTok >> fieldTok;
        int member = 0;
        if (memTok == "all") member = 0;
        else if (memTok == "selected") member = 9;
        else if (startsWith(memTok, "member=")) member = std::stoi(memTok.substr(7));
        else if (startsWith(memTok, "0x")) member = parseHex(memTok, 2);
        else
            try {
                member = std::stoi(memTok);
            } catch (...) {
            }
        int field = -1;
        if (startsWith(fieldTok, "field=0x") || startsWith(fieldTok, "field=0X"))
            field = parseHex(fieldTok, 8);
        else if (startsWith(fieldTok, "0x"))
            field = parseHex(fieldTok, 2);
        else
            field = partyFieldByName(fieldTok);
        if (field < 0) field = 0;
        if (isSet) {
            auto grab = [&](const char* key) -> int {
                size_t p = s.find(key);
                if (p == std::string::npos) return 0;
                size_t hx = s.find("0x", p);
                if (hx == std::string::npos) hx = s.find("0X", p);
                return hx == std::string::npos ? 0 : parseHex(s, hx + 2);
            };
            return Stmt::make("set_party_bits")
                .set("member", member)
                .set("field", field)
                .set("and", grab("and="))
                .set("or", grab("or="));
        }
        int mask = 0;
        size_t mp = s.find("mask=0x");
        if (mp == std::string::npos) mp = s.find("mask=0X");
        if (mp != std::string::npos) mask = parseHex(s, mp + 7);
        return Stmt::make("apply_party")
            .set("count", member)
            .set("op", field)
            .set("val", mask);
    }
    if (s == "set quest_complete") return Stmt::make("set_quest_complete");
    if (startsWith(s, "set quest_flag ")) {
        std::istringstream iss(s.substr(15));
        std::string name, val = "ok";
        iss >> name >> val;
        return Stmt::make("set_quest_flag").set("name", name).set("value", val);
    }
    if (startsWith(s, "selector 0x") || startsWith(s, "selector 0X")) {
        size_t hx = s.find("0x");
        if (hx == std::string::npos) hx = s.find("0X");
        return Stmt::make("selector").set("value", parseHex(s, hx + 2));
    }
    if (startsWith(s, "engine 0x") || startsWith(s, "engine 0X")) {
        size_t hx = s.find("0x");
        if (hx == std::string::npos) hx = s.find("0X");
        return Stmt::make("selector").set("value", parseHex(s, hx + 2));
    }
    if (startsWith(s, "overlay ")) {
        // overlay 61 event_20   OR   overlay 61.20
        int cat = 0, evt = 0;
        if (std::sscanf(s.c_str(), "overlay %d event_%d", &cat, &evt) == 2 ||
            std::sscanf(s.c_str(), "overlay %d.%d", &cat, &evt) == 2) {
            int sel = selectorFromOverlayBin(cat, evt);
            if (sel < 0) sel = 0;
            return Stmt::make("selector").set("value", sel);
        }
    }
    if (startsWith(s, "shop "))
        return Stmt::make("shop").set("name", trim(s.substr(5))).set("selector", 0);
    if (startsWith(s, "quest "))
        return Stmt::make("quest").set("name", trim(s.substr(6))).set("selector", 0);
    if (startsWith(s, "service_title ")) {
        // Preferred: service_title sign=0xNN mode=M  (OP_0B sign-table index)
        size_t sp = s.find("sign=0x");
        if (sp == std::string::npos) sp = s.find("sign=0X");
        int mode = 0;
        size_t mp = s.find("mode=");
        if (mp != std::string::npos) mode = std::stoi(s.substr(mp + 5));
        if (sp != std::string::npos) {
            return Stmt::make("service_title")
                .set("sign", parseHex(s, sp + 7))
                .set("mode", mode);
        }
        // Legacy: service_title sN mode=M — treat sN as literal sign index, not string pool.
        std::istringstream iss(s.substr(14));
        std::string ref, modeTok;
        iss >> ref >> modeTok;
        if (startsWith(modeTok, "mode=")) mode = std::stoi(modeTok.substr(5));
        int sign = 0;
        if (!ref.empty() && ref[0] == 's') {
            try {
                sign = std::stoi(ref.substr(1));
            } catch (...) {
            }
        } else if (startsWith(ref, "0x") || startsWith(ref, "0X")) {
            sign = parseHex(ref, 2);
        }
        return Stmt::make("service_title").set("sign", sign).set("mode", mode);
    }
    if (startsWith(s, "say_door ")) return Stmt::make("say").set("variant", "door").set("string", trim(s.substr(9)));
    if (startsWith(s, "say_block "))
        return Stmt::make("say").set("variant", "block").set("string", trim(s.substr(10)));
    if (startsWith(s, "say_popup "))
        return Stmt::make("say").set("variant", "popup_a").set("string", trim(s.substr(10)));
    if (startsWith(s, "say ")) return Stmt::make("say").set("variant", "").set("string", trim(s.substr(4)));
    if (startsWith(s, "go_to ")) {
        // go_to screen N pos 0xNN
        size_t sc = s.find("screen ");
        size_t ps = s.find("pos 0x");
        int screen = std::stoi(s.substr(sc + 7));
        int pos = parseHex(s, ps + 6);
        return Stmt::make("go_to").set("screen", screen).set("pos", pos);
    }
    if (startsWith(s, "delay ")) return Stmt::make("delay").set("ticks", std::stoi(trim(s.substr(6))));
    if (startsWith(s, "load_var8 ")) {
        size_t g = s.find("group=0x");
        int group = parseHex(s, g + 8);
        int idx = 0;
        size_t ix = s.find("index=0x");
        if (ix != std::string::npos) idx = parseHex(s, ix + 8);
        return Stmt::make("load_var8").set("group", group).set("index", idx);
    }
    if (startsWith(s, "store_var8 ")) {
        size_t g = s.find("group=0x");
        size_t v = s.find("value=0x");
        return Stmt::make("store_var8")
            .set("group", parseHex(s, g + 8))
            .set("value", parseHex(s, v + 8));
    }
    if (startsWith(s, "@op ")) {
        std::istringstream iss(s.substr(4));
        std::string opTok;
        iss >> opTok;
        int op = parseHex(opTok, opTok[1] == 'x' || opTok[1] == 'X' ? 2 : 0);
        if (startsWith(opTok, "0x") || startsWith(opTok, "0X")) op = parseHex(opTok, 2);
        std::vector<int> args;
        std::string tok;
        while (iss >> tok) {
            if (startsWith(tok, "0x") || startsWith(tok, "0X")) args.push_back(parseHex(tok, 2));
            else args.push_back(std::stoi(tok));
        }
        return Stmt::make("raw_op").set("op", op).setList("args", args);
    }
    if (startsWith(s, "@apply_party_masked ")) {
        // count= set= and= or=
        auto grab = [&](const char* key) -> int {
            std::string k = key;
            size_t p = s.find(k);
            if (p == std::string::npos) return 0;
            size_t hx = s.find("0x", p);
            return parseHex(s, hx + 2);
        };
        return Stmt::make("apply_party_masked")
            .set("count", grab("count="))
            .set("set", grab("set="))
            .set("and", grab("and="))
            .set("or", grab("or="));
    }
    if (startsWith(s, "@apply_party ")) {
        auto grab = [&](const char* key) -> int {
            size_t p = s.find(key);
            size_t hx = s.find("0x", p);
            return parseHex(s, hx + 2);
        };
        return Stmt::make("apply_party")
            .set("count", grab("count="))
            .set("op", grab("op="))
            .set("val", grab("val="));
    }
    if (startsWith(s, "@engine_call ")) {
        size_t hx = s.find("0x");
        return Stmt::make("engine_call").set("code", parseHex(s, hx + 2));
    }
    if (startsWith(s, "set_tile ")) {
        // set_tile (y,x) 0xA 0xB
        size_t lp = s.find('(');
        size_t comma = s.find(',', lp);
        size_t rp = s.find(')', comma);
        int y = std::stoi(s.substr(lp + 1, comma - lp - 1));
        int x = std::stoi(s.substr(comma + 1, rp - comma - 1));
        size_t h1 = s.find("0x", rp);
        size_t h2 = s.find("0x", h1 + 1);
        return Stmt::make("set_tile")
            .set("y", y)
            .set("x", x)
            .set("a", parseHex(s, h1 + 2))
            .set("b", parseHex(s, h2 + 2));
    }
    if (startsWith(s, "fight monsters ")) {
        std::vector<int> monsters, flags;
        std::istringstream iss(s.substr(15));
        std::string tok;
        bool inFlags = false;
        while (iss >> tok) {
            if (tok == "flags") {
                inFlags = true;
                continue;
            }
            int v = (startsWith(tok, "0x") || startsWith(tok, "0X")) ? parseHex(tok, 2) : std::stoi(tok);
            if (inFlags) flags.push_back(v);
            else monsters.push_back(v);
        }
        return Stmt::make("fight").setList("monsters", monsters).setList("flags", flags);
    }
    if (startsWith(s, "fight_b ")) {
        std::vector<int> data;
        std::istringstream iss(s.substr(8));
        std::string tok;
        while (iss >> tok) {
            data.push_back((startsWith(tok, "0x") || startsWith(tok, "0X")) ? parseHex(tok, 2)
                                                                            : std::stoi(tok));
        }
        return Stmt::make("fight_b").setList("data", data);
    }
    return Stmt::make("unknown_line").set("text", s);
}

std::pair<std::vector<Stmt>, int> parseBody(const std::vector<std::string>& lines, int start,
                                            int baseIndent) {
    std::vector<Stmt> body;
    int i = start;
    while (i < static_cast<int>(lines.size())) {
        const std::string& raw = lines[i];
        if (trim(raw).empty()) {
            ++i;
            continue;
        }
        int lvl = indentLevel(raw);
        if (lvl < baseIndent) break;
        if (lvl == 0 && baseIndent > 0 && raw[0] != ' ') break;

        std::string rel = trim(raw);
        if (startsWith(rel, "if ")) {
            std::string condText = rel.substr(3);
            if (!condText.empty() && condText.back() == ':') condText.pop_back();
            Expr cond = parseExpr(condText);
            auto thenPair = parseBody(lines, i + 1, baseIndent + 1);
            i = thenPair.second;
            std::vector<Stmt> elseBody;
            if (i < static_cast<int>(lines.size()) && trim(lines[i]) == "else:") {
                auto elsePair = parseBody(lines, i + 1, baseIndent + 1);
                elseBody = std::move(elsePair.first);
                i = elsePair.second;
            }
            Stmt ifs = Stmt::make("if");
            ifs.cond = cond;
            ifs.thenBody = std::move(thenPair.first);
            ifs.elseBody = std::move(elseBody);
            body.push_back(std::move(ifs));
            continue;
        }
        if (rel == "else:") break;

        Stmt st = parseStmtLine(raw);
        if (!st.kind.empty()) body.push_back(std::move(st));
        ++i;
    }
    return {body, i};
}

}  // namespace

ParseResult parseLocationText(const std::string& text) {
    ParseResult result;
    result.ok = true;
    Location& loc = result.loc;
    loc.modified = true;
    loc.terminated = true;
    loc.recordKind = RecordKind::Standard;

    std::vector<std::string> lines;
    {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
    }

    std::unordered_map<std::string, int> scriptMap;
    int i = 0;
    try {
        while (i < static_cast<int>(lines.size())) {
            std::string stripped = trim(lines[i]);
            if (stripped.empty() || stripped[0] == '#') {
                ++i;
                continue;
            }
            if (startsWith(stripped, "location ")) {
                loc.id = std::stoi(stripped.substr(9));
                ++i;
                continue;
            }
            if (startsWith(stripped, "record ")) {
                loc.recordKind = recordKindFromName(trim(stripped.substr(7)));
                ++i;
                continue;
            }
            if (stripped == "raw_record:") {
                ++i;
                std::vector<uint8_t> blob;
                while (i < static_cast<int>(lines.size()) &&
                       (lines[i].empty() || lines[i][0] == ' ')) {
                    std::istringstream iss(lines[i]);
                    std::string tok;
                    while (iss >> tok) blob.push_back(static_cast<uint8_t>(parseHex(tok, 0)));
                    ++i;
                }
                loc.rawBlob = std::move(blob);
                continue;
            }
            if (stripped == "strings:") {
                ++i;
                int idx = 0;
                while (i < static_cast<int>(lines.size())) {
                    std::string sl = trim(lines[i]);
                    if (sl.empty() || startsWith(sl, "on ") || startsWith(sl, "script ")) break;
                    // name: "text"
                    size_t colon = sl.find(':');
                    if (colon == std::string::npos) break;
                    std::string name = trim(sl.substr(0, colon));
                    std::string rest = trim(sl.substr(colon + 1));
                    if (rest.empty()) {
                        // block form
                        ++i;
                        if (i < static_cast<int>(lines.size()) &&
                            trim(lines[i]).find("\"\"\"") != std::string::npos) {
                            std::string block = trim(lines[i]);
                            size_t a = block.find("\"\"\"");
                            size_t b = block.find("\"\"\"", a + 3);
                            std::string textVal;
                            if (b != std::string::npos) {
                                textVal = block.substr(a + 3, b - a - 3);
                            } else {
                                textVal = block.substr(a + 3);
                                ++i;
                                while (i < static_cast<int>(lines.size()) &&
                                       lines[i].find("\"\"\"") == std::string::npos) {
                                    textVal += trim(lines[i]);
                                    ++i;
                                }
                                if (i < static_cast<int>(lines.size())) {
                                    auto last = lines[i].substr(0, lines[i].find("\"\"\""));
                                    if (!trim(last).empty()) textVal += trim(last);
                                }
                            }
                            for (char& c : textVal)
                                if (c == '@') c = '\n';
                            StringDef sd;
                            sd.name = name;
                            sd.text = textVal;
                            sd.index = idx++;
                            loc.strings.push_back(std::move(sd));
                            ++i;
                            continue;
                        }
                        break;
                    }
                    if (rest.size() >= 2 && rest.front() == '"' && rest.back() == '"') {
                        std::string textVal = rest.substr(1, rest.size() - 2);
                        for (char& c : textVal)
                            if (c == '@') c = '\n';
                        StringDef sd;
                        sd.name = name;
                        sd.text = textVal;
                        sd.index = idx++;
                        loc.strings.push_back(std::move(sd));
                        ++i;
                        continue;
                    }
                    break;
                }
                continue;
            }
            if (startsWith(stripped, "on tile ")) {
                // on tile (y, x) cond -> name  @event N
                size_t lp = stripped.find('(');
                size_t comma = stripped.find(',', lp);
                size_t rp = stripped.find(')', comma);
                int y = std::stoi(stripped.substr(lp + 1, comma - lp - 1));
                int x = std::stoi(trim(stripped.substr(comma + 1, rp - comma - 1)));
                size_t arrow = stripped.find("->", rp);
                std::string mid = trim(stripped.substr(rp + 1, arrow - rp - 1));
                uint8_t raw = 0;
                TriggerCond cond = parseCond(mid, raw);
                std::string rest = trim(stripped.substr(arrow + 2));
                std::string name;
                int eid = 0;
                size_t at = rest.find("@event");
                if (at != std::string::npos) {
                    name = trim(rest.substr(0, at));
                    eid = std::stoi(trim(rest.substr(at + 6)));
                } else {
                    name = rest;
                    eid = scriptMap.count(name) ? scriptMap[name] : 0;
                }
                scriptMap[name] = eid;
                Trigger t;
                t.y = y;
                t.x = x;
                t.cond = cond;
                t.condRaw = raw ? raw : triggerCondByte(cond, 0);
                t.scriptName = name;
                t.eventId = eid;
                loc.triggers.push_back(std::move(t));
                ++i;
                continue;
            }
            if (startsWith(stripped, "script ")) {
                // script name:  @event N
                size_t colon = stripped.find(':');
                std::string name = trim(stripped.substr(7, colon - 7));
                int eid = scriptMap.count(name) ? scriptMap[name]
                                                : static_cast<int>(loc.scripts.size());
                size_t at = stripped.find("@event");
                if (at != std::string::npos) eid = std::stoi(trim(stripped.substr(at + 6)));
                scriptMap[name] = eid;
                auto bodyPair = parseBody(lines, i + 1, 1);
                i = bodyPair.second;
                Script sc;
                sc.name = name;
                sc.eventId = eid;
                sc.body = std::move(bodyPair.first);
                sc.isPlainText = !sc.body.empty() && sc.body[0].kind == "plain_text";
                loc.scripts.push_back(std::move(sc));
                continue;
            }
            ++i;
        }
    } catch (const std::exception& ex) {
        result.ok = false;
        result.error = ex.what();
        return result;
    }

    if (loc.recordKind == RecordKind::CastleBlob && loc.rawBlob.empty()) {
        // keep empty
    }
    return result;
}

EventFileAst parseFileText(const std::string& text) {
    EventFileAst file;
    // Split on lines starting with "location "
    std::vector<std::string> chunks;
    std::string cur;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trim(line);
        if (startsWith(t, "location ") && !cur.empty()) {
            chunks.push_back(cur);
            cur.clear();
        }
        cur += line;
        cur += '\n';
    }
    if (!cur.empty()) chunks.push_back(cur);
    for (const auto& c : chunks) {
        auto r = parseLocationText(c);
        if (r.ok) file.locations.push_back(std::move(r.loc));
    }
    return file;
}

}  // namespace mm2::eventlang

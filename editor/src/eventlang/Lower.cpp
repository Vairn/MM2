#include "eventlang/Lower.h"

#include "eventlang/Semantics.h"

namespace mm2::eventlang {
namespace {

// Hand-author sugar for OP_18 masked party writes (not used on decompile).
// Matches tools/mm2_event_lang/semantic_tables.py QUEST_FLAG_PATTERNS.
struct QuestFlagPat {
    uint8_t setB, andM, orM;
    const char* name;
    const char* value;
};
constexpr QuestFlagPat kQuestFlagPatterns[] = {
    {0x75, 0xFE, 0x01, "quest_complete", "ok"},
    {0x75, 0xFB, 0x04, "ancients_code", "ok"},
    {0x75, 0x73, 0x80, "sorcerer_stasis", "ybmug"},
    {0x75, 0x00, 0x03, "sorcerer_stasis", "both_freed"},
    {0x75, 0xFD, 0x02, "sorcerer_stasis", "ybmug_freed"},
};

std::vector<uint8_t> encodeAnswer(const std::string& text) {
    std::vector<uint8_t> out(10, 0xFA);
    for (size_t i = 0; i < text.size() && i < 10; ++i)
        out[i] = static_cast<uint8_t>((0x11A - static_cast<unsigned char>(text[i])) & 0xFF);
    return out;
}

int resolveString(const Stmt& stmt, const std::unordered_map<std::string, int>& stringIndex) {
    auto it = stmt.num.find("string");
    if (it != stmt.num.end()) return it->second;
    auto sit = stmt.str.find("string");
    if (sit != stmt.str.end()) {
        auto found = stringIndex.find(sit->second);
        if (found != stringIndex.end()) return found->second;
        if (!sit->second.empty() && sit->second[0] == 's') {
            try {
                return std::stoi(sit->second.substr(1));
            } catch (...) {
            }
        }
    }
    return 0;
}

// OP_0B sign index: prefer numeric `sign`, else parse sign= / raw int / sN as literal index
// (NOT string-table lookup — ASM 0x15756 uses a per-env signboard table).
int resolveSignIndex(const Stmt& stmt) {
    if (stmt.num.count("sign")) return stmt.getNum("sign");
    if (stmt.num.count("string")) return stmt.getNum("string");  // legacy decompile field
    const std::string& ref = stmt.getStr("sign");
    if (!ref.empty()) {
        if (ref.size() > 2 && (ref[0] == '0') && (ref[1] == 'x' || ref[1] == 'X'))
            return static_cast<int>(std::stoul(ref.substr(2), nullptr, 16));
        try {
            return std::stoi(ref);
        } catch (...) {
        }
    }
    const std::string& s = stmt.getStr("string");
    if (!s.empty()) {
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            return static_cast<int>(std::stoul(s.substr(2), nullptr, 16));
        if (s[0] == 's') {
            try {
                return std::stoi(s.substr(1));
            } catch (...) {
            }
        }
        try {
            return std::stoi(s);
        } catch (...) {
        }
    }
    return 0;
}

}  // namespace

std::vector<LoweredOp> exprToCondOps(const Expr& expr) {
    const std::string& k = expr.kind;
    if (k == "class_field" || k == "member_attr") {
        // OP_2D: arg1 packed field/mode, arg2 alt nibble (EventRuntime 0x16DBA).
        int a1 = expr.getNum("arg1", expr.getNum("field"));
        int a2 = expr.getNum("arg2", expr.getNum("mask", a1));
        return {LoweredOp{0x2D, {static_cast<uint8_t>(a1), static_cast<uint8_t>(a2)}, 0}};
    }
    if (k == "class_is") {
        int field = classFieldByName(expr.getStr("class"));
        return {LoweredOp{0x2D, {static_cast<uint8_t>(field), static_cast<uint8_t>(field)}, 0}};
    }
    if (k == "gold_at_least") {
        int v = expr.getNum("amount");
        return {LoweredOp{0x24, {static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF)},
                          0}};
    }
    if (k == "gems_at_least" || k == "code16") {
        int v = expr.getNum("amount", expr.getNum("value"));
        return {LoweredOp{0x25, {static_cast<uint8_t>((v >> 8) & 0xFF), static_cast<uint8_t>(v & 0xFF)},
                          0}};
    }
    if (k == "answer_eq") {
        auto enc = encodeAnswer(expr.getStr("text"));
        return {LoweredOp{0x30, enc, 0}};
    }
    if (k == "consume_item" || k == "has_item_id" || k == "has_item")
        return {LoweredOp{0x28,
                           {static_cast<uint8_t>(expr.getNum("probe", 0)),
                            static_cast<uint8_t>(expr.getNum("item"))},
                           0}};
    if (k == "era_in")
        return {LoweredOp{0x22,
                           {static_cast<uint8_t>(expr.getNum("lo")),
                            static_cast<uint8_t>(expr.getNum("hi"))},
                           0}};
    if (k == "day_in")
        return {LoweredOp{0x23,
                           {static_cast<uint8_t>(expr.getNum("lo")),
                            static_cast<uint8_t>(expr.getNum("hi"))},
                           0}};
    if (k == "day_odd") return {LoweredOp{0x23, {0xB5, 0x00}, 0}};
    if (k == "day_even") return {LoweredOp{0x23, {0xB6, 0x00}, 0}};
    if (k == "monster_present" || k == "party_has_item")
        return {LoweredOp{0x16,
                           {static_cast<uint8_t>(expr.getNum("a", 0)),
                            static_cast<uint8_t>(expr.getNum("item", expr.getNum("b")))},
                           0}};
    if (k == "count_title_nibble")
        return {LoweredOp{0x32, {static_cast<uint8_t>(expr.getNum("id"))}, 0}};
    if (k == "party_bits")
        return {LoweredOp{0x15,
                           {static_cast<uint8_t>(expr.getNum("member")),
                            static_cast<uint8_t>(expr.getNum("field")),
                            static_cast<uint8_t>(expr.getNum("mask"))},
                           0}};
    if (k == "rng_roll")
        return {LoweredOp{0x1C, {static_cast<uint8_t>(expr.getNum("max"))}, 0}};
    if (k == "give_item_ok")
        return {LoweredOp{0x19,
                           {static_cast<uint8_t>(expr.getNum("member")),
                            static_cast<uint8_t>(expr.getNum("item")),
                            static_cast<uint8_t>(expr.getNum("charges")),
                            static_cast<uint8_t>(expr.getNum("flags"))},
                           0}};
    if (k == "party_effect_ok") {
        std::vector<uint8_t> args;
        auto it = expr.lists.find("args");
        if (it != expr.lists.end())
            for (int x : it->second) args.push_back(static_cast<uint8_t>(x));
        while (args.size() < 6) args.push_back(0);
        return {LoweredOp{static_cast<uint8_t>(expr.getNum("mode") ? 0x20 : 0x1F), args, 0}};
    }
    if (k == "prior_cond" || k == "unknown")
        return {};  // cond already produced by a prior stmt / sticky VM flag
    if (k == "combat_victory")
        return {};  // OP_2B is emitted from the if stmt, not as a cond-op prefix
    if (k == "yes_no")
        return {LoweredOp{static_cast<uint8_t>(expr.getNum("mode") ? 0x0A : 0x09), {}, 0}};
    if (k == "threshold")
        return {LoweredOp{0x1B, {static_cast<uint8_t>(expr.getNum("value"))}, 0}};
    if (k == "load_var8")
        return {LoweredOp{0x17,
                           {static_cast<uint8_t>(expr.getNum("group")),
                            static_cast<uint8_t>(expr.getNum("index", 0))},
                           0}};
    if (k == "raw_op") {
        std::vector<uint8_t> args;
        auto it = expr.lists.find("args");
        if (it != expr.lists.end())
            for (int x : it->second) args.push_back(static_cast<uint8_t>(x));
        return {LoweredOp{static_cast<uint8_t>(expr.getNum("op")), args, 0}};
    }
    return {};
}

std::vector<LoweredOp> stmtToOps(const Stmt& stmt,
                                 const std::unordered_map<std::string, int>& stringIndex);

std::vector<LoweredOp> lowerStmts(const std::vector<Stmt>& stmts,
                                  const std::unordered_map<std::string, int>& stringIndex) {
    std::vector<LoweredOp> out;
    for (const auto& s : stmts) {
        auto part = stmtToOps(s, stringIndex);
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

std::vector<LoweredOp> stmtToOps(const Stmt& stmt,
                                 const std::unordered_map<std::string, int>& stringIndex) {
    const std::string& k = stmt.kind;
    if (k == "say") {
        uint8_t op = sayOpForVariant(stmt.getStr("variant"));
        return {LoweredOp{op, {static_cast<uint8_t>(resolveString(stmt, stringIndex))}, 0}};
    }
    if (k == "service_title")
        return {LoweredOp{0x0B,
                           {static_cast<uint8_t>(resolveSignIndex(stmt)),
                            static_cast<uint8_t>(stmt.getNum("mode"))},
                           0}};
    if (k == "selector")
        return {LoweredOp{0x0E, {static_cast<uint8_t>(stmt.getNum("value"))}, 0}};
    if (k == "wait") {
        // OP_07 space; OP_08 space+SCRIPTED_KEY_MODE. Legacy "key" → OP_08.
        const bool scripted = stmt.getNum("mode") != 0 || stmt.getStr("kind") == "key";
        return {LoweredOp{static_cast<uint8_t>(scripted ? 0x08 : 0x07), {}, 0}};
    }
    if (k == "ask_yes_no")
        return {LoweredOp{static_cast<uint8_t>(stmt.getNum("mode") ? 0x0A : 0x09), {}, 0}};
    // Exact token skips (fidelity-first linear DSL). N is author/decompile-provided.
    if (k == "skip_if_true")
        return {LoweredOp{0x10, {static_cast<uint8_t>(stmt.getNum("n"))}, 0}};
    if (k == "skip_if_false")
        return {LoweredOp{0x11, {static_cast<uint8_t>(stmt.getNum("n"))}, 0}};
    if (k == "skip_if_victory")
        return {LoweredOp{0x2B, {static_cast<uint8_t>(stmt.getNum("n"))}, 0}};
    if (k == "set_cond") return exprToCondOps(stmt.cond);
    if (k == "if") {
        auto thenOps = lowerStmts(stmt.thenBody, stringIndex);
        auto elseOps = lowerStmts(stmt.elseBody, stringIndex);
        // OP_2B: skip N when combat victory (same shape as OP_10 else-only).
        if (stmt.cond.kind == "combat_victory") {
            std::vector<LoweredOp> out;
            const auto& skipped = !elseOps.empty() ? elseOps : thenOps;
            // Prefer explicit skip count when present (shared-tail fidelity).
            const int n = stmt.getNum("n", static_cast<int>(skipped.size()));
            out.push_back(LoweredOp{0x2B, {static_cast<uint8_t>(n)}, 0});
            out.insert(out.end(), skipped.begin(), skipped.end());
            return out;
        }
        auto condOps = exprToCondOps(stmt.cond);
        std::vector<LoweredOp> out = condOps;
        if (!thenOps.empty() && !elseOps.empty()) {
            const int nThen = stmt.getNum("n_then", static_cast<int>(thenOps.size() + 1));
            const int nElse = stmt.getNum("n_else", static_cast<int>(elseOps.size()));
            out.push_back(LoweredOp{0x11, {static_cast<uint8_t>(nThen)}, 0});
            out.insert(out.end(), thenOps.begin(), thenOps.end());
            out.push_back(LoweredOp{0x10, {static_cast<uint8_t>(nElse)}, 0});
            out.insert(out.end(), elseOps.begin(), elseOps.end());
        } else if (!thenOps.empty()) {
            const int n = stmt.getNum("n", static_cast<int>(thenOps.size()));
            out.push_back(LoweredOp{0x11, {static_cast<uint8_t>(n)}, 0});
            out.insert(out.end(), thenOps.begin(), thenOps.end());
        } else if (!elseOps.empty()) {
            const int n = stmt.getNum("n", static_cast<int>(elseOps.size()));
            out.push_back(LoweredOp{0x10, {static_cast<uint8_t>(n)}, 0});
            out.insert(out.end(), elseOps.begin(), elseOps.end());
        }
        return out;
    }
    if (k == "set_quest_complete")
        return {LoweredOp{0x18, {0x00, 0x75, 0xFE, 0x01}, 0}};
    if (k == "set_quest_flag") {
        const std::string name = stmt.getStr("name");
        const std::string val = stmt.getStr("value", "ok");
        for (const auto& p : kQuestFlagPatterns) {
            if (name == p.name && val == p.value)
                return {LoweredOp{0x18, {0x00, p.setB, p.andM, p.orM}, 0}};
        }
        return {LoweredOp{0x18, {0x00, 0x75, 0x00, 0x01}, 0}};
    }
    if (k == "apply_party")
        return {LoweredOp{0x15,
                           {static_cast<uint8_t>(stmt.getNum("count", stmt.getNum("member"))),
                            static_cast<uint8_t>(stmt.getNum("op", stmt.getNum("field"))),
                            static_cast<uint8_t>(stmt.getNum("val", stmt.getNum("mask")))},
                           0}};
    if (k == "apply_party_masked" || k == "set_party_bits")
        return {LoweredOp{0x18,
                           {static_cast<uint8_t>(stmt.getNum("count", stmt.getNum("member"))),
                            static_cast<uint8_t>(stmt.getNum("set", stmt.getNum("field"))),
                            static_cast<uint8_t>(stmt.getNum("and")),
                            static_cast<uint8_t>(stmt.getNum("or"))},
                           0}};
    if (k == "select_member")
        return {LoweredOp{static_cast<uint8_t>(stmt.getNum("mode") ? 0x27 : 0x26), {}, 0}};
    if (k == "give_item")
        return {LoweredOp{0x19,
                           {static_cast<uint8_t>(stmt.getNum("member")),
                            static_cast<uint8_t>(stmt.getNum("item")),
                            static_cast<uint8_t>(stmt.getNum("charges")),
                            static_cast<uint8_t>(stmt.getNum("flags"))},
                           0}};
    if (k == "shop") {
        int sel = stmt.getNum("selector");
        if (sel <= 0) sel = selectorByShopOrQuest("shop", stmt.getStr("name"));
        if (sel < 0) return {};  // unknown name: refuse silent wrong default
        return {LoweredOp{0x0E, {static_cast<uint8_t>(sel)}, 0}};
    }
    if (k == "quest") {
        int sel = stmt.getNum("selector");
        if (sel <= 0) sel = selectorByShopOrQuest("quest", stmt.getStr("name"));
        if (sel < 0) return {};
        return {LoweredOp{0x0E, {static_cast<uint8_t>(sel)}, 0}};
    }
    if (k == "engine_call" || k == "play_sound")
        return {LoweredOp{0x0D, {static_cast<uint8_t>(stmt.getNum("code", stmt.getNum("id")))}, 0}};
    if (k == "party_effect") {
        std::vector<uint8_t> args;
        auto it = stmt.lists.find("args");
        if (it != stmt.lists.end())
            for (int x : it->second) args.push_back(static_cast<uint8_t>(x));
        while (args.size() < 6) args.push_back(0);
        return {LoweredOp{static_cast<uint8_t>(stmt.getNum("mode") ? 0x20 : 0x1F), args, 0}};
    }
    if (k == "treasure") {
        std::vector<uint8_t> args;
        auto it = stmt.lists.find("data");
        if (it != stmt.lists.end())
            for (int x : it->second) args.push_back(static_cast<uint8_t>(x));
        return {LoweredOp{0x2A, args, 0}};
    }
    if (k == "add_counter")
        return {LoweredOp{0x2C, {static_cast<uint8_t>(stmt.getNum("amount"))}, 0}};
    if (k == "or_member_field")
        return {LoweredOp{0x2E,
                           {static_cast<uint8_t>(stmt.getNum("arg1")),
                            static_cast<uint8_t>(stmt.getNum("arg2"))},
                           0}};
    if (k == "party_damage") {
        int v = stmt.getNum("value");
        return {LoweredOp{0x31,
                           {static_cast<uint8_t>(stmt.getNum("member")),
                            static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF)},
                           0}};
    }
    if (k == "audio_wait")
        return {LoweredOp{0x1D, {static_cast<uint8_t>(stmt.getNum("index"))}, 0}};
    // Dead tails after go_to/abort/end still exist in retail bytecode — re-emit.
    if (k == "unreachable") return lowerStmts(stmt.body, stringIndex);
    if (k == "go_to")
        return {LoweredOp{0x0C,
                           {static_cast<uint8_t>(stmt.getNum("screen")),
                            static_cast<uint8_t>(stmt.getNum("pos"))},
                           0}};
    if (k == "fight") {
        std::vector<uint8_t> args;
        auto mit = stmt.lists.find("monsters");
        if (mit != stmt.lists.end())
            for (int x : mit->second) args.push_back(static_cast<uint8_t>(x));
        auto fit = stmt.lists.find("flags");
        if (fit != stmt.lists.end())
            for (int x : fit->second) args.push_back(static_cast<uint8_t>(x));
        return {LoweredOp{0x12, args, 0}};
    }
    if (k == "fight_b") {
        std::vector<uint8_t> args;
        auto it = stmt.lists.find("data");
        if (it != stmt.lists.end())
            for (int x : it->second) args.push_back(static_cast<uint8_t>(x));
        return {LoweredOp{0x13, args, 0}};
    }
    if (k == "set_tile") {
        uint8_t pos = static_cast<uint8_t>(((stmt.getNum("y") & 0xF) << 4) | (stmt.getNum("x") & 0xF));
        return {LoweredOp{0x21,
                           {pos, static_cast<uint8_t>(stmt.getNum("a")),
                            static_cast<uint8_t>(stmt.getNum("b"))},
                           0}};
    }
    if (k == "clear_tile_event") return {LoweredOp{0x14, {}, 0}};
    if (k == "clear_input" || k == "read_answer") return {LoweredOp{0x2F, {}, 0}};
    if (k == "abort") return {LoweredOp{0x29, {}, 0}};
    if (k == "end") return {LoweredOp{0x0F, {}, 0}};
    if (k == "delay") return {LoweredOp{0x1E, {static_cast<uint8_t>(stmt.getNum("ticks"))}, 0}};
    if (k == "load_var8")
        return {LoweredOp{0x17,
                           {static_cast<uint8_t>(stmt.getNum("group")),
                            static_cast<uint8_t>(stmt.getNum("index", 0))},
                           0}};
    if (k == "store_var8")
        return {LoweredOp{0x1A,
                           {static_cast<uint8_t>(stmt.getNum("group")),
                            static_cast<uint8_t>(stmt.getNum("value"))},
                           0}};
    if (k == "plain_text") return {};
    if (k == "unlifted") return lowerStmts(stmt.body, stringIndex);
    if (k == "raw_op") {
        std::vector<uint8_t> args;
        auto it = stmt.lists.find("args");
        if (it != stmt.lists.end())
            for (int x : it->second) args.push_back(static_cast<uint8_t>(x));
        return {LoweredOp{static_cast<uint8_t>(stmt.getNum("op")), args, 0}};
    }
    return {};
}

uint8_t triggerCondToByte(const Trigger& t) {
    if (t.cond == TriggerCond::Raw) return t.condRaw;
    return triggerCondByte(t.cond, t.condRaw);
}

}  // namespace mm2::eventlang

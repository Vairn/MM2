#pragma once
// Lifted event-script AST for the editor decompiler/compiler.
// Mirrors tools/mm2_event_lang/ast.py (shape), not the fixed-array C stub.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mm2::eventlang {

enum class RecordKind {
    Standard,
    StringBank,
    MixedPool,
    CastleBlob,
    OverlayBank,  // locs 60..70: LE string anchor @ [0..1], scripts @ [2..]
    Unknown,
};

enum class TriggerCond {
    Always,
    Enter,
    FromNorth,
    DirSpecial,
    AnyDirection,
    FacingNs,
    EnterSpecial,
    Raw,
};

inline const char* recordKindName(RecordKind k) {
    switch (k) {
        case RecordKind::Standard: return "standard";
        case RecordKind::StringBank: return "string_bank";
        case RecordKind::MixedPool: return "mixed_pool";
        case RecordKind::CastleBlob: return "castle_blob";
        case RecordKind::OverlayBank: return "overlay_bank";
        default: return "unknown";
    }
}

inline RecordKind recordKindFromName(const std::string& s) {
    if (s == "standard") return RecordKind::Standard;
    if (s == "string_bank") return RecordKind::StringBank;
    if (s == "mixed_pool") return RecordKind::MixedPool;
    if (s == "castle_blob") return RecordKind::CastleBlob;
    if (s == "overlay_bank") return RecordKind::OverlayBank;
    return RecordKind::Unknown;
}

inline const char* triggerCondName(TriggerCond c) {
    switch (c) {
        case TriggerCond::Always: return "always";
        case TriggerCond::Enter: return "enter";
        case TriggerCond::FromNorth: return "from north";
        case TriggerCond::DirSpecial: return "dir special";
        case TriggerCond::AnyDirection: return "any_direction";
        case TriggerCond::FacingNs: return "facing ns";
        case TriggerCond::EnterSpecial: return "enter special";
        default: return "raw";
    }
}

struct Trigger {
    int y = 0;
    int x = 0;
    TriggerCond cond = TriggerCond::Raw;
    uint8_t condRaw = 0;
    std::string scriptName;
    int eventId = 0;
};

struct StringDef {
    std::string name;
    std::string text;
    int index = -1;
    /** Exact on-disk bytes (no 0xFF terminator). Prefer on encode when set. */
    std::vector<uint8_t> rawBytes;
};

struct LoweredOp {
    uint8_t op = 0;
    std::vector<uint8_t> args;
    int off = 0;  // offset within segment
};

// Flexible key/value bags keep parity with the Python AST without a huge enum.
struct Expr {
    std::string kind;
    std::map<std::string, std::string> str;
    std::map<std::string, int> num;
    std::map<std::string, std::vector<int>> lists;

    static Expr make(const std::string& k) {
        Expr e;
        e.kind = k;
        return e;
    }
    Expr& set(const std::string& key, int v) {
        num[key] = v;
        return *this;
    }
    Expr& set(const std::string& key, const std::string& v) {
        str[key] = v;
        return *this;
    }
    Expr& setList(const std::string& key, const std::vector<int>& v) {
        lists[key] = v;
        return *this;
    }
    int getNum(const std::string& key, int def = 0) const {
        auto it = num.find(key);
        return it == num.end() ? def : it->second;
    }
    std::string getStr(const std::string& key, const std::string& def = {}) const {
        auto it = str.find(key);
        return it == str.end() ? def : it->second;
    }
};

struct Stmt {
    std::string kind;
    std::map<std::string, std::string> str;
    std::map<std::string, int> num;
    std::map<std::string, std::vector<int>> lists;
    Expr cond;
    std::vector<Stmt> thenBody;
    std::vector<Stmt> elseBody;
    std::vector<Stmt> body;  // unlifted

    static Stmt make(const std::string& k) {
        Stmt s;
        s.kind = k;
        return s;
    }
    Stmt& set(const std::string& key, int v) {
        num[key] = v;
        return *this;
    }
    Stmt& set(const std::string& key, const std::string& v) {
        str[key] = v;
        return *this;
    }
    Stmt& setList(const std::string& key, const std::vector<int>& v) {
        lists[key] = v;
        return *this;
    }
    int getNum(const std::string& key, int def = 0) const {
        auto it = num.find(key);
        return it == num.end() ? def : it->second;
    }
    std::string getStr(const std::string& key, const std::string& def = {}) const {
        auto it = str.find(key);
        return it == str.end() ? def : it->second;
    }
};

struct Script {
    std::string name;
    int eventId = 0;
    std::vector<Stmt> body;
    bool isPlainText = false;
    std::vector<uint8_t> rawSegment;
};

struct Location {
    int id = 0;
    RecordKind recordKind = RecordKind::Unknown;
    bool terminated = true;
    bool modified = false;
    std::vector<Trigger> triggers;
    std::vector<StringDef> strings;
    std::vector<Script> scripts;
    std::vector<uint8_t> rawBlob;
    std::string comment;
};

struct EventFileAst {
    std::vector<Location> locations;
    std::vector<std::pair<uint32_t, uint16_t>> header;  // offset, length
    std::vector<std::vector<uint8_t>> rawRecords;
};

}  // namespace mm2::eventlang

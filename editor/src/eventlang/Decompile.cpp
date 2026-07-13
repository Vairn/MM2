#include "eventlang/Decompile.h"

#include "eventlang/BytecodeParse.h"
#include "eventlang/CfgLift.h"
#include "eventlang/Semantics.h"

#include <tuple>
#include <unordered_map>

namespace mm2::eventlang {
namespace {

void walkReplaceStringRefs(std::vector<Stmt>& stmts,
                           const std::unordered_map<int, std::string>& nameByIndex) {
    for (auto& s : stmts) {
        if (s.kind == "say" || s.kind == "service_title") {
            auto it = s.num.find("string");
            if (it != s.num.end()) {
                int idx = it->second;
                auto nit = nameByIndex.find(idx);
                s.str["string"] = nit != nameByIndex.end() ? nit->second : ("s" + std::to_string(idx));
                s.num.erase(it);
            }
        }
        if (s.kind == "if") {
            walkReplaceStringRefs(s.thenBody, nameByIndex);
            walkReplaceStringRefs(s.elseBody, nameByIndex);
        } else if (s.kind == "unlifted") {
            walkReplaceStringRefs(s.body, nameByIndex);
        }
    }
}

}  // namespace

bool loadEventRecords(const uint8_t* data, size_t len, EventFileAst& out) {
    out = EventFileAst{};
    constexpr size_t kHeader = 71 * 6;
    if (!data || len < kHeader) return false;
    out.header.resize(71);
    out.rawRecords.resize(71);
    for (int i = 0; i < 71; ++i) {
        const uint8_t* p = data + i * 6;
        uint32_t off = (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
                       (static_cast<uint32_t>(p[2]) << 8) | p[3];
        uint16_t length = static_cast<uint16_t>((p[4] << 8) | p[5]);
        out.header[i] = {off, length};
        if (off + length <= len) {
            out.rawRecords[i].assign(data + off, data + off + length);
        }
    }
    return true;
}

Location decompileLocation(const uint8_t* blob, size_t len, int locId) {
    Location loc;
    loc.id = locId;
    if (!blob || len == 0) {
        loc.recordKind = RecordKind::Unknown;
        return loc;
    }
    loc.rawBlob.assign(blob, blob + len);

    ParsedLocation parsed = parseLocationRecord(blob, len, locId);
    loc.recordKind = parsed.recordKind;
    loc.terminated = parsed.terminated;

    if (loc.recordKind == RecordKind::CastleBlob) {
        loc.comment = "castle_blob — raw record preserved";
        return loc;
    }
    // Overlay banks (60..70) decompile normally: scripts @2, strings @ LE anchor.

    std::unordered_map<int, std::string> nameByIndex;
    for (int i = 0; i < static_cast<int>(parsed.strings.size()); ++i) {
        StringDef sd;
        sd.name = autoStringName(i);
        sd.text = parsed.strings[i];
        sd.index = i;
        if (i < static_cast<int>(parsed.stringRaw.size())) sd.rawBytes = parsed.stringRaw[i];
        nameByIndex[i] = sd.name;
        loc.strings.push_back(std::move(sd));
    }

    std::unordered_map<int, std::vector<int>> triggerIdxByEvent;
    for (const auto& trip : parsed.triplets) {
        uint8_t pos = std::get<0>(trip);
        uint8_t evt = std::get<1>(trip);
        uint8_t condRaw = std::get<2>(trip);
        Trigger t;
        t.y = (pos >> 4) & 0xF;
        t.x = pos & 0xF;
        t.cond = triggerCondFromByte(condRaw);
        t.condRaw = condRaw;
        t.eventId = evt;
        t.scriptName = autoScriptName(evt);
        triggerIdxByEvent[evt].push_back(static_cast<int>(loc.triggers.size()));
        loc.triggers.push_back(std::move(t));
    }

    for (int eid = 0; eid < static_cast<int>(parsed.segmentsRaw.size()); ++eid) {
        const auto& segRaw = parsed.segmentsRaw[eid];
        Script sc;
        sc.name = autoScriptName(eid);
        sc.eventId = eid;
        sc.rawSegment = segRaw;

        if (looksLikeTextRecord(segRaw.data(), segRaw.size())) {
            std::string text;
            for (uint8_t b : segRaw) text += (b == 0x40) ? '\n' : static_cast<char>(b);
            sc.isPlainText = true;
            sc.body.push_back(Stmt::make("plain_text").set("text", text));
        } else {
            const auto& ops = parsed.segmentsOps[eid];
            sc.body = liftSegment(ops, segRaw.data(), segRaw.size());
            walkReplaceStringRefs(sc.body, nameByIndex);
        }

        for (int ti : triggerIdxByEvent[eid]) {
            if (ti >= 0 && ti < static_cast<int>(loc.triggers.size()))
                loc.triggers[ti].scriptName = sc.name;
        }
        loc.scripts.push_back(std::move(sc));
    }

    return loc;
}

EventFileAst decompileEventDat(const uint8_t* data, size_t len) {
    EventFileAst file;
    if (!loadEventRecords(data, len, file)) return file;
    file.locations.reserve(71);
    for (int i = 0; i < 71; ++i) {
        const auto& rec = file.rawRecords[i];
        file.locations.push_back(decompileLocation(rec.data(), rec.size(), i));
    }
    return file;
}

}  // namespace mm2::eventlang

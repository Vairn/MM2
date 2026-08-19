#include "eventlang/Encode.h"

#include "core/ByteIO.h"
#include "core/EventOps.h"
#include "eventlang/Decompile.h"
#include "eventlang/Lower.h"

#include <algorithm>
#include <unordered_map>

namespace mm2::eventlang {
namespace {

std::vector<uint8_t> encodeStrings(const std::vector<StringDef>& strings) {
    std::vector<uint8_t> out;
    for (const auto& s : strings) {
        if (!s.rawBytes.empty()) {
            // Preserve retail bytes (e.g. lone 0x0A that is not MM2 '@'/0x40 newline).
            out.insert(out.end(), s.rawBytes.begin(), s.rawBytes.end());
        } else {
            for (char c : s.text) out.push_back(c == '\n' ? 0x40 : static_cast<uint8_t>(c));
        }
        out.push_back(0xFF);
    }
    return out;
}

std::vector<uint8_t> opsToBytes(const std::vector<LoweredOp>& ops) {
    std::vector<uint8_t> out;
    for (const auto& op : ops) {
        out.push_back(op.op);
        out.insert(out.end(), op.args.begin(), op.args.end());
    }
    return out;
}

}  // namespace

std::vector<uint8_t> encodeLocation(Location& loc) {
    if (!loc.rawBlob.empty() && !loc.modified) return loc.rawBlob;

    std::vector<StringDef> strings = loc.strings;
    std::sort(strings.begin(), strings.end(),
              [](const StringDef& a, const StringDef& b) { return a.index < b.index; });
    for (int i = 0; i < static_cast<int>(strings.size()); ++i) {
        if (strings[i].index < 0) strings[i].index = i;
    }

    std::unordered_map<std::string, int> stringIndex;
    for (const auto& s : strings) stringIndex[s.name] = s.index;

    auto strTable = encodeStrings(strings);

    int maxEvent = 0;
    for (const auto& sc : loc.scripts) maxEvent = std::max(maxEvent, sc.eventId);
    for (const auto& t : loc.triggers) maxEvent = std::max(maxEvent, t.eventId);

    std::unordered_map<int, const Script*> byId;
    for (const auto& sc : loc.scripts) byId[sc.eventId] = &sc;

    std::vector<uint8_t> scriptPool;
    for (int eid = 0; eid <= maxEvent; ++eid) {
        auto it = byId.find(eid);
        if (it == byId.end()) {
            scriptPool.push_back(0xFF);
            continue;
        }
        const Script* sc = it->second;
        if (sc->isPlainText) {
            std::string text;
            if (!sc->body.empty()) text = sc->body[0].getStr("text");
            for (char c : text) scriptPool.push_back(c == '\n' ? 0x40 : static_cast<uint8_t>(c));
        } else if (!sc->rawSegment.empty() && sc->body.empty()) {
            // Preserve unlifted overlay bytecode (no AST body).
            scriptPool.insert(scriptPool.end(), sc->rawSegment.begin(), sc->rawSegment.end());
        } else {
            // Re-lower from AST when a body is present.
            auto ops = lowerStmts(sc->body, stringIndex);
            auto bytes = opsToBytes(ops);
            scriptPool.insert(scriptPool.end(), bytes.begin(), bytes.end());
        }
        scriptPool.push_back(0xFF);
    }

    // Overlay banks (ASM queued dispatch): [0..1]=LE absolute string anchor, [2..]=scripts.
    if (loc.recordKind == RecordKind::OverlayBank || (loc.id >= 60 && loc.id <= 70)) {
        const uint16_t anchor = static_cast<uint16_t>(2 + scriptPool.size());
        std::vector<uint8_t> record;
        record.push_back(static_cast<uint8_t>(anchor & 0xFF));
        record.push_back(static_cast<uint8_t>((anchor >> 8) & 0xFF));
        record.insert(record.end(), scriptPool.begin(), scriptPool.end());
        record.insert(record.end(), strTable.begin(), strTable.end());
        return record;
    }

    std::vector<uint8_t> triplets;
    for (const auto& t : loc.triggers) {
        uint8_t pos = static_cast<uint8_t>(((t.y & 0xF) << 4) | (t.x & 0xF));
        triplets.push_back(pos);
        triplets.push_back(static_cast<uint8_t>(t.eventId & 0xFF));
        triplets.push_back(triggerCondToByte(t));
    }
    if (loc.terminated) {
        triplets.push_back(0);
        triplets.push_back(0);
        triplets.push_back(0);
    }

    uint16_t strRel = static_cast<uint16_t>(2 + scriptPool.size());
    std::vector<uint8_t> record;
    record.insert(record.end(), triplets.begin(), triplets.end());
    record.push_back(static_cast<uint8_t>(strRel & 0xFF));
    record.push_back(static_cast<uint8_t>((strRel >> 8) & 0xFF));
    record.insert(record.end(), scriptPool.begin(), scriptPool.end());
    record.insert(record.end(), strTable.begin(), strTable.end());
    return record;
}

std::vector<uint8_t> encodeEventDat(const EventFileAst& file) {
    std::vector<std::vector<uint8_t>> records;
    records.reserve(file.locations.size());
    for (auto loc : file.locations) records.push_back(encodeLocation(loc));

    const bool canReuseHeader =
        file.header.size() == records.size() &&
        [&] {
            for (size_t i = 0; i < records.size(); ++i) {
                if (file.header[i].second != static_cast<uint16_t>(records[i].size()))
                    return false;
            }
            return true;
        }();

    std::vector<std::pair<uint32_t, uint16_t>> entries;
    if (canReuseHeader) {
        entries = file.header;
    } else {
        uint32_t cursor = static_cast<uint32_t>(kEventHeaderSize);
        for (const auto& rec : records) {
            entries.emplace_back(cursor, static_cast<uint16_t>(rec.size()));
            cursor += static_cast<uint32_t>(rec.size());
        }
    }

    std::vector<uint8_t> out(static_cast<size_t>(kEventHeaderSize), 0);
    for (int i = 0; i < kEventLocationCount && i < static_cast<int>(entries.size()); ++i) {
        writeU32BE(&out[i * 6], entries[i].first);
        writeU16BE(&out[i * 6 + 4], entries[i].second);
    }
    for (const auto& rec : records) out.insert(out.end(), rec.begin(), rec.end());
    return out;
}

std::vector<uint8_t> patchLocationInEventDat(const uint8_t* data, size_t len, int locId,
                                            const std::vector<uint8_t>& newRecord) {
    EventFileAst file;
    if (!loadEventRecords(data, len, file)) return {};
    if (locId < 0 || locId >= kEventLocationCount) return {};
    file.rawRecords[locId] = newRecord;

    std::vector<uint8_t> out(static_cast<size_t>(kEventHeaderSize), 0);
    uint32_t cursor = static_cast<uint32_t>(kEventHeaderSize);
    for (int i = 0; i < kEventLocationCount; ++i) {
        const auto& rec = file.rawRecords[i];
        writeU32BE(&out[i * 6], cursor);
        writeU16BE(&out[i * 6 + 4], static_cast<uint16_t>(rec.size()));
        cursor += static_cast<uint32_t>(rec.size());
    }
    for (int i = 0; i < kEventLocationCount; ++i)
        out.insert(out.end(), file.rawRecords[i].begin(), file.rawRecords[i].end());
    return out;
}

}  // namespace mm2::eventlang

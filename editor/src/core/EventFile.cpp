#include "core/EventFile.h"

#include <filesystem>

#include "core/EventOps.h"
#include "core/PcDatLzw.h"
#include "eventlang/BytecodeParse.h"
#include "eventlang/Encode.h"
#include "eventlang/OpcodeTable.h"

namespace mm2 {

bool EventFile::load(const std::string& path) {
    // Plain event.dat first; else GOG EVENTSI.DAT + EVENTSO.DAT via pcDatLoadEventAuto.
    if (pcDatReadFlexible(path, raw)) {
        decode();
        return true;
    }
    std::string dir = std::filesystem::path(path).parent_path().string();
    if (dir.empty()) dir = ".";
    if (!pcDatLoadEventAuto(dir, raw)) return false;
    decode();
    return true;
}

bool EventFile::save(const std::string& path) const {
    return writeFile(path, raw);
}

// Parse one 0xFF-delimited segment; stamp absOff for in-place hex edits.
static EventSegment parseSegment(const uint8_t* seg, size_t len, size_t segAbs) {
    EventSegment out;
    out.rawLen = len;
    if (eventlang::looksLikeTextRecord(seg, len)) {
        out.isText = true;
        out.text = eventlang::decodeEventText(seg, len);
        return out;
    }
    for (const auto& lo : eventlang::parseSegmentOps(seg, len)) {
        EventOp node;
        node.op = lo.op;
        node.args = lo.args;
        node.absOff = segAbs + static_cast<size_t>(lo.off);
        const int argc = eventlang::opcodeArgc(lo.op);
        node.variable = argc < 0;
        node.truncated = argc >= 0 && static_cast<int>(lo.args.size()) < argc;
        out.ops.push_back(std::move(node));
    }
    return out;
}

static EventLocation decodeLocation(const Bytes& raw, int id, uint32_t off, uint16_t length) {
    EventLocation loc;
    loc.id = id;
    loc.offset = off;
    loc.length = length;

    size_t base = off;
    size_t end = off + length;
    if (end > raw.size()) end = raw.size();
    size_t blobLen = (base <= end) ? end - base : 0;
    const uint8_t* blob = (base < raw.size()) ? &raw[base] : nullptr;

    // 1. Tile-event triplets until 00 00 00.
    size_t pos = 0;
    loc.terminated = false;
    while (blob && pos + 2 < blobLen) {
        uint8_t a = blob[pos], b = blob[pos + 1], c = blob[pos + 2];
        if (a == 0 && b == 0 && c == 0) {
            loc.terminated = true;
            pos += 3;
            break;
        }
        EventTriplet t;
        t.pos = a; t.event = b; t.cond = c;
        t.absOff = base + pos;
        loc.triplets.push_back(t);
        pos += 3;
    }

    // 2. String-table relative offset (u16 LE).
    uint8_t lo = (blob && pos < blobLen) ? blob[pos] : 0;
    uint8_t hi = (blob && pos + 1 < blobLen) ? blob[pos + 1] : 0;
    int strRel = (hi << 8) | lo;
    loc.stringTableOffset = static_cast<int>(pos) + strRel;
    loc.scriptOffset = static_cast<int>(pos) + 2;

    // 3. Script bytes -> 0xFF-delimited segments (indexed by event id).
    size_t scriptStart = static_cast<size_t>(loc.scriptOffset);
    size_t scriptEnd = static_cast<size_t>(loc.stringTableOffset);
    if (scriptEnd > blobLen) scriptEnd = blobLen;
    if (scriptStart < scriptEnd && blob) {
        size_t segStart = scriptStart;
        for (size_t i = scriptStart; i <= scriptEnd; ++i) {
            bool atDelim = (i < scriptEnd && blob[i] == 0xFF);
            bool atEnd = (i == scriptEnd);
            if (atDelim || atEnd) {
                size_t segLen = i - segStart;
                loc.segments.push_back(
                    parseSegment(blob + segStart, segLen, base + segStart));
                segStart = i + 1;
                if (atEnd) break;
            }
        }
    }

    // 4. String table (0xFF-terminated strings, @ = newline).
    if (blob && static_cast<size_t>(loc.stringTableOffset) < blobLen) {
        size_t sp = static_cast<size_t>(loc.stringTableOffset);
        while (sp < blobLen) {
            size_t e = sp;
            while (e < blobLen && blob[e] != 0xFF) ++e;
            loc.strings.push_back(eventlang::decodeEventText(blob + sp, e - sp));
            sp = e + 1;
        }
    }

    return loc;
}

void EventFile::decode() {
    locations.clear();
    if (raw.size() < static_cast<size_t>(kEventHeaderSize)) return;

    for (int i = 0; i < kEventLocationCount; ++i) {
        uint32_t off = readU32BE(&raw[i * 6]);
        uint16_t length = readU16BE(&raw[i * 6 + 4]);
        locations.push_back(decodeLocation(raw, i, off, length));
    }
}

bool EventFile::replaceLocationRecord(int locId, const std::vector<uint8_t>& newRecord) {
    if (locId < 0 || locId >= kEventLocationCount) return false;
    auto patched = eventlang::patchLocationInEventDat(raw.data(), raw.size(), locId, newRecord);
    if (patched.empty()) return false;
    raw = std::move(patched);
    decode();
    return true;
}

}  // namespace mm2

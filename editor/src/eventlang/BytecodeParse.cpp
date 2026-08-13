#include "eventlang/BytecodeParse.h"

#include "eventlang/OpcodeTable.h"

#include <algorithm>
#include <tuple>

namespace mm2::eventlang {
namespace {

void appendStringTable(ParsedLocation& loc, const uint8_t* blob, size_t len, size_t strOff) {
    if (strOff >= len) return;
    size_t sp = strOff;
    while (sp < len) {
        size_t e = sp;
        while (e < len && blob[e] != 0xFF) ++e;
        std::string s;
        for (size_t k = sp; k < e; ++k) {
            uint8_t b = blob[k];
            s += (b == 0x40) ? '\n' : static_cast<char>(b);
        }
        loc.strings.push_back(std::move(s));
        loc.stringRaw.emplace_back(blob + sp, blob + e);
        sp = e + 1;
    }
}

void splitScriptPool(ParsedLocation& loc, const uint8_t* blob, size_t scriptStart, size_t scriptEnd) {
    if (scriptStart >= scriptEnd) return;
    size_t segStart = scriptStart;
    for (size_t i = scriptStart; i < scriptEnd; ++i) {
        if (blob[i] != 0xFF) continue;
        size_t segLen = i - segStart;
        std::vector<uint8_t> raw(blob + segStart, blob + segStart + segLen);
        loc.segmentsOps.push_back(parseSegmentOps(raw.data(), raw.size()));
        loc.segmentsRaw.push_back(std::move(raw));
        segStart = i + 1;
    }
    // Trailing bytes without a final 0xFF (rare) — keep as last segment.
    // Do NOT invent an empty segment after a terminating 0xFF (that +1 FF broke roundtrip).
    if (segStart < scriptEnd) {
        std::vector<uint8_t> raw(blob + segStart, blob + scriptEnd);
        loc.segmentsOps.push_back(parseSegmentOps(raw.data(), raw.size()));
        loc.segmentsRaw.push_back(std::move(raw));
    }
}

/** locs 60..70: queued-dispatch overlay banks (ASM 0x176B6).
 *  work_buf[0..1] = LE string anchor; scripts start at offset 2.
 *  Must NOT scan for 00 00 00 — opcode streams embed that pattern. */
ParsedLocation parseOverlayRecord(const uint8_t* blob, size_t len) {
    ParsedLocation loc;
    loc.terminated = true;
    loc.recordKind = RecordKind::OverlayBank;
    if (!blob || len < 2) return loc;

    const int anchor = blob[0] | (blob[1] << 8);
    loc.stringTableOffset = anchor;
    loc.scriptOffset = 2;

    size_t scriptEnd = static_cast<size_t>(anchor);
    if (scriptEnd > len) scriptEnd = len;
    splitScriptPool(loc, blob, 2, scriptEnd);
    appendStringTable(loc, blob, len, static_cast<size_t>(std::max(0, anchor)));
    return loc;
}

ParsedLocation parseStandardRecord(const uint8_t* blob, size_t len) {
    ParsedLocation loc;
    if (!blob || len == 0) return loc;

    size_t pos = 0;
    while (pos + 2 < len) {
        uint8_t a = blob[pos], b = blob[pos + 1], c = blob[pos + 2];
        if (a == 0 && b == 0 && c == 0) {
            loc.terminated = true;
            pos += 3;
            break;
        }
        loc.triplets.emplace_back(a, b, c);
        pos += 3;
    }

    uint8_t lo = (pos < len) ? blob[pos] : 0;
    uint8_t hi = (pos + 1 < len) ? blob[pos + 1] : 0;
    int strRel = (hi << 8) | lo;
    loc.stringTableOffset = static_cast<int>(pos) + strRel;
    loc.scriptOffset = static_cast<int>(pos) + 2;

    size_t scriptStart = static_cast<size_t>(loc.scriptOffset);
    size_t scriptEnd = static_cast<size_t>(loc.stringTableOffset);
    if (scriptEnd > len) scriptEnd = len;
    splitScriptPool(loc, blob, scriptStart, scriptEnd);
    appendStringTable(loc, blob, len, static_cast<size_t>(std::max(0, loc.stringTableOffset)));

    int scriptLen = loc.stringTableOffset - loc.scriptOffset;
    if (scriptLen < 0) scriptLen = 0;
    if (!loc.terminated)
        loc.recordKind = RecordKind::CastleBlob;
    else if (scriptLen == 0 && !loc.strings.empty())
        loc.recordKind = RecordKind::StringBank;
    else if (scriptLen > 500)
        loc.recordKind = RecordKind::MixedPool;
    else if (!loc.triplets.empty() && scriptLen > 0)
        loc.recordKind = RecordKind::Standard;
    else
        loc.recordKind = RecordKind::Unknown;

    return loc;
}

}  // namespace

bool looksLikeTextRecord(const uint8_t* seg, size_t len) {
    if (!seg || len < 4) return false;
    size_t printable = 0, letters = 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t b = seg[i];
        if ((b >= 0x20 && b <= 0x7E) || b == 0x40) ++printable;
        if (b >= 0x41 && b <= 0x7A) ++letters;
    }
    return letters >= 3 && (static_cast<double>(printable) / static_cast<double>(len)) >= 0.85;
}

std::vector<LoweredOp> parseSegmentOps(const uint8_t* seg, size_t len) {
    std::vector<LoweredOp> out;
    if (!seg) return out;
    size_t i = 0;
    while (i < len) {
        LoweredOp node;
        node.off = static_cast<int>(i);
        node.op = seg[i++];
        if (!opcodeKnown(node.op)) {
            out.push_back(node);
            break;
        }
        int argc = opcodeArgc(node.op);
        if (argc < 0) {
            if (i < len) node.args.push_back(seg[i++]);
            out.push_back(node);
            continue;
        }
        for (int k = 0; k < argc && i < len; ++k) node.args.push_back(seg[i++]);
        out.push_back(node);
        if (static_cast<int>(node.args.size()) < argc) break;
    }
    return out;
}

ParsedLocation parseLocationRecord(const uint8_t* blob, size_t len, int locId) {
    // event.dat locs 60..70 are overlay/queued-dispatch banks (EventRuntime
    // runQueuedDispatch @ 0x176B6): LE u16 string anchor at [0], scripts at [2].
    if (locId >= 60 && locId <= 70) return parseOverlayRecord(blob, len);
    return parseStandardRecord(blob, len);
}

}  // namespace mm2::eventlang

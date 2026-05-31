#include "core/EventFile.h"

#include "core/EventOps.h"

namespace mm2 {

const char* eventCondName(uint8_t cond) {
    switch (cond) {
        case 0x10: return "ALWAYS";
        case 0x20: return "DIR_N?";
        case 0x40: return "DIR_SPECIAL";
        case 0x80: return "ENTER";
        case 0xC0: return "ENTER+SPECIAL";
        case 0xF0: return "ANY_DIR";
        default: return nullptr;
    }
}

bool EventFile::load(const std::string& path) {
    if (!readFile(path, raw)) return false;
    decode();
    return true;
}

bool EventFile::save(const std::string& path) const {
    return writeFile(path, raw);
}

// Parse one 0xFF-delimited segment into opcode nodes. `segAbs` is the absolute
// file offset of the segment's first byte. Mirrors parse_segment_stream_nodes:
// the byte stream is walked in order and does NOT stop at token-skip opcodes.
static EventSegment parseSegment(const uint8_t* seg, size_t len, size_t segAbs) {
    EventSegment out;
    out.rawLen = len;

    // Plain-text record heuristic (decode_event.looks_like_text_record).
    if (len >= 4) {
        size_t printable = 0, letters = 0;
        for (size_t i = 0; i < len; ++i) {
            uint8_t b = seg[i];
            if ((b >= 0x20 && b <= 0x7E) || b == 0x40) ++printable;
            if (b >= 0x41 && b <= 0x7A) ++letters;
        }
        if (letters >= 3 && static_cast<double>(printable) / len >= 0.85) {
            out.isText = true;
            for (size_t i = 0; i < len; ++i) {
                uint8_t b = seg[i];
                out.text += (b == 0x40) ? '\n' : static_cast<char>(b);
            }
            return out;
        }
    }

    size_t i = 0;
    while (i < len) {
        EventOp node;
        node.op = seg[i];
        node.absOff = segAbs + i;
        ++i;

        if (!opcodeKnown(node.op)) {
            out.ops.push_back(node);  // unknown opcode -> stop
            break;
        }

        int argc = opcodeArgc(node.op);
        if (argc < 0) {
            node.variable = true;
            if (i < len) {
                node.args.push_back(seg[i]);
                ++i;
            }
            out.ops.push_back(node);
            continue;  // stream continues past selector byte
        }

        for (int k = 0; k < argc && i < len; ++k, ++i)
            node.args.push_back(seg[i]);
        if (static_cast<int>(node.args.size()) < argc) {
            node.truncated = true;
            out.ops.push_back(node);
            break;
        }
        out.ops.push_back(node);
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
            std::string s;
            for (size_t k = sp; k < e; ++k) {
                uint8_t b = blob[k];
                s += (b == 0x40) ? '\n' : static_cast<char>(b);
            }
            loc.strings.push_back(s);
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

}  // namespace mm2

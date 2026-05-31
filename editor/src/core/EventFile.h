#pragma once
// Structured decoder for event.dat (Might & Magic II, Amiga).
//
// Layout (see EXTRACTED/docs/06-event-dat-format.md, tools/decode_event.py):
//   [header]  71 entries x 6 bytes  (u32 abs offset BE, u16 length BE)
//   per location:
//     tile-event triplets (pos, event, cond) until 00 00 00
//     u16 LE string-table relative offset
//     script bytecodes (0xFF-delimited records, indexed by event id)
//     0xFF-terminated strings
//
// We keep the original bytes in `raw` and decode views into it. Opcode argument
// bytes and triplet bytes carry absolute file offsets so the GUI can edit them
// in place (length-preserving) and re-save the file faithfully.

#include <cstdint>
#include <string>
#include <vector>

#include "core/ByteIO.h"

namespace mm2 {

struct EventTriplet {
    uint8_t pos = 0;    // (y << 4) | x in the 16x16 grid
    uint8_t event = 0;  // event/script id (indexes segments)
    uint8_t cond = 0;   // condition flag mask
    size_t absOff = 0;  // absolute file offset of this triplet's first byte
    int y() const { return (pos >> 4) & 0xF; }
    int x() const { return pos & 0xF; }
};

struct EventOp {
    uint8_t op = 0;
    std::vector<uint8_t> args;  // decoded at parse time (count = bytes consumed)
    size_t absOff = 0;          // absolute file offset of the opcode byte
    bool variable = false;      // token-skip family (0x10/0x11/0x2B)
    bool truncated = false;     // ran past end of segment
};

struct EventSegment {
    bool isText = false;
    std::string text;            // when isText
    std::vector<EventOp> ops;    // when script
    size_t rawLen = 0;           // segment byte length (excluding 0xFF delim)
};

struct EventLocation {
    int id = 0;
    uint32_t offset = 0;         // absolute file offset of this record
    uint16_t length = 0;
    bool terminated = false;     // had a 00 00 00 triplet terminator
    std::vector<EventTriplet> triplets;
    std::vector<EventSegment> segments;   // indexed by event id
    std::vector<std::string> strings;
    int scriptOffset = 0;        // within the record blob
    int stringTableOffset = 0;   // within the record blob
};

struct EventFile {
    Bytes raw;
    std::vector<EventLocation> locations;

    bool load(const std::string& path);
    bool save(const std::string& path) const;  // writes raw back (in-place edits)
    void decode();                              // (re)build locations from raw

    // Live argument byte (reads current raw, reflecting in-place edits).
    uint8_t argByte(const EventOp& op, int idx) const {
        size_t p = op.absOff + 1 + static_cast<size_t>(idx);
        return p < raw.size() ? raw[p] : 0;
    }
    void setArgByte(const EventOp& op, int idx, uint8_t v) {
        size_t p = op.absOff + 1 + static_cast<size_t>(idx);
        if (p < raw.size()) raw[p] = v;
    }
    void setTripletByte(const EventTriplet& t, int idx, uint8_t v) {
        size_t p = t.absOff + static_cast<size_t>(idx);
        if (p < raw.size()) raw[p] = v;
    }
};

// Condition-flag name (towns/outdoor), matching decode_event.FLAG_NAMES.
const char* eventCondName(uint8_t cond);

}  // namespace mm2

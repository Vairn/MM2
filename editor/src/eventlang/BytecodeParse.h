#pragma once
#include "eventlang/Ast.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mm2::eventlang {

struct ParsedLocation {
    bool terminated = false;
    RecordKind recordKind = RecordKind::Unknown;
    int scriptOffset = 0;
    int stringTableOffset = 0;
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> triplets;  // pos, evt, cond
    std::vector<std::string> strings;
    std::vector<std::vector<uint8_t>> stringRaw;
    std::vector<std::vector<uint8_t>> segmentsRaw;
    std::vector<std::vector<LoweredOp>> segmentsOps;
};

bool looksLikeTextRecord(const uint8_t* seg, size_t len);
std::vector<LoweredOp> parseSegmentOps(const uint8_t* seg, size_t len);
ParsedLocation parseLocationRecord(const uint8_t* blob, size_t len, int locId);

}  // namespace mm2::eventlang

#pragma once
#include "eventlang/Ast.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mm2::eventlang {

std::vector<uint8_t> encodeLocation(Location& loc);
std::vector<uint8_t> encodeEventDat(const EventFileAst& file);

/** Replace one location record in an existing event.dat blob; returns new file bytes. */
std::vector<uint8_t> patchLocationInEventDat(const uint8_t* data, size_t len, int locId,
                                            const std::vector<uint8_t>& newRecord);

}  // namespace mm2::eventlang

#pragma once
#include "eventlang/Ast.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mm2::eventlang {

Location decompileLocation(const uint8_t* blob, size_t len, int locId);
EventFileAst decompileEventDat(const uint8_t* data, size_t len);

/** Load header + records from raw event.dat bytes. */
bool loadEventRecords(const uint8_t* data, size_t len, EventFileAst& out);

}  // namespace mm2::eventlang

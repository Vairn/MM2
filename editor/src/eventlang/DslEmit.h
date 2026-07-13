#pragma once
#include "eventlang/Ast.h"

#include <string>

namespace mm2::eventlang {

/** When `file` is set, overlay OP_0E calls expand with the target script as comments. */
std::string emitLocation(const Location& loc, const std::string& areaComment = {},
                         const EventFileAst* file = nullptr);
std::string emitFile(const EventFileAst& file);

}  // namespace mm2::eventlang

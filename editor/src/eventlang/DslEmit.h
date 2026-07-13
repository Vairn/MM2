#pragma once
#include "eventlang/Ast.h"

#include <string>

namespace mm2::eventlang {

/** When `file` is set, overlay OP_0E lines get a short target hint (no body dump). */
std::string emitLocation(const Location& loc, const std::string& areaComment = {},
                         const EventFileAst* file = nullptr);
std::string emitFile(const EventFileAst& file);

}  // namespace mm2::eventlang

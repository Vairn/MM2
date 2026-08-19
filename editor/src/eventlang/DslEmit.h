#pragma once
#include "eventlang/Ast.h"

#include <functional>
#include <string>

namespace mm2::eventlang {

struct EmitLookups {
    std::function<std::string(int)> itemName;
    std::function<std::string(int)> monsterName;
};

std::string emitLocation(const Location& loc, const std::string& areaComment = {},
                         const EventFileAst* file = nullptr,
                         const EmitLookups* lookups = nullptr);
std::string emitFile(const EventFileAst& file);

}  // namespace mm2::eventlang
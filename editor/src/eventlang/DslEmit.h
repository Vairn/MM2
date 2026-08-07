#pragma once
#include "eventlang/Ast.h"

#include <functional>
#include <string>

namespace mm2::eventlang {

/** Optional resolvers that let the emitter append human-readable `# Name` comments. */
struct EmitLookups {
    // Return the display name for an item / monster id ("" if unknown).
    std::function<std::string(int)> itemName;
    std::function<std::string(int)> monsterName;
};

/** When `file` is set, overlay OP_0E lines get a short target hint (no body dump). */
std::string emitLocation(const Location& loc, const std::string& areaComment = {},
                         const EventFileAst* file = nullptr,
                         const EmitLookups* lookups = nullptr);
std::string emitFile(const EventFileAst& file);

}  // namespace mm2::eventlang
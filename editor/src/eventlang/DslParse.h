#pragma once
#include "eventlang/Ast.h"

#include <string>

namespace mm2::eventlang {

struct ParseResult {
    Location loc;
    bool ok = false;
    std::string error;
};

ParseResult parseLocationText(const std::string& text);
EventFileAst parseFileText(const std::string& text);

}  // namespace mm2::eventlang

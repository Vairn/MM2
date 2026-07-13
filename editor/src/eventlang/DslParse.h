#pragma once
#include "eventlang/Ast.h"

#include <string>

namespace mm2::eventlang {

struct ParseResult {
    Location loc;
    bool ok = false;
    std::string error;
    /** 0-based source line when known; -1 if unavailable. */
    int errorLine = -1;
};

ParseResult parseLocationText(const std::string& text);
EventFileAst parseFileText(const std::string& text);

}  // namespace mm2::eventlang

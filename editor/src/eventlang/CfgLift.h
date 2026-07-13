#pragma once
#include "eventlang/Ast.h"

#include <cstdint>
#include <vector>

namespace mm2::eventlang {

std::vector<Stmt> liftSegment(const std::vector<LoweredOp>& ops, const uint8_t* seg, size_t segLen);

}  // namespace mm2::eventlang

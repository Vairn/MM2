#pragma once
#include "eventlang/Ast.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace mm2::eventlang {

std::vector<LoweredOp> lowerStmts(const std::vector<Stmt>& stmts,
                                  const std::unordered_map<std::string, int>& stringIndex);
std::vector<LoweredOp> exprToCondOps(const Expr& expr);
uint8_t triggerCondToByte(const Trigger& t);

}  // namespace mm2::eventlang

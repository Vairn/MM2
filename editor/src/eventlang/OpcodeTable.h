#pragma once
// Opcode argc + ROM token-skip deltas.
//
// Truth sources (not EXTRACTED/docs):
//   - ASM dispatcher 0x172CA / jump table 0x17494 / handlers
//   - game EventRuntime::dispatchOp + EventRuntime::skipTokens
//   - ROM opcode_len_tbl @ A4-$6CC8 (EventVmHelpers::kOpTokenDelta / eventVmTokenDelta)
//
// Linear bytecode parse uses argc (bytes the handler consumes when executed).
// OP_10/11/2B CFG lift uses tokenDelta (bytes skipTokens advances per token).
// Known ROM quirks vs 1+argc: OP_00 delta=0, OP_25 delta=2 (handler still reads 2 args).

#include <cstdint>

namespace mm2::eventlang {

constexpr int kOpcodeCount = 0x33;

struct OpcodeInfo {
    const char* name;
    int argc;  // fixed arg bytes; -1 = token-skip family (selector byte only)
};

const OpcodeInfo& opcodeInfo(uint8_t op);
inline bool opcodeKnown(uint8_t op) { return op <= 0x32; }
inline int opcodeArgc(uint8_t op) { return opcodeInfo(op).argc; }
inline const char* opcodeName(uint8_t op) { return opcodeInfo(op).name; }

/** Bytes skipTokens advances for one token starting with `op` (ROM table). */
uint8_t tokenDelta(uint8_t op);

/** Decode u16 payload: OP_24 LE, OP_25 BE (matches EventRuntime / decode_event). */
int decodeU16Arg(uint8_t op, const uint8_t* args, int argc);

}  // namespace mm2::eventlang

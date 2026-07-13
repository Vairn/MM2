#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mm2::eventlang {

/** Advance past one token using ROM tokenDelta (EventRuntime::skipTokens). */
std::optional<size_t> advanceOneToken(const uint8_t* seg, size_t len, size_t pos);

/** Skip N tokens starting at pos; returns new position or nullopt. */
std::optional<size_t> skipNTokens(const uint8_t* seg, size_t len, size_t pos, int n);

}  // namespace mm2::eventlang

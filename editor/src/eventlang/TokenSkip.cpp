#include "eventlang/TokenSkip.h"

#include "eventlang/OpcodeTable.h"

namespace mm2::eventlang {

std::optional<size_t> advanceOneToken(const uint8_t* seg, size_t len, size_t pos) {
    if (!seg || pos >= len) return std::nullopt;
    const uint8_t delta = tokenDelta(seg[pos]);
    if (delta == 0) return std::nullopt;
    const size_t end = pos + delta;
    if (end > len) return std::nullopt;
    return end;
}

std::optional<size_t> skipNTokens(const uint8_t* seg, size_t len, size_t pos, int n) {
    size_t cur = pos;
    for (int i = 0; i < n; ++i) {
        auto nxt = advanceOneToken(seg, len, cur);
        if (!nxt) return std::nullopt;
        cur = *nxt;
    }
    return cur;
}

}  // namespace mm2::eventlang

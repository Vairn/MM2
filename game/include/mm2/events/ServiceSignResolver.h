#pragma once

#include <cstdint>

namespace mm2::events {

/** Resolve OP_0B str_idx -> NN.anm id via 0x15756 town tables (A4-$6C62..$6C02). */
class ServiceSignResolver {
public:
    /** env_id = A4-$79E3 (map screen 0..4 town index for standard towns). */
    static int resolveAnmId(int env_id, uint8_t str_idx);

    /** Returns -1 if str_idx has no mapped sign (e.g. str_idx >= 0x80 -> 0x4B path). */
    static int resolveForLocation(int location_id, uint8_t str_idx);
};

}  // namespace mm2::events

#include "mm2/events/ServiceSignResolver.h"

namespace mm2::events {

namespace {

/* Dumped from EXTRACTED/ghidra/mm2_data_00.bin @ A4-$6C62..$6C02 (0x15756). */
constexpr uint8_t kMiddlegate[] = {70, 62, 63, 66, 67, 68, 65};
constexpr uint8_t kAtlantium[] = {73, 33, 42, 43, 17, 14, 69, 34, 9, 26, 24, 52,
                                 53, 21, 25, 28, 44, 49, 11, 31, 55, 36, 1, 61};
constexpr uint8_t kTundara[] = {72, 16, 10, 23, 6, 51, 15, 8, 5, 49, 40, 11,
                               30, 39, 4, 46, 20, 36, 1, 57, 13, 58, 18, 47};
constexpr uint8_t kVulcania[] = {74, 42, 2, 17, 14, 69, 19, 34, 9, 26, 24, 52,
                                54, 8, 21, 25, 3, 29, 44, 50, 27, 39, 61, 48};
constexpr uint8_t kSandsobar[] = {71, 59, 33, 19, 35, 10, 24, 6, 75, 51, 15, 7,
                                  60, 56, 29, 5, 22, 50, 30, 41, 46, 37, 58, 0};

const uint8_t *tableForEnv(int env_id, size_t *out_len)
{
    switch (env_id) {
    case 0:
        *out_len = sizeof(kMiddlegate);
        return kMiddlegate;
    case 1:
        *out_len = sizeof(kAtlantium);
        return kAtlantium;
    case 2:
        *out_len = sizeof(kTundara);
        return kTundara;
    case 3:
        *out_len = sizeof(kVulcania);
        return kVulcania;
    case 4:
        *out_len = sizeof(kSandsobar);
        return kSandsobar;
    default:
        *out_len = 0;
        return nullptr;
    }
}

}  // namespace

int ServiceSignResolver::resolveAnmId(int env_id, uint8_t str_idx)
{
    if (str_idx >= 0x80) {
        return 0x4B;
    }
    if (env_id < 0 || env_id >= 7) {
        return -1;
    }
    if (str_idx == 0) {
        return -1;
    }

    size_t len = 0;
    const uint8_t *tbl = tableForEnv(env_id, &len);
    if (!tbl) {
        return -1;
    }

    const unsigned idx = static_cast<unsigned>(str_idx) - 1u;
    if (idx >= len) {
        return -1;
    }
    return static_cast<int>(tbl[idx]);
}

int ServiceSignResolver::resolveForLocation(int location_id, uint8_t str_idx)
{
    /* Walkable town maps 0..4 use env_id = location_id @ 0x15772. */
    if (location_id >= 0 && location_id <= 4) {
        return resolveAnmId(location_id, str_idx);
    }
    return -1;
}

}  // namespace mm2::events

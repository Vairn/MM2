#include "mm2/events/ServiceSignResolver.h"

#include "mm2_gamestate.h"

#include <stddef.h>

namespace mm2::events {

namespace {

/* Dumped from EXTRACTED/ghidra/mm2_data_00.bin @ A4-$6C62..$6C02 (0x15756).
 * Each table is 24 bytes; bases are 0x16 bytes apart in the data hunk.
 * Retail indexes str_idx-1 with no bounds check @ 0x15776C. */
constexpr uint8_t kMiddlegate[] = {70, 62, 63, 66, 67, 68, 65, 2,  19, 26, 51, 54, 53, 12, 60, 27,
                                   39, 4,  45, 37, 57, 47, 73, 33};
constexpr uint8_t kAtlantium[] = {73, 33, 42, 43, 17, 14, 69, 34, 9,  26, 24, 52, 53, 21, 25, 28,
                                  44, 49, 11, 31, 55, 36, 1,  61};
constexpr uint8_t kTundara[] = {1,  61, 18, 47, 72, 16, 10, 23, 6,  51, 15, 8,  5,  49, 40, 11,
                                30, 39, 4,  46, 20, 36, 1,  57};
constexpr uint8_t kVulcania[] = {1,  57, 13, 58, 18, 47, 74, 42, 2,  17, 14, 69, 19, 34, 9,  26,
                                 24, 52, 54, 8,  21, 25, 3,  29};
constexpr uint8_t kSandsobar[] = {3,  29, 44, 50, 27, 39, 61, 48, 71, 59, 33, 19, 35, 10, 24, 6,
                                  75, 51, 15, 7,  60, 56, 29, 5};

constexpr size_t kTableLen = 24;

/* area_env_lookup @ 0x18AE (A4-$7654/$764D/$765B); ghidra mm2_data_00 @ 0x09AA.
 * First matching range wins (cmpi #7 / bne exits loop @ 0x18EE). Unmatched → 7. */
constexpr uint8_t kScreenEnvRangeLo[] = {0, 5, 17, 33, 41, 45, 55};
constexpr uint8_t kScreenEnvRangeHi[] = {4, 16, 32, 40, 44, 54, 59};
constexpr uint8_t kScreenEnvId[] = {0, 3, 1, 6, 4, 5, 2};

const uint8_t *tableForEnv(int env_id, size_t *out_len)
{
    /* OP_0B dispatch @ 0x157D2: env 0..6 → jump table; env >= 7 leaves id 0. */
    switch (env_id) {
    case 0:
        *out_len = kTableLen;
        return kMiddlegate;
    case 1:
        *out_len = kTableLen;
        return kAtlantium;
    case 2:
        *out_len = kTableLen;
        return kTundara;
    case 3:
        *out_len = kTableLen;
        return kVulcania;
    case 4:
    case 5:
        *out_len = kTableLen;
        return kSandsobar;
    case 6:
        *out_len = kTableLen;
        return kVulcania;
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
    if (env_id < 0 || env_id > 6) {
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

    /* 0x15776C: subq.b #1 on str_idx before (a0,d0) table fetch. */
    const unsigned idx = static_cast<unsigned>(str_idx) - 1u;
    if (idx >= len) {
        return -1;
    }
    return static_cast<int>(tbl[idx]);
}

int ServiceSignResolver::envIdForScreen(int screen_id, const Mm2AttribRecord *attrib)
{
    (void)attrib;

    for (size_t i = 0; i < sizeof(kScreenEnvRangeLo) / sizeof(kScreenEnvRangeLo[0]); ++i) {
        const uint8_t lo = kScreenEnvRangeLo[i];
        const uint8_t hi = kScreenEnvRangeHi[i];
        if (screen_id >= static_cast<int>(lo) && screen_id <= static_cast<int>(hi)) {
            return static_cast<int>(kScreenEnvId[i]);
        }
    }

    return 7;
}

void ServiceSignResolver::syncSignEnvId(uint8_t *a4, int screen_id, const Mm2AttribRecord *attrib)
{
    if (!a4) {
        return;
    }
    const int env_id = envIdForScreen(screen_id, attrib);
    if (env_id < 0) {
        return;
    }
    mm2_gs_set_u8(a4, MM2_GS_SIGN_ENV_ID, static_cast<uint8_t>(env_id));
}

int ServiceSignResolver::resolveForGameState(const uint8_t *a4, int screen_id,
                                             const Mm2AttribRecord *attrib, uint8_t str_idx)
{
    if (a4) {
        const uint8_t env = mm2_gs_u8(a4, MM2_GS_SIGN_ENV_ID);
        if (env <= 6) {
            return resolveAnmId(static_cast<int>(env), str_idx);
        }
    }
    return resolveForScreen(screen_id, attrib, str_idx);
}

int ServiceSignResolver::resolveForScreen(int screen_id, const Mm2AttribRecord *attrib, uint8_t str_idx)
{
    const int env_id = envIdForScreen(screen_id, attrib);
    if (env_id < 0 || env_id > 6) {
        return -1;
    }
    return resolveAnmId(env_id, str_idx);
}

int ServiceSignResolver::resolveForLocation(int location_id, uint8_t str_idx)
{
    return resolveForScreen(location_id, nullptr, str_idx);
}

}  // namespace mm2::events

#pragma once



#include <stdint.h>



#include "mm2_attrib_codec.h"



namespace mm2::events {



/** Resolve OP_0B str_idx -> NN.anm id via 0x15756 town tables (A4-$6C62..$6C02). */

class ServiceSignResolver {

public:

    /** env_id = A4-$79E3 (0..6); indexes per-town tables @ 0x15756. */

    static int resolveAnmId(int env_id, uint8_t str_idx);



    /** area_env_lookup @ 0x18AE — first matching range; town screens 0..4 → env 0. */

    static int envIdForScreen(int screen_id, const Mm2AttribRecord *attrib);



    /** Mirror map load @ 0x1C44: stash area_env_lookup(screen) in A4-$79E3. */

    static void syncSignEnvId(uint8_t *a4, int screen_id, const Mm2AttribRecord *attrib);



    /** Prefer A4-$79E3 (OP_0B @ 0x15772); falls back to envIdForScreen. */

    static int resolveForGameState(const uint8_t *a4, int screen_id, const Mm2AttribRecord *attrib,

                                   uint8_t str_idx);



    /** Returns -1 if str_idx has no mapped sign (e.g. str_idx >= 0x80 -> 0x4B path). */

    static int resolveForScreen(int screen_id, const Mm2AttribRecord *attrib, uint8_t str_idx);



    /** @deprecated Use resolveForScreen — interior maps 0..4 only. */

    static int resolveForLocation(int location_id, uint8_t str_idx);

};



}  // namespace mm2::events


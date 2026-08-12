#include "mm2/platform/Platform.h"

#include <chrono>

namespace mm2::platform {

/* Hostless unit-test clock (≈60 Hz), matching PlatformSDL::nowTicks without SDL.
 * CombatSession ack pacing and .anm overlays call this; game binaries link the
 * real SDL/Amiga platform instead. */

uint32_t nowTicks()
{
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
    return static_cast<uint32_t>((ms * 60) / 1000);
}

}  // namespace mm2::platform

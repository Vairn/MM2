#pragma once

#if defined(AMIGA) || defined(MM2_FORCE_AMIGA)
#  define MM2_HOST_AMIGA 1
#  undef MM2_HOST_SDL
#elif defined(MM2_HOST_SDL) || (!defined(MM2_HOST_AMIGA) && !defined(MM2_FORCE_AMIGA))
#  define MM2_HOST_SDL 1
#  undef MM2_HOST_AMIGA
#endif

#if MM2_HOST_AMIGA
#  define MM2_NO_STL 1
#  define MM2_HOST_AMIGA_AGA 1
#else
#  define MM2_NO_STL 0
#endif

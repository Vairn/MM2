#ifndef MM2_DBG_H
#define MM2_DBG_H

/** Amiga trace lines via ACE logWrite (WinUAE/Bartman console). Enabled when ACE_DEBUG
 * is ON at configure time (Debug or RelWithDebInfo+DBG preset). No-op on PC SDL. */
#if defined(ACE_DEBUG)
#include <ace/managers/log.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#define MM2_DBG(...) logWrite(__VA_ARGS__)
#pragma GCC diagnostic pop
#else
#define MM2_DBG(...) ((void)0)
#endif

#endif /* MM2_DBG_H */

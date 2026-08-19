#pragma once

#include "mm2/GameState.h"

#include "mm2_gamestate.h"

namespace mm2::events {

inline uint8_t eventCond(const uint8_t *a4)
{
    return mm2_gs_u8(a4, MM2_GS_COND_FLAG);
}
inline uint8_t eventCond(const GameStateView &gs)
{
    return eventCond(gs.a4());
}

inline void setEventCond(uint8_t *a4, uint8_t v)
{
    mm2_gs_set_u8(a4, MM2_GS_COND_FLAG, v);
}
inline void setEventCond(GameStateView &gs, uint8_t v)
{
    setEventCond(gs.a4(), v);
}

inline uint8_t eventExit(const uint8_t *a4)
{
    return mm2_gs_u8(a4, MM2_GS_EXIT_FLAGS);
}
inline uint8_t eventExit(const GameStateView &gs)
{
    return eventExit(gs.a4());
}

inline void setEventExit(uint8_t *a4, uint8_t v)
{
    mm2_gs_set_u8(a4, MM2_GS_EXIT_FLAGS, v);
}
inline void setEventExit(GameStateView &gs, uint8_t v)
{
    setEventExit(gs.a4(), v);
}

inline void orEventExit(uint8_t *a4, uint8_t bits)
{
    setEventExit(a4, static_cast<uint8_t>(eventExit(a4) | bits));
}
inline void orEventExit(GameStateView &gs, uint8_t bits)
{
    orEventExit(gs.a4(), bits);
}

inline uint8_t eventAbort(const uint8_t *a4)
{
    return mm2_gs_u8(a4, MM2_GS_SCRIPT_ABORT);
}
inline uint8_t eventAbort(const GameStateView &gs)
{
    return eventAbort(gs.a4());
}

inline void setEventAbort(uint8_t *a4, uint8_t v)
{
    mm2_gs_set_u8(a4, MM2_GS_SCRIPT_ABORT, v);
}
inline void setEventAbort(GameStateView &gs, uint8_t v)
{
    setEventAbort(gs.a4(), v);
}
inline void setEventAbort(uint8_t *a4)
{
    setEventAbort(a4, 1);
}
inline void setEventAbort(GameStateView &gs)
{
    setEventAbort(gs, 1);
}

inline void clearEventAbort(uint8_t *a4)
{
    mm2_gs_set_u8(a4, MM2_GS_SCRIPT_ABORT, 0);
}
inline void clearEventAbort(GameStateView &gs)
{
    clearEventAbort(gs.a4());
}

inline uint8_t eventWalkSpellLatch(const uint8_t *a4)
{
    return mm2_gs_u8(a4, MM2_GS_WALK_SPELL_LATCH);
}
inline uint8_t eventWalkSpellLatch(const GameStateView &gs)
{
    return eventWalkSpellLatch(gs.a4());
}

inline void setEventWalkSpellLatch(uint8_t *a4, uint8_t v)
{
    mm2_gs_set_u8(a4, MM2_GS_WALK_SPELL_LATCH, v);
}
inline void setEventWalkSpellLatch(GameStateView &gs, uint8_t v)
{
    setEventWalkSpellLatch(gs.a4(), v);
}
inline void setEventWalkSpellLatch(uint8_t *a4)
{
    setEventWalkSpellLatch(a4, 1);
}
inline void setEventWalkSpellLatch(GameStateView &gs)
{
    setEventWalkSpellLatch(gs, 1);
}

inline void clearEventWalkSpellLatch(uint8_t *a4)
{
    mm2_gs_set_u8(a4, MM2_GS_WALK_SPELL_LATCH, 0);
}
inline void clearEventWalkSpellLatch(GameStateView &gs)
{
    clearEventWalkSpellLatch(gs.a4());
}

}  // namespace mm2::events

#pragma once

/**
 * Amiga / ACE configuration for MM2 (mirrors LandsOfLore + EXTRACTED/docs/41-aga-port-plan.md).
 *
 * CMake must set the ACE_* cache variables before FetchContent(ace_engine), and the game
 * target must define ACE_USE_AGA_FEATURES / ACE_USE_ECS_FEATURES so headers match the
 * linked ACE build (see game/CMakeLists.txt).
 */

/* --- ACE feature flags (LoL: CMakeLists.txt ACE_USE_AGA_FEATURES / ACE_USE_ECS_FEATURES) --- */
#ifndef ACE_USE_AGA_FEATURES
#define ACE_USE_AGA_FEATURES 1
#endif

#ifndef ACE_USE_ECS_FEATURES
#define ACE_USE_ECS_FEATURES 1
#endif

/* --- MM2 AGA playfield (6 bitplanes → 64 colour indices) --- */
#ifndef MM2_AGA_SCREEN_BPP
#define MM2_AGA_SCREEN_BPP 6
#endif

#ifndef MM2_AGA_SCREEN_WIDTH
#define MM2_AGA_SCREEN_WIDTH 320
#endif

#ifndef MM2_AGA_SCREEN_HEIGHT
#define MM2_AGA_SCREEN_HEIGHT 200
#endif

#define MM2_AGA_PALETTE_PENS (1u << MM2_AGA_SCREEN_BPP)

/* ACE view/vport tags used in mm2_amiga_display.c (see LoL src/misc/screen.c) */
#define MM2_AGA_VIEW_TAG_GLOBAL_PALETTE TAG_VIEW_GLOBAL_PALETTE
#define MM2_AGA_VIEW_TAG_USES_AGA TAG_VIEW_USES_AGA
#define MM2_AGA_VPORT_TAG_BPP TAG_VPORT_BPP
#define MM2_AGA_VPORT_TAG_USES_AGA TAG_VPORT_USES_AGA
#define MM2_AGA_VPORT_TAG_FMODE TAG_VPORT_FMODE

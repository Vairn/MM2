#pragma once

/**
 * Amiga / ACE configuration for MM2 (AmigaPorts/ACE + EXTRACTED/docs/41-aga-port-plan.md).
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

/*
 * AGA chipset palette (12-bit COLOR, BPLCON3 banks) is independent of playfield depth:
 * a 4bpp screen on AGA still uses AGA palettes, not "ECS palette mode".
 *
 * MM2 playfield depth is 6bpp (64 indices) per 41-aga-port-plan.md — a port choice,
 * not a requirement of AGA colour hardware. Retail .32/.anm remain ECS on disk:
 * 5 planes, 32-word 0RGB sheets; decode promotes to playfield depth (bp5 cleared).
 */
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

/*
 * Palette banks on 6bpp AGA (ACE viewUpdateGlobalPalette):
 *   bank 0 → pens 0-31  (.32 / .anm world art — 5 planes on disk)
 *   bank 1 → pens 32-63 (CPU-drawn UI text and chrome)
 * mm2_amiga_push_palette() updates only the dirty bank(s), not all 64 pens.
 */
#define MM2_WORLD_PALETTE_FIRST 0u
#define MM2_WORLD_PALETTE_COUNT MM2_IMAGE32_PALETTE_COLORS
#define MM2_UI_PALETTE_BANK 1u

/*
 * Viewport .anm overlays (OP_0B / mode $17 @ 0x23C8C): retail loads only palette
 * entries 3-17 into hardware pens 3-17 (1-based colours 4-18). Env .32 art keeps
 * pens 0-2 and 18-31; overlay sprites use 3-17 without host pen remapping.
 */
#define MM2_ANM_OVERLAY_PALETTE_FIRST 3u
#define MM2_ANM_OVERLAY_PALETTE_LAST 17u

/*
 * Title / menu UI pens — indices 32+ so CPU-drawn text survives .32 blits
 * mm2_amiga_apply_play_world_palette() loads art pens 0-31 once per env load.
 */
#define MM2_UI_PEN_WHITE 32
#define MM2_UI_PEN_RED 33
#define MM2_UI_PEN_GOLD 34
#define MM2_UI_PEN_YELLOW 35
#define MM2_UI_PEN_GREY_LIGHT 36
#define MM2_UI_PEN_GREY_MID 37
#define MM2_UI_PEN_GREY_FOOTER 38
#define MM2_UI_PEN_GREY_DIM 39
#define MM2_UI_PEN_WARN 40
#define MM2_UI_PEN_BLACK 41

/* ACE view/vport tags used in mm2_amiga_display.c (see LoL src/misc/screen.c) */
#define MM2_AGA_VIEW_TAG_GLOBAL_PALETTE TAG_VIEW_GLOBAL_PALETTE
#define MM2_AGA_VIEW_TAG_USES_AGA TAG_VIEW_USES_AGA
#define MM2_AGA_VPORT_TAG_BPP TAG_VPORT_BPP
#define MM2_AGA_VPORT_TAG_USES_AGA TAG_VPORT_USES_AGA
#define MM2_AGA_VPORT_TAG_FMODE TAG_VPORT_FMODE
/* LoL uses FMODE 1 with TAG_VPORT_BPP 8 (chunky). MM2 title/playfield: FMODE 0 (classic fetch). */
#define MM2_AGA_VPORT_FMODE_VALUE 0

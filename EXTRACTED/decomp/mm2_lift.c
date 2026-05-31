#include "mm2_types.h"

#include <stdbool.h>
#include <string.h>

/*
 * This file is intentionally a scaffold.
 * Each function maps to one 68k routine by address/label.
 * Replace TODO blocks with recovered logic while preserving signatures.
 */

typedef struct Mm2Context {
    Mm2Workspace *ws;
    Mm2Runtime rt;
    Mm2EngineApi api;
} Mm2Context;

/* ---- Address-based helper macros ---- */

#define MM2_A4_OFF_PTR(base, off) ((uint8_t *)(base) - (off))

/* ---- 68k lifted stubs ---- */

/* 0x24920: LEA $7FFE,A4 ; RTS */
static inline Mm2Workspace *sub_24920_set_workspace_base(uintptr_t absolute_7ffe)
{
    return (Mm2Workspace *)absolute_7ffe;
}

/* 0x2567C: AllocMem wrapper through Exec base. */
static void *sub_2567c_allocmem(Mm2Context *ctx, uint32_t size, uint32_t flags)
{
    (void)ctx;
    (void)size;
    (void)flags;
    /* TODO: wire to emulator hooks or host allocator mirror. */
    return NULL;
}

/* 0x248AE..0x24A84 startup dispatcher (high-level reconstruction). */
int sub_248ae_startup(Mm2Context *ctx)
{
    if (!ctx || !ctx->ws) {
        return -1;
    }

    /*
     * Observed high-level flow:
     * - clear MANX arena
     * - capture Exec base from $4.w
     * - OpenLibrary("dos.library")
     * - allocate and initialize runtime blocks
     * - call engine bind/init thunks
     */

    /* TODO: implement memory clear loop and field writes from 0x248B0..0x248CA. */
    /* TODO: emulate OpenLibrary branch and alert path. */
    /* TODO: call sub_24928_init_runtime(ctx). */
    return 0;
}

/* 0x24928 init helper (LAB_24950 in IRA naming). */
int sub_24928_init_runtime(Mm2Context *ctx)
{
    if (!ctx || !ctx->ws) {
        return -1;
    }

    /* TODO: recover exact MANX block layout from 0x24928..0x24A5C. */
    return 0;
}

/* 0x256E screen loader: materialize mode-specific 0x100-byte pages. */
int sub_256e_load_screen_state(Mm2Context *ctx, uint8_t mode_id)
{
    uint8_t *src;
    uint8_t *dst_a;
    uint8_t *dst_b;
    size_t i;
    uintptr_t page;

    if (!ctx || !ctx->ws || !ctx->rt.map_blob_base) {
        return -1;
    }

    /* mode_previous compare / early return happens in caller in original code. */
    page = ((uintptr_t)mode_id << 9) + 0x100;
    src = (uint8_t *)ctx->rt.map_blob_base + page;

    /* A4-$AB46 / A4-$AA46 are currently modeled as raw pointer arithmetic. */
    dst_a = MM2_A4_OFF_PTR(ctx->ws, 0xAB46);
    dst_b = MM2_A4_OFF_PTR(ctx->ws, 0xAA46);

    for (i = 0; i < 0x100; i++) {
        dst_a[i] = src[i];
    }
    for (i = 0; i < 0x100; i++) {
        dst_b[i] = src[i];
    }

    /* TODO: lift remaining table copies from 0x260A+ blocks. */
    return 0;
}

/* 0x24C4 overland draw/update loop (simplified). */
int sub_24c4_overland_loop(Mm2Context *ctx)
{
    int row;
    int col;

    if (!ctx || !ctx->ws) {
        return -1;
    }

    for (row = 0; row < 16; row++) {
        for (col = 0; col < 16; col++) {
            /* TODO: decode tile fetch and draw invocation through api.draw_cell. */
        }
    }

    /* TODO: poll key, map N/S/E/W to 0x20..0x23, set exit_request on ESC. */
    return 0;
}

/* 0x1280 scheduler hub (main interactive loop). */
int sub_1280_main_loop(Mm2Context *ctx)
{
    bool running = true;

    if (!ctx || !ctx->ws) {
        return -1;
    }

    while (running) {
        int16_t key = 0;

        /* TODO: model event/timer pre-loop checks from 0x1280..0x12CA. */

        if (ctx->api.pump) {
            (void)ctx->api.pump();
        }
        if (ctx->api.read_key) {
            key = ctx->api.read_key();
        }

        /* TODO: dispatch key via lifted jump table (LAB_14AA family). */
        if (key == 0x1b) {
            /* ESC path. */
            running = false;
        }
    }

    return 0;
}

/* 0x545E session refresh / return-to-play gate. */
int sub_545e_session_refresh(Mm2Context *ctx)
{
    if (!ctx || !ctx->ws) {
        return -1;
    }

    /* TODO: lift branches using new_game_flag and helper calls around 0x545E. */
    if (ctx->api.pump) {
        (void)ctx->api.pump();
    }
    return 0;
}


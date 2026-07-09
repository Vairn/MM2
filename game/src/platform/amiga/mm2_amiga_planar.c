/**
 * Present .32 assets on the ACE playfield — planar blit only (no RGBA / chunky).
 * Rect fills use ACE blitRect; 8×8 text uses a 1bpp chip glyph atlas + masked ink stamp.
 */

#include "mm2/platform/amiga/Mm2AmigaPlanar.h"

#ifdef AMIGA

#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#include "mm2/platform/amiga/Mm2AmigaDisplay.h"
#include "mm2/gfx/mm2_font8x8.h"

#include <mini_std/string.h>

#include <ace/managers/blit.h>
#ifdef ACE_DEBUG
#include <ace/managers/log.h>
#endif
#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>

#ifdef ACE_USE_AGA_FEATURES
#include <hardware/custom.h>
#endif

static const mm2_image32_file *s_applied_palette_img;
static const mm2_image32_file *s_play_world_palette_src;

static UBYTE s_palette_dirty_world;
static UBYTE s_palette_dirty_ui;

static tBitMap *s_ui_cache_bm;
static tBitMap *s_pixel_target;

/* Retail buf_copy_rect @ 0x171AC — 3D viewport before sprite overlay. */
static tBitMap *s_viewport_cache_bm;
static UBYTE s_viewport_cache_valid;

static tBitMap *s_play_chrome_bm;

/* Font LSB = left pixel → Amiga MSB in byte (matches put_pixel_pen bit math). */
static UBYTE s_rev_byte[256];
static UBYTE s_rev_byte_ready;

/* 128 glyphs in a 16×8 cell grid → 128×64 1bpp mask atlas (chip). */
#define MM2_FONT_ATLAS_COLS 16u
#define MM2_FONT_ATLAS_ROWS 8u
#define MM2_FONT_ATLAS_W (MM2_FONT_ATLAS_COLS * 8u)
#define MM2_FONT_ATLAS_H (MM2_FONT_ATLAS_ROWS * 8u)
#define MM2_FONT_GLYPH_W 8u
#define MM2_FONT_GLYPH_H 8u

static tBitMap *s_font_mask_atlas; /* 128×64 × 1bpp */
static tBitMap *s_font_ink;        /* 8×8 × playfield depth — solid pen stamp */
static tBitMap *s_font_glyph_mask; /* 8×8 × 1bpp — one glyph extracted from atlas */
static UBYTE s_font_ink_pen = 0xFFu;
static UBYTE s_font_atlas_ready;
/* Dense UI text: CPU plot wins. Set 1 to use 2-blit atlas path (batched strings later). */
static UBYTE s_font_use_atlas_blit = 0;

#define MM2_VIEWPORT_CACHE_X 8u
#define MM2_VIEWPORT_CACHE_Y 8u
#define MM2_VIEWPORT_CACHE_W_MAX 208u
#define MM2_VIEWPORT_CACHE_H_MAX 120u
/* Live save/restore size — exploration uses full hood; combat round uses narrow. */
static UWORD s_viewport_cache_w = MM2_VIEWPORT_CACHE_W_MAX;
static UWORD s_viewport_cache_h = MM2_VIEWPORT_CACHE_H_MAX;

void mm2_amiga_push_palette(void);

static void mm2_amiga_ensure_rev_byte(void)
{
    UWORD i;
    UBYTE col;

    if (s_rev_byte_ready) {
        return;
    }
    for (i = 0; i < 256u; ++i) {
        UBYTE rev = 0;
        for (col = 0; col < 8u; ++col) {
            if (i & (1u << col)) {
                rev |= (UBYTE)(0x80u >> col);
            }
        }
        s_rev_byte[i] = rev;
    }
    s_rev_byte_ready = 1;
}

static tBitMap *mm2_amiga_pixel_dest(void)
{
    if (s_pixel_target) {
        return s_pixel_target;
    }
    {
        tSimpleBufferManager *pBfr = mm2AmigaDisplayGetBuffer();
        return (pBfr && pBfr->pBack) ? pBfr->pBack : NULL;
    }
}

static UBYTE mm2_amiga_rgb_to_ui_pen(UBYTE ubR, UBYTE ubG, UBYTE ubB)
{
    if (ubR <= 48 && ubG <= 48 && ubB <= 48) {
        return 0;
    }
    if (ubR >= 200 && ubG <= 96 && ubB <= 96) {
        return MM2_UI_PEN_RED;
    }
    if (ubR >= 200 && ubG >= 100 && ubG <= 170 && ubB >= 100 && ubB <= 170) {
        return MM2_UI_PEN_WARN;
    }
    if (ubR >= 200 && ubG >= 240 && ubB >= 80 && ubB <= 130) {
        return MM2_UI_PEN_YELLOW;
    }
    if (ubR >= 200 && ubG >= 170 && ubG < 240 && ubB <= 120) {
        return MM2_UI_PEN_GOLD;
    }
    if (ubR >= 210 && ubG >= 210 && ubB >= 210) {
        return MM2_UI_PEN_GREY_LIGHT;
    }
    if (ubR >= 180 && ubG >= 180 && ubB >= 180) {
        return MM2_UI_PEN_GREY_MID;
    }
    if (ubR >= 130 && ubG >= 130 && ubB >= 130) {
        return MM2_UI_PEN_GREY_FOOTER;
    }
    if (ubR >= 90 && ubG >= 90 && ubB >= 90) {
        return MM2_UI_PEN_GREY_DIM;
    }
    if (ubR > 96 || ubG > 96 || ubB > 96) {
        return MM2_UI_PEN_WHITE;
    }
    return 0;
}

static void mm2_amiga_brect(tBitMap *pDst, UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH, UBYTE pen)
{
    if (!pDst || uwW == 0 || uwH == 0 || !bitmapIsChip(pDst)) {
        return;
    }
    blitRect(pDst, (WORD)uwX, (WORD)uwY, (WORD)uwW, (WORD)uwH, pen);
}

void mm2_amiga_put_pixel_pen(UWORD uwX, UWORD uwY, UBYTE pen)
{
    tBitMap *pDst;
    UBYTE pl;
    UWORD bpr;

    pDst = mm2_amiga_pixel_dest();
    if (!pDst) {
        return;
    }
    bpr = pDst->BytesPerRow;
    for (pl = 0; pl < pDst->Depth; ++pl) {
        UBYTE *row = pDst->Planes[pl] + (ULONG)uwY * (ULONG)bpr;
        UBYTE bit = (UBYTE)(0x80 >> (uwX & 7));
        UWORD byte_x = uwX >> 3;
        if (pen & (1u << pl)) {
            row[byte_x] |= bit;
        } else {
            row[byte_x] &= (UBYTE)~bit;
        }
    }
}

void mm2_amiga_put_pixel_rgb(UWORD uwX, UWORD uwY, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA)
{
    if (ubA == 0 || uwX >= MM2_AGA_SCREEN_WIDTH || uwY >= MM2_AGA_SCREEN_HEIGHT) {
        return;
    }
    mm2_amiga_put_pixel_pen(uwX, uwY, mm2_amiga_rgb_to_ui_pen(ubR, ubG, ubB));
}

void mm2_amiga_fill_rect_pen(UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH, UBYTE pen)
{
    mm2_amiga_brect(mm2_amiga_pixel_dest(), uwX, uwY, uwW, uwH, pen);
}

void mm2_amiga_fill_rect_rgb(UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH, UBYTE ubR, UBYTE ubG, UBYTE ubB,
                             UBYTE ubA)
{
    if (ubA == 0) {
        return;
    }
    mm2_amiga_fill_rect_pen(uwX, uwY, uwW, uwH, mm2_amiga_rgb_to_ui_pen(ubR, ubG, ubB));
}

static void mm2_amiga_plot_glyph_row(UWORD uwX, UWORD uwY, UBYTE glyph_bits, UBYTE pen, tBitMap *pDst)
{
    const UWORD bpr = pDst->BytesPerRow;
    const UWORD byte_x = uwX >> 3;
    const UBYTE x_off = (UBYTE)(uwX & 7u);
    const UBYTE rev = s_rev_byte[glyph_bits];
    UBYTE pl;
    UBYTE left;
    UBYTE right = 0;

    if (x_off == 0u) {
        left = rev;
    } else {
        left = (UBYTE)(rev >> x_off);
        right = (UBYTE)(rev << (8u - x_off));
    }

    for (pl = 0; pl < pDst->Depth; ++pl) {
        UBYTE *p0 = pDst->Planes[pl] + (ULONG)uwY * (ULONG)bpr + (ULONG)byte_x;
        UBYTE *p1 = NULL;
        const UBYTE pen_bit = (UBYTE)((pen >> pl) & 1u);

        if (x_off != 0u && byte_x + 1u < bpr) {
            p1 = pDst->Planes[pl] + (ULONG)uwY * (ULONG)bpr + (ULONG)byte_x + 1u;
        }

        if (pen_bit) {
            *p0 |= left;
            if (p1) {
                *p1 |= right;
            }
        } else {
            *p0 &= (UBYTE)~left;
            if (p1) {
                *p1 &= (UBYTE)~right;
            }
        }
    }
}

static void mm2_amiga_font_atlas_destroy(void)
{
    if (s_font_mask_atlas) {
        bitmapDestroy(s_font_mask_atlas);
        s_font_mask_atlas = NULL;
    }
    if (s_font_ink) {
        bitmapDestroy(s_font_ink);
        s_font_ink = NULL;
    }
    if (s_font_glyph_mask) {
        bitmapDestroy(s_font_glyph_mask);
        s_font_glyph_mask = NULL;
    }
    s_font_ink_pen = 0xFFu;
    s_font_atlas_ready = 0;
}

/** Build 1bpp atlas + reusable 8×8 ink stamp (called from mm2_amiga_font_init). */
UBYTE mm2_amiga_font_atlas_create(void)
{
    const uint8_t *table;
    UWORD cp;
    UBYTE row;

    if (s_font_atlas_ready) {
        return 1;
    }
    mm2_amiga_ensure_rev_byte();

    s_font_mask_atlas = bitmapCreate((UWORD)MM2_FONT_ATLAS_W, (UWORD)MM2_FONT_ATLAS_H, 1, BMF_CLEAR);
    s_font_ink = bitmapCreate((UWORD)MM2_FONT_GLYPH_W, (UWORD)MM2_FONT_GLYPH_H, MM2_AGA_SCREEN_BPP, BMF_CLEAR);
    s_font_glyph_mask = bitmapCreate((UWORD)MM2_FONT_GLYPH_W, (UWORD)MM2_FONT_GLYPH_H, 1, BMF_CLEAR);
    if (!s_font_mask_atlas || !s_font_ink || !s_font_glyph_mask || !bitmapIsChip(s_font_mask_atlas) ||
        !bitmapIsChip(s_font_ink) || !bitmapIsChip(s_font_glyph_mask)) {
        mm2_amiga_font_atlas_destroy();
        return 0;
    }

    table = mm2_font8x8_live();
    for (cp = 0; cp < MM2_FONT8X8_GLYPHS; ++cp) {
        const UWORD gx = (UWORD)((cp % MM2_FONT_ATLAS_COLS) * MM2_FONT_GLYPH_W);
        const UWORD gy = (UWORD)((cp / MM2_FONT_ATLAS_COLS) * MM2_FONT_GLYPH_H);
        const uint8_t *glyph = table + (ULONG)cp * (ULONG)MM2_FONT8X8_ROWS;
        for (row = 0; row < MM2_FONT8X8_ROWS; ++row) {
            /* Stamp into 1bpp atlas with pen 1 (ink bits). */
            mm2_amiga_plot_glyph_row(gx, gy + (UWORD)row, glyph[row], 1u, s_font_mask_atlas);
        }
    }

    s_font_atlas_ready = 1;
    return 1;
}

void mm2_amiga_font_atlas_shutdown(void)
{
    mm2_amiga_font_atlas_destroy();
}

static void mm2_amiga_font_prepare_ink(UBYTE pen)
{
    if (!s_font_ink || pen == s_font_ink_pen) {
        return;
    }
    blitRect(s_font_ink, 0, 0, (WORD)MM2_FONT_GLYPH_W, (WORD)MM2_FONT_GLYPH_H, pen);
    s_font_ink_pen = pen;
}

static UBYTE mm2_amiga_blit_glyph_atlas(UWORD uwX, UWORD uwY, UBYTE ubCodepoint, UBYTE pen, tBitMap *pDst)
{
    UWORD gx;
    UWORD gy;

    if (!s_font_atlas_ready || !s_font_mask_atlas || !s_font_ink || !s_font_glyph_mask || !pDst) {
        return 0;
    }
    if (!bitmapIsChip(pDst) || !bitmapIsChip(s_font_ink) || !bitmapIsChip(s_font_mask_atlas)) {
        return 0;
    }

    gx = (UWORD)((ubCodepoint % MM2_FONT_ATLAS_COLS) * MM2_FONT_GLYPH_W);
    gy = (UWORD)((ubCodepoint / MM2_FONT_ATLAS_COLS) * MM2_FONT_GLYPH_H);

    /* Extract one 8×8 mask cell (aligned) then cookie-cut the solid-pen stamp. */
    blitCopy(s_font_mask_atlas, (WORD)gx, (WORD)gy, s_font_glyph_mask, 0, 0, (WORD)MM2_FONT_GLYPH_W,
             (WORD)MM2_FONT_GLYPH_H, MINTERM_COPY);
    mm2_amiga_font_prepare_ink(pen);
    blitCopyMask(s_font_ink, 0, 0, pDst, (WORD)uwX, (WORD)uwY, (WORD)MM2_FONT_GLYPH_W,
                 (WORD)MM2_FONT_GLYPH_H, s_font_glyph_mask->Planes[0]);
    return 1;
}

void mm2_amiga_draw_glyph8_pen(UWORD uwX, UWORD uwY, UBYTE ubCodepoint, UBYTE pen, UBYTE ubA)
{
    const uint8_t *table;
    const uint8_t *glyph;
    tBitMap *pDst;
    UBYTE row;

    if (ubA == 0 || ubCodepoint >= MM2_FONT8X8_GLYPHS) {
        return;
    }
    pDst = mm2_amiga_pixel_dest();
    if (!pDst || uwX >= MM2_AGA_SCREEN_WIDTH || uwY + 8u > MM2_AGA_SCREEN_HEIGHT) {
        return;
    }
    /*
     * Dense UI (party chooser, chrome, console boxes) draws hundreds of glyphs
     * per frame. Two 8×8 blitter jobs per glyph is slower than CPU planar plot;
     * atlas blit stays available via s_font_use_atlas_blit for a batched path.
     */
    if (s_font_use_atlas_blit && mm2_amiga_blit_glyph_atlas(uwX, uwY, ubCodepoint, pen, pDst)) {
        return;
    }
    mm2_amiga_ensure_rev_byte();
    table = mm2_font8x8_live();
    glyph = table + (ULONG)ubCodepoint * (ULONG)MM2_FONT8X8_ROWS;
    for (row = 0; row < MM2_FONT8X8_ROWS; ++row) {
        mm2_amiga_plot_glyph_row(uwX, uwY + (UWORD)row, glyph[row], pen, pDst);
    }
}

void mm2_amiga_draw_text_pen(UWORD uwX, UWORD uwY, const char *text, UBYTE pen, UBYTE ubA)
{
    UWORD cx;

    if (!text || ubA == 0) {
        return;
    }
    cx = uwX;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n') {
            cx = uwX;
            uwY += 8u;
            continue;
        }
        const unsigned uch = (unsigned)*p;
        if (uch < 32u || uch > 127u) {
            continue;
        }
        mm2_amiga_draw_glyph8_pen(cx, uwY, (UBYTE)uch, pen, ubA);
        cx += 8u;
    }
}

void mm2_amiga_apply_ui_palette(void)
{
    tVPort *pVp;
    ULONG *pPal;
    pVp = mm2AmigaDisplayGetVPort();
    if (!pVp || !pVp->pPalette) {
        return;
    }
    pPal = (ULONG *)pVp->pPalette;
    pPal[0] = 0;
    pPal[MM2_UI_PEN_WHITE] = 0x00FFFFFFUL;
    pPal[MM2_UI_PEN_RED] = 0x00FF0000UL;
    pPal[MM2_UI_PEN_GOLD] = 0x00FFDC5AUL;
    pPal[MM2_UI_PEN_YELLOW] = 0x00FFFF64UL;
    pPal[MM2_UI_PEN_GREY_LIGHT] = 0x00E6E6E6UL;
    pPal[MM2_UI_PEN_GREY_MID] = 0x00C8C8C8UL;
    pPal[MM2_UI_PEN_GREY_FOOTER] = 0x00969696UL;
    pPal[MM2_UI_PEN_GREY_DIM] = 0x006E6E6EUL;
    pPal[MM2_UI_PEN_WARN] = 0x00FF8080UL;
    pPal[MM2_UI_PEN_BLACK] = 0;
    s_palette_dirty_ui = 1;
    mm2_amiga_push_palette();
}

void mm2_amiga_blit_sync(void)
{
    blitWait();
}

void mm2_amiga_draw_glyph8(UWORD uwX, UWORD uwY, UBYTE ubCodepoint, UBYTE ubR, UBYTE ubG, UBYTE ubB, UBYTE ubA)
{
    mm2_amiga_draw_glyph8_pen(uwX, uwY, ubCodepoint, mm2_amiga_rgb_to_ui_pen(ubR, ubG, ubB), ubA);
}

void mm2_amiga_clear_screen(void)
{
    tBitMap *pDst = mm2_amiga_pixel_dest();
    if (!pDst) {
        return;
    }
    mm2_amiga_brect(pDst, 0, 0, MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, 0);
}

UBYTE mm2_amiga_ui_cache_create(void)
{
    if (s_ui_cache_bm) {
        return 1;
    }
    s_ui_cache_bm = bitmapCreate(MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, MM2_AGA_SCREEN_BPP, BMF_CLEAR);
    return s_ui_cache_bm != NULL;
}

void mm2_amiga_ui_cache_begin(void)
{
    if (!s_ui_cache_bm) {
        return;
    }
    s_pixel_target = s_ui_cache_bm;
    mm2_amiga_brect(s_ui_cache_bm, 0, 0, MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, 0);
}

void mm2_amiga_ui_cache_begin_keep(void)
{
    if (!s_ui_cache_bm) {
        return;
    }
    s_pixel_target = s_ui_cache_bm;
}

void mm2_amiga_ui_cache_end(void)
{
    s_pixel_target = NULL;
}

void mm2_amiga_ui_cache_present(void)
{
    mm2_amiga_ui_cache_present_rect(0, 0, (UWORD)MM2_AGA_SCREEN_WIDTH, (UWORD)MM2_AGA_SCREEN_HEIGHT);
}

void mm2_amiga_ui_cache_present_rect(UWORD uwX, UWORD uwY, UWORD uwW, UWORD uwH)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;
    UWORD x;
    UWORD w;

    if (!s_ui_cache_bm || uwW == 0 || uwH == 0) {
        return;
    }
    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    if (!bitmapIsChip(s_ui_cache_bm) || !bitmapIsChip(pDst)) {
        return;
    }
    /* Blitter copy width must be a multiple of 16. */
    x = (UWORD)(uwX & ~15U);
    w = (UWORD)(((ULONG)uwX + (ULONG)uwW + 15UL) & ~15UL);
    if (w > x) {
        w = (UWORD)(w - x);
    } else {
        return;
    }
    if ((ULONG)x + (ULONG)w > (ULONG)MM2_AGA_SCREEN_WIDTH) {
        w = (UWORD)(MM2_AGA_SCREEN_WIDTH - x);
    }
    if ((ULONG)uwY + (ULONG)uwH > (ULONG)MM2_AGA_SCREEN_HEIGHT) {
        uwH = (UWORD)(MM2_AGA_SCREEN_HEIGHT - uwY);
    }
    if (w == 0 || uwH == 0) {
        return;
    }
    blitCopy(s_ui_cache_bm, (WORD)x, (WORD)uwY, pDst, (WORD)x, (WORD)uwY, (WORD)w, (WORD)uwH, MINTERM_COPY);
}

void mm2_amiga_ui_cache_invalidate(void)
{
    s_pixel_target = NULL;
}

void mm2_amiga_ui_cache_destroy(void)
{
    s_pixel_target = NULL;
    if (s_ui_cache_bm) {
        bitmapDestroy(s_ui_cache_bm);
        s_ui_cache_bm = NULL;
    }
}

UBYTE mm2_amiga_ui_cache_ready(void)
{
    return s_ui_cache_bm != NULL;
}

UBYTE mm2_amiga_play_chrome_cache_create(void)
{
    if (s_play_chrome_bm) {
        return 1;
    }
    s_play_chrome_bm =
        bitmapCreate(MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, MM2_AGA_SCREEN_BPP, BMF_CLEAR);
    return s_play_chrome_bm != NULL;
}

void mm2_amiga_play_chrome_cache_begin(void)
{
    if (!s_play_chrome_bm) {
        return;
    }
    s_pixel_target = s_play_chrome_bm;
    mm2_amiga_brect(s_play_chrome_bm, 0, 0, MM2_AGA_SCREEN_WIDTH, MM2_AGA_SCREEN_HEIGHT, 0);
}

void mm2_amiga_play_chrome_cache_end(void)
{
    s_pixel_target = NULL;
}

void mm2_amiga_play_chrome_cache_present(void)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;

    if (!s_play_chrome_bm) {
        return;
    }
    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    if (!bitmapIsChip(s_play_chrome_bm) || !bitmapIsChip(pDst)) {
        return;
    }
    blitCopy(s_play_chrome_bm, 0, 0, pDst, 0, 0, (WORD)MM2_AGA_SCREEN_WIDTH, (WORD)MM2_AGA_SCREEN_HEIGHT,
             MINTERM_COPY);
}

void mm2_amiga_play_chrome_cache_invalidate(void)
{
    /* Bitmap kept alive — rebuild on next chrome_dirty_. */
}

void mm2_amiga_play_chrome_cache_destroy(void)
{
    if (s_play_chrome_bm) {
        bitmapDestroy(s_play_chrome_bm);
        s_play_chrome_bm = NULL;
    }
}

UBYTE mm2_amiga_play_chrome_cache_ready(void)
{
    return s_play_chrome_bm != NULL;
}

UBYTE mm2_amiga_viewport_cache_create(void)
{
    if (s_viewport_cache_bm) {
        return 1;
    }
    s_viewport_cache_bm = bitmapCreate(MM2_VIEWPORT_CACHE_W_MAX + 16, MM2_VIEWPORT_CACHE_H_MAX,
                                       MM2_AGA_SCREEN_BPP, BMF_CLEAR);
    return s_viewport_cache_bm != NULL;
}

void mm2_amiga_viewport_cache_set_size(UWORD uwW, UWORD uwH)
{
    if (uwW == 0 || uwH == 0 || uwW > MM2_VIEWPORT_CACHE_W_MAX || uwH > MM2_VIEWPORT_CACHE_H_MAX) {
        return;
    }
    if (uwW != s_viewport_cache_w || uwH != s_viewport_cache_h) {
        s_viewport_cache_w = uwW;
        s_viewport_cache_h = uwH;
        s_viewport_cache_valid = 0;
    }
}

void mm2_amiga_viewport_cache_save(void)
{
    tBitMap *pSrc;

    if (!s_viewport_cache_bm) {
        return;
    }
    pSrc = mm2_amiga_pixel_dest();
    if (!pSrc || !bitmapIsChip(s_viewport_cache_bm) || !bitmapIsChip(pSrc)) {
        return;
    }
    blitCopy(pSrc, (WORD)MM2_VIEWPORT_CACHE_X, (WORD)MM2_VIEWPORT_CACHE_Y, s_viewport_cache_bm, 0, 0,
             (WORD)s_viewport_cache_w, (WORD)s_viewport_cache_h, MINTERM_COPY);
    s_viewport_cache_valid = 1;
}

void mm2_amiga_viewport_cache_restore(void)
{
    tBitMap *pDst;

    if (!s_viewport_cache_valid || !s_viewport_cache_bm) {
        return;
    }
    pDst = mm2_amiga_pixel_dest();
    if (!pDst || !bitmapIsChip(s_viewport_cache_bm) || !bitmapIsChip(pDst)) {
        return;
    }
    blitCopy(s_viewport_cache_bm, 0, 0, pDst, (WORD)MM2_VIEWPORT_CACHE_X, (WORD)MM2_VIEWPORT_CACHE_Y,
             (WORD)s_viewport_cache_w, (WORD)s_viewport_cache_h, MINTERM_COPY);
}

void mm2_amiga_viewport_cache_invalidate(void)
{
    s_viewport_cache_valid = 0;
}

UBYTE mm2_amiga_viewport_cache_valid(void)
{
    return s_viewport_cache_valid;
}

UBYTE mm2_amiga_copy_front_to_back(void)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pFront;
    tBitMap *pBack;

    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pFront || !pBfr->pBack) {
        return 0;
    }
    pFront = pBfr->pFront;
    pBack = pBfr->pBack;
    if (pFront == pBack) {
        return 0;
    }
    if (!bitmapIsChip(pFront) || !bitmapIsChip(pBack)) {
        return 0;
    }
    blitWait();
    blitCopy(pFront, 0, 0, pBack, 0, 0, (WORD)MM2_AGA_SCREEN_WIDTH, (WORD)MM2_AGA_SCREEN_HEIGHT,
             MINTERM_COPY);
    return 1;
}

static void mm2_amiga_write_asset_palette(const mm2_image32_file *img)
{
    tVPort *pVp;
    ULONG *pPal;
    int i;

    if (!img) {
        return;
    }

    pVp = mm2AmigaDisplayGetVPort();
    if (!pVp || !pVp->pPalette) {
        return;
    }

    pPal = (ULONG *)pVp->pPalette;
    pPal[0] = 0;
    for (i = 1; i < MM2_IMAGE32_PALETTE_COLORS; ++i) {
        pPal[i] = img->palette_pen[i];
    }

    s_applied_palette_img = img;
}

void mm2_amiga_stage_asset_palette(const mm2_image32_file *img)
{
    if (!img) {
        return;
    }
    mm2_amiga_write_asset_palette(img);
}

void mm2_amiga_apply_play_world_palette(const mm2_image32_file *img)
{
    if (!img) {
        return;
    }
    s_play_world_palette_src = img;
    mm2_amiga_apply_palette(img);
}

void mm2_amiga_restore_play_world_palette(void)
{
    if (s_play_world_palette_src) {
        mm2_amiga_apply_palette(s_play_world_palette_src);
    }
}

void mm2_amiga_clear_play_world_palette(void)
{
    s_play_world_palette_src = NULL;
    s_applied_palette_img = NULL;
}

void mm2_amiga_apply_palette(const mm2_image32_file *img)
{
    if (!img || img == s_applied_palette_img) {
        return;
    }
    mm2_amiga_write_asset_palette(img);
    s_palette_dirty_world = 1;
    mm2_amiga_push_palette();
}

void mm2_amiga_blit_frame(const mm2_image32_file *img, uint16_t frame_index, UWORD uwDstX, UWORD uwDstY,
                          UBYTE opaque)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;
    const mm2_image32_frame *frame;
    tBitMap *pSrc;
    tBitMap *pMsk;
    WORD w;
    WORD h;

    if (!img || !img->frames || frame_index >= img->frame_count) {
        return;
    }

    frame = &img->frames[frame_index];
    pSrc = (tBitMap *)frame->bitmap;
    pMsk = (tBitMap *)frame->mask;
    if (!pSrc) {
        return;
    }

    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    if (!bitmapIsChip(pSrc) || !bitmapIsChip(pDst)) {
#if defined(ACE_DEBUG)
        logWrite(
            "MM2: blit SKIP %s: src chip=%u (%p) dst chip=%u (%p)\n",
            img->debug_label[0] ? img->debug_label : "?",
            (unsigned)bitmapIsChip(pSrc), pSrc->Planes[0],
            (unsigned)bitmapIsChip(pDst), pDst->Planes[0]
        );
#endif
        return;
    }

    w = (WORD)frame->width;
    h = (WORD)frame->height;
    if (uwDstX + (UWORD)w > MM2_AGA_SCREEN_WIDTH) {
        w = (WORD)(MM2_AGA_SCREEN_WIDTH - uwDstX);
    }

    if (uwDstY + (UWORD)h > MM2_AGA_SCREEN_HEIGHT) {
        h = (WORD)(MM2_AGA_SCREEN_HEIGHT - uwDstY);
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (opaque) {
        blitCopy(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, MINTERM_COPY);
    } else if (pMsk && pMsk->Planes[0]) {
        blitCopyMask(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, pMsk->Planes[0]);
    }
}

#ifdef ACE_USE_AGA_FEATURES
static void mm2_amiga_push_aga_palette_bank_range(const ULONG *pPalette, UBYTE ubBank, UBYTE ubFirst,
                                                  UBYTE ubLast)
{
    UBYTE i;

    for (i = ubFirst; i <= ubLast; ++i) {
        const ULONG ul = pPalette[((ULONG)ubBank * 32u) + i];
        const UBYTE r = (UBYTE)(ul >> 16);
        const UBYTE g = (UBYTE)(ul >> 8);
        const UBYTE b = (UBYTE)ul;
        g_pCustom->bplcon3 = (UWORD)(ubBank << 13);
        g_pCustom->color[i] = (UWORD)(((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
        g_pCustom->bplcon3 = (UWORD)((ubBank << 13) | (1u << 9));
        g_pCustom->color[i] = (UWORD)(((0x0Fu & r) << 8) | ((0x0Fu & g) << 4) | (0x0Fu & b));
    }
}

static void mm2_amiga_push_aga_palette_bank(const ULONG *pPalette, UBYTE ubBank)
{
    mm2_amiga_push_aga_palette_bank_range(pPalette, ubBank, 0, 31);
}
#endif

void mm2_amiga_apply_anm_palette(const uint16_t palette_words[MM2_ANM_PALETTE_COLORS])
{
    tVPort *pVp;
    ULONG *pPal;
    int i;

    if (!palette_words) {
        return;
    }

    pVp = mm2AmigaDisplayGetVPort();
    if (!pVp || !pVp->pPalette) {
        return;
    }

    pPal = (ULONG *)pVp->pPalette;
    for (i = (int)MM2_ANM_OVERLAY_PALETTE_FIRST; i <= (int)MM2_ANM_OVERLAY_PALETTE_LAST; ++i) {
        const uint16_t w = palette_words[i];
        const uint8_t r = (uint8_t)(((w >> 8) & 0x0F) * 17);
        const uint8_t g = (uint8_t)(((w >> 4) & 0x0F) * 17);
        const uint8_t b = (uint8_t)((w & 0x0F) * 17);
        pPal[i] = ((ULONG)r << 16) | ((ULONG)g << 8) | (ULONG)b;
    }

#ifdef ACE_USE_AGA_FEATURES
    if (pVp->eFlags & VP_FLAG_AGA) {
        mm2_amiga_push_aga_palette_bank_range(pPal, 0, (UBYTE)MM2_ANM_OVERLAY_PALETTE_FIRST,
                                              (UBYTE)MM2_ANM_OVERLAY_PALETTE_LAST);
        return;
    }
#endif
    s_palette_dirty_world = 1;
    mm2_amiga_push_palette();
}

void mm2_amiga_blit_anm_composed(const mm2_anm_composite_planar *img, UWORD uwDstX, UWORD uwDstY)
{
    tSimpleBufferManager *pBfr;
    tBitMap *pDst;
    tBitMap *pSrc;
    tBitMap *pMsk;
    WORD w;
    WORD h;

    if (!img || !img->bitmap) {
        return;
    }

    pSrc = (tBitMap *)img->bitmap;
    pMsk = (tBitMap *)img->mask;
    if (!pSrc) {
        return;
    }

    pBfr = mm2AmigaDisplayGetBuffer();
    if (!pBfr || !pBfr->pBack) {
        return;
    }
    pDst = pBfr->pBack;
    if (!bitmapIsChip(pSrc) || !bitmapIsChip(pDst)) {
        return;
    }

    w = (WORD)img->width;
    h = (WORD)img->height;
    if (uwDstX + (UWORD)w > MM2_AGA_SCREEN_WIDTH) {
        w = (WORD)(MM2_AGA_SCREEN_WIDTH - uwDstX);
    }
    if (uwDstY + (UWORD)h > MM2_AGA_SCREEN_HEIGHT) {
        h = (WORD)(MM2_AGA_SCREEN_HEIGHT - uwDstY);
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    if (pMsk && pMsk->Planes[0]) {
        blitCopyMask(pSrc, 0, 0, pDst, (WORD)uwDstX, (WORD)uwDstY, w, h, pMsk->Planes[0]);
    }
}

/* Release -flto inlines this into mm2AmigaDisplayFrameEnd after vPortWaitForEnd;
 * the wait loop leaves a stale a2, so viewUpdateGlobalPalette reads garbage. */
__attribute__((noinline)) void mm2_amiga_push_palette(void)
{
    tVPort *pVp;
    tView *pView;

    if (!s_palette_dirty_world && !s_palette_dirty_ui) {
        return;
    }
    pVp = mm2AmigaDisplayGetVPort();
    pView = pVp ? pVp->pView : NULL;
    if (!pView || !pVp || !pVp->pPalette) {
        s_palette_dirty_world = 0;
        s_palette_dirty_ui = 0;
        return;
    }
    if (!(pView->uwFlags & VIEW_FLAG_GLOBAL_PALETTE)) {
        s_palette_dirty_world = 0;
        s_palette_dirty_ui = 0;
        return;
    }

#ifdef ACE_USE_AGA_FEATURES
    if (pVp->eFlags & VP_FLAG_AGA) {
        const ULONG *pPal = (const ULONG *)pVp->pPalette;
        if (s_palette_dirty_world) {
            mm2_amiga_push_aga_palette_bank(pPal, 0);
            s_palette_dirty_world = 0;
        }
        if (s_palette_dirty_ui) {
            mm2_amiga_push_aga_palette_bank(pPal, MM2_UI_PALETTE_BANK);
            s_palette_dirty_ui = 0;
        }
        return;
    }
#endif
    viewUpdateGlobalPalette(pView);
    s_palette_dirty_world = 0;
    s_palette_dirty_ui = 0;
}

void mm2_amiga_present_end(void)
{
    /* Palette + buffer swap happen in mm2AmigaDisplayFrameEnd(). */
}

#endif /* AMIGA */

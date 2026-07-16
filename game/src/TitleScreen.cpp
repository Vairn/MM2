#include "mm2/TitleScreen.h"
#include "mm2/CppStdCompat.h"
#include "mm2/DataPath.h"
#include "mm2/Mm2Dbg.h"
#include "mm2/gfx/GfxBackend.h"
#include "mm2/platform/Audio.h"
#include "mm2/platform/Platform.h"
#include "mm2/runtime/PathScratch.h"

#include "mm2_gfx_sheet.h"
#include "mm2_pc_gfx_codec.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#include "mm2/platform/amiga/mm2_amiga_file.h"
#endif

#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
#include <ace/managers/memory.h>
#endif

namespace mm2 {

namespace {

constexpr int kIntroBgX = 3;
constexpr int kIntroBgY = 0;
constexpr int kMasterTitleFrame = 14;
constexpr int kScreenW = gfx::ScreenCompositor::kWidth;   // 320
// Center nwcp splash on the 320px field using visible (non-transparent) pixels so
// trailing transparent padding in the 300×90 frame does not skew the logo left.
int logoSplashCenterX(const mm2_image32_frame &frame)
{
#if MM2_HOST_AMIGA
    return (kScreenW - static_cast<int>(frame.width)) / 2;
#else
    if (!frame.rgba) {
        return (kScreenW - frame.width) / 2;
    }
    int min_x = frame.width;
    int max_x = -1;
    for (int y = 0; y < frame.height; ++y) {
        const uint8_t *row = frame.rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) * 4;
        for (int x = 0; x < frame.width; ++x) {
            if (row[x * 4 + 3] != 0) {
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
            }
        }
    }
    if (max_x < min_x) {
        return (kScreenW - frame.width) / 2;
    }
    const int content_w = max_x - min_x + 1;
    return (kScreenW - content_w) / 2 - min_x;
#endif
}

// Title menu reuses the Amiga manual/password screen layout (book.32 two-box
// frame). Two red-bordered boxes: TOP holds the open-book art flanking the
// centered game title; BOTTOM holds the selectable menu options.
constexpr int kBorderThickness = 2;
constexpr uint8_t kBorderR = 255, kBorderG = 0, kBorderB = 0;
// NTSC 320x200: top box (books + title) over bottom box (6 menu options).
constexpr int kTopBoxX = 8, kTopBoxY = 3, kTopBoxW = 304, kTopBoxH = 98;
constexpr int kBotBoxX = 8, kBotBoxY = 104, kBotBoxW = 304, kBotBoxH = 93;
constexpr int kBookY = 9;    // both books near the top of the top box
constexpr int kBookLeftX = 16;
// book.32 page-turn animation: one frame step per N ticks (frames 0..N-1 loop).
constexpr int kBookStepTicks = 3;
// Centered title lines (between the two books) + full-width tagline beneath.
const char *const kTitleLines[] = {"MIGHT", "and", "MAGIC", "Book Two"};
const char *const kTitleTagline = "Gates To Another World!";
// introclips.32 overlays on intro.32 (320×200). Each cel is pinned to the matching
// art on the pegasus scene — NOT the A4-$632E/$6344 words used @ 0x27096 (those
// track title-music / draw_cell staff positions, many Y values exceed 200).
// Screen coords below: fit each cel to intro.32 on a 320×200 field (validated by
// overlay alignment on retail assets).
struct IntroClipSlot {
    int frame;
    int x;
    int y;
};

// Pegasus overlay sequence: one cel at a time, mod 5 @ 0x26A1E (A4-$647A). Frames 0/1
// are the tail patch (subtle); 2 leg, 3 accent, 4 mouth — each at its own anchor.
constexpr IntroClipSlot kOverlayAnim[] = {
    {0, 169, 164}, {1, 169, 164}, {2, 35, 124}, {3, 170, 155}, {4, 265, 73},
};
constexpr int kOverlayPhaseCount = static_cast<int>(sizeof(kOverlayAnim) / sizeof(kOverlayAnim[0]));
// Frozen menu: all non-animated patches composited (ASM 0x1062 keeps last blits).
constexpr IntroClipSlot kOverlayFrozen[] = {
    {0, 169, 164}, {2, 35, 124}, {3, 170, 155}, {4, 265, 73},
};

// Scenery peekers (tree hollows + a gnome behind the pegasus). intro.32 already holds
// the correct art behind every hole — there is NO baked face — so an inactive peeker
// just isn't drawn; the re-blitted background shows through. Painting a solid cover
// box here would overwrite the bark/grass/wing, so only the active cel is composited.
constexpr IntroClipSlot kPeekerSlots[] = {
    {6, 250, 106},
    {8, 246, 128},
    {9, 246, 130},
    {10, 95, 96},
};
constexpr int kPeekerSlotCount = static_cast<int>(sizeof(kPeekerSlots) / sizeof(kPeekerSlots[0]));
// ~60 Hz via delayMs(16): pegasus phase ~0.33 s; peeker ~6–10 s on, ~2 s off, ~5 s before first.
constexpr int kOverlayStepTicks = 20;
constexpr int kPeekerDelayMinTicks = 360;
constexpr int kPeekerDelayMaxTicks = 600;
constexpr int kPeekerGapTicks = 120;
constexpr int kPeekerInitialGapTicks = 300;
uint32_t titleRngNext(uint32_t &state) {
    state = state * 1664525u + 1013904223u;
    return state;
}
// LoL-style frame-tick scheduling: deadlines live in the VBlank counter's space.
// The Amiga counter is 16-bit, so compare with a signed 16-bit delta (wrap-safe for
// any interval < ~32 k ticks, which covers every animation step here).
inline bool tickReached(uint32_t now, uint32_t deadline) {
    return static_cast<int16_t>(static_cast<uint16_t>(now) - static_cast<uint16_t>(deadline)) >= 0;
}
constexpr int kFadeTicks = 20;
constexpr int kLogoHoldTicks = 312;
// Amiga logo splash: brief hold then attract (was a raw "state_ticks_ >= 90" loop count).
constexpr int kLogoHoldAmigaTicks = 90;
int textPixelWidth(const char *s) { return static_cast<int>(std::strlen(s)) * 8; }
// Centered text x for the whole screen (or a sub-span when w0/x0 given).
int centerX(const char *s, int x0 = 0, int w0 = kScreenW)
{
    return x0 + (w0 - textPixelWidth(s)) / 2;
}

// Hollow red frame (matches the Amiga console_box outline; thickness px walls).
void drawRedFrame(gfx::ScreenCompositor &c, int x, int y, int w, int h)
{
    c.fillRect(x, y, w, kBorderThickness, kBorderR, kBorderG, kBorderB);
    c.fillRect(x, y + h - kBorderThickness, w, kBorderThickness, kBorderR, kBorderG, kBorderB);
    c.fillRect(x, y, kBorderThickness, h, kBorderR, kBorderG, kBorderB);
    c.fillRect(x + w - kBorderThickness, y, kBorderThickness, h, kBorderR, kBorderG, kBorderB);
}

}  // namespace

bool TitleScreen::bootPrepare(const char *data_dir, ui::CharacterUiKind ui_kind)
{
    data_dir_ = data_dir;
    ui_kind_ = ui_kind;
    boot_step_ = BootStep::UiCache;
    quit_ = false;
    pc_title_mode_ = false;
    has_master_title_ = false;
    freePcTitleSheets();
    mm2_image32_set_preview_opaque(0);
    gfx::resolveGfxBackend(data_dir);
    const gfx::GfxBackend resolved = gfx::gfxSettings().resolved;
    pc_title_mode_ = (resolved == gfx::GfxBackend::Cga || resolved == gfx::GfxBackend::Ega);
#if defined(ACE_DEBUG)
    MM2_DBG("MM2 DBG: TitleScreen bootPrepare (data_dir='%s' pc_title=%d)\n", data_dir ? data_dir : "",
            pc_title_mode_ ? 1 : 0);
#endif
    return true;
}

TitleScreen::BootOutcome TitleScreen::bootAdvance()
{
#if MM2_HOST_AMIGA
    switch (boot_step_) {
    case BootStep::UiCache:
        if (!mm2_amiga_ui_cache_create()) {
            return BootOutcome::Failed;
        }
        boot_step_ = BootStep::Nwcp;
        return BootOutcome::Working;
    case BootStep::Nwcp:
        mm2_image32_set_preview_opaque(0);
        if (pc_title_mode_) {
            has_nwcp_ = loadPcTitleSheet("nwcp.32", MM2_GFX_ROLE_TITLE, &nwcp_pc_);
            if (has_nwcp_) {
                const mm2_gfx_frame *f = mm2_gfx_sheet_frame(&nwcp_pc_, 0);
                logo_splash_x_ = f ? (kScreenW - static_cast<int>(f->width)) / 2 : 10;
                logo_phase_ = LogoPhase::Static;
                logo_alpha_ = 255;
            }
        } else {
            has_nwcp_ = loadImage("nwcp.32", &nwcp_);
            if (has_nwcp_) {
                logo_splash_x_ = logoSplashCenterX(nwcp_.frames[0]);
                logo_phase_ = LogoPhase::Static;
                logo_alpha_ = 255;
            }
        }
        boot_step_ = BootStep::Intro;
        return BootOutcome::Working;
    case BootStep::Intro:
        if (pc_title_mode_) {
            has_master_title_ = loadPcMasterSheet(&master_pc_);
            has_intro_ = has_master_title_;
        } else {
            has_intro_ = loadImage("intro.32", &intro_);
        }
        boot_step_ = BootStep::IntroClips;
        return BootOutcome::Working;
    case BootStep::IntroClips:
        if (!pc_title_mode_) {
            has_introclips_ = loadImage("introclips.32", &introclips_);
        }
        boot_step_ = BootStep::Book;
        return BootOutcome::Working;
    case BootStep::Book:
        mm2_image32_set_preview_opaque(0);
        if (pc_title_mode_) {
            has_book_ = loadPcTitleSheet("book.32", MM2_GFX_ROLE_TITLE, &book_pc_);
        } else {
            has_book_ = loadImage("book.32", &book_);
        }
        boot_step_ = BootStep::Roster;
        return BootOutcome::Working;
    case BootStep::Roster: {
        char *const roster_path = mm2_path_scratch_a();
        auto tryRoster = [&](const char *dir) -> bool {
            if (!joinDataPath(roster_path, MM2_PATH_SCRATCH_CAP, dir, "roster.dat")) {
                return false;
            }
            return mm2_roster_load_file(roster_path, &roster_) == MM2_ROSTER_OK;
        };
        has_roster_ = tryRoster(data_dir_);
        if (!has_roster_ && data_dir_ && data_dir_[0]) {
            has_roster_ = tryRoster("");
        }
        if (!has_roster_) {
            has_roster_ = tryRoster("dh1:");
        }
        if (!has_roster_) {
            has_roster_ = tryRoster("dh0:");
        }
        boot_step_ = BootStep::Items;
        return BootOutcome::Working;
    }
    case BootStep::Items:
        loadItemsDat();
        boot_step_ = BootStep::MenuCache;
        return BootOutcome::Working;
    case BootStep::MenuCache:
        buildTitleMenuCache();
        boot_step_ = BootStep::Done;
        return BootOutcome::Working;
    case BootStep::Done:
        break;
    }
#else
    switch (boot_step_) {
    case BootStep::UiCache:
        boot_step_ = BootStep::Nwcp;
        return BootOutcome::Working;
    case BootStep::Nwcp:
        mm2_image32_set_preview_opaque(0);
        if (pc_title_mode_) {
            has_nwcp_ = loadPcTitleSheet("nwcp.32", MM2_GFX_ROLE_TITLE, &nwcp_pc_);
            if (has_nwcp_) {
                const mm2_gfx_frame *f = mm2_gfx_sheet_frame(&nwcp_pc_, 0);
                logo_splash_x_ = f ? (kScreenW - static_cast<int>(f->width)) / 2 : 10;
                logo_phase_ = LogoPhase::Static;
                logo_alpha_ = 255;
            }
        } else {
            has_nwcp_ = loadImage("nwcp.32", &nwcp_);
            if (has_nwcp_) {
                logo_splash_x_ = logoSplashCenterX(nwcp_.frames[0]);
                logo_phase_ = LogoPhase::Static;
                logo_alpha_ = 255;
            }
        }
        boot_step_ = BootStep::Intro;
        return BootOutcome::Working;
    case BootStep::Intro:
        if (pc_title_mode_) {
            has_master_title_ = loadPcMasterSheet(&master_pc_);
            has_intro_ = has_master_title_;
        } else {
            has_intro_ = loadImage("intro.32", &intro_);
        }
        boot_step_ = BootStep::IntroClips;
        return BootOutcome::Working;
    case BootStep::IntroClips:
        if (!pc_title_mode_) {
            has_introclips_ = loadImage("introclips.32", &introclips_);
        }
        boot_step_ = BootStep::Book;
        return BootOutcome::Working;
    case BootStep::Book:
        mm2_image32_set_preview_opaque(0);
        if (pc_title_mode_) {
            has_book_ = loadPcTitleSheet("book.32", MM2_GFX_ROLE_TITLE, &book_pc_);
        } else {
            has_book_ = loadImage("book.32", &book_);
        }
        boot_step_ = BootStep::Roster;
        return BootOutcome::Working;
    case BootStep::Roster: {
        char *const roster_path = mm2_path_scratch_a();
        if (joinDataPath(roster_path, MM2_PATH_SCRATCH_CAP, data_dir_, "roster.dat")) {
            has_roster_ = mm2_roster_load_file(roster_path, &roster_) == MM2_ROSTER_OK;
        }
        boot_step_ = BootStep::Done;
        return BootOutcome::Working;
    }
    case BootStep::Done:
        break;
    default:
        break;
    }
#endif
    if (!has_intro_ && !has_nwcp_) {
        if (has_roster_) {
            peeker_rng_ = 1;
            return BootOutcome::ReadyAttract;
        }
        return BootOutcome::Failed;
    }
    peeker_rng_ = 1;
    if (pc_title_mode_ && !has_nwcp_ && has_master_title_) {
        return BootOutcome::ReadyAttract;
    }
    return has_nwcp_ ? BootOutcome::ReadyFade : BootOutcome::ReadyAttract;
}

void TitleScreen::bootLogoDraw()
{
    if (has_nwcp_ && logo_phase_ == LogoPhase::Static) {
        drawLogoSplash();
        return;
    }
#if MM2_HOST_AMIGA
    platform::clearScreen();
#else
    compositor_.clear(0, 0, 0, 255);
#endif
}

void TitleScreen::bootBeginLogoFade()
{
    logo_phase_ = LogoPhase::Hold;
    logo_alpha_ = 255;
    state_ticks_ = 0;
    logo_hold_until_ = 0;
    logo_fade_out_armed_ = false;
#if MM2_HOST_AMIGA
    logo_fade_armed_ = true;
    if (has_nwcp_) {
        mm2_amiga_apply_palette(&nwcp_);
        platform::logoFadeCapturePalette();
    }
#endif
}

bool TitleScreen::loadImage(const char *name, mm2_image32_file *out)
{
    char *const path = mm2_path_scratch_a();
    ::memset(out, 0, sizeof(*out));
    auto tryPath = [&](const char *dir) -> bool {
        if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, dir, name)) {
            return false;
        }
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
        MM2_DBG("MM2 DBG: Loading %s from '%s'\n", name, path);
#endif
        const mm2_image32_error err = mm2_image32_load_file(path, out);
        if (err != MM2_IMAGE32_OK || out->frame_count == 0) {
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
            MM2_DBG("MM2 DBG: Loading %s FAIL (err=%d)\n", name, (int)err);
#endif
            return false;
        }
#if MM2_HOST_AMIGA
        if (!out->frames[0].bitmap) {
#if defined(ACE_DEBUG)
            MM2_DBG("MM2 DBG: Loading %s FAIL (no planar bitmap)\n", name);
#endif
            return false;
        }
#if defined(ACE_DEBUG)
        MM2_DBG("MM2 DBG: Loading %s ok (%u frames, f0 %ux%u, chip free %lu)\n", name,
                (unsigned)out->frame_count, (unsigned)out->frames[0].width,
                (unsigned)out->frames[0].height, (unsigned long)memGetFreeChipSize());
#endif
        return true;
#else
        if (!out->frames[0].rgba) {
            return false;
        }
        return true;
#endif
    };

    if (tryPath(data_dir_)) {
        return true;
    }
#if MM2_HOST_AMIGA
    if (data_dir_ && data_dir_[0] && tryPath("")) {
        return true;
    }
    if (tryPath("dh1:")) {
        return true;
    }
    if (tryPath("dh0:")) {
        return true;
    }
#endif
    return false;
}

void TitleScreen::freePcTitleSheets()
{
    mm2_gfx_sheet_free(&nwcp_pc_);
    mm2_gfx_sheet_free(&book_pc_);
    mm2_gfx_sheet_free(&master_pc_);
}

bool TitleScreen::loadPcTitleSheet(const char *amiga_name, mm2_gfx_sheet_role role, mm2_gfx_sheet *out)
{
    char *const path = mm2_path_scratch_a();
    const gfx::GfxBackend backend = gfx::gfxSettings().resolved;
    const char *filename = gfx::resolveGfxFilename(backend, amiga_name);

    auto tryLoad = [&](const char *dir) -> bool {
        if (!dir || !joinDataPath(path, MM2_PATH_SCRATCH_CAP, dir, filename)) {
            return false;
        }
        mm2_gfx_sheet_free(out);
        mm2_pc_gfx_set_cga_palette(gfx::gfxSettings().cga_palette);
        if (mm2_pc_wall_sheet_load(path, role, NULL, out) != MM2_IMAGE32_OK || out->img.frame_count == 0) {
            mm2_gfx_sheet_free(out);
            return false;
        }
#if MM2_HOST_AMIGA
        return out->img.frames[0].bitmap != NULL;
#else
        return out->img.frames[0].rgba != NULL;
#endif
    };

    if (tryLoad(data_dir_)) {
        return true;
    }
    const char *fallback = gfx::gfxSettings().pc_gfx_dir;
    if (fallback[0] && tryLoad(fallback)) {
        return true;
    }
    return false;
}

bool TitleScreen::loadPcMasterSheet(mm2_gfx_sheet *out)
{
    char *const path = mm2_path_scratch_a();
    const gfx::GfxBackend backend = gfx::gfxSettings().resolved;
    const char *filename = (backend == gfx::GfxBackend::Cga) ? "MASTER.4" : "MASTER.16";

    auto tryLoad = [&](const char *dir) -> bool {
        if (!dir || !joinDataPath(path, MM2_PATH_SCRATCH_CAP, dir, filename)) {
            return false;
        }
        mm2_gfx_sheet_free(out);
        mm2_pc_gfx_set_cga_palette(gfx::gfxSettings().cga_palette);
        if (mm2_pc_wall_sheet_load(path, MM2_GFX_ROLE_TITLE, NULL, out) != MM2_IMAGE32_OK ||
            out->img.frame_count <= kMasterTitleFrame) {
            mm2_gfx_sheet_free(out);
            return false;
        }
        const mm2_gfx_frame *title = mm2_gfx_sheet_frame(out, kMasterTitleFrame);
#if MM2_HOST_AMIGA
        return title && title->bitmap;
#else
        return title && title->rgba;
#endif
    };

    if (tryLoad(data_dir_)) {
        return true;
    }
    const char *fallback = gfx::gfxSettings().pc_gfx_dir;
    if (fallback[0] && tryLoad(fallback)) {
        return true;
    }
    return false;
}

void TitleScreen::blitPcTitleFrame(const mm2_gfx_sheet &sheet, int frame, int x, int y, uint8_t alpha)
{
    const mm2_gfx_frame *f = mm2_gfx_sheet_frame(&sheet, frame);
    if (!f) {
        return;
    }
#if MM2_HOST_AMIGA
    (void)alpha;
    if (!f->bitmap) {
        return;
    }
    platform::blitImage32(&sheet.img, static_cast<uint16_t>(frame), static_cast<UWORD>(x), static_cast<UWORD>(y), 0);
#else
    if (!f->rgba) {
        return;
    }
    compositor_.blitRgba(f->rgba, f->width, f->height, x, y, true, alpha);
#endif
}

#if MM2_HOST_AMIGA
bool TitleScreen::consumeItemsDat(uint8_t **out, std::size_t *out_size)
{
    if (!out || !out_size || !has_items_dat_ || !items_dat_) {
        return false;
    }
    *out = items_dat_;
    *out_size = items_dat_size_;
    items_dat_ = nullptr;
    items_dat_size_ = 0;
    has_items_dat_ = false;
    return true;
}
#endif

bool TitleScreen::loadItemsDat()
{
#if MM2_HOST_AMIGA
    releaseItemsDat();
    char *const path = mm2_path_scratch_b();
    auto tryLoad = [&](const char *dir) -> bool {
        if (!joinDataPath(path, MM2_PATH_SCRATCH_CAP, dir, "items.dat")) {
            return false;
        }
        return mm2_amiga_read_file(path, &items_dat_, &items_dat_size_) != 0;
    };
    has_items_dat_ = tryLoad(data_dir_);
    if (!has_items_dat_ && data_dir_ && data_dir_[0]) {
        has_items_dat_ = tryLoad("");
    }
    if (!has_items_dat_) {
        has_items_dat_ = tryLoad("dh1:");
    }
    if (!has_items_dat_) {
        has_items_dat_ = tryLoad("dh0:");
    }
#if defined(ACE_DEBUG)
    if (has_items_dat_) {
        MM2_DBG("MM2 DBG: Loading items.dat ok (%lu bytes)\n", (unsigned long)items_dat_size_);
    } else {
        MM2_DBG("MM2 DBG: Loading items.dat FAIL\n");
    }
#endif
    return has_items_dat_;
#else
    return true;
#endif
}

void TitleScreen::releaseItemsDat()
{
#if MM2_HOST_AMIGA
    if (items_dat_) {
        platform::freeFileBuffer(items_dat_);
        items_dat_ = nullptr;
    }
    items_dat_size_ = 0;
    has_items_dat_ = false;
#endif
}

void TitleScreen::shutdown()
{
    releaseAttractAssets();
    mm2_image32_free(&book_);
    freePcTitleSheets();
#if MM2_HOST_AMIGA
    releaseItemsDat();
    mm2_amiga_ui_cache_destroy();
#endif
}

void TitleScreen::releaseIntroClips()
{
    mm2_image32_free(&introclips_);
    has_introclips_ = false;
}

void TitleScreen::releaseLogoAsset()
{
    if (pc_title_mode_) {
        mm2_gfx_sheet_free(&nwcp_pc_);
    } else if (has_nwcp_) {
        mm2_image32_free(&nwcp_);
    }
    has_nwcp_ = false;
}

void TitleScreen::releaseAttractAssets()
{
    if (pc_title_mode_) {
        mm2_gfx_sheet_free(&master_pc_);
        has_master_title_ = false;
    } else if (has_intro_) {
        mm2_image32_free(&intro_);
    }
    has_intro_ = false;
    releaseIntroClips();
    releaseLogoAsset();
#if MM2_HOST_AMIGA
    invalidatePegasusPaint();
#endif
}

void TitleScreen::ensureAttractAssetsLoaded()
{
    if (!data_dir_) {
        return;
    }
    if (!has_intro_) {
        has_intro_ = loadImage("intro.32", &intro_);
    }
    if (!has_introclips_) {
        has_introclips_ = loadImage("introclips.32", &introclips_);
    }
}

#if MM2_HOST_AMIGA
void TitleScreen::releaseChipForPlayMode()
{
    mm2_amiga_ui_cache_end();
    mm2_amiga_ui_cache_destroy();
    releaseAttractAssets();
}
#endif

#if MM2_HOST_AMIGA
void TitleScreen::invalidatePegasusPaint()
{
    pegasus_painted_overlay_phase_ = -1;
    pegasus_painted_peeker_slot_ = -1;
    pegasus_painted_peeker_visible_ = false;
}

void TitleScreen::invalidateTitleMenuPaint()
{
    mm2_amiga_ui_cache_invalidate();
    title_menu_painted_ = false;
    title_menu_painted_book_frame_ = -1;
    /* s_ui_cache_bm kept alive — do not bitmapDestroy from setState/tick (stack). */
}
#endif

void TitleScreen::skipLogoToAttract()
{
#if MM2_HOST_AMIGA
    platform::logoFadeCancel();
    logo_fade_armed_ = false;
    logo_fade_out_armed_ = false;
    invalidatePegasusPaint();
#endif
    logo_alpha_ = 0;
    state_ticks_ = 0;
    logo_hold_until_ = 0;
    logo_phase_ = LogoPhase::Done;
    resetAttractTiming();
}

void TitleScreen::skipLogoToMenu()
{
#if MM2_HOST_AMIGA
    platform::logoFadeCancel();
    logo_fade_armed_ = false;
    logo_fade_out_armed_ = false;
#endif
    logo_alpha_ = 0;
    state_ticks_ = 0;
    logo_hold_until_ = 0;
    logo_phase_ = LogoPhase::Done;
}

// Restart the attract-mode animation clocks from the current frame tick.
void TitleScreen::resetAttractTiming()
{
    const uint32_t now = platform::nowTicks();
    overlay_phase_ = 0;
    overlay_until_ = now + static_cast<uint32_t>(kOverlayStepTicks);
    peeker_visible_ = false;
    peeker_until_ = now + static_cast<uint32_t>(kPeekerInitialGapTicks);
    pickRandomPeekerSlot();
}

void TitleScreen::pickRandomPeekerSlot()
{
    if (kPeekerSlotCount <= 0) {
        return;
    }
    int idx = 0;
    if (kPeekerSlotCount > 1) {
        const int prev = peeker_slot_;
        do {
            idx = static_cast<int>((titleRngNext(peeker_rng_) >> 16) % static_cast<uint32_t>(kPeekerSlotCount));
        } while (idx == prev);
    }
    peeker_slot_ = idx;
    const uint32_t span =
        static_cast<uint32_t>(kPeekerDelayMaxTicks - kPeekerDelayMinTicks + 1);
    peeker_delay_ticks_ =
        kPeekerDelayMinTicks + static_cast<int>((titleRngNext(peeker_rng_) >> 16) % span);
}

void TitleScreen::logoEnter()
{
    logo_phase_ = LogoPhase::FadeIn;
    logo_alpha_ = 0;
    state_ticks_ = 0;
    logo_hold_until_ = 0;
    logo_fade_armed_ = false;
    logo_fade_out_armed_ = false;
}

TitleScreen::LogoAdvance TitleScreen::logoTick(const platform::KeyState &keys)
{
    if (keys.quit) {
        quit_ = true;
        return LogoAdvance::Continue;
    }
    if (keys.any_key) {
        return has_intro_ ? LogoAdvance::GoAttract : LogoAdvance::GoMenu;
    }

#if MM2_HOST_AMIGA
    if (logo_phase_ == LogoPhase::FadeIn) {
        if (platform::logoFadeConsumeDone()) {
            logo_phase_ = LogoPhase::Hold;
            logo_hold_until_ = 0;
        }
    } else if (logo_phase_ == LogoPhase::Hold) {
        const uint32_t now = platform::nowTicks();
        if (logo_hold_until_ == 0) {
            logo_hold_until_ = now + static_cast<uint32_t>(kLogoHoldAmigaTicks);
        }
        if (tickReached(now, logo_hold_until_)) {
            logo_phase_ = LogoPhase::FadeOut;
            logo_fade_out_armed_ = false;
        }
    } else if (logo_phase_ == LogoPhase::FadeOut) {
        if (!logo_fade_out_armed_) {
            platform::logoFadeBeginOut(kFadeTicks);
            logo_fade_out_armed_ = true;
        }
        if (platform::logoFadeConsumeDone()) {
            return has_intro_ ? LogoAdvance::GoAttract : LogoAdvance::GoMenu;
        }
    }
#else
    ++state_ticks_;
    switch (logo_phase_) {
    case LogoPhase::FadeIn:
        logo_alpha_ = static_cast<uint8_t>(std::min(255, (state_ticks_ * 255) / kFadeTicks));
        if (state_ticks_ >= kFadeTicks) {
            logo_phase_ = LogoPhase::Hold;
            logo_alpha_ = 255;
            state_ticks_ = 0;
        }
        break;
    case LogoPhase::Hold:
        logo_alpha_ = 255;
        if (state_ticks_ >= kLogoHoldTicks) {
            logo_phase_ = LogoPhase::FadeOut;
            state_ticks_ = 0;
        }
        break;
    case LogoPhase::FadeOut:
        logo_alpha_ = static_cast<uint8_t>(std::max(0, 255 - (state_ticks_ * 255) / kFadeTicks));
        if (state_ticks_ >= kFadeTicks) {
            return has_intro_ ? LogoAdvance::GoAttract : LogoAdvance::GoMenu;
        }
        break;
    default:
        break;
    }
#endif
    return LogoAdvance::Continue;
}

void TitleScreen::logoDraw() { drawLogoSplash(); }

void TitleScreen::attractEnter()
{
    resetAttractTiming();
#if MM2_HOST_AMIGA
    invalidatePegasusPaint();
#endif
    audio::playTitleTheme(true);
}

TitleScreen::AttractAdvance TitleScreen::attractTick(const platform::KeyState &keys)
{
    if (keys.quit) {
        quit_ = true;
        return AttractAdvance::Continue;
    }
    tickPegasusAnimation();
#if MM2_HOST_AMIGA
    /* Overlay 0x283FC: one note per attract iter (blocking Delay ticks). */
    audio::pumpTitleTheme();
#endif
    if (keys.any_key) {
        audio::stopTitleTheme();
        return AttractAdvance::GoMenu;
    }
    return AttractAdvance::Continue;
}

void TitleScreen::attractDraw() { drawAttract(); }

void TitleScreen::menuEnter()
{
    audio::stopTitleTheme();
    if (pc_title_mode_) {
        releaseLogoAsset();
        releaseIntroClips();
    } else {
        releaseAttractAssets();
    }
#if MM2_HOST_AMIGA
    invalidateTitleMenuPaint();
#endif
}

void TitleScreen::menuResume() {}

void TitleScreen::menuSuspend()
{
#if MM2_HOST_AMIGA
    MM2_DBG("MM2 GOTO: menuSuspend (blit_sync+clear)\n");
    mm2_amiga_blit_sync();
    platform::clearScreen();
    platform::applyUiPalette();
    title_menu_painted_ = false;
    title_menu_painted_book_frame_ = -1;
#endif
}

TitleScreen::MenuAdvance TitleScreen::menuTick(const platform::KeyState &keys)
{
    if (keys.quit) {
        quit_ = true;
        return MenuAdvance::None;
    }
    tickBookAnimation();
    if (keys.key_q) {
        quit_ = true;
        return MenuAdvance::Quit;
    }
    const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
    if (ch == 'V' && has_roster_) {
        MM2_DBG("MM2 GOTO: menu key V -> ViewParty\n");
        return MenuAdvance::ViewParty;
    }
    if ((keys.key_c || ch == 'C') && has_roster_) {
        MM2_DBG("MM2 GOTO: menu key C -> CreateCharacter (key_c=%d ch=%c)\n", keys.key_c ? 1 : 0, ch);
        return MenuAdvance::CreateCharacter;
    }
    if (ch == 'G' && has_roster_) {
        MM2_DBG("MM2 GOTO: menu key G -> ChooseParty (Goto Town)\n");
        return MenuAdvance::ChooseParty;
    }
    if (keys.key_m) {
        return MenuAdvance::Controls;
    }
    if (keys.key_o) {
        return MenuAdvance::Options;
    }
    return MenuAdvance::None;
}

void TitleScreen::menuDraw() { drawTitleMenu(); }

bool TitleScreen::controlsTick(const platform::KeyState &keys)
{
    if (keys.quit) {
        quit_ = true;
        return false;
    }
    return keys.escape;
}

void TitleScreen::controlsEnter()
{
#if MM2_HOST_AMIGA
    controls_subscreen_painted_ = false;
#endif
}

void TitleScreen::optionsEnter()
{
#if MM2_HOST_AMIGA
    options_subscreen_painted_ = false;
#endif
}

void TitleScreen::controlsDraw()
{
#if MM2_HOST_AMIGA
    if (!controls_subscreen_painted_) {
        buildControlsCache();
    }
    platform::applyUiPalette();
    mm2_amiga_ui_cache_present();
#else
    drawControls();
#endif
}

void TitleScreen::optionsDraw()
{
#if MM2_HOST_AMIGA
    if (!options_subscreen_painted_) {
        buildOptionsCache();
    }
    platform::applyUiPalette();
    mm2_amiga_ui_cache_present();
#else
    drawOptions();
#endif
}

void TitleScreen::buildControlsCache()
{
#if MM2_HOST_AMIGA
    platform::applyUiPalette();
    mm2_amiga_ui_cache_begin();
    drawControls();
    mm2_amiga_ui_cache_end();
    controls_subscreen_painted_ = true;
#endif
}

void TitleScreen::buildOptionsCache()
{
#if MM2_HOST_AMIGA
    platform::applyUiPalette();
    mm2_amiga_ui_cache_begin();
    drawOptions();
    mm2_amiga_ui_cache_end();
    options_subscreen_painted_ = true;
#endif
}

bool TitleScreen::optionsTick(const platform::KeyState &keys)
{
    if (keys.quit) {
        quit_ = true;
        return false;
    }
    return keys.escape;
}

void TitleScreen::returnToMenu()
{
#if MM2_HOST_AMIGA
    if (!mm2_amiga_ui_cache_ready()) {
        mm2_amiga_ui_cache_create();
    }
    invalidateTitleMenuPaint();
    if (mm2_amiga_ui_cache_ready()) {
        buildTitleMenuCache();
    }
#endif
}

void TitleScreen::blitPcTitleBackground()
{
    if (!pc_title_mode_ || !has_master_title_) {
        return;
    }
    const mm2_gfx_frame *title = mm2_gfx_sheet_frame(&master_pc_, kMasterTitleFrame);
    if (!title) {
        return;
    }
    const int y = (gfx::ScreenCompositor::kHeight - static_cast<int>(title->height)) / 2;
    blitPcTitleFrame(master_pc_, kMasterTitleFrame, 0, y);
}

void TitleScreen::drawTitleMenuOverlay()
{
    drawRedFrame(compositor_, kTopBoxX, kTopBoxY, kTopBoxW, kTopBoxH);
    int book_w = 89;
    int book_right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
    if (pc_title_mode_ && has_book_) {
        blitTitleMenuBooks();
        const mm2_gfx_frame *bk = mm2_gfx_sheet_frame(&book_pc_, 0);
        if (bk) {
            book_w = static_cast<int>(bk->width);
            book_right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
        }
    } else if (has_book_ && book_.frame_count > 0 && book_.frames) {
        const int fi = (book_frame_ < book_.frame_count) ? book_frame_ : 0;
        const mm2_image32_frame &bk = book_.frames[fi];
        if (bk.rgba) {
            book_w = bk.width;
            const int left_x = kBookLeftX;
            const int right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
            compositor_.blitRgba(bk.rgba, bk.width, bk.height, left_x, kBookY);
            compositor_.blitRgba(bk.rgba, bk.width, bk.height, right_x, kBookY);
            book_right_x = right_x;
        }
    }

    const int gap_x0 = kBookLeftX + book_w;
    const int gap_w = book_right_x - gap_x0;
    int ty = kBookY + 4;
    for (const char *line : kTitleLines) {
        compositor_.drawTextShadow(centerX(line, gap_x0, gap_w), ty, line, 255, 220, 90);
        ty += 12;
    }
    compositor_.drawTextShadow(centerX(kTitleTagline, kTopBoxX, kTopBoxW), kTopBoxY + kTopBoxH - 15, kTitleTagline,
                               255, 220, 90);

    drawRedFrame(compositor_, kBotBoxX, kBotBoxY, kBotBoxW, kBotBoxH);
    struct MenuItem {
        const char *text;
        bool enabled;
    };
    const MenuItem items[] = {
        {"C - Create Character", has_roster_},
        {"V - View Party", has_roster_},
        {"G - Goto Town", has_roster_},
        {"M - Controls", true},
        {"O - Options", true},
        {"Q - Quit", true},
    };

    int my = kBotBoxY + 8;
    for (const MenuItem &it : items) {
        if (it.enabled) {
            compositor_.drawTextShadow(centerX(it.text), my, it.text, 230, 230, 230);
        } else {
            compositor_.drawText(centerX(it.text), my, it.text, 110, 110, 110, 255);
        }
        my += 12;
    }
    if (!has_roster_) {
        const char *warn = "(roster.dat not loaded)";
        compositor_.drawText(centerX(warn), my + 1, warn, 255, 128, 128, 255);
    }
}

void TitleScreen::blitIntroClipFrame(int frame_index, int x, int y)
{
    if (!has_introclips_ || frame_index < 0 || frame_index >= introclips_.frame_count) {
        return;
    }
#if MM2_HOST_AMIGA
    platform::blitImage32(&introclips_, frame_index, x, y, 0);
#else
    const mm2_image32_frame &frame = introclips_.frames[frame_index];
    if (!frame.rgba) {
        return;
    }
    compositor_.blitRgba(frame.rgba, frame.width, frame.height, x, y);
#endif
}

void TitleScreen::drawIntroPegasus(bool animate_overlays)
{
#if MM2_HOST_AMIGA
    ensureAttractAssetsLoaded();
#endif
#if MM2_HOST_AMIGA
    pegasus_painted_overlay_phase_ = overlay_phase_;
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    MM2_DBG("MM2 DBG: Draw TitleAttract intro=%d overlay_phase=%d peeker=%d\n", has_intro_ ? 1 : 0,
            overlay_phase_, peeker_visible_ ? 1 : 0);
#endif
    pegasus_painted_peeker_visible_ = peeker_visible_;
    pegasus_painted_peeker_slot_ = peeker_slot_;
    /* Always composite to pBack — skipping redraw leaves the alternate DB buffer
     * stale (e.g. nwcp logo) and causes a flash when ACE swaps front/back. */
    platform::clearScreen();
    if (has_intro_) {
#if MM2_HOST_AMIGA
        mm2_amiga_apply_palette(&intro_);
#endif
        platform::blitImage32(&intro_, 0, kIntroBgX, kIntroBgY, 1);
    }
#else
    compositor_.clear(0, 0, 0, 255);
    if (pc_title_mode_) {
        blitPcTitleBackground();
    } else if (has_intro_ && intro_.frames[0].rgba) {
        compositor_.blitRgba(intro_.frames[0].rgba, intro_.frames[0].width, intro_.frames[0].height, kIntroBgX,
                             kIntroBgY);
    }
#endif
    if (pc_title_mode_ || !has_introclips_) {
        return;
    }

    if (animate_overlays) {
        if (overlay_phase_ >= 0 && overlay_phase_ < kOverlayPhaseCount) {
            const IntroClipSlot &slot = kOverlayAnim[overlay_phase_];
            blitIntroClipFrame(slot.frame, slot.x, slot.y);
        }
    } else {
        for (const IntroClipSlot &slot : kOverlayFrozen) {
            blitIntroClipFrame(slot.frame, slot.x, slot.y);
        }
    }

    // intro.32 was just re-blitted, so every inactive hole already shows the correct
    // scenery — only composite the single active peeker cel on top.
    if (peeker_visible_ && peeker_slot_ >= 0 && peeker_slot_ < kPeekerSlotCount) {
        const IntroClipSlot &slot = kPeekerSlots[peeker_slot_];
        blitIntroClipFrame(slot.frame, slot.x, slot.y);
    }
#if MM2_HOST_AMIGA
    mm2_amiga_blit_sync();
#endif
}

void TitleScreen::tickPegasusAnimation()
{
    if (!has_introclips_) {
        return;
    }
    const uint32_t now = platform::nowTicks();

    // Pegasus overlay cels advance one phase per kOverlayStepTicks frame ticks.
    if (tickReached(now, overlay_until_)) {
        overlay_phase_ = (overlay_phase_ + 1) % kOverlayPhaseCount;
        overlay_until_ = now + static_cast<uint32_t>(kOverlayStepTicks);
    }

    // Scenery peekers: hidden for a gap, then one random slot shows for a random duration.
    if (tickReached(now, peeker_until_)) {
        if (peeker_visible_) {
            peeker_visible_ = false;
            peeker_until_ = now + static_cast<uint32_t>(kPeekerGapTicks);
        } else {
            pickRandomPeekerSlot();  // sets peeker_delay_ticks_ (visible duration)
            peeker_visible_ = true;
            peeker_until_ = now + static_cast<uint32_t>(peeker_delay_ticks_);
        }
    }
}

void TitleScreen::drawAttract()
{
    drawIntroPegasus(true);
}

void TitleScreen::drawLogoSplash()
{
#if MM2_HOST_AMIGA
    if (has_nwcp_ && nwcp_.frames[0].bitmap) {
        mm2_amiga_apply_palette(&nwcp_);
        if (logo_phase_ == LogoPhase::Static) {
            platform::clearScreen();
            const int logo_y = (gfx::ScreenCompositor::kHeight - static_cast<int>(nwcp_.frames[0].height)) / 2;
            platform::blitImage32(&nwcp_, 0, logo_splash_x_, logo_y, 0);
            return;
        }
        if (logo_phase_ == LogoPhase::FadeIn && !logo_fade_armed_) {
            platform::logoFadeCapturePalette();
            platform::logoFadeBeginIn(kFadeTicks);
            logo_fade_armed_ = true;
        }
        platform::clearScreen();
        const int logo_y = (gfx::ScreenCompositor::kHeight - static_cast<int>(nwcp_.frames[0].height)) / 2;
        platform::blitImage32(&nwcp_, 0, logo_splash_x_, logo_y, 0);
    } else {
        platform::clearScreen();
    }
#else
    compositor_.clear(0, 0, 0, 255);
    if (pc_title_mode_ && has_nwcp_) {
        const int logo_h = static_cast<int>(nwcp_pc_.img.frames[0].height);
        const int logo_y = (gfx::ScreenCompositor::kHeight - logo_h) / 2;
        const uint8_t alpha = (logo_phase_ == LogoPhase::Static) ? 255 : logo_alpha_;
        if (alpha > 0) {
            blitPcTitleFrame(nwcp_pc_, 0, logo_splash_x_, logo_y, alpha);
        }
    } else if (has_nwcp_ && nwcp_.frames[0].rgba) {
        const int logo_h = nwcp_.frames[0].height;
        const int logo_y = (gfx::ScreenCompositor::kHeight - logo_h) / 2;
        const uint8_t alpha = (logo_phase_ == LogoPhase::Static) ? 255 : logo_alpha_;
        if (alpha > 0) {
            compositor_.blitRgba(nwcp_.frames[0].rgba, nwcp_.frames[0].width, logo_h, logo_splash_x_, logo_y, true,
                                 alpha);
        }
    }
#endif
}

void TitleScreen::drawControls()
{
    compositor_.clear(0, 0, 0, 255);
#if MM2_HOST_AMIGA
    platform::applyUiPalette();
#endif
    drawRedFrame(compositor_, 8, 8, 304, 184);
    int y = 20;
    compositor_.drawTextShadow(centerX("COMMAND REFERENCE", 0, 320), y, "COMMAND REFERENCE", 255, 255, 100);
    y += 24;
    const char* controls[] = {
        "A - Attack",
        "B - Block",
        "C - Cast",
        "D - Defend",
        "E - Exchange",
        "F - Flee",
        "H - Hide",
        "M - Move",
        "Q - Quit",
        "S - Shoot",
        "U - Use",
        "V - View",
        "W - Wait",
        "X - Exit"
    };

    int x1 = 40;
    int x2 = 180;
    for (size_t i = 0; i < std::size(controls); ++i) {
        int x = (i % 2 == 0) ? x1 : x2;
        compositor_.drawTextShadow(x, y, controls[i], 200, 200, 200);
        if (i % 2 != 0) y += 12;
    }

    y += 24;
    compositor_.drawTextShadow(centerX("( 'ESC' to go back )", 0, 320), y, "( 'ESC' to go back )", 150, 150, 150);
}

void TitleScreen::drawOptions()
{
    compositor_.clear(0, 0, 0, 255);
#if MM2_HOST_AMIGA
    platform::applyUiPalette();
#endif
    drawRedFrame(compositor_, 40, 40, 240, 120);
    int y = 56;
    compositor_.drawTextShadow(centerX("GAME OPTIONS", 0, 320), y, "GAME OPTIONS", 255, 255, 100);
    y += 24;
    compositor_.drawTextShadow(centerX("S - Sound (On/Off)", 0, 320), y, "S - Sound (On/Off)", 200, 200, 200);
    y += 16;
    compositor_.drawTextShadow(centerX("D - Delay (1-9)", 0, 320), y, "D - Delay (1-9)", 200, 200, 200);
    y += 24;
    compositor_.drawTextShadow(centerX("( 'ESC' to go back )", 0, 320), y, "( 'ESC' to go back )", 150, 150, 150);
}

void TitleScreen::blitTitleMenuBooks()
{
    if (pc_title_mode_) {
        if (!has_book_ || book_pc_.img.frame_count <= 0) {
            return;
        }
        const int fi = (book_frame_ < static_cast<int>(book_pc_.img.frame_count)) ? book_frame_ : 0;
        const mm2_gfx_frame *bk = mm2_gfx_sheet_frame(&book_pc_, fi);
        if (!bk) {
            return;
        }
        const int book_w = static_cast<int>(bk->width);
        const int left_x = kBookLeftX;
        const int right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
        blitPcTitleFrame(book_pc_, fi, left_x, kBookY);
        blitPcTitleFrame(book_pc_, fi, right_x, kBookY);
        return;
    }
    if (!has_book_ || book_.frame_count <= 0 || !book_.frames) {
        return;
    }
    const int fi = (book_frame_ < book_.frame_count) ? book_frame_ : 0;
    const mm2_image32_frame &bk = book_.frames[fi];
#if MM2_HOST_AMIGA
    if (!bk.bitmap) {
        return;
    }
    const int book_w = static_cast<int>(bk.width);
    const int left_x = kBookLeftX;
    const int right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
    platform::blitImage32(&book_, fi, left_x, kBookY);
    platform::blitImage32(&book_, fi, right_x, kBookY);
#else
    if (!bk.rgba) {
        return;
    }
    const int book_w = static_cast<int>(bk.width);
    const int left_x = kBookLeftX;
    const int right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
    compositor_.blitRgba(bk.rgba, bk.width, bk.height, left_x, kBookY);
    compositor_.blitRgba(bk.rgba, bk.width, bk.height, right_x, kBookY);
#endif
}

void TitleScreen::presentTitleMenu()
{
#if MM2_HOST_AMIGA
    if (has_book_ && book_.frame_count > 0) {
        mm2_amiga_apply_palette(&book_);
    }
    platform::applyUiPalette();
    mm2_amiga_blit_sync();
    mm2_amiga_ui_cache_present();
    blitTitleMenuBooks();
#else
    drawTitleMenu();
#endif
}

void TitleScreen::buildTitleMenuCache()
{
#if MM2_HOST_AMIGA
    if (has_book_ && book_.frame_count > 0) {
        mm2_amiga_apply_palette(&book_);
    }
    platform::applyUiPalette();
    mm2_amiga_ui_cache_begin();
#endif

    drawRedFrame(compositor_, kTopBoxX, kTopBoxY, kTopBoxW, kTopBoxH);

    int book_w = 89;
    int book_right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
    if (pc_title_mode_ && has_book_ && book_pc_.img.frame_count > 0) {
        const mm2_gfx_frame *bk = mm2_gfx_sheet_frame(&book_pc_, 0);
        if (bk) {
            book_w = static_cast<int>(bk->width);
            book_right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
        }
    } else if (has_book_ && book_.frame_count > 0 && book_.frames && book_.frames[0].width) {
        book_w = static_cast<int>(book_.frames[0].width);
        book_right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
    }

    const int gap_x0 = kBookLeftX + book_w;
    const int gap_w = book_right_x - gap_x0;
    int ty = kBookY + 4;
    for (const char *line : kTitleLines) {
        compositor_.drawTextShadow(centerX(line, gap_x0, gap_w), ty, line, 255, 220, 90);
        ty += 12;
    }
    compositor_.drawTextShadow(centerX(kTitleTagline, kTopBoxX, kTopBoxW), kTopBoxY + kTopBoxH - 15, kTitleTagline,
                               255, 220, 90);

    drawRedFrame(compositor_, kBotBoxX, kBotBoxY, kBotBoxW, kBotBoxH);
    struct MenuItem {
        const char *text;
        bool enabled;
    };
    const MenuItem items[] = {
        {"C - Create Character", has_roster_},
        {"V - View Party", has_roster_},
        {"G - Goto Town", has_roster_},
        {"M - Controls", true},
        {"O - Options", true},
        {"Q - Quit", true},
    };

    int my = kBotBoxY + 8;
    for (const MenuItem &it : items) {
        if (it.enabled) {
            compositor_.drawTextShadow(centerX(it.text), my, it.text, 230, 230, 230);
        } else {
            compositor_.drawText(centerX(it.text), my, it.text, 110, 110, 110, 255);
        }
        my += 12;
    }
    if (!has_roster_) {
        const char *warn = "(roster.dat not loaded)";
        compositor_.drawText(centerX(warn), my + 1, warn, 255, 128, 128, 255);
    }

#if MM2_HOST_AMIGA
    mm2_amiga_ui_cache_end();
#endif
}

void TitleScreen::drawTitleMenu()
{
#if MM2_HOST_AMIGA
    presentTitleMenu();
    return;
#endif

    compositor_.clear(0, 0, 0, 255);
    /* TitleMenu is black + book/menu chrome (Amiga @ 0x1062; PC CGA/EGA same layout).
     * Attract uses MASTER.16 frame 14 / intro.32 — not under the menu boxes. */
    if (!pc_title_mode_ && has_intro_ && intro_.frames[0].rgba) {
        compositor_.blitRgba(intro_.frames[0].rgba, intro_.frames[0].width, intro_.frames[0].height, kIntroBgX,
                             kIntroBgY);
    }

    drawTitleMenuOverlay();
}

void TitleScreen::tickBookAnimation()
{
    const int frame_count =
        pc_title_mode_ ? static_cast<int>(book_pc_.img.frame_count) : static_cast<int>(book_.frame_count);
    if (!has_book_ || frame_count <= 1) {
        return;
    }
    const uint32_t now = platform::nowTicks();
    if (tickReached(now, book_until_)) {
        book_frame_ = (book_frame_ + 1) % frame_count;
        book_until_ = now + static_cast<uint32_t>(kBookStepTicks);
    }
}

}  // namespace mm2

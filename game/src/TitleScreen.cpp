#include "mm2/TitleScreen.h"
#include "mm2/CppStdCompat.h"
#include "mm2/DataPath.h"
#include "mm2/Mm2Dbg.h"
#include "mm2/platform/Platform.h"
#include "mm2/runtime/PathScratch.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/Mm2AmigaPlanar.h"
#endif

#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
#include <ace/managers/memory.h>
#endif

namespace mm2 {

namespace {

#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
const char *titleStateName(TitleState s)
{
    switch (s) {
    case TitleState::LogoFadeIn:
        return "LogoFadeIn";
    case TitleState::LogoHold:
        return "LogoHold";
    case TitleState::LogoFadeOut:
        return "LogoFadeOut";
    case TitleState::TitleAttract:
        return "TitleAttract";
    case TitleState::TitleMenu:
        return "TitleMenu";
    case TitleState::Controls:
        return "Controls";
    case TitleState::Options:
        return "Options";
    case TitleState::CharacterUi:
        return "CharacterUi";
    }
    return "?";
}
#endif

constexpr int kIntroBgX = 3;
constexpr int kIntroBgY = 0;
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
constexpr int kFadeTicks = 60;
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
void TitleScreen::setState(TitleState next, const char *reason)
{
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    if (state_ != next) {
        MM2_DBG("MM2 DBG: TitleScreen %s -> %s (%s)\n", titleStateName(state_), titleStateName(next),
                reason ? reason : "");
    }
#else
    (void)reason;
#endif
#if MM2_HOST_AMIGA
    if (next == TitleState::TitleAttract && state_ == TitleState::TitleMenu) {
        invalidatePegasusPaint();
    }
    if (next == TitleState::TitleMenu && state_ != TitleState::TitleMenu) {
        invalidateTitleMenuPaint();
    }
#endif
    state_ = next;
}

#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
void TitleScreen::dbgLogDisplay() const
{
    static TitleState s_last = TitleState::LogoFadeIn;
    if (state_ == s_last) {
        return;
    }
    s_last = state_;
    switch (state_) {
    case TitleState::LogoFadeIn:
    case TitleState::LogoHold:
    case TitleState::LogoFadeOut:
        MM2_DBG("MM2 DBG: Displaying LogoSplash (nwcp.32)\n");
        break;
    case TitleState::TitleAttract:
        MM2_DBG("MM2 DBG: Displaying TitleAttract (intro.32 + pegasus overlays)\n");
        break;
    case TitleState::TitleMenu:
        MM2_DBG("MM2 DBG: Displaying TitleMenu (book.32 + menu text)\n");
        break;
    case TitleState::Controls:
        MM2_DBG("MM2 DBG: Displaying Controls screen\n");
        break;
    case TitleState::Options:
        MM2_DBG("MM2 DBG: Displaying Options screen\n");
        break;
    case TitleState::CharacterUi:
        MM2_DBG("MM2 DBG: Displaying CharacterUi\n");
        break;
    }
}
#endif

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

bool TitleScreen::init(const char *data_dir, ui::CharacterUiKind ui_kind)
{
    data_dir_ = data_dir;
    ui_kind_ = ui_kind;
    character_ui_ready_ = false;
    // Palette index 0 = transparent for all title assets (masked blit skips index 0).
    mm2_image32_set_preview_opaque(0);
#if defined(ACE_DEBUG)
    MM2_DBG("MM2 DBG: TitleScreen init (data_dir='%s')\n", data_dir ? data_dir : "");
#endif
    has_intro_ = loadImage("intro.32", &intro_);
    has_introclips_ = loadImage("introclips.32", &introclips_);
    mm2_image32_set_preview_opaque(0);  // nwcp: index 0 must stay transparent (not a black rect)
    has_nwcp_ = loadImage("nwcp.32", &nwcp_);
    if (has_nwcp_) {
        logo_splash_x_ = logoSplashCenterX(nwcp_.frames[0]);
    }
    // book.32 = 5-frame open-book art (89x61); frame 0 flanks the title menu boxes.
    mm2_image32_set_preview_opaque(0);
    has_book_ = loadImage("book.32", &book_);
    char *const roster_path = mm2_path_scratch_a();
    auto tryRoster = [&](const char *dir) -> bool {
        if (!joinDataPath(roster_path, MM2_PATH_SCRATCH_CAP, dir, "roster.dat")) {
            return false;
        }
        return mm2_roster_load_file(roster_path, &roster_) == MM2_ROSTER_OK;
    };
    has_roster_ = tryRoster(data_dir_);
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    if (has_roster_) {
        MM2_DBG("MM2 DBG: Loading roster.dat ok\n");
    } else {
        MM2_DBG("MM2 DBG: Loading roster.dat FAIL\n");
    }
#endif
#if MM2_HOST_AMIGA
    if (!has_roster_ && data_dir_ && data_dir_[0]) {
        has_roster_ = tryRoster("");
    }
    if (!has_roster_) {
        has_roster_ = tryRoster("dh1:");
    }
    if (!has_roster_) {
        has_roster_ = tryRoster("dh0:");
    }
#endif

    if (!has_intro_ && !has_nwcp_) {
        return false;
    }
    (void)has_introclips_;
    if (has_nwcp_) {
        setState(TitleState::LogoHold, "boot with nwcp");
    } else if (has_intro_) {
        setState(TitleState::TitleAttract, "boot intro only");
    } else {
        return false;
    }
    logo_alpha_ = 0;
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    MM2_DBG("MM2 DBG: TitleScreen assets intro=%d clips=%d nwcp=%d book=%d roster=%d chip free %lu\n",
            has_intro_ ? 1 : 0, has_introclips_ ? 1 : 0, has_nwcp_ ? 1 : 0, has_book_ ? 1 : 0,
            has_roster_ ? 1 : 0, (unsigned long)memGetFreeChipSize());
#endif
    state_ticks_ = 0;
    logo_hold_until_ = 0;
    peeker_rng_ = 1;
    resetAttractTiming();
#if MM2_HOST_AMIGA
    invalidatePegasusPaint();
#endif
    return true;
}

bool TitleScreen::ensureCharacterUi()
{
    if (character_ui_ready_) {
        return true;
    }
    if (!character_ui_.init(data_dir_, ui_kind_)) {
        return false;
    }
    character_ui_ready_ = true;
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    MM2_DBG("MM2 DBG: TitleScreen character UI ready, chip free %lu\n",
            (unsigned long)memGetFreeChipSize());
#endif
    return true;
}

void TitleScreen::shutdown()
{
    if (character_ui_ready_) {
        character_ui_.shutdown();
        character_ui_ready_ = false;
    }
    mm2_image32_free(&intro_);
    releaseIntroClips();
    releaseLogoAsset();
    mm2_image32_free(&book_);
}

void TitleScreen::releaseIntroClips()
{
    mm2_image32_free(&introclips_);
    has_introclips_ = false;
}

void TitleScreen::releaseLogoAsset()
{
    if (!has_nwcp_) {
        return;
    }
    mm2_image32_free(&nwcp_);
    has_nwcp_ = false;
}

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
}
#endif

void TitleScreen::skipLogoToTitle()
{
    setState(TitleState::TitleAttract, "logo done or skipped");
#if MM2_HOST_AMIGA
    /* Do not releaseLogoAsset() here — bitmapDestroy + ACE_DEBUG memCheckIntegrity
     * overflows the 4K Bartman stack from this deep tick() call chain. nwcp stays
     * loaded until shutdown (~17 KB chip); intro.32 is already in memory. */
    invalidatePegasusPaint();
#endif
    logo_alpha_ = 0;
    state_ticks_ = 0;
    logo_hold_until_ = 0;
    resetAttractTiming();
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

bool TitleScreen::isLogoState() const
{
    return state_ == TitleState::LogoFadeIn || state_ == TitleState::LogoHold || state_ == TitleState::LogoFadeOut;
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
    if (animate_overlays && has_intro_ && overlay_phase_ == pegasus_painted_overlay_phase_
        && peeker_visible_ == pegasus_painted_peeker_visible_
        && peeker_slot_ == pegasus_painted_peeker_slot_) {
        /* ACE simpleBuffer DB: composite already in the alternating buffers. */
        return;
    }
    pegasus_painted_overlay_phase_ = overlay_phase_;
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    MM2_DBG("MM2 DBG: Draw TitleAttract intro=%d overlay_phase=%d peeker=%d\n", has_intro_ ? 1 : 0,
            overlay_phase_, peeker_visible_ ? 1 : 0);
#endif
    pegasus_painted_peeker_visible_ = peeker_visible_;
    pegasus_painted_peeker_slot_ = peeker_slot_;
    /* Full redraw: clear playfield then background (retail keeps intro.32 under logo/menu;
     * attract path does not reuse the logo clear). */
    platform::clearScreen();
    if (has_intro_) {
        platform::blitImage32(&intro_, 0, kIntroBgX, kIntroBgY, 1);
    }
#else
    compositor_.clear(0, 0, 0, 255);
    if (has_intro_ && intro_.frames[0].rgba) {
        compositor_.blitRgba(intro_.frames[0].rgba, intro_.frames[0].width, intro_.frames[0].height, kIntroBgX,
                             kIntroBgY);
    }
#endif
    if (!has_introclips_) {
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
    platform::clearScreen();
    if (has_nwcp_ && nwcp_.frames[0].bitmap) {
        const int logo_y = (gfx::ScreenCompositor::kHeight - static_cast<int>(nwcp_.frames[0].height)) / 2;
        platform::blitImage32(&nwcp_, 0, logo_splash_x_, logo_y, 0);
        platform::logoFadeCapturePalette();
    }
#else
    compositor_.clear(0, 0, 0, 255);
    if (has_nwcp_ && nwcp_.frames[0].rgba && logo_alpha_ > 0) {
        const int logo_h = nwcp_.frames[0].height;
        const int logo_y = (gfx::ScreenCompositor::kHeight - logo_h) / 2;
        compositor_.blitRgba(nwcp_.frames[0].rgba, nwcp_.frames[0].width, logo_h, logo_splash_x_, logo_y, true,
                             logo_alpha_);
    }
#endif
}

void TitleScreen::drawControls()
{
#if MM2_HOST_AMIGA
    platform::clearScreen();
    platform::applyUiPalette();
#else
    compositor_.clear(0, 0, 0, 255);
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
#if MM2_HOST_AMIGA
    platform::clearScreen();
    platform::applyUiPalette();
#else
    compositor_.clear(0, 0, 0, 255);
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
    mm2_amiga_blit_sync();
    platform::applyUiPalette();
    mm2_amiga_ui_cache_present();
    blitTitleMenuBooks();
#else
    drawTitleMenu();
#endif
}

void TitleScreen::buildTitleMenuCache()
{
#if MM2_HOST_AMIGA
    platform::applyUiPalette();
    mm2_amiga_ui_cache_begin();
#endif

    drawRedFrame(compositor_, kTopBoxX, kTopBoxY, kTopBoxW, kTopBoxH);

    int book_w = 89;
    int book_right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
    if (has_book_ && book_.frame_count > 0 && book_.frames && book_.frames[0].width) {
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
    buildTitleMenuCache();
    presentTitleMenu();
    return;
#endif

    compositor_.clear(0, 0, 0, 255);

    drawRedFrame(compositor_, kTopBoxX, kTopBoxY, kTopBoxW, kTopBoxH);
    int book_w = 89;
    int book_right_x = kTopBoxX + kTopBoxW - kBookLeftX - book_w;
    if (has_book_ && book_.frame_count > 0 && book_.frames) {
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
    // Wide tagline beneath the books, centered on the whole top box.
    compositor_.drawTextShadow(centerX(kTitleTagline, kTopBoxX, kTopBoxW), kTopBoxY + kTopBoxH - 15, kTitleTagline,
                               255, 220, 90);
    // --- Bottom box: selectable menu options -----------------------------
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

void TitleScreen::tickBookAnimation()
{
    if (!has_book_ || book_.frame_count <= 1) {
        return;
    }
    const uint32_t now = platform::nowTicks();
    if (tickReached(now, book_until_)) {
        book_frame_ = (book_frame_ + 1) % book_.frame_count;
        book_until_ = now + static_cast<uint32_t>(kBookStepTicks);
    }
}

void TitleScreen::render()
{
#if MM2_HOST_AMIGA && defined(ACE_DEBUG)
    dbgLogDisplay();
#endif
    switch (state_) {
    case TitleState::LogoFadeIn:
    case TitleState::LogoHold:
    case TitleState::LogoFadeOut:
        drawLogoSplash();
        break;
    case TitleState::TitleAttract:
        drawAttract();
        break;
    case TitleState::TitleMenu:
#if MM2_HOST_AMIGA
        if (!mm2_amiga_ui_cache_ready()) {
            buildTitleMenuCache();
        }
        presentTitleMenu();
#else
        drawTitleMenu();
#endif
        break;
    case TitleState::Controls:
        drawControls();
        break;
    case TitleState::Options:
        drawOptions();
        break;
    case TitleState::CharacterUi:
        if (character_ui_ready_) {
            character_ui_.render(compositor_);
        }
        break;
    }
}

void TitleScreen::tick(const platform::KeyState &keys)
{
    if (keys.quit) {
        quit_ = true;
        return;
    }

    if (isLogoState()) {
        if (keys.any_key) {
            skipLogoToTitle();
            return;
        }

#if MM2_HOST_AMIGA
        /* Fades off: static nwcp logo at full palette, then attract (or key to skip).
         * Hold is timed off the VBlank frame clock, not loop iterations. */
        if (state_ == TitleState::LogoHold) {
            const uint32_t now = platform::nowTicks();
            if (logo_hold_until_ == 0) {
                logo_hold_until_ = now + static_cast<uint32_t>(kLogoHoldAmigaTicks);
            }
            if (tickReached(now, logo_hold_until_)) {
                skipLogoToTitle();
            }
        }
        return;
#else
        ++state_ticks_;
        switch (state_) {
        case TitleState::LogoFadeIn:
            logo_alpha_ = static_cast<uint8_t>(std::min(255, (state_ticks_ * 255) / kFadeTicks));
            if (state_ticks_ >= kFadeTicks) {
                setState(TitleState::LogoHold, "fade in done");
                logo_alpha_ = 255;
                state_ticks_ = 0;
            }
            break;
        case TitleState::LogoHold:
            logo_alpha_ = 255;
            if (state_ticks_ >= kLogoHoldTicks) {
                setState(TitleState::LogoFadeOut, "hold done");
                state_ticks_ = 0;
            }
            break;
        case TitleState::LogoFadeOut:
            logo_alpha_ = static_cast<uint8_t>(std::max(0, 255 - (state_ticks_ * 255) / kFadeTicks));
            if (state_ticks_ >= kFadeTicks) {
                skipLogoToTitle();
            }
            break;
        default:
            break;
        }
        return;
#endif
    }

    if (state_ == TitleState::CharacterUi) {
        if (!character_ui_ready_) {
            setState(TitleState::TitleMenu, "character UI not ready");
            return;
        }
        character_ui_.tick(keys);
        if (!character_ui_.active()) {
            if (character_ui_.quitRequested()) {
                quit_ = true;
                return;
            }
            setState(TitleState::TitleMenu, "character UI done");
        }
        return;
    }

    if (state_ == TitleState::TitleAttract) {
        tickPegasusAnimation();
        if (keys.any_key) {
            setState(TitleState::TitleMenu, "key from attract");
#if MM2_HOST_AMIGA
            invalidatePegasusPaint();
#endif
            return;
        }
        return;
    }

    if (state_ == TitleState::Controls || state_ == TitleState::Options) {
        if (keys.escape) {
            setState(TitleState::TitleMenu, "esc from sub-screen");
            return;
        }
        return;
    }

    if (state_ == TitleState::TitleMenu) {
        tickBookAnimation();
        if (keys.escape) {
            setState(TitleState::TitleAttract, "esc to attract");
            resetAttractTiming();
            return;
        }
        if (keys.key_q) {
            quit_ = true;
            return;
        }
        {
            const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
            if (ch == 'V' && has_roster_ && ensureCharacterUi()) {
                setState(TitleState::CharacterUi, "menu V view party");
                character_ui_.startViewParty(roster_);
                return;
            }
            if ((keys.key_c || ch == 'C') && has_roster_ && ensureCharacterUi()) {
                setState(TitleState::CharacterUi, "menu C create");
                character_ui_.startCreateCharacter(roster_);
                return;
            }
            if (ch == 'G' && has_roster_ && ensureCharacterUi()) {
                setState(TitleState::CharacterUi, "menu G goto town");
                character_ui_.startChooseParty(roster_);
                return;
            }
        }
        if (keys.key_m) {
            setState(TitleState::Controls, "menu M");
            return;
        }
        if (keys.key_o) {
            setState(TitleState::Options, "menu O");
            return;
        }
    }
}

bool TitleScreen::takePartyLaunch(Mm2PartyLaunch *out)
{
    return character_ui_.takePartyLaunch(out);
}

void TitleScreen::returnToMenu()
{
    setState(TitleState::TitleMenu, "return from game");
}

}  // namespace mm2

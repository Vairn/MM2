#include "mm2/TitleScreen.h"

#include "mm2/CppStdCompat.h"
#include "mm2/platform/Platform.h"

namespace mm2 {

namespace {

constexpr int kIntroBgX = 3;
constexpr int kIntroBgY = 0;
constexpr int kScreenW = gfx::ScreenCompositor::kWidth;   // 320

// Center nwcp splash on the 320px field using visible (non-transparent) pixels so
// trailing transparent padding in the 300×90 frame does not skew the logo left.
int logoSplashCenterX(const mm2_image32_frame &frame)
{
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
constexpr int kBookStepTicks = 8;

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
constexpr int kFadeTicks = 60;
constexpr int kLogoHoldTicks = 312;

bool joinPath(char *out, std::size_t out_cap, const char *dir, const char *name)
{
    const std::size_t dir_len = std::strlen(dir);
    const std::size_t name_len = std::strlen(name);
    const bool need_sep = dir_len > 0 && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\';
    if (dir_len + name_len + (need_sep ? 1u : 0u) + 1u > out_cap) {
        return false;
    }
    std::snprintf(out, out_cap, "%s%s%s", dir, need_sep ? "/" : "", name);
    return true;
}

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

bool TitleScreen::loadImage(const char *name, mm2_image32_file *out)
{
    char path[512];
    if (!joinPath(path, sizeof(path), data_dir_, name)) {
        return false;
    }
    ::memset(out, 0, sizeof(*out));
    return mm2_image32_load_file(path, out) == MM2_IMAGE32_OK && out->frame_count > 0 && out->frames[0].rgba;
}

bool TitleScreen::init(const char *data_dir, ui::CharacterUiKind ui_kind)
{
    data_dir_ = data_dir;
    // Palette index 0 = transparent for all title assets (masked blit skips index 0).
    mm2_image32_set_preview_opaque(0);
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

    char roster_path[512];
    if (joinPath(roster_path, sizeof(roster_path), data_dir_, "roster.dat")) {
        has_roster_ = (mm2_roster_load_file(roster_path, &roster_) == MM2_ROSTER_OK);
    }

    if (!character_ui_.init(data_dir, ui_kind)) {
        return false;
    }

    if (!has_intro_ && !has_nwcp_) {
        return false;
    }
    (void)has_introclips_;

    state_ = has_nwcp_ ? TitleState::LogoFadeIn : TitleState::TitleAttract;
    logo_alpha_ = 0;
    state_ticks_ = 0;
    overlay_gate_ = 0;
    overlay_phase_ = 0;
    peeker_gate_ = 0;
    peeker_gap_ticks_ = 0;
    peeker_rng_ = 1;
    peeker_visible_ = false;
    peeker_gap_ticks_ = kPeekerInitialGapTicks;
    pickRandomPeekerSlot();
    return true;
}

void TitleScreen::shutdown()
{
    character_ui_.shutdown();
    mm2_image32_free(&intro_);
    releaseIntroClips();
    mm2_image32_free(&nwcp_);
    mm2_image32_free(&book_);
}

void TitleScreen::releaseIntroClips()
{
    mm2_image32_free(&introclips_);
    has_introclips_ = false;
}

void TitleScreen::skipLogoToTitle()
{
    state_ = TitleState::TitleAttract;
    logo_alpha_ = 0;
    state_ticks_ = 0;
    overlay_gate_ = 0;
    overlay_phase_ = 0;
    peeker_gate_ = 0;
    peeker_gap_ticks_ = 0;
    peeker_visible_ = false;
    peeker_gap_ticks_ = kPeekerInitialGapTicks;
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
    peeker_gate_ = 0;
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
    const mm2_image32_frame &frame = introclips_.frames[frame_index];
    if (!frame.rgba) {
        return;
    }
    compositor_.blitRgba(frame.rgba, frame.width, frame.height, x, y);
}

void TitleScreen::drawIntroPegasus(bool animate_overlays)
{
    compositor_.clear(0, 0, 0, 255);
    if (has_intro_ && intro_.frames[0].rgba) {
        compositor_.blitRgba(intro_.frames[0].rgba, intro_.frames[0].width, intro_.frames[0].height, kIntroBgX,
                             kIntroBgY);
    }
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
    if (overlay_gate_ >= kOverlayStepTicks) {
        overlay_gate_ = 0;
        overlay_phase_ = (overlay_phase_ + 1) % kOverlayPhaseCount;
    } else {
        ++overlay_gate_;
    }

    if (peeker_gap_ticks_ > 0) {
        --peeker_gap_ticks_;
        if (peeker_gap_ticks_ == 0 && !peeker_visible_) {
            pickRandomPeekerSlot();
            peeker_visible_ = true;
        }
        return;
    }
    if (peeker_gate_ >= peeker_delay_ticks_) {
        peeker_visible_ = false;
        peeker_gap_ticks_ = kPeekerGapTicks;
        peeker_gate_ = 0;
    } else {
        ++peeker_gate_;
    }
}

void TitleScreen::drawAttract()
{
    drawIntroPegasus(true);
}

void TitleScreen::drawLogoSplash()
{
    compositor_.clear(0, 0, 0, 255);
    if (has_nwcp_ && nwcp_.frames[0].rgba && logo_alpha_ > 0) {
        const int logo_h = nwcp_.frames[0].height;
        const int logo_y = (gfx::ScreenCompositor::kHeight - logo_h) / 2;
        compositor_.blitRgba(nwcp_.frames[0].rgba, nwcp_.frames[0].width, logo_h, logo_splash_x_, logo_y, true,
                             logo_alpha_);
    }
}

void TitleScreen::drawControls()
{
    compositor_.clear(0, 0, 0, 255);
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

void TitleScreen::drawTitleMenu()
{
    // Menu is book + text on black (ASM keeps frozen intro.32 underneath; avoid
    // double-drawing pegasus/title art under the red boxes).
    compositor_.clear(0, 0, 0, 255);

    // --- Top box: open-book art flanking the centered game title ---------
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

    // Title lines centered in the gap between the two books.
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
    if (book_gate_ < kBookStepTicks) {
        ++book_gate_;
        return;
    }
    book_gate_ = 0;
    book_frame_ = (book_frame_ + 1) % book_.frame_count;
}

void TitleScreen::render()
{
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
        drawTitleMenu();
        break;
    case TitleState::Controls:
        drawControls();
        break;
    case TitleState::Options:
        drawOptions();
        break;
    case TitleState::CharacterUi:
        character_ui_.render(compositor_);
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

        ++state_ticks_;
        switch (state_) {
        case TitleState::LogoFadeIn:
            logo_alpha_ = static_cast<uint8_t>(std::min(255, (state_ticks_ * 255) / kFadeTicks));
            if (state_ticks_ >= kFadeTicks) {
                state_ = TitleState::LogoHold;
                logo_alpha_ = 255;
                state_ticks_ = 0;
            }
            break;
        case TitleState::LogoHold:
            logo_alpha_ = 255;
            if (state_ticks_ >= kLogoHoldTicks) {
                state_ = TitleState::LogoFadeOut;
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
    }

    if (state_ == TitleState::CharacterUi) {
        character_ui_.tick(keys);
        if (!character_ui_.active()) {
            if (character_ui_.quitRequested()) {
                quit_ = true;
                return;
            }
            state_ = TitleState::TitleMenu;
        }
        return;
    }

    if (state_ == TitleState::TitleAttract) {
        tickPegasusAnimation();
        if (keys.any_key) {
            state_ = TitleState::TitleMenu;
            return;
        }
        return;
    }

    if (state_ == TitleState::Controls || state_ == TitleState::Options) {
        if (keys.escape) {
            state_ = TitleState::TitleMenu;
            return;
        }
        return;
    }

    if (state_ == TitleState::TitleMenu) {
        tickBookAnimation();
        if (keys.escape) {
            state_ = TitleState::TitleAttract;
            overlay_gate_ = 0;
            overlay_phase_ = 0;
            peeker_gate_ = 0;
            peeker_gap_ticks_ = 0;
            peeker_visible_ = false;
            peeker_gap_ticks_ = kPeekerInitialGapTicks;
            pickRandomPeekerSlot();
            return;
        }
        if (keys.key_q) {
            quit_ = true;
            return;
        }
        {
            const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(keys.last_ascii)));
            if (ch == 'V' && has_roster_) {
                state_ = TitleState::CharacterUi;
                character_ui_.startViewParty(roster_);
                return;
            }
            if ((keys.key_c || ch == 'C') && has_roster_) {
                state_ = TitleState::CharacterUi;
                character_ui_.startCreateCharacter(roster_);
                return;
            }
            if (ch == 'G' && has_roster_) {
                state_ = TitleState::CharacterUi;
                character_ui_.startChooseParty(roster_);
                return;
            }
        }
        if (keys.key_m) { state_ = TitleState::Controls; return; } if (keys.key_o) {
            state_ = TitleState::Options;
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
    state_ = TitleState::TitleMenu;
}

}  // namespace mm2

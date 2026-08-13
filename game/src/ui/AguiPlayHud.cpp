#include "mm2/ui/AguiPlayHud.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/gfx/PlayScreenChrome.h"
#include "mm2/ui/AguiPlayScreenLayout.h"

#include <cstdio>
#include <cstring>

namespace mm2::ui {
namespace {

using namespace agui_layout;

/* UI palette (matches game/data/ui/agui/palette.json — pens conceptually 32–63). */
constexpr uint8_t kBgR = 0x10, kBgG = 0x10, kBgB = 0x18;
constexpr uint8_t kPanelR = 0x28, kPanelG = 0x28, kPanelB = 0x28;
constexpr uint8_t kPanel2R = 0x38, kPanel2G = 0x38, kPanel2B = 0x38;
constexpr uint8_t kHiR = 0xa0, kHiG = 0xa0, kHiB = 0xa0;
constexpr uint8_t kLoR = 0x18, kLoG = 0x18, kLoB = 0x18;
constexpr uint8_t kInkR = 0xe0, kInkG = 0xe0, kInkB = 0xc0;
constexpr uint8_t kGoldR = 0xc0, kGoldG = 0xa0, kGoldB = 0x60;
constexpr uint8_t kAccentR = 0x50, kAccentG = 0xa0, kAccentB = 0x90;
constexpr uint8_t kHpR = 0x40, kHpG = 0xc0, kHpB = 0x40;
constexpr uint8_t kHpWarnR = 0xc0, kHpWarnG = 0xa0, kHpWarnB = 0x20;
constexpr uint8_t kHpBadR = 0xc0, kHpBadG = 0x40, kHpBadB = 0x40;
constexpr uint8_t kSpR = 0x40, kSpG = 0x80, kSpB = 0xc0;

struct RailCmd {
    const char *label;
    const char *icon; /* atlas name without path prefix mismatch — full key */
    char key;         /* explore command letter for hit-test */
};

constexpr RailCmd kExploreRail[8] = {
    {"Cast", "icons/cast", 'C'},   {"Shoot", "icons/shoot", 'S'}, {"Unlock", "icons/unlock", 'U'},
    {"Bash", "icons/bash", 'B'},   {"Rest", "icons/rest", 'R'},   {"Search", "icons/search", 'E'},
    {"Map", "icons/map", 'M'},     {"Quick", "icons/quick", 'Q'},
};

constexpr RailCmd kCombatRail[8] = {
    {"Attack", "icons/attack", 'A'}, {"Fight", "icons/fight", 'F'}, {"Run", "icons/run", 'U'},
    {"Block", "icons/block", 'B'},   {"Cast", "icons/cast", 'C'},   {"Use", "icons/use", 'D'},
    {"Shoot", "icons/shoot", 'S'},   {"Quick", "icons/quick", 'Q'},
};

void diamond(gfx::ScreenCompositor &c, int cx, int cy, int half, uint8_t r, uint8_t g, uint8_t b)
{
    for (int y = -half; y <= half; ++y) {
        const int span = half - (y < 0 ? -y : y);
        c.fillRect(cx - span, cy + y, span * 2 + 1, 1, r, g, b);
    }
}

}  // namespace

bool AguiPlayHud::init(const char *data_dir)
{
    atlas_ok_ = atlas_.load(data_dir);
    status_msg_[0] = '\0';
    /* Soft-fail: procedural bevel HUD still works without atlas. */
    return true;
}

void AguiPlayHud::shutdown()
{
    atlas_.unload();
    atlas_ok_ = false;
}

int AguiPlayHud::viewOriginX() const { return kViewOriginX; }
int AguiPlayHud::viewOriginY() const { return kViewOriginY; }
int AguiPlayHud::viewW() const { return kViewW; }
int AguiPlayHud::viewH() const { return kViewH; }

void AguiPlayHud::blitNamed(gfx::ScreenCompositor &c, const char *name, int dst_x, int dst_y) const
{
    if (!atlas_ok_) {
        return;
    }
    const AguiAtlasRect *r = atlas_.find(name);
    if (r) {
        atlas_.blit(c, *r, dst_x, dst_y);
    }
}

void AguiPlayHud::drawBevelRect(gfx::ScreenCompositor &c, int x, int y, int w, int h, bool inset) const
{
    const uint8_t tr = inset ? kLoR : kHiR;
    const uint8_t tg = inset ? kLoG : kHiG;
    const uint8_t tb = inset ? kLoB : kHiB;
    const uint8_t br = inset ? kHiR : kLoR;
    const uint8_t bg = inset ? kHiG : kLoG;
    const uint8_t bb = inset ? kHiB : kLoB;
    c.fillRect(x, y, w, h, kPanel2R, kPanel2G, kPanel2B);
    c.fillRect(x, y, w, 1, tr, tg, tb);
    c.fillRect(x, y, 1, h, tr, tg, tb);
    c.fillRect(x, y + h - 1, w, 1, br, bg, bb);
    c.fillRect(x + w - 1, y, 1, h, br, bg, bb);
}

void AguiPlayHud::drawChromeStatic(gfx::ScreenCompositor &c)
{
    c.fillRect(0, 0, kScreenW, kScreenH, kBgR, kBgG, kBgB);
    /* Outer stone frame around viewport. */
    drawBevelRect(c, kFrameX, kFrameY, kFrameW, kFrameH, false);
    c.fillRect(kViewOriginX - 1, kViewOriginY - 1, kViewW + 2, kViewH + 2, kLoR, kLoG, kLoB);
    /* Rail panel */
    drawBevelRect(c, kRailX, kRailY, kRailW, kRailH, false);
    /* Message strip */
    drawBevelRect(c, kMsgX, kMsgY, kMsgW, kMsgH, true);
    c.fillRect(kMsgX + 1, kMsgY + 1, kMsgW - 2, kMsgH - 2, 0x08, 0x08, 0x08);
    /* Party band + d-pad cradles */
    drawBevelRect(c, kPartyX, kPartyY, kPartyW, kPartyH, true);
    drawBevelRect(c, kDpadX, kDpadY, kDpadW, kDpadH, true);
    blitNamed(c, "chrome/frame", kFrameX, kFrameY);
}

void AguiPlayHud::drawChrome(gfx::ScreenCompositor &c)
{
    drawChromeStatic(c);
}

void AguiPlayHud::drawViewportDivider(gfx::ScreenCompositor &c)
{
    /* Agui has no classic col-27 red divider; re-stamp rail left edge. */
    c.fillRect(kRailX, kRailY, 1, kRailH, kHiR, kHiG, kHiB);
    (void)c;
}

void AguiPlayHud::drawProtectGems(gfx::ScreenCompositor &c, const gfx::PlayProtectValues *protect) const
{
    const bool light = protect && protect->light > 0;
    const bool magic = protect && protect->magic > 0;
    const bool forces = protect && protect->forces > 0;
    const bool levitate = protect && protect->levitate > 0;

    const int half = kProtectGemSize / 2;
    const int tlx = kFrameX + kProtectGemInset + half;
    const int tly = kFrameY + kProtectGemInset + half;
    const int trx = kFrameX + kFrameW - kProtectGemInset - half - 1;
    const int try_ = kFrameY + kProtectGemInset + half;
    const int blx = kFrameX + kProtectGemInset + half;
    const int bly = kFrameY + kFrameH - kProtectGemInset - half - 1;
    const int brx = kFrameX + kFrameW - kProtectGemInset - half - 1;
    const int bry = kFrameY + kFrameH - kProtectGemInset - half - 1;

    auto gem = [&](int cx, int cy, bool on) {
        if (atlas_ok_ && on) {
            blitNamed(c, "chrome/gem_on", cx - half, cy - half);
        } else if (atlas_ok_) {
            blitNamed(c, "chrome/gem_off", cx - half, cy - half);
        } else {
            diamond(c, cx, cy, half, on ? kAccentR : 0x30, on ? kAccentG : 0x30, on ? kAccentB : 0x30);
        }
    };
    gem(tlx, tly, light);
    gem(trx, try_, magic);
    gem(blx, bly, forces);
    gem(brx, bry, levitate);
}

void AguiPlayHud::drawFaceGem(gfx::ScreenCompositor &c, char facing_key) const
{
    drawBevelRect(c, kFaceGemX, kFaceGemY, kFaceGemW, kFaceGemH, true);
    c.fillRect(kFaceGemX + 1, kFaceGemY + 1, kFaceGemW - 2, kFaceGemH - 2, 0x20, 0x30, 0x28);
    char buf[2] = {facing_key ? facing_key : 'N', '\0'};
    c.drawText(kFaceGemX + 5, kFaceGemY + 1, buf, kHpR, kHpG, kHpB);
    blitNamed(c, "chrome/face_gem", kFaceGemX, kFaceGemY);
}

void AguiPlayHud::drawDpad(gfx::ScreenCompositor &c) const
{
    const int cx = kDpadX + 2;
    const int cy = kDpadY + 2;
    const int s = kDpadCell;
    auto cell = [&](int col, int row, const char *label, const char *icon) {
        const int x = cx + col * (s + 1);
        const int y = cy + row * (s + 1);
        drawBevelRect(c, x, y, s, s, false);
        if (icon) {
            blitNamed(c, icon, x + (s - 10) / 2, y + (s - 10) / 2);
        }
        if (label && label[0]) {
            c.drawText(x + 3, y + 3, label, kGoldR, kGoldG, kGoldB);
        }
    };
    cell(1, 0, "^", "icons/dpad_n");
    cell(0, 1, "<", "icons/dpad_w");
    cell(1, 1, "W", "icons/dpad_wait");
    cell(2, 1, ">", "icons/dpad_e");
    cell(1, 2, "v", "icons/dpad_s");
}

void AguiPlayHud::drawIconButton(gfx::ScreenCompositor &c, int index, const char *label,
                                 const char *icon_name, bool /*combat*/) const
{
    const int y = kRailY + 2 + index * (kRailBtnH + 1);
    drawBevelRect(c, kRailX + 2, y, kRailW - 4, kRailBtnH, false);
    blitNamed(c, icon_name, kRailX + 4, y + (kRailBtnH - kIconSize) / 2);
    if (label) {
        c.drawText(kRailX + 4 + kIconSize + 2, y + 3, label, kGoldR, kGoldG, kGoldB);
    }
}

void AguiPlayHud::drawStatusBar(gfx::ScreenCompositor &c, int day, int year, char facing_key,
                                bool protect_panel)
{
    last_facing_ = facing_key ? facing_key : 'N';
    (void)protect_panel;
    drawFaceGem(c, last_facing_);
    drawProtectGems(c, &last_protect_);

    char line[64];
    std::snprintf(line, sizeof(line), "Day %d  Yr %d  Face %c", day, year, last_facing_);
    c.fillRect(kMsgX + 2, kMsgY + 2, kMsgW - 4, kMsgH - 4, 0x08, 0x08, 0x08);
    c.drawText(kMsgX + 3, kMsgY + 3, line, kInkR, kInkG, kInkB);
    if (status_msg_[0]) {
        c.drawText(kMsgX + 3, kMsgY + 3 + 8, status_msg_, kGoldR, kGoldG, kGoldB);
    }
}

void AguiPlayHud::drawPartyPanel(gfx::ScreenCompositor &c, const gfx::PlayPartySlot slots[8])
{
    for (int i = 0; i < kPartySlots; ++i) {
        const int x = kPartyX + 2 + i * kPartySlotW;
        const int y = kPartyY + 2;
        drawBevelRect(c, x, y, kPartySlotW - 1, kPartyH - 4, true);
        c.fillRect(x + 1, y + 1, kPartySlotW - 3, kPartyH - 6, 0x18, 0x14, 0x10);

        if (!slots[i].present) {
            continue;
        }

        char face_name[32];
        std::snprintf(face_name, sizeof(face_name), "faces/face_%02d", slots[i].face_id % 8);
        blitNamed(c, face_name, x + 1, y + 1);

        /* Tiny name under face */
        char nm[8];
        std::snprintf(nm, sizeof(nm), "%.5s", slots[i].name);
        c.drawText(x + 1, y + kFaceSize + 1, nm, 0xb0, 0xa0, 0x80);

        const int hp_cur = slots[i].hp_current > 0 ? slots[i].hp_current : slots[i].hp;
        const int hp_max = slots[i].hp_max > 0 ? slots[i].hp_max : (slots[i].hp > 0 ? slots[i].hp : 1);
        int hp_pct = hp_max > 0 ? (hp_cur * 100) / hp_max : 0;
        if (hp_pct < 0) {
            hp_pct = 0;
        }
        if (hp_pct > 100) {
            hp_pct = 100;
        }

        uint8_t hr = kHpR, hg = kHpG, hb = kHpB;
        if (hp_pct <= 25 || slots[i].condition >= 0x40) {
            hr = kHpBadR;
            hg = kHpBadG;
            hb = kHpBadB;
        } else if (hp_pct <= 60 || slots[i].bad_condition) {
            hr = kHpWarnR;
            hg = kHpWarnG;
            hb = kHpWarnB;
        }
        if (slots[i].condition >= 0x40 && hp_cur == 0) {
            hr = kSpR;
            hg = kSpG;
            hb = kSpB;
        }

        const int gem_x = x + 2;
        const int gem_y = y + kFaceSize + 8;
        diamond(c, gem_x + 2, gem_y + 2, 2, hr, hg, hb);

        const int bar_x = x + 2;
        const int bar_w = kPartySlotW - 5;
        const int hp_y = y + kPartyH - 10;
        const int sp_y = hp_y + kBarH + 1;
        c.fillRect(bar_x, hp_y, bar_w, kBarH, 0, 0, 0);
        c.fillRect(bar_x, hp_y, (bar_w * hp_pct) / 100, kBarH, hr, hg, hb);
        c.fillRect(bar_x, sp_y, bar_w, kBarH, 0, 0, 0);
        if (slots[i].sp_max > 0) {
            const int sp_pct = (slots[i].sp_current * 100) / slots[i].sp_max;
            c.fillRect(bar_x, sp_y, (bar_w * sp_pct) / 100, kBarH, kSpR, kSpG, kSpB);
        }
    }
    drawDpad(c);
}

void AguiPlayHud::drawRightColumn(gfx::ScreenCompositor &c, gfx::PlayRightPanel panel,
                                  const gfx::PlayProtectValues *protect)
{
    combat_mode_ = false;
    if (protect) {
        last_protect_ = *protect;
    } else {
        last_protect_ = {};
    }
    drawProtectGems(c, protect);

    if (panel == gfx::PlayRightPanel::Protect && protect) {
        /* Compact protect readout in the rail when toggled. */
        char line[24];
        std::snprintf(line, sizeof(line), "L%d M%d F%d", protect->light, protect->magic, protect->forces);
        c.drawText(kRailX + 4, kRailY + kRailH - 10, line, kAccentR, kAccentG, kAccentB);
    }

    for (int i = 0; i < 8; ++i) {
        drawIconButton(c, i, kExploreRail[i].label, kExploreRail[i].icon, false);
    }
}

void AguiPlayHud::drawCombatChrome(gfx::ScreenCompositor &c)
{
    combat_mode_ = true;
    drawChromeStatic(c);
    /* Narrow combat hood uses classic combat chrome overlays on top. */
    gfx::drawCombatScreenChrome(c);
}

void AguiPlayHud::drawCombatLines(gfx::ScreenCompositor &c) { gfx::drawCombatScreenLines(c); }

void AguiPlayHud::drawCombatViewportFrame(gfx::ScreenCompositor &c)
{
    gfx::drawCombatViewportFrame(c);
}

void AguiPlayHud::drawCombatViewportDivider(gfx::ScreenCompositor &c)
{
    gfx::drawCombatViewportDivider(c);
}

void AguiPlayHud::drawCombatRightColumn(gfx::ScreenCompositor &c, const gfx::CombatPanelView &view)
{
    combat_mode_ = true;
    for (int i = 0; i < 8; ++i) {
        drawIconButton(c, i, kCombatRail[i].label, kCombatRail[i].icon, true);
    }
    gfx::drawCombatRightColumn(c, view);
}

void AguiPlayHud::drawCombatOptionsBar(gfx::ScreenCompositor &c, const gfx::CombatPanelView &view)
{
    gfx::drawCombatOptionsBar(c, view);
}

void AguiPlayHud::drawModalBackdrop(gfx::ScreenCompositor &c)
{
    gfx::drawPlayModalBackdrop(c);
}

bool AguiPlayHud::hitTest(int x, int y, char *out_cmd, int *out_special) const
{
    if (out_cmd) {
        *out_cmd = 0;
    }
    if (out_special) {
        *out_special = 0;
    }

    if (x >= kFaceGemX && x < kFaceGemX + kFaceGemW && y >= kFaceGemY && y < kFaceGemY + kFaceGemH) {
        if (out_special) {
            *out_special = 6;
        }
        return true;
    }

    /* D-pad */
    const int cx = kDpadX + 2;
    const int cy = kDpadY + 2;
    const int s = kDpadCell;
    auto in_cell = [&](int col, int row) {
        const int px = cx + col * (s + 1);
        const int py = cy + row * (s + 1);
        return x >= px && x < px + s && y >= py && y < py + s;
    };
    if (in_cell(1, 0)) {
        if (out_special) {
            *out_special = 1;
        }
        return true;
    }
    if (in_cell(1, 2)) {
        if (out_special) {
            *out_special = 2;
        }
        return true;
    }
    if (in_cell(0, 1)) {
        if (out_special) {
            *out_special = 3;
        }
        return true;
    }
    if (in_cell(2, 1)) {
        if (out_special) {
            *out_special = 4;
        }
        return true;
    }
    if (in_cell(1, 1)) {
        if (out_special) {
            *out_special = 5;
        }
        return true;
    }

    /* Icon rail */
    if (x >= kRailX + 2 && x < kRailX + kRailW - 2) {
        for (int i = 0; i < 8; ++i) {
            const int by = kRailY + 2 + i * (kRailBtnH + 1);
            if (y >= by && y < by + kRailBtnH) {
                const RailCmd &cmd = combat_mode_ ? kCombatRail[i] : kExploreRail[i];
                if (out_cmd) {
                    *out_cmd = cmd.key;
                }
                return true;
            }
        }
    }
    return false;
}

}  // namespace mm2::ui

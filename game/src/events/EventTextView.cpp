#include "mm2/events/EventTextView.h"

#include "mm2/events/ServiceSignResolver.h"
#include "mm2/gfx/Mm2FontGlyphs.h"

namespace mm2::events {

namespace {

constexpr uint8_t kGlyphHSeg = 0x05;
constexpr uint8_t kGlyphHCapL = 0x06;
constexpr uint8_t kGlyphHCapR = 0x07;
constexpr uint8_t kGlyphVSeg = 0x0B;
// OP_06 sign border/post @ 0x15AEE uses A4-$7A50 (outdoor init → pen $12); gold/yellow on Amiga.
constexpr uint8_t kPenSignBorderR = 255;
constexpr uint8_t kPenSignBorderG = 204;
constexpr uint8_t kPenSignBorderB = 0;
constexpr uint8_t kPenSignTextR = 255;
constexpr uint8_t kPenSignTextG = 255;
constexpr uint8_t kPenSignTextB = 255;
constexpr uint8_t kPenSignFillR = 0;
constexpr uint8_t kPenSignFillG = 0;
constexpr uint8_t kPenSignFillB = 0;
// OP_0B service signboard sprite stub — horizontal tavern/building board (brown wood).
constexpr uint8_t kPenServiceBoardR = 170;
constexpr uint8_t kPenServiceBoardG = 102;
constexpr uint8_t kPenServiceBoardB = 34;

void glyphAt(gfx::ScreenCompositor &c, int col, int row, uint8_t glyph, uint8_t r = 255, uint8_t g = 255,
             uint8_t b = 255)
{
    c.drawGlyph(col * 8, row * 8, glyph, r, g, b);
}

void charAt(gfx::ScreenCompositor &c, int col, int row, char ch, uint8_t r = 255, uint8_t g = 255,
            uint8_t b = 255)
{
    char buf[2] = {ch, '\0'};
    c.drawText(col * 8, row * 8, buf, r, g, b);
}

void textAt(gfx::ScreenCompositor &c, int col, int row, const char *text, uint8_t r = 255, uint8_t g = 255,
            uint8_t b = 255)
{
    c.drawText(col * 8, row * 8, text, r, g, b);
}

void hLine(gfx::ScreenCompositor &c, int col0, int col1, int row, uint8_t r = kPenSignBorderR, uint8_t g = kPenSignBorderG,
           uint8_t b = kPenSignBorderB)
{
    glyphAt(c, col0, row, kGlyphHCapL, r, g, b);
    for (int col = col0 + 1; col < col1; ++col) {
        glyphAt(c, col, row, kGlyphHSeg, r, g, b);
    }
    glyphAt(c, col1, row, kGlyphHCapR, r, g, b);
}

void clearCells(gfx::ScreenCompositor &c, int x1, int y1, int x2, int y2)
{
    c.clearRect(x1 * 8, y1 * 8, (x2 - x1 + 1) * 8, (y2 - y1 + 1) * 8, 0, 0, 0, 255);
}

void chromeMsgTop(gfx::ScreenCompositor &c)
{
    constexpr uint8_t kPenChromeR = 255;
    constexpr uint8_t kPenChromeG = 128;
    constexpr uint8_t kPenChromeB = 128;
    glyphAt(c, 0, 18, kGlyphVSeg, kPenChromeR, kPenChromeG, kPenChromeB);
    glyphAt(c, 39, 18, kGlyphVSeg, kPenChromeR, kPenChromeG, kPenChromeB);
    glyphAt(c, 12, 16, kGlyphHSeg, kPenChromeR, kPenChromeG, kPenChromeB);
    glyphAt(c, 21, 16, kGlyphHSeg, kPenChromeR, kPenChromeG, kPenChromeB);
    glyphAt(c, 31, 16, kGlyphHSeg, kPenChromeR, kPenChromeG, kPenChromeB);
}

int visibleLen(const char *s)
{
    int n = 0;
    while (s[n] && s[n] != '\n') {
        ++n;
    }
    return n;
}

int centeredCol(int start_col, int end_col, const char *text, int line_start)
{
    const int width = end_col - start_col + 1;
    const int len = visibleLen(text + line_start);
    int col = start_col + (width - len) / 2;
    if (col < start_col) {
        col = start_col;
    }
    return col;
}

void printWrapped(gfx::ScreenCompositor &c, int start_col, int start_row, int end_col, int end_row,
                  const char *text, bool center_lines = false, uint8_t r = 255, uint8_t g = 255,
                  uint8_t b = 255)
{
    int col = start_col;
    int row = start_row;

    if (center_lines && text[0]) {
        col = centeredCol(start_col, end_col, text, 0);
    }

    for (int i = 0; text[i]; ++i) {
        if (text[i] == '\n') {
            ++row;
            if (row > end_row) {
                break;
            }
            if (center_lines && text[i + 1]) {
                col = centeredCol(start_col, end_col, text, i + 1);
            } else {
                col = start_col;
            }
            continue;
        }
        charAt(c, col, row, text[i], r, g, b);
        ++col;
        if (col > end_col) {
            col = start_col;
            ++row;
            if (row > end_row) {
                break;
            }
        }
    }
}

void fillSignInteriorRow(gfx::ScreenCompositor &c, int row)
{
    // OP_06 @ 0x15AEE: win_print("           ") ×11 spaces, JAM2 bg pen 0 (doc 44 §3.6).
    c.fillRect(8 * 8, row * 8, 11 * 8, 8, kPenSignFillR, kPenSignFillG, kPenSignFillB, 255);
}

void fillSignInterior(gfx::ScreenCompositor &c)
{
    // win_open(sign) auto-clear of window interior (8,8)-(18,9) → px (64,64)-(151,79).
    fillSignInteriorRow(c, 8);
    fillSignInteriorRow(c, 9);
}

void drawSignpost(gfx::ScreenCompositor &c, const char *text)
{
    using namespace gfx::font_glyphs;

    for (int col = 8; col <= 18; ++col) {
        glyphAt(c, col, 7, kTopBar, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
    }
    glyphAt(c, 7, 7, kTopLeft, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
    glyphAt(c, 19, 7, kTopRight, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);

    for (int row = 8; row <= 9; ++row) {
        glyphAt(c, 7, row, kLeft, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
        fillSignInteriorRow(c, row);
        glyphAt(c, 19, row, kRight, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
    }

    for (int col = 8; col <= 18; ++col) {
        glyphAt(c, col, 10, kBottomBar, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
    }
    glyphAt(c, 7, 10, kBottomLeft, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
    glyphAt(c, 19, 10, kBottomRight, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);

    for (int row = 11; row <= 13; ++row) {
        glyphAt(c, 12, row, kRight, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
        glyphAt(c, 13, row, kSolidBlock, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
        glyphAt(c, 14, row, kLeft, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
    }
    glyphAt(c, 12, 14, kBottomRight, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
    glyphAt(c, 14, 14, kBottomLeft, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);
    glyphAt(c, 13, 14, kSolidBlock, kPenSignBorderR, kPenSignBorderG, kPenSignBorderB);

    fillSignInterior(c);

    char rewritten[256];
    int wi = 0;
    for (int i = 0; text[i] && wi + 1 < static_cast<int>(sizeof(rewritten)); ++i) {
        rewritten[wi++] = (text[i] == '-') ? '{' : text[i];
    }
    rewritten[wi] = '\0';
    printWrapped(c, 8, 8, 18, 9, rewritten, true, kPenSignTextR, kPenSignTextG, kPenSignTextB);
}

void drawDoorLabel(gfx::ScreenCompositor &c, const char *text)
{
    // OP_04 @ 0x159F4 / doc 44 §3.4: bg pen $FF (JAM1), pen 1, cursor((28-len)/2,3), win_print.
    // No clear, no restore — text only over the 3D viewport (not OP_0B's signboard sprite).
    const int len = visibleLen(text);
    const int col = (28 - len) / 2;
    textAt(c, col, 3, text);
}

void drawServiceSignStub(gfx::ScreenCompositor &c, const char *text)
{
    // OP_0B @ 0x15DB0: bitmap signboard via sign_sprite_load (0x316E) + mode $17 place.
    // Stub: horizontal board over viewport until 0x9A30 sprite path is wired.
    const int len = visibleLen(text);
    const int board_w = len + 4;
    const int col0 = (26 - board_w) / 2;
    const int row = 4;
    c.fillRect(col0 * 8, row * 8, board_w * 8, 16, kPenServiceBoardR, kPenServiceBoardG, kPenServiceBoardB, 255);
    textAt(c, col0 + 2, row, text, kPenSignTextR, kPenSignTextG, kPenSignTextB);
}

int countPopupLines(const char *text)
{
    int lines = 1;
    int col = 0;
    for (int i = 0; text[i]; ++i) {
        if (text[i] == '\n') {
            ++lines;
            col = 0;
            continue;
        }
        ++col;
        if (col >= 20) {
            ++lines;
            col = 0;
        }
    }
    return lines;
}

bool textContains(const char *haystack, const char *needle)
{
    if (!haystack || !needle || needle[0] == '\0') {
        return false;
    }
    for (size_t i = 0; haystack[i] != '\0'; ++i) {
        size_t j = 0;
        while (needle[j] != '\0' && haystack[i + j] == needle[j]) {
            ++j;
        }
        if (needle[j] == '\0') {
            return true;
        }
    }
    return false;
}

}  // namespace

void EventTextView::copyResolvedText(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < cap; ++i) {
        const char ch = (src[i] == '@') ? '\n' : src[i];
        dst[j++] = ch;
    }
    dst[j] = '\0';
}

void EventTextView::reset()
{
    layer_count_ = 0;
    space_prompt_ = false;
    exit_bit0_ = false;
    exit_bit1_ = false;
    sign_overlay_.unload();
    sign_placement_ = 0;
    for (EventTextLayer &layer : layers_) {
        layer.op = EventTextOp::None;
        layer.text[0] = '\0';
    }
}

void EventTextView::showOp01(const char *text)
{
    if (layer_count_ < static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        EventTextLayer &layer = layers_[layer_count_++];
        layer.op = EventTextOp::Op01CenterRow17;
        copyResolvedText(layer.text, sizeof(layer.text), text);
    }
    setExitFlagBit0();
}

void EventTextView::showOp02(const char *text, int base_row)
{
    (void)base_row;
    if (layer_count_ < static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        EventTextLayer &layer = layers_[layer_count_++];
        layer.op = EventTextOp::Op02BlockRows19_22;
        copyResolvedText(layer.text, sizeof(layer.text), text);
    }
    setExitFlagBit1();
}

void EventTextView::showOp03(const char *text)
{
    if (layer_count_ < static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        EventTextLayer &layer = layers_[layer_count_++];
        layer.op = EventTextOp::Op03TallBlock;
        copyResolvedText(layer.text, sizeof(layer.text), text);
    }
    setExitFlagBit0();
    setExitFlagBit1();
}

void EventTextView::showOp04(const char *text)
{
    if (layer_count_ < static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        EventTextLayer &layer = layers_[layer_count_++];
        layer.op = EventTextOp::Op04DoorLabel;
        copyResolvedText(layer.text, sizeof(layer.text), text);
    }
}

void EventTextView::showOp05(const char *text)
{
    if (layer_count_ < static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        EventTextLayer &layer = layers_[layer_count_++];
        layer.op = EventTextOp::Op05PopupA;
        copyResolvedText(layer.text, sizeof(layer.text), text);
    }
}

void EventTextView::showOp06(const char *text)
{
    if (layer_count_ < static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        EventTextLayer &layer = layers_[layer_count_++];
        layer.op = EventTextOp::Op06Signpost;
        copyResolvedText(layer.text, sizeof(layer.text), text);
    }
}

void EventTextView::showOp0B(const char *text, const char *data_dir, const GameStateView &gs,
                             const Mm2AttribRecord *attrib, uint8_t str_idx, uint8_t placement)
{
    sign_placement_ = placement;
    sign_overlay_.unload();
    bool has_sign = false;
    if (data_dir) {
        const int anm_id =
            ServiceSignResolver::resolveForGameState(gs.a4(), static_cast<int>(gs.screenId()), attrib, str_idx);
        if (anm_id > 0) {
            has_sign = sign_overlay_.loadFromId(data_dir, anm_id);
        }
    }
    const bool has_text = text != nullptr && text[0] != '\0';
    if (!has_sign && !has_text) {
        return;
    }
    if (layer_count_ < static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        EventTextLayer &layer = layers_[layer_count_++];
        layer.op = EventTextOp::Op0BServiceSign;
        copyResolvedText(layer.text, sizeof(layer.text), text);
    }
}

void EventTextView::showOp0B(const char *text, const char *data_dir, int screen_id,
                             const Mm2AttribRecord *attrib, uint8_t str_idx, uint8_t placement)
{
    sign_placement_ = placement;
    sign_overlay_.unload();
    bool has_sign = false;
    if (data_dir) {
        const int anm_id = ServiceSignResolver::resolveForScreen(screen_id, attrib, str_idx);
        if (anm_id > 0) {
            has_sign = sign_overlay_.loadFromId(data_dir, anm_id);
        }
    }
    const bool has_text = text != nullptr && text[0] != '\0';
    if (!has_sign && !has_text) {
        return;
    }
    if (layer_count_ < static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        EventTextLayer &layer = layers_[layer_count_++];
        layer.op = EventTextOp::Op0BServiceSign;
        copyResolvedText(layer.text, sizeof(layer.text), text);
    }
}

void EventTextView::showSpacePrompt()
{
    space_prompt_ = true;
}

void EventTextView::clearSpacePrompt()
{
    space_prompt_ = false;
}

bool EventTextView::tickAnimation()
{
    return sign_overlay_.tick();
}

void EventTextView::draw(gfx::ScreenCompositor &c, bool outdoor_viewport) const
{
    for (int i = 0; i < layer_count_; ++i) {
        const EventTextLayer &layer = layers_[i];
        switch (layer.op) {
        case EventTextOp::Op01CenterRow17:
            hLine(c, 0, 39, 18);
            chromeMsgTop(c);
            clearCells(c, 1, 17, 38, 17);
            printWrapped(c, 1, 17, 38, 17, layer.text, true);
            break;
        case EventTextOp::Op02BlockRows19_22:
            clearCells(c, 1, 19, 38, 22);
            printWrapped(c, 1, 19, 38, 22, layer.text, false);
            break;
        case EventTextOp::Op03TallBlock:
            chromeMsgTop(c);
            clearCells(c, 1, 17, 38, 22);
            printWrapped(c, 1, 17, 38, 22, layer.text, false);
            break;
        case EventTextOp::Op04DoorLabel:
            drawDoorLabel(c, layer.text);
            break;
        case EventTextOp::Op05PopupA: {
            const int lines = countPopupLines(layer.text);
            int row = (lines / 2 < 5) ? (4 - lines / 2) : 0;
            row = 3 + row;
            printWrapped(c, 4, row, 24, 13, layer.text, true);
            break;
        }
        case EventTextOp::Op06Signpost:
            drawSignpost(c, layer.text);
            break;
        case EventTextOp::Op0BServiceSign:
            if (sign_overlay_.loaded()) {
                /* OP_0B @ 0x15DB0 → sign_sprite_place @ 0x3266 mode $17 @ 0x23C8C:
                 * all service signs blit over the 3D viewport (8,8)–(215,127).
                 * Arg2 is a placement-table index (A4-$7538 → A4-$56E), not a
                 * viewport-vs-panel switch — never route by sprite dimensions. */
                sign_overlay_.blitCentered(c, sign_placement_, outdoor_viewport);
            } else if (layer.text[0] != '\0') {
                drawServiceSignStub(c, layer.text);
            }
            break;
        default:
            break;
        }
    }

    if (space_prompt_) {
        textAt(c, 9, 23, "('Space' to continue)");
    }
}

bool EventTextView::containsText(const char *needle) const
{
    if (!needle || needle[0] == '\0') {
        return false;
    }
    for (int i = 0; i < layer_count_; ++i) {
        if (textContains(layers_[i].text, needle)) {
            return true;
        }
    }
    return false;
}

bool EventTextView::isPersistentOverlay(EventTextOp op)
{
    switch (op) {
    case EventTextOp::Op04DoorLabel:
    case EventTextOp::Op05PopupA:
    case EventTextOp::Op06Signpost:
    case EventTextOp::Op0BServiceSign:
        return true;
    default:
        return false;
    }
}

void EventTextView::clearPersistentOverlays()
{
    int kept = 0;
    for (int i = 0; i < layer_count_; ++i) {
        if (!isPersistentOverlay(layers_[i].op)) {
            if (kept != i) {
                layers_[kept] = layers_[i];
            }
            ++kept;
        }
    }
    layer_count_ = kept;
    sign_overlay_.unload();
    sign_placement_ = 0;
}

void EventTextView::scriptCleanup(bool *redraw_status, bool *redraw_roster, bool *redraw_divider)
{
    if (redraw_status) {
        *redraw_status = exit_bit0_;
    }
    if (redraw_roster) {
        *redraw_roster = exit_bit1_;
    }
    if (redraw_divider) {
        *redraw_divider = exit_bit0_ && exit_bit1_;
    }

    /* OP_04/05/06/0B are ambient overlays — survive script end (0x171AC); cleared on
     * the next tile scan when movement/turn relatches the scanner. */
    int kept = 0;
    for (int i = 0; i < layer_count_; ++i) {
        if (isPersistentOverlay(layers_[i].op)) {
            if (kept != i) {
                layers_[kept] = layers_[i];
            }
            ++kept;
        }
    }
    layer_count_ = kept;
    space_prompt_ = false;
    exit_bit0_ = false;
    exit_bit1_ = false;
}

}  // namespace mm2::events

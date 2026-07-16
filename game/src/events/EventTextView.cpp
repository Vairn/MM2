#include "mm2/events/EventTextView.h"

#include "mm2/Config.h"
#include "mm2/events/ServiceSignResolver.h"
#include "mm2/gfx/Mm2FontGlyphs.h"

#if MM2_HOST_AMIGA
#include "mm2/platform/amiga/Mm2AmigaConfig.h"
#endif

#if !MM2_HOST_AMIGA
#include <chrono>
#include <cstdio>
#endif

namespace mm2::events {

namespace {

// #region agent log
inline void agentDbgLog(const char *hyp, const char *loc, const char *msg, const char *data_json)
{
#if MM2_HOST_AMIGA
    (void)hyp;
    (void)loc;
    (void)msg;
    (void)data_json;
#else
    FILE *f = std::fopen("C:/_20260421_/D-REC/development/MM2/debug-5e7785.log", "a");
    if (!f) {
        return;
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    std::fprintf(f,
                 "{\"sessionId\":\"5e7785\",\"hypothesisId\":\"%s\",\"location\":\"%s\",\"message\":\"%s\","
                 "\"data\":%s,\"timestamp\":%lld}\n",
                 hyp, loc, msg, data_json ? data_json : "{}", static_cast<long long>(ms));
    std::fclose(f);
#endif
}
// #endregion

constexpr uint8_t kGlyphHSeg = 0x05;
constexpr uint8_t kGlyphHCapL = 0x06;
constexpr uint8_t kGlyphHCapR = 0x07;
constexpr uint8_t kGlyphVSeg = 0x0B;
/* Retail pens (doc 44): A4-$7A52=1 text, -$7A51=2 chrome, -$7A50=8 sign border.
 * Play chrome uses pen 2 as UI red (PlayScreenChrome kBorderR). Match that lattice. */
constexpr uint8_t kPenChromeR = 255;
constexpr uint8_t kPenChromeG = 0;
constexpr uint8_t kPenChromeB = 0;
constexpr uint8_t kPenTextR = 255;
constexpr uint8_t kPenTextG = 255;
constexpr uint8_t kPenTextB = 255;
// OP_06 sign border/post @ 0x15AEE uses A4-$7A50 (pen 8); gold/yellow on Amiga.
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

void hLine(gfx::ScreenCompositor &c, int col0, int col1, int row, uint8_t r = kPenChromeR, uint8_t g = kPenChromeG,
           uint8_t b = kPenChromeB)
{
    glyphAt(c, col0, row, kGlyphHCapL, r, g, b);
    for (int col = col0 + 1; col < col1; ++col) {
        glyphAt(c, col, row, kGlyphHSeg, r, g, b);
    }
    glyphAt(c, col1, row, kGlyphHCapR, r, g, b);
}

/* row23_separator @ 0x44E0 / -$7F4A: pen 2, glyph 5 ×36 cols 2..37. */
void row23Separator(gfx::ScreenCompositor &c)
{
    for (int col = 2; col <= 37; ++col) {
        glyphAt(c, col, 23, kGlyphHSeg, kPenChromeR, kPenChromeG, kPenChromeB);
    }
}

void clearCells(gfx::ScreenCompositor &c, int x1, int y1, int x2, int y2)
{
    c.clearRect(x1 * 8, y1 * 8, (x2 - x1 + 1) * 8, (y2 - y1 + 1) * 8, 0, 0, 0, 255);
}

void chromeMsgTop(gfx::ScreenCompositor &c)
{
    /* chrome_msg_top @ 0x43A8 / -$7F5C — pen 2 ticks + $0B junctions. */
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
    // On Amiga use UI pen black — world pen 0 is the .32 palette sky/terrain index.
#if MM2_HOST_AMIGA
    c.fillRectPen(8 * 8, row * 8, 11 * 8, 8, MM2_UI_PEN_BLACK);
#else
    c.fillRect(8 * 8, row * 8, 11 * 8, 8, kPenSignFillR, kPenSignFillG, kPenSignFillB, 255);
#endif
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
    restore_row23_rule_ = false;
    text_entry_ = false;
    text_entry_len_ = 0;
    text_entry_buf_[0] = '\0';
    exit_bit0_ = false;
    exit_bit1_ = false;
    sign_overlay_.unload();
    pegasus_overlay_.unload();
    sign_placement_ = 0;
    sign_dst_override_ = false;
    for (EventTextLayer &layer : layers_) {
        layer.op = EventTextOp::None;
        layer.text[0] = '\0';
        layer.base_row = 19;
    }
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

EventTextLayer *EventTextView::pushConsoleLayer(EventTextOp op)
{
    /* ASM OP_01/02/03 clear the message cells then paint — one active console block. */
    clearConsoleMessageLayers();
    if (layer_count_ >= static_cast<int>(sizeof(layers_) / sizeof(layers_[0]))) {
        return nullptr;
    }
    EventTextLayer &layer = layers_[layer_count_++];
    layer.op = op;
    layer.text[0] = '\0';
    layer.base_row = 19;
    return &layer;
}

void EventTextView::showOp01(const char *text)
{
    if (EventTextLayer *layer = pushConsoleLayer(EventTextOp::Op01CenterRow17)) {
        copyResolvedText(layer->text, sizeof(layer->text), text);
    }
    setExitFlagBit0();
}

void EventTextView::showOp02(const char *text, int base_row)
{
    if (EventTextLayer *layer = pushConsoleLayer(EventTextOp::Op02BlockRows19_22)) {
        copyResolvedText(layer->text, sizeof(layer->text), text);
        layer->base_row = (base_row >= 17 && base_row <= 22) ? base_row : 19;
    }
    setExitFlagBit1();
}

void EventTextView::showOp03(const char *text)
{
    if (EventTextLayer *layer = pushConsoleLayer(EventTextOp::Op03TallBlock)) {
        copyResolvedText(layer->text, sizeof(layer->text), text);
        layer->base_row = 17;
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
        /* OP_06 @ 0x15AFE–0x15B22: rewrite ASCII '-' (0x2D) → '{' (0x7B) in the
         * resolved buffer before the glyph draw (presentation glyph map). */
        copyResolvedText(layer.text, sizeof(layer.text), text);
        for (char *p = layer.text; *p; ++p) {
            if (*p == '-') {
                *p = '{';
            }
        }
    }
}

void EventTextView::showOp0B(const char *text, const char *data_dir, const GameStateView &gs,
                             const Mm2AttribRecord *attrib, uint8_t str_idx, uint8_t placement)
{
    sign_placement_ = placement;
    sign_dst_override_ = false;
    sign_overlay_.unload();
    bool has_sign = false;
    int anm_id = -1;
    if (data_dir) {
        anm_id =
            ServiceSignResolver::resolveForGameState(gs.a4(), static_cast<int>(gs.screenId()), attrib, str_idx);
        if (anm_id > 0) {
            /* Retail applies .anm pens 3-17 at blit time (mode $17), not at load —
             * same as combat / scripted sprites. */
            has_sign = sign_overlay_.loadFromId(data_dir, anm_id, gfx::AnmLoopMode::Loop, false);
        }
    }
    /* OP_0B → sign_sprite_place(pos, $40, $20) @ 0x3266 stores place args in
     * A4-$71DA/-$71D8. Also drop any leftover FD bit7 place-stream: a prior
     * OP_08/0A/$FD chrome can leave MODE=$FD + BUF bit7 bytes that YesNo then
     * re-applies every poll (wrong (0,8) jump, or sticky canvasAt override).
     * Inline of eventVmScriptedKeyReset — event_op_demo does not link Helpers. */
    uint8_t *a4 = const_cast<uint8_t *>(gs.a4());
    if (a4) {
        mm2_gs_set_u16(a4, MM2_GS_SCRIPTED_KEY_IDX, 0xFFFF);
        mm2_gs_set_u16(a4, MM2_GS_SCRIPTED_KEY_REP, 0xFFFF);
        mm2_gs_set_u8(a4, MM2_GS_SCRIPTED_KEY_DLY, 0);
        mm2_gs_set_u16(a4, MM2_GS_SCRIPTED_KEY_DY, 0x40);
        mm2_gs_set_u16(a4, MM2_GS_SCRIPTED_KEY_DX, 0x20);
        mm2_gs_set_u8(a4, MM2_GS_SCRIPTED_KEY_BUF, 0xFF);
        mm2_gs_set_u8(a4, MM2_GS_SCRIPTED_KEY_MODE, 0xFF);
    }
    // #region agent log
    {
        char data[256];
        const unsigned dy = a4 ? mm2_gs_u16(a4, MM2_GS_SCRIPTED_KEY_DY) : 0;
        const unsigned dx = a4 ? mm2_gs_u16(a4, MM2_GS_SCRIPTED_KEY_DX) : 0;
        const unsigned mode = a4 ? mm2_gs_u8(a4, MM2_GS_SCRIPTED_KEY_MODE) : 0;
        std::snprintf(data, sizeof(data),
                      "{\"str_idx\":%u,\"placement\":%u,\"anm_id\":%d,\"loaded\":%s,\"w\":%d,\"h\":%d,"
                      "\"screen\":%d,\"seed_dy\":%u,\"seed_dx\":%u,\"mode\":%u}",
                      static_cast<unsigned>(str_idx), static_cast<unsigned>(placement), anm_id,
                      has_sign ? "true" : "false", sign_overlay_.width(), sign_overlay_.height(),
                      static_cast<int>(gs.screenId()), dy, dx, mode);
        agentDbgLog("A", "EventTextView.cpp:showOp0B", "op0b_load", data);
    }
    // #endregion
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
    sign_dst_override_ = false;
    sign_overlay_.unload();
    bool has_sign = false;
    if (data_dir) {
        const int anm_id = ServiceSignResolver::resolveForScreen(screen_id, attrib, str_idx);
        if (anm_id > 0) {
            /* Retail applies .anm pens 3-17 at blit time (mode $17), not at load —
             * same as combat / scripted sprites. */
            has_sign = sign_overlay_.loadFromId(data_dir, anm_id, gfx::AnmLoopMode::Loop, false);
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

void EventTextView::showPegasusIllustration(const char *data_dir)
{
    pegasus_overlay_.unload();
    if (data_dir) {
        pegasus_overlay_.loadFromDiskIndex(data_dir, 21, gfx::AnmLoopMode::Loop, false);
    }
}

void EventTextView::showSpacePrompt()
{
    space_prompt_ = true;
    restore_row23_rule_ = false;
}

void EventTextView::clearSpacePrompt()
{
    if (space_prompt_) {
        /* SPACE wait end @ 0x15CE6: row23_separator() erases the prompt. */
        restore_row23_rule_ = true;
    }
    space_prompt_ = false;
}

void EventTextView::setTextEntry(const char *typed, int typed_len)
{
    text_entry_ = true;
    text_entry_len_ = 0;
    text_entry_buf_[0] = '\0';
    if (typed && typed_len > 0) {
        if (typed_len > 10) {
            typed_len = 10;
        }
        for (int i = 0; i < typed_len; ++i) {
            text_entry_buf_[i] = typed[i];
        }
        text_entry_buf_[typed_len] = '\0';
        text_entry_len_ = typed_len;
    }
    ++text_entry_revision_;
}

void EventTextView::clearTextEntry()
{
    if (!text_entry_ && text_entry_len_ == 0) {
        return;
    }
    text_entry_ = false;
    text_entry_len_ = 0;
    text_entry_buf_[0] = '\0';
    ++text_entry_revision_;
}

void EventTextView::applyScriptedSignPlace(uint8_t placement, uint16_t dst_x, uint16_t dst_y)
{
    /* -$7FBC / 0x3266 → mode $17 @ 0x23E24: dst_x=arg2, dst_y=arg3+8.
     * OP_0B's default place is ($40,$20)+8 → (64,40). That is the same simple
     * path blitCentered(placement) already uses — keep override false so
     * placement-table / flipbook stay on the normal path. Absolute canvasAt
     * only when choreography moves away from those defaults. */
    constexpr uint16_t kSimpleDstX = 0x40;
    constexpr uint16_t kSimpleDstY = 0x20 + 8;
    sign_placement_ = placement;
    const bool simple = (dst_x == kSimpleDstX && dst_y == kSimpleDstY);
    if (simple) {
        sign_dst_override_ = false;
    } else {
        sign_dst_x_ = dst_x;
        sign_dst_y_ = dst_y;
        sign_dst_override_ = true;
    }
    // #region agent log
    {
        static uint16_t s_last_x = 0xFFFF, s_last_y = 0xFFFF;
        static int s_last_ov = -1;
        const int ov = sign_dst_override_ ? 1 : 0;
        if (dst_x != s_last_x || dst_y != s_last_y || ov != s_last_ov) {
            s_last_x = dst_x;
            s_last_y = dst_y;
            s_last_ov = ov;
            char data[192];
            std::snprintf(data, sizeof(data),
                          "{\"placement\":%u,\"dst_x\":%u,\"dst_y\":%u,\"override\":%s,\"simple\":%s}",
                          static_cast<unsigned>(placement), static_cast<unsigned>(dst_x),
                          static_cast<unsigned>(dst_y), sign_dst_override_ ? "true" : "false",
                          simple ? "true" : "false");
            agentDbgLog("A", "EventTextView.cpp:applyScriptedSignPlace", "scripted_place_armed", data);
        }
    }
    // #endregion
}

void EventTextView::clearServiceSignSprite()
{
    /* place(-1) @ 0x3280 clears A4-$79FE. */
    // #region agent log
    agentDbgLog("C", "EventTextView.cpp:clearServiceSignSprite", "sign_cleared", "{}");
    // #endregion
    sign_overlay_.unload();
    sign_placement_ = 0;
    sign_dst_override_ = false;
    for (int i = 0; i < layer_count_; ++i) {
        if (layers_[i].op == EventTextOp::Op0BServiceSign) {
            layers_[i].op = EventTextOp::None;
            layers_[i].text[0] = '\0';
        }
    }
}

bool EventTextView::tickAnimation()
{
    bool changed = sign_overlay_.tick();
    if (pegasus_overlay_.loaded()) {
        changed |= pegasus_overlay_.tick();
    }
    return changed;
}

void EventTextView::drawPersistentViewportOverlays(gfx::ScreenCompositor &c) const
{
    for (int i = 0; i < layer_count_; ++i) {
        const EventTextLayer &layer = layers_[i];
        switch (layer.op) {
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
        default:
            break;
        }
    }
}

void EventTextView::drawServiceSignOverlay(gfx::ScreenCompositor &c) const
{
    for (int i = 0; i < layer_count_; ++i) {
        const EventTextLayer &layer = layers_[i];
        if (layer.op != EventTextOp::Op0BServiceSign) {
            continue;
        }
        if (sign_overlay_.loaded()) {
            // #region agent log
            {
                static int s_last_dx = -9999, s_last_dy = -9999, s_last_ov = -1, s_last_fr = -1;
                static int s_n = 0;
                const int dx = sign_dst_override_ ? static_cast<int>(sign_dst_x_) : -1;
                const int dy = sign_dst_override_ ? static_cast<int>(sign_dst_y_) : -1;
                const int fr = sign_overlay_.composedFrame();
                const bool changed = (dx != s_last_dx) || (dy != s_last_dy) ||
                                     (static_cast<int>(sign_dst_override_) != s_last_ov) ||
                                     (fr != s_last_fr) || ((s_n++ % 30) == 0);
                if (changed) {
                    s_last_dx = dx;
                    s_last_dy = dy;
                    s_last_ov = static_cast<int>(sign_dst_override_);
                    s_last_fr = fr;
                    char data[256];
                    std::snprintf(data, sizeof(data),
                                  "{\"path\":\"%s\",\"placement\":%u,\"dst_x\":%d,\"dst_y\":%d,"
                                  "\"w\":%d,\"h\":%d,\"frame\":%d,\"disk\":%d}",
                                  sign_dst_override_ ? "canvasAt" : "centered",
                                  static_cast<unsigned>(sign_placement_), dx, dy,
                                  sign_overlay_.width(), sign_overlay_.height(), fr,
                                  sign_overlay_.diskIndex());
                    agentDbgLog("A", "EventTextView.cpp:drawServiceSignOverlay", "sign_draw", data);
                }
            }
            // #endregion
            if (sign_dst_override_) {
                /* Scripted place (0x3266) @ 0x23E24: whole-canvas origin. */
                sign_overlay_.blitCanvasAt(c, static_cast<int>(sign_dst_x_),
                                           static_cast<int>(sign_dst_y_));
            } else {
                sign_overlay_.blitCentered(c, sign_placement_);
            }
        } else if (layer.text[0] != '\0') {
            // #region agent log
            agentDbgLog("C", "EventTextView.cpp:drawServiceSignOverlay", "sign_stub_not_loaded",
                        "{\"loaded\":false}");
            // #endregion
            drawServiceSignStub(c, layer.text);
        }
        break;
    }
}

void EventTextView::drawPegasusIllustration(gfx::ScreenCompositor &c) const
{
    if (pegasus_overlay_.loaded()) {
        pegasus_overlay_.blitCentered(c, 0);
    }
}

void EventTextView::draw(gfx::ScreenCompositor &c) const
{
    for (int i = 0; i < layer_count_; ++i) {
        const EventTextLayer &layer = layers_[i];
        switch (layer.op) {
        case EventTextOp::Op01CenterRow17:
            /* OP_01 @ 0x15924: pen-2 divider row 18, chrome_msg_top, clear row 17, center. */
            hLine(c, 0, 39, 18);
            chromeMsgTop(c);
            clearCells(c, 1, 17, 38, 17);
            printWrapped(c, 1, 17, 38, 17, layer.text, true, kPenTextR, kPenTextG, kPenTextB);
            break;
        case EventTextOp::Op02BlockRows19_22: {
            /* OP_02 @ 0x15942: win_clear_cells(1, base, 38, 22) then print from base. */
            const int base = layer.base_row;
            clearCells(c, 1, base, 38, 22);
            printWrapped(c, 1, base, 38, 22, layer.text, false, kPenTextR, kPenTextG, kPenTextB);
            break;
        }
        case EventTextOp::Op03TallBlock:
            /* OP_03 @ 0x159CE: chrome_msg_top + preset 2 clear + OP_02 base 17. */
            chromeMsgTop(c);
            clearCells(c, 1, 17, 38, 22);
            printWrapped(c, 1, 17, 38, 22, layer.text, false, kPenTextR, kPenTextG, kPenTextB);
            break;
        default:
            break;
        }
    }

    drawPersistentViewportOverlays(c);
    drawServiceSignOverlay(c);
    drawPegasusIllustration(c);

    /* OP_2F @ 0x16FEA: putchar(' ') then -$7F92 echoes typed chars + leaves the
     * text cursor as a solid block. Y/N (OP_09) draws nothing extra (doc 44). */
    if (text_entry_) {
        using namespace gfx::font_glyphs;
        const int col = 2; /* after the "?" prompt at (1,19) */
        const int row = 19;
        if (text_entry_len_ > 0) {
            textAt(c, col, row, text_entry_buf_, kPenTextR, kPenTextG, kPenTextB);
        }
        glyphAt(c, col + text_entry_len_, row, kSolidBlock, kPenTextR, kPenTextG, kPenTextB);
    }

    if (space_prompt_) {
        textAt(c, 9, 23, "('Space' to continue)", kPenTextR, kPenTextG, kPenTextB);
    } else if (restore_row23_rule_) {
        row23Separator(c);
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
    pegasus_overlay_.unload();
    sign_placement_ = 0;
    sign_dst_override_ = false;
}

void EventTextView::clearConsoleMessageLayers()
{
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
    restore_row23_rule_ = false;
    clearTextEntry();
}

void EventTextView::applyScriptExitCleanup(bool *redraw_status, bool *redraw_roster, bool *redraw_divider)
{
    scriptCleanup(redraw_status, redraw_roster, redraw_divider);
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

    /* 0x171AC: sign_sprite_place(-1) always; OP_04/05/06 stay until viewport refresh /
     * next tile scan. Console OP_01/02/03 are erased by status/roster redraw (bits 0/1)
     * — drop the retained layers so remake re-paint matches that overwrite. */
    clearServiceSignSprite();
    int kept = 0;
    for (int i = 0; i < layer_count_; ++i) {
        if (layers_[i].op == EventTextOp::Op04DoorLabel || layers_[i].op == EventTextOp::Op05PopupA ||
            layers_[i].op == EventTextOp::Op06Signpost) {
            if (kept != i) {
                layers_[kept] = layers_[i];
            }
            ++kept;
        }
    }
    layer_count_ = kept;
    space_prompt_ = false;
    restore_row23_rule_ = false;
    clearTextEntry();
    exit_bit0_ = false;
    exit_bit1_ = false;
    pegasus_overlay_.unload();
}

}  // namespace mm2::events

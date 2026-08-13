#pragma once
// Event text overlay rendering — OP_01..OP_07, OP_09 draw paths per doc 44.

#include "mm2/CppStdCompat.h"
#include "mm2/GameState.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/ViewportAnmOverlay.h"

#include "mm2_attrib_codec.h"

namespace mm2::events {

enum class EventTextOp : uint8_t {
    None = 0,
    Op01CenterRow17,
    Op02BlockRows19_22,
    Op03TallBlock,
    Op04DoorLabel,
    Op05PopupA,
    Op06Signpost,
    Op0BServiceSign,
    SpacePromptRow23,
};

struct EventTextLayer {
    EventTextOp op = EventTextOp::None;
    char text[256] = {};
    /** OP_02 base row (ASM push `$13`=19; OP_03 delegates with `$11`=17). */
    int base_row = 19;
};

/** Holds visible event text overlays and prompt state for one script session. */
class EventTextView {
public:
    void reset();
    void setExitFlagBit0() { exit_bit0_ = true; }
    void setExitFlagBit1() { exit_bit1_ = true; }

    bool exitBit0() const { return exit_bit0_; }
    bool exitBit1() const { return exit_bit1_; }
    int layerCount() const { return layer_count_; }

    void showOp01(const char *text);
    void showOp02(const char *text, int base_row = 19);
    void showOp03(const char *text);
    void showOp04(const char *text);
    void showOp05(const char *text);
    void showOp06(const char *text);
    /** OP_0B @ 0x15DB0 — str_idx → 0x15756 table[A4-$79E3][str_idx-1] → NN.anm. */
    void showOp0B(const char *text, const char *data_dir, const GameStateView &gs,
                  const Mm2AttribRecord *attrib, uint8_t str_idx, uint8_t placement);
    void showOp0B(const char *text, const char *data_dir, int screen_id,
                  const Mm2AttribRecord *attrib, uint8_t str_idx, uint8_t placement);

    void showSpacePrompt();
    void clearSpacePrompt();

    /** OP_2F text entry — solid block cursor after echoed input (not used for Y/N). */
    void setTextEntry(const char *typed, int typed_len);
    void clearTextEntry();
    bool textEntryActive() const { return text_entry_; }
    /** Bumps when typed text / cursor visibility changes (partial redraw). */
    int textEntryRevision() const { return text_entry_revision_; }

    /** True when OP_0B loaded an .anm portrait/sign overlay. */
    bool hasServicePortrait() const { return sign_overlay_.loaded(); }

    /** -$7FBC @ 0x3266 from scripted bit7 stream — reposition loaded sign. */
    void applyScriptedSignPlace(uint8_t placement, uint16_t dst_x, uint16_t dst_y);
    void clearServiceSignSprite();

    /** Loc 11 evt 04 OP_03 str[5]: Pegasus monster #131 → 21.anm over the 3D hood. */
    void showPegasusIllustration(const char *data_dir);
    bool hasPegasusIllustration() const { return pegasus_overlay_.loaded(); }

    /** Drop OP_01/02/03 console message layers before a shop menu repaints the
     *  lower band (ASM -$7ED8 preset 2: cells (1,17)-(38,22)). Keeps OP_04/05/06/0B. */
    void clearConsoleMessageLayers();

    /** Explore/service epilogue matching -$7D40 / 0x171AC: drop console messages,
     *  unload OP_0B sign (place(-1)), keep OP_04/05/06 until the next tile scan. */
    void applyScriptExitCleanup(bool *redraw_status, bool *redraw_roster, bool *redraw_divider);

    /** Draw all active layers onto the compositor (after 3D view). */
    void draw(gfx::ScreenCompositor &c) const;

    /** OP_04/05/06 text overlays inside the 3D viewport (cheap — partial redraw path). */
    void drawPersistentViewportOverlays(gfx::ScreenCompositor &c) const;

    /** OP_0B service sign .anm only (masked blit — skip when cel unchanged on idle frames). */
    void drawServiceSignOverlay(gfx::ScreenCompositor &c) const;

    /** Pegasus 21.anm illustration (drawn above sign overlay when both active). */
    void drawPegasusIllustration(gfx::ScreenCompositor &c) const;

    /** Advance OP_0B sign .anm one game tick; true when cel changed. */
    bool tickAnimation();

    /** Current composed frame index (tests / demo verification). */
    int serviceSignFrame() const { return sign_overlay_.composedFrame(); }

    /** Test helper: true if any layer text contains needle (case-sensitive). */
    bool containsText(const char *needle) const;

    /** Drop OP_04/05/06/0B ambient layers (and sign .anm) before a tile re-scan. */
    void clearPersistentOverlays();

    /** Script end cleanup @ 0x171AC — clears overlay state; returns bits for chrome redraw. */
    void scriptCleanup(bool *redraw_status, bool *redraw_roster, bool *redraw_divider);

private:
    static void copyResolvedText(char *dst, size_t cap, const char *src);
    static bool isPersistentOverlay(EventTextOp op);
    /** Replace any prior OP_01/02/03 (ASM clears cells then paints — one active message). */
    EventTextLayer *pushConsoleLayer(EventTextOp op);

    EventTextLayer layers_[4]{};
    int layer_count_ = 0;
    bool space_prompt_ = false;
    /** After SPACE wait ends — redraw row-23 rule (0x44E0 / -$7F4A). */
    bool restore_row23_rule_ = false;
    bool text_entry_ = false;
    char text_entry_buf_[11]{};
    int text_entry_len_ = 0;
    int text_entry_revision_ = 0;
    bool exit_bit0_ = false;
    bool exit_bit1_ = false;

    gfx::ViewportAnmOverlay sign_overlay_{};
    gfx::ViewportAnmOverlay pegasus_overlay_{};
    uint8_t sign_placement_ = 0;
    bool sign_dst_override_ = false;
    uint16_t sign_dst_x_ = 0x40;
    uint16_t sign_dst_y_ = 0x28;
};

}  // namespace mm2::events

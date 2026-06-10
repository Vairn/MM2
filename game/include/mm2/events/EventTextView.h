#pragma once
// Event text overlay rendering — OP_01..OP_07, OP_09 draw paths per doc 44.

#include "mm2/CppStdCompat.h"
#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/gfx/ViewportAnmOverlay.h"

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
    /** OP_0B @ 0x15DB0 — blits service sign .anm when data_dir/location_id resolve. */
    void showOp0B(const char *text, const char *data_dir = nullptr, int location_id = 0,
                  uint8_t str_idx = 0, uint8_t placement = 0);

    void showSpacePrompt();
    void clearSpacePrompt();

    /** Draw all active layers onto the compositor (after 3D view). */
    void draw(gfx::ScreenCompositor &c) const;

    /** Script end cleanup @ 0x171AC — clears overlay state; returns bits for chrome redraw. */
    void scriptCleanup(bool *redraw_status, bool *redraw_roster, bool *redraw_divider);

private:
    static void copyResolvedText(char *dst, size_t cap, const char *src);
    static bool isPersistentOverlay(EventTextOp op);

    EventTextLayer layers_[4]{};
    int layer_count_ = 0;
    bool space_prompt_ = false;
    bool exit_bit0_ = false;
    bool exit_bit1_ = false;

    gfx::ViewportAnmOverlay sign_overlay_{};
    uint8_t sign_placement_ = 0;
};

}  // namespace mm2::events

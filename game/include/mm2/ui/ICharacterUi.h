#pragma once

#include "mm2/gfx/ScreenCompositor.h"
#include "mm2/platform/Platform.h"
#include "mm2/ui/UiResult.h"
#include "mm2_party_launch.h"
#include "mm2_roster_codec.h"

namespace mm2::ui {

class ICharacterUi {
public:
    virtual ~ICharacterUi() = default;

    /** Bind data directory; must not touch the filesystem (keep stack shallow on Amiga). */
    virtual bool init(const char *data_dir) = 0;
    /** Load backing .dat assets; call from a shallow stack frame, free via shutdown(). */
    virtual bool loadDataFiles() = 0;
    /** Amiga: take ownership of items.dat preloaded at title init (avoids disk I/O on V/C/G). */
    virtual bool adoptItemNames(uint8_t *data, std::size_t size)
    {
        (void)data;
        (void)size;
        return false;
    }
    /** Load create-character-only assets (throw.32); call from a shallow stack frame on Amiga. */
    virtual void prepareCreateCharacterAssets() {}
    virtual void shutdown() = 0;

    /** Amiga: mark the off-screen cache stale (rebuild before next present). */
    virtual void requestRedraw() {}
    /** Amiga: playfield cache needs rebuilding. SDL: always true. */
    virtual bool needsRedraw() const { return true; }
    /** Amiga: call after cache rebuild. */
    virtual void ackRedraw() {}
    /**
     * Amiga party chooser: Ctrl+letter tick toggle can patch the existing UI
     * cache instead of a full clear+repaint. Default = always full rebuild.
     */
    virtual bool needsIncrementalRedraw() const { return false; }
    /** Patch only dirty tick/count cells into the current UI cache. */
    virtual void renderIncremental(gfx::ScreenCompositor & /*compositor*/) {}
    /** Union rect of the last incremental patch (for partial present). */
    virtual bool takeIncrementalPresentRect(int *out_x, int *out_y, int *out_w, int *out_h)
    {
        (void)out_x;
        (void)out_y;
        (void)out_w;
        (void)out_h;
        return false;
    }

    virtual void beginViewParty(Mm2RosterFile &roster) = 0;
    virtual UiResult tickViewParty(const platform::KeyState &keys) = 0;
    virtual void renderViewParty(gfx::ScreenCompositor &compositor) = 0;

    virtual void beginCreateCharacter(Mm2RosterFile &roster, int slot) = 0;
    virtual UiResult tickCreateCharacter(const platform::KeyState &keys) = 0;
    virtual void renderCreateCharacter(gfx::ScreenCompositor &compositor) = 0;

    virtual void beginChooseParty(Mm2RosterFile &roster, uint8_t town_filter = 1,
                                  const Mm2PartyLaunch *saved_party = nullptr) = 0;
    virtual UiResult tickChooseParty(const platform::KeyState &keys) = 0;
    virtual void renderChooseParty(gfx::ScreenCompositor &compositor) = 0;

    /** Set when tickChooseParty returns Done (Z confirm @ ASM 0x0E06). */
    virtual bool takePartyLaunch(Mm2PartyLaunch *out) = 0;
};

}  // namespace mm2::ui

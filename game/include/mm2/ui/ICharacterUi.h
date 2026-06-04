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

    virtual bool init(const char *data_dir) = 0;
    virtual void shutdown() = 0;

    virtual void beginViewParty(Mm2RosterFile &roster) = 0;
    virtual UiResult tickViewParty(const platform::KeyState &keys) = 0;
    virtual void renderViewParty(gfx::ScreenCompositor &compositor) = 0;

    virtual void beginCreateCharacter(Mm2RosterFile &roster, int slot) = 0;
    virtual UiResult tickCreateCharacter(const platform::KeyState &keys) = 0;
    virtual void renderCreateCharacter(gfx::ScreenCompositor &compositor) = 0;

    virtual void beginChooseParty(Mm2RosterFile &roster) = 0;
    virtual UiResult tickChooseParty(const platform::KeyState &keys) = 0;
    virtual void renderChooseParty(gfx::ScreenCompositor &compositor) = 0;

    /** Set when tickChooseParty returns Done (Z confirm @ ASM 0x0E06). */
    virtual bool takePartyLaunch(Mm2PartyLaunch *out) = 0;
};

}  // namespace mm2::ui

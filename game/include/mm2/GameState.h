#pragma once

#include "mm2/CppStdCompat.h"

#include "mm2_gamestate.h"

namespace mm2 {

/* Non-owning accessor over an A4 base pointer (== image + MM2_A4_ANCHOR). */
class GameStateView {
public:
    GameStateView() = default;
    explicit GameStateView(uint8_t *a4) : a4_(a4) {}

    bool valid() const { return a4_ != nullptr; }
    uint8_t *a4() { return a4_; }
    const uint8_t *a4() const { return a4_; }

    /* Session: current screen + party position (set by party-launch @ 0x0E2C
     * and by screen transitions). */
    uint8_t screenId() const { return mm2_gs_u8(a4_, MM2_GS_SCREEN_MODE_ID); }
    uint8_t coordX() const { return mm2_gs_u8(a4_, MM2_GS_COORD_B); } /* A4-$79F1 */
    uint8_t coordY() const { return mm2_gs_u8(a4_, MM2_GS_COORD_A); } /* A4-$79F0 */
    void setScreenId(uint8_t v) { mm2_gs_set_u8(a4_, MM2_GS_SCREEN_MODE_ID, v); }
    void setCoordX(uint8_t v) { mm2_gs_set_u8(a4_, MM2_GS_COORD_B, v); }
    void setCoordY(uint8_t v) { mm2_gs_set_u8(a4_, MM2_GS_COORD_A, v); }

    /* Facing: movement key 'N'/'E'/'S'/'W' (A4-$864F) + facing index
     * (A4-$AA29, set by map_facing_from_key @ 0x5636: N=6 E=4 S=2 W=0). */
    char facingKey() const { return static_cast<char>(mm2_gs_u8(a4_, MM2_GS_LAST_MOVE_KEY)); }
    uint8_t facingIndex() const { return mm2_gs_u8(a4_, MM2_GS_FACING_INDEX); }
    /* 0=N 1=E 2=S 3=W (View3D convention) from the ASM facing index. */
    int facing03() const
    {
        switch (facingKey()) {
            case 'E': return 1;
            case 'S': return 2;
            case 'W': return 3;
            default: return 0;
        }
    }

    /* map_facing_from_key @ 0x5636: latches the key, the direction-mask
     * bundle hi byte (A4-$AA28) and the facing index (A4-$AA29).
     * Mirrors apply_facing_key in EXTRACTED/decomp/mm2_party_launch.c. */
    void setFacingKey(char key)
    {
        uint8_t bundle_hi = 0;
        uint8_t facing_idx = 0;
        switch (key) {
            case 'N': bundle_hi = 0xC0; facing_idx = 6; break;
            case 'E': bundle_hi = 0x30; facing_idx = 4; break;
            case 'S': bundle_hi = 0x0C; facing_idx = 2; break;
            case 'W': bundle_hi = 0x03; facing_idx = 0; break;
            default: return;
        }
        mm2_gs_set_u8(a4_, MM2_GS_LAST_MOVE_KEY, static_cast<uint8_t>(key));
        mm2_gs_set_u8(a4_, -0x55D8, bundle_hi); /* A4-$AA28 tile bundle hi */
        mm2_gs_set_u8(a4_, MM2_GS_FACING_INDEX, facing_idx);
    }

    /* Calendar (doc 13-time-era-calendar.md): 10 parallel eras. */
    uint16_t era() const { return mm2_gs_u16(a4_, MM2_GS_ERA); }
    uint16_t day() const { return mm2_gs_day(a4_, eraClamped()); }
    uint16_t year() const { return mm2_gs_year(a4_, eraClamped()); }

    /* Right panel mode A4-$79B2: 0=OPTIONS, 1=PROTECT (doc 43 §4). */
    uint8_t rightPanelMode() const { return mm2_gs_u8(a4_, MM2_GS_NEW_GAME_FLAG); }
    void setRightPanelMode(uint8_t v) { mm2_gs_set_u8(a4_, MM2_GS_NEW_GAME_FLAG, v); }

    /* Protect panel @ 0x5E28 (A4 -$79AB..-$79A6). */
    uint8_t lightFactor() const { return mm2_gs_u8(a4_, MM2_GS_LIGHT_FACTOR); }
    uint8_t magicProtect() const { return mm2_gs_u8(a4_, MM2_GS_MAGIC_PROTECT); }
    uint8_t forcesProtect() const { return mm2_gs_u8(a4_, MM2_GS_FORCES_PROTECT); }
    uint8_t levitateFlag() const { return mm2_gs_u8(a4_, MM2_GS_LEVITATE_FLAG); }
    uint8_t walkWaterFlag() const { return mm2_gs_u8(a4_, MM2_GS_WALK_WATER_FLAG); }
    uint8_t guardDogFlag() const { return mm2_gs_u8(a4_, MM2_GS_GUARD_DOG_FLAG); }
    void setLightFactor(uint8_t v) { mm2_gs_set_u8(a4_, MM2_GS_LIGHT_FACTOR, v); }
    void setMagicProtect(uint8_t v) { mm2_gs_set_u8(a4_, MM2_GS_MAGIC_PROTECT, v); }
    void setForcesProtect(uint8_t v) { mm2_gs_set_u8(a4_, MM2_GS_FORCES_PROTECT, v); }

    bool soundsEnabled() const { return (mm2_gs_u8(a4_, MM2_GS_SOUNDS_FLAG) & 1) != 0; }
    bool walkBeepEnabled() const { return (mm2_gs_u8(a4_, MM2_GS_WALK_BEEP_FLAG) & 1) != 0; }
    uint8_t disposition() const { return mm2_gs_u8(a4_, MM2_GS_DISPOSITION); }
    uint8_t delaySetting() const { return mm2_gs_u8(a4_, MM2_GS_DELAY); }
    void toggleSounds() { mm2_gs_set_u8(a4_, MM2_GS_SOUNDS_FLAG, static_cast<uint8_t>(soundsEnabled() ? 0 : 1)); }
    void toggleWalkBeep()
    {
        mm2_gs_set_u8(a4_, MM2_GS_WALK_BEEP_FLAG, static_cast<uint8_t>(walkBeepEnabled() ? 0 : 1));
    }
    void cycleDisposition() { mm2_gs_set_u8(a4_, MM2_GS_DISPOSITION, static_cast<uint8_t>((disposition() + 1) % 4)); }
    void cycleDelay() { mm2_gs_set_u8(a4_, MM2_GS_DELAY, static_cast<uint8_t>((delaySetting() + 1) % 10)); }

    /* New-game defaults verified from the DATA hunk (A4 image is loaded from
     * disk; ghidra/mm2_data_00.bin @ A4-$79DE/-$79CA/-$79B6/-$79B4):
     *   day[era]  = 1 for all 10 eras
     *   year[era] = era * 100 (0,100,...,900)
     *   era       = 9  -> new game starts Day 1, Year 900
     *   subday    = 1
     * Our gs image is zero-initialized, so materialize these explicitly. */
    void initCalendarDefaults()
    {
        for (int e = 0; e < MM2_GS_ERA_COUNT; ++e) {
            mm2_gs_set_u16(a4_, MM2_GS_DAY + e * 2, 1);
            mm2_gs_set_u16(a4_, MM2_GS_YEAR + e * 2, static_cast<uint16_t>(e * 100));
        }
        mm2_gs_set_u16(a4_, MM2_GS_ERA, 9);
        mm2_gs_set_u16(a4_, MM2_GS_TIME_SUBDAY, 1);
    }

    /* Controls defaults @ 0x13CCE: sounds/walk-beep on, disposition 1.
     * Retail delay is 0; on a modern vsync display that is ~2 frames and combat
     * hit/miss lines are unreadable. Default to 3 (still 0..9 via Controls). */
    void initControlsDefaults()
    {
        mm2_gs_set_u8(a4_, MM2_GS_SOUNDS_FLAG, 1);
        mm2_gs_set_u8(a4_, MM2_GS_WALK_BEEP_FLAG, 1);
        mm2_gs_set_u8(a4_, MM2_GS_DISPOSITION, 1);
        mm2_gs_set_u8(a4_, MM2_GS_DELAY, 3);
    }

    /* New game @ 0x19B28 clears protect vars -$79A6..-$79AB; panel mode 1. */
    void initProtectDefaults()
    {
        mm2_gs_set_u8(a4_, MM2_GS_GUARD_DOG_FLAG, 0);
        mm2_gs_set_u8(a4_, MM2_GS_WALK_WATER_FLAG, 0);
        mm2_gs_set_u8(a4_, MM2_GS_LEVITATE_FLAG, 0);
        mm2_gs_set_u8(a4_, MM2_GS_FORCES_PROTECT, 0);
        mm2_gs_set_u8(a4_, MM2_GS_MAGIC_PROTECT, 0);
        mm2_gs_set_u8(a4_, MM2_GS_LIGHT_FACTOR, 0);
        setRightPanelMode(1);
    }

    /* Area enter @ 0x6E84 sets A4-$79E9 on first visit (doc 46 §3.6). */
    bool firstTimeFlag() const { return mm2_gs_u8(a4_, MM2_GS_FIRST_TIME_FLAG) != 0; }
    void setFirstTimeFlag(bool v) { mm2_gs_set_u8(a4_, MM2_GS_FIRST_TIME_FLAG, v ? 1 : 0); }

    /* Party class-quest/guild bits (e.g. "seen Pegasus" 0x40) live PER CHARACTER
     * at roster record offset 0x79 (Mm2RosterRecord::class_quest_guild_mask),
     * written by OP_15/18 selector 0x74. They are no longer mirrored in a global
     * here; read them from the relevant party member's record. The Corak-intro
     * one-shot is tracked by GameSession (port scene scheduling), not game state. */

private:
    int eraClamped() const
    {
        const int e = era();
        return (e >= 0 && e < MM2_GS_ERA_COUNT) ? e : MM2_GS_ERA_COUNT - 1;
    }

    uint8_t *a4_ = nullptr;
};

}  // namespace mm2

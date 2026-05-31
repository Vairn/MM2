#pragma once
// monsters.dat - record geometry confirmed (256 * 26 = 6656 bytes).
//   accessor at asm 0x99C8 uses index * 0x1A.
//
// Field map below comes from b3dmm2 (proto.bb / mm2ed.bb). Names are decoded
// as Chr(byte - 128) in the legacy tools; HP/XP are piecewise-coded. We keep
// the full record as raw bytes plus named accessors so unknown bytes are never
// lost on round-trip.
//
//   0x00-0x0D name (14 bytes, each byte = char + 128)
//   0x0E      hp code
//   0x0F      xp code
//   0x10      treasure/reward pack (gold/gems/item/XP bonus bits; NOT a combat
//                    ability - decoded by the reward routine at asm 0x10b74)
//   0x11      Pabil (group attack: low 5 bits = verb index into kPartyVerbNames,
//                    bits 5-7 = use-chance tier)
//   0x12      Sabil (single-target attack: low 5 bits = effect index into
//                    kAbilityNames, bit7 = undead, bit6 = archer, bit5 = misc)
//   0x13      Oabil (low nibble+1, x10 if bit4 = "adds friends" reinforcement
//                    count; bits 5-6 = flee/run chance tier; bit7 = multiplies)
//
// THIS DECODE IS ASM-CONFIRMED (not b3d-guessed). The authoritative monster
// unpacker is at asm 0x4C8E (called via accessor 0x99C4, `mulu.w #$1a`). Combat
// reads: party verb dispatch 0x10002 (indexes verb table by Pabil low-5, e.g.
// idx 29 = "frenzies"), single-attack 0xFEEA, flee ("runs") 0x10DFC gated by
// Oabil bits 5-6, "adds friends" 0x11F0A gated by Oabil low nibble, multiply
// 0x100B0 gated by Oabil bit7. All bits are preserved on round-trip.
//   0x14      speed
//   0x15      picture  -> sprite file (picture & 0x7F) as "NN.anm"; the 0x80
//                         bit is a separate placement/size flag. Validated:
//                         01.anm = 12-frame 84x86 "Creepy Crawler" (picture 1).
//   0x16      AC
//   0x17      damage
//   0x18      speed2
//   0x19      magic resistance

#include <array>
#include <string>

#include "core/ByteIO.h"

namespace mm2 {

constexpr int kMonstersCount = 256;
constexpr int kMonsterRecordSize = 0x1A;
constexpr int kMonsterNameSize = 14;
constexpr int kMonstersFileSize = kMonstersCount * kMonsterRecordSize;

// Single-target status effects, indexed 0..31. The low 5 bits of the Sabil
// byte (0x12) select one of these. Confirmed against the victim-message table
// in the binary (asm ~0xFA1A: "lost gold", "is poisoned", ...).
constexpr int kAbilityCount = 32;
extern const char* const kAbilityNames[kAbilityCount];
const char* abilityName(uint8_t index);

// Group/party attack verbs, indexed 0..31. The low 5 bits of the Pabil byte
// (0x11) select one of these. Transcribed directly from the verb table the
// game prints in the combat dispatcher at asm 0x10002 (strings at 0xFB98+);
// index 29 = "frenzies" (e.g. the Cuisinart, Pabil 0x3D -> 0x1D = 29).
constexpr int kPartyVerbCount = 32;
extern const char* const kPartyVerbNames[kPartyVerbCount];
const char* partyVerbName(uint8_t index);

struct MonsterRecord {
    std::array<uint8_t, kMonsterRecordSize> raw{};

    // Names are stored as (char + 128) per byte in the legacy tooling.
    std::string nameStr() const;
    void setName(const std::string& s);

    uint8_t& byteAt(int off) { return raw[off]; }
    uint8_t hpCode() const { return raw[0x0E]; }
    uint8_t xpCode() const { return raw[0x0F]; }
    uint8_t pabil() const { return raw[0x11]; }
    uint8_t sabil() const { return raw[0x12]; }
    uint8_t oabil() const { return raw[0x13]; }
    uint8_t speed() const { return raw[0x14]; }

    // --- Decoded abilities (ASM-confirmed at 0x4C8E; bits preserved on write) ---
    // Single-target attack: Sabil (0x12) low 5 bits = status effect index.
    uint8_t singleEffect() const { return raw[0x12] & 0x1F; }
    bool isUndead() const { return (raw[0x12] & 0x80) != 0; }
    bool isArcher() const { return (raw[0x12] & 0x40) != 0; }
    void setSingleEffect(uint8_t e) { raw[0x12] = (raw[0x12] & ~0x1F) | (e & 0x1F); }
    void setUndead(bool v) { raw[0x12] = v ? (raw[0x12] | 0x80) : (raw[0x12] & ~0x80); }
    void setArcher(bool v) { raw[0x12] = v ? (raw[0x12] | 0x40) : (raw[0x12] & ~0x40); }

    // Group attack: Pabil (0x11) low 5 bits = verb index into kPartyVerbNames
    // (29 = "frenzies"); bits 5-7 = a use-chance tier (asm masks 0xE0 >> 4 and
    // looks it up in a frequency table - it is NOT a flee flag).
    uint8_t partyVerb() const { return raw[0x11] & 0x1F; }
    uint8_t partyChance() const { return (raw[0x11] >> 5) & 0x07; }
    void setPartyVerb(uint8_t e) { raw[0x11] = (raw[0x11] & ~0x1F) | (e & 0x1F); }
    void setPartyChance(uint8_t c) { raw[0x11] = (raw[0x11] & ~0xE0) | ((c & 0x07) << 5); }

    // Oabil (0x13): low nibble+1 = "adds friends" reinforcement count (x10 if
    // bit4 set); bits 5-6 = flee/run chance tier; bit7 = multiplies/breeds.
    uint8_t addFriends() const {
        uint8_t n = (raw[0x13] & 0x0F) + 1;
        return (raw[0x13] & 0x10) ? static_cast<uint8_t>(n * 10) : n;
    }
    uint8_t fleeTier() const { return (raw[0x13] >> 5) & 0x03; }
    bool multiplies() const { return (raw[0x13] & 0x80) != 0; }
    void setMultiplies(bool v) { raw[0x13] = v ? (raw[0x13] | 0x80) : (raw[0x13] & ~0x80); }

    uint8_t picture() const { return raw[0x15]; }
    uint8_t ac() const { return raw[0x16]; }
    uint8_t damage() const { return raw[0x17]; }
    uint8_t magicResist() const { return raw[0x19]; }
};

struct MonstersFile {
    std::array<MonsterRecord, kMonstersCount> records{};

    bool decode(const Bytes& bytes);
    Bytes encode() const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace mm2

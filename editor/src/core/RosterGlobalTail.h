#pragma once
// Global quest/state bytes packed in roster.dat slots 48..63 (file offset 0x1860).
// Runtime addresses use A4 displacements from anchor $7FFE; the on-disk layout is the
// 0x1860-byte serialized stream written by save_game_state @ 0x823C (reload @ 0x86F6).
// See EXTRACTED/docs/06-roster-format.md.

namespace mm2::roster_global {

// --- Runtime A4 displacements (signed, true offset from A4) ---
constexpr int kA4DayBase = -0x79DE;
constexpr int kA4YearBase = -0x79CA;
constexpr int kA4RosterIndexTbl = -0x796A;
constexpr int kA4Era = -0x79B6;
constexpr int kA4EraLow = -0x79B5;  // g=0x84; low byte of era word
constexpr int kA4TimeSubday = -0x79B4;
constexpr int kA4EventBank = -0x798B;  // g=0x00..0x17 (24 bytes)
constexpr int kA4GateBank = -0x7995;   // g=0x80..0x83 and g=0x3B..0x3E
constexpr int kA4TalismanBase = -0x79A4;  // g=0x27..0x2A
constexpr int kA4SpecialQuest = -0x79A8;  // g=0x23
constexpr int kA4EndgameScoreA = -0x79AA;
constexpr int kA4EndgameScoreB = -0x79A9;
constexpr int kA4AchievementA = -0x79A7;
constexpr int kA4AchievementB = -0x79A6;
constexpr int kA4EncounterMod = -0x79A5;
constexpr int kA4ClassQuestCounter = -0x79A0;  // g=0x2B
constexpr int kA4SecondaryQuestCounter = -0x799F;  // g=0x2C
constexpr int kA4TempleDonation = -0x799E;  // g=0x13
constexpr int kA4GuardianGate = -0x7996;  // g=0x32
constexpr int kA4TundaraLever = -0x7990;  // g=0x33
constexpr int kA4CastleXabran = -0x798F;  // g=0x3B
constexpr int kA4DawnsMistGate = -0x798E;  // g=0x3C
constexpr int kA4PeriodFlagB = -0x798D;  // g=0x3D; calendar period, day 60/120/180
constexpr int kA4PeriodFlagA = -0x798C;  // g=0x3E; calendar period, day 60/120/180
constexpr int kA4LightFactor = -0x79AB;
constexpr int kA4NewGameFlag = -0x79B2;
constexpr int kA4Sounds = -0x79B0;
constexpr int kA4WalkBeep = -0x79AF;
constexpr int kA4Disposition = -0x79AE;
constexpr int kA4Delay = -0x79AD;
constexpr int kA4InputState = -0x799D;
constexpr int kA4InputStateEnd = -0x7999;
constexpr int kA4MoveCounter = -0x796C;
constexpr int kA4EncounterMode = -0x796B;

// --- Indices within the concatenated tail (slots 48..63, 2080 bytes) ---
// Order matches memcpy/read sequence in save_game_state @ 0x823C.
namespace tail_off {
constexpr int kDayByEra = 0x000;       // 10 x u16 LE
constexpr int kYearByEra = 0x014;      // 10 x u16 LE
constexpr int kPartyRosterIdx = 0x028; // 8 x u16 LE -> A4-$796A
constexpr int kPartySize = 0x038;      // u16 -> A4-$795A
constexpr int kEra = 0x03A;            // u16 -> A4-$79B6
constexpr int kSubday = 0x03C;         // u16 -> A4-$79B4
constexpr int kGateBank = 0x7C4;       // 10 bytes -> A4-$7995..-$798C
constexpr int kGateG80 = 0x7C4;
constexpr int kGateG81 = 0x7C5;
constexpr int kGateG82 = 0x7C6;
constexpr int kGateG83 = 0x7C7;
constexpr int kTundaraLever = 0x7C9;   // g=0x33 @ A4-$7990
constexpr int kCastleXabran = 0x7CA;   // g=0x3B
constexpr int kDawnsMistGate = 0x7CB;  // g=0x3C
constexpr int kPeriodFlagB = 0x7CC;    // g=0x3D
constexpr int kPeriodFlagA = 0x7CD;    // g=0x3E
constexpr int kEventBank = 0x7CE;      // 24 bytes -> A4-$798B (g=0x00..0x17)
constexpr int kTalismans = 0x7E6;      // 4 bytes -> A4-$79A4..-$79A1
constexpr int kNewGameFlag = 0x7EA;
constexpr int kSounds = 0x7EB;
constexpr int kWalkBeep = 0x7EC;
constexpr int kDisposition = 0x7ED;
constexpr int kDelay = 0x7EE;
constexpr int kLightFactor = 0x7F0;
constexpr int kEndgameScoreA = 0x7F1;
constexpr int kEndgameScoreB = 0x7F2;
constexpr int kSpecialQuest = 0x7F3;   // g=0x23 @ A4-$79A8
constexpr int kAchievementA = 0x7F4;
constexpr int kAchievementB = 0x7F5;
constexpr int kEncounterMod = 0x7F6;
constexpr int kClassQuestCounter = 0x7F7;   // g=0x2B
constexpr int kSecondaryQuestCounter = 0x7F8;  // g=0x2C
constexpr int kTempleDonation = 0x7F9;    // g=0x13
constexpr int kInputState0 = 0x7FA;
constexpr int kInputState4 = 0x7FE;
constexpr int kGuardianGate = 0x801;     // g=0x32
constexpr int kMoveCounter = 0x802;
constexpr int kEncounterMode = 0x803;
}  // namespace tail_off

}  // namespace mm2::roster_global

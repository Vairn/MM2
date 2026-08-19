#pragma once
// event.dat opcode bytes. Values are the bytecode; names match
// editor/src/eventlang/OpcodeTable.cpp (OP_01_TEXT → Text, …).

#include <cstdint>

namespace mm2::events {

enum class EventOp : uint8_t {
    Invalid = 0x00,
    Text = 0x01,
    TextBlock = 0x02,
    Text3 = 0x03, /* OP_03_TEXT */
    TextDoor = 0x04,
    TextPopupA = 0x05,
    TextPopupB = 0x06,
    WaitSpace = 0x07,
    WaitSpaceScripted = 0x08,
    PromptYn = 0x09,
    PromptYnB = 0x0A,
    ShowServiceWindow = 0x0B,
    MapTransition = 0x0C,
    PlaySoundSeq = 0x0D,
    ExecSelector = 0x0E,
    EndScript = 0x0F,
    IfTrueSkiptok = 0x10,
    IfFalseSkiptok = 0x11,
    EncounterSetup = 0x12,
    EncounterSetupB = 0x13,
    ClearTileEvent = 0x14,
    ApplyParty = 0x15,
    ScanPartyItems = 0x16,
    LoadVarRawToCond = 0x17,
    ApplyPartyMasked = 0x18,
    GiveItem = 0x19,
    StoreVar8 = 0x1A,
    CondThreshold = 0x1B,
    RngRollToCond = 0x1C,
    AudioWait = 0x1D,
    Delay = 0x1E,
    PartyEffect = 0x1F,
    PartyEffectB = 0x20,
    SetTile = 0x21,
    CheckEraRange = 0x22,
    CheckDayRange = 0x23,
    PayGoldToCond = 0x24,
    PayGemsToCond = 0x25,
    SelectMember = 0x26,
    SelectMemberB = 0x27,
    ConsumeItemToCond = 0x28,
    SetAbort = 0x29,
    SetTreasure = 0x2A,
    SkiptokIfVictory = 0x2B,
    AddWordCounter = 0x2C,
    CheckMemberAttr = 0x2D,
    OrMemberField = 0x2E,
    ReadAnswer = 0x2F,
    CheckAnswer = 0x30,
    PartyIterateDamage = 0x31,
    CountTitleNibble = 0x32,
    EndRecord = 0xFF,
};

/* First byte that is not a dispatched opcode (ROM table is 0x00..0x32). */
constexpr uint8_t kEventOpFirstInvalid = 0x33;

/* Script-visible high-bit modes (not extra opcodes). */
constexpr uint8_t kOp0cRandomScreen = 0x40;
constexpr uint8_t kOp0cRandomTile = 0x80;
constexpr uint8_t kOp19ItemFromCond = 0x80;
constexpr uint8_t kOp23OddDay = 0xB5;
constexpr uint8_t kOp23EvenDay = 0xB6;
constexpr uint8_t kOp2eClericPair = 0x80;

}  // namespace mm2::events

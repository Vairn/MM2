#pragma once
#include "eventlang/Ast.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace mm2::eventlang {

TriggerCond triggerCondFromByte(uint8_t raw);
uint8_t triggerCondByte(TriggerCond cond, uint8_t rawFallback = 0);

bool isCondSetOp(uint8_t op);
const char* selectorHandlerLabel(int sel);
std::string autoScriptName(int eventId);
std::string autoStringName(int index);

/** OP_15/18 member_spec → DSL token (all / selected / member=N). */
std::string partyMemberToken(int memberSpec);
/** Field selector → short name when known (skills, quest_flags, …). */
const char* partyFieldName(int fieldSel);
int partyFieldByName(const std::string& name);

/** OP_17/OP_1A group id → name from eventVmResolveVarOffset + ASM hireling gate.
 *  g=0x00..0x17 = hireling_a..hireling_x (party UI tst.b -$798B+letter @ 0x586).
 *  Other ids match MM2_GS_* singletons in the resolver switch. Unknown → nullptr. */
const char* varGroupName(int group);
/** Inverse of varGroupName; also accepts group=0xNN hex token. -1 if unknown. */
int varGroupByName(const std::string& name);
/** Skill nibble id (low 4 bits) → name when known. */
const char* skillNibbleName(int nibble);

uint8_t sayOpForVariant(const std::string& variant);
const char* sayVariantForOp(uint8_t op);

int classFieldByName(const std::string& name);
/** Named shop/quest → OP_0E selector, or -1 if unknown (no silent default). */
int selectorByShopOrQuest(const std::string& kind, const std::string& name);

/** OP_0E default-range bin @ 0x15EDC → (overlay_loc, event_index). */
std::optional<std::pair<int, int>> binExecSelector(int sel);
/** Inverse of binExecSelector; -1 if out of range. */
int selectorFromOverlayBin(int overlayLoc, int eventIndex);

/** Human DSL line body for an OP_0E selector (no indent). */
std::string formatSelectorDsl(int sel);
/** Short comment for inspector / outline. */
std::string formatSelectorSummary(int sel);

}  // namespace mm2::eventlang

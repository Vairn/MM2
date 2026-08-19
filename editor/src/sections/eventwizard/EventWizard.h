#pragma once
// Prefab → form → .mm2evt snippet for EventSection.

#include <functional>
#include <string>
#include <vector>

namespace mm2 {

enum class WizardTemplateKind {
    DoorSign = 0,
    DialogueReward,
    GoldToll,
    GemToll,
    ItemGate,
    ServiceShop,
    Encounter,
    Transition,
    Riddle,
    Trap,
    Count,
};

struct WizardTemplate {
    WizardTemplateKind kind;
    const char* name;
    const char* blurb;
};

const std::vector<WizardTemplate>& wizardTemplates();
const char* const* wizardCondNames(int* count);  // same order as DslParse::triggerCondName

struct WizardForm {
    // DoorSign / DialogueReward
    std::string message = "A door. It is locked.";
    int sayVariant = 0;           // 0=say_door 1=say_block 2=say_popup_a 3=say_basic
    bool withQuest = false;       // DialogueReward: end with quest handler
    int itemId = 0x01;            // reward item / gate item
    // GoldToll / GemToll / Trap
    int amount = 10;              // gold / gems / damage value
    std::string payMsg = "The toll is paid. You may pass.";
    std::string refuseMsg = "You cannot afford the toll.";
    bool clearTile = true;        // clear_tile_event after passing
    // ServiceShop
    int signIndex = 0;
    int serviceMode = 0;
    int shopSelector = 0;         // raw selector byte (0 = none)
    // Encounter
    std::vector<int> monsters = {0x01};
    // Transition
    int screen = 0;
    int pos = 0x00;
    // Riddle
    std::string question = "What walks on four legs in the morning?";
    std::string answer = "man";
    // Trap
    int trapMember = 0;           // 0 = all
};

struct WizardSnippet {
    std::string stringsBlock;   // "  name: \"text\"" lines (may be empty)
    std::string triggerLine;    // "on tile (y, x) cond -> name  @event NN"
    std::string scriptBlock;    // "script name:  @event NN\n  ..."
};

WizardSnippet buildWizardSnippet(
    WizardTemplateKind kind,
    int eventId,
    const std::string& scriptName,
    int tileY, int tileX,
    int condIndex,
    const std::function<std::string(int)>& itemNameOf,
    const std::function<std::string(int)>& monsterNameOf,
    const WizardForm& form);

struct WizardState {
    int templateIdx = 0;
    WizardForm form;
    int eventId = 0;
    char scriptName[32] = "event_00";
    int tileY = 0;
    int tileX = 0;
    int condIndex = 0;
};

std::string mergeWizardSnippet(const std::string& buffer, const WizardSnippet& snip);

}  // namespace mm2

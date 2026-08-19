#include "sections/eventwizard/EventWizard.h"

#include "core/ByteIO.h"
#include "eventlang/Ast.h"

#include <cstdio>
#include <sstream>

namespace mm2 {

namespace {

const char* kCondNames[] = {
    eventlang::triggerCondName(eventlang::TriggerCond::Always),
    eventlang::triggerCondName(eventlang::TriggerCond::Enter),
    eventlang::triggerCondName(eventlang::TriggerCond::FromNorth),
    eventlang::triggerCondName(eventlang::TriggerCond::DirSpecial),
    eventlang::triggerCondName(eventlang::TriggerCond::AnyDirection),
    eventlang::triggerCondName(eventlang::TriggerCond::FacingNs),
    eventlang::triggerCondName(eventlang::TriggerCond::EnterSpecial),
};
constexpr int kCondCount = sizeof(kCondNames) / sizeof(kCondNames[0]);

std::string condName(int idx) {
    if (idx < 0 || idx >= kCondCount) idx = 0;
    return kCondNames[idx];
}

/** Escape @ for newlines and \ for backslashes in string text. */
std::string escapeText(std::string s) {
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '\n') { s[i] = '@'; ++i; }
        else if (s[i] == '\\') { s.insert(i, 1, '\\'); i += 2; }
        else if (s[i] == '"') { s[i] = '\''; ++i; }  // quotes force raw — avoid
        else ++i;
    }
    return s;
}

std::string stringDef(const std::string& name, const std::string& text) {
    return "  " + name + ": \"" + escapeText(text) + "\"";
}

std::string sayStmt(int variant, const std::string& strName) {
    const char* verb = "say";
    switch (variant) {
        case 0: verb = "say_door"; break;
        case 1: verb = "say_block"; break;
        case 2: verb = "say_popup_a"; break;
        case 3: verb = "say_basic"; break;
        default: verb = "say"; break;
    }
    return std::string("  ") + verb + " " + strName;
}

}  // namespace

const std::vector<WizardTemplate>& wizardTemplates() {
    static const std::vector<WizardTemplate> kTemplates = {
        {WizardTemplateKind::DoorSign, "Door / sign text",
         "Triggered message when entering a tile — doors, signs, blocked paths"},
        {WizardTemplateKind::DialogueReward, "Dialogue + reward",
         "Talk to an NPC or object, optionally give an item or quest"},
        {WizardTemplateKind::GoldToll, "Gold toll",
         "Pay gold to pass; refuse if too poor"},
        {WizardTemplateKind::GemToll, "Gem toll",
         "Pay gems to pass; refuse if too poor"},
        {WizardTemplateKind::ItemGate, "Item gate",
         "Require a specific item to proceed"},
        {WizardTemplateKind::ServiceShop, "Service / shop",
         "Signboard service (bank, temple, shop, inn)"},
        {WizardTemplateKind::Encounter, "Encounter",
         "Trigger a monster battle"},
        {WizardTemplateKind::Transition, "Transition",
         "Teleport to another screen"},
        {WizardTemplateKind::Riddle, "Riddle",
         "Ask a question, check the typed answer"},
        {WizardTemplateKind::Trap, "Trap",
         "Damage the party when triggered"},
    };
    return kTemplates;
}

const char* const* wizardCondNames(int* count) {
    if (count) *count = kCondCount;
    return kCondNames;
}

WizardSnippet buildWizardSnippet(
    WizardTemplateKind kind,
    int eventId,
    const std::string& scriptName,
    int tileY, int tileX,
    int condIndex,
    const std::function<std::string(int)>& itemNameOf,
    const std::function<std::string(int)>& monsterNameOf,
    const WizardForm& form) {
    WizardSnippet out;
    std::ostringstream s;  // strings block
    std::ostringstream b;  // script body

    const std::string evtTag = "  @event " + std::to_string(eventId);

    // Trigger line (shared by all templates)
    {
        std::ostringstream t;
        t << "on tile (" << tileY << ", " << tileX << ") " << condName(condIndex)
          << " -> " << scriptName << evtTag;
        out.triggerLine = t.str();
    }

    switch (kind) {
        case WizardTemplateKind::DoorSign: {
            s << stringDef("msg", form.message) << "\n";
            b << sayStmt(form.sayVariant, "msg") << "\n";
            break;
        }
        case WizardTemplateKind::DialogueReward: {
            s << stringDef("msg", form.message) << "\n";
            b << sayStmt(form.sayVariant, "msg") << "\n";
            b << "  wait space\n";
            if (form.withQuest) {
                b << "  quest " << scriptName << "_q\n";
            } else if (form.itemId > 0) {
                std::string itemCmt;
                if (itemNameOf) {
                    auto n = itemNameOf(form.itemId);
                    if (!n.empty()) itemCmt = "  # " + n;
                }
                b << "  give_item item=" << hexByte(form.itemId)
                  << " member=0 charges=0 flags=0x00" << itemCmt << "\n";
            }
            break;
        }
        case WizardTemplateKind::GoldToll: {
            s << stringDef("pay", form.payMsg) << "\n";
            s << stringDef("refuse", form.refuseMsg) << "\n";
            b << "  if gold >= " << form.amount << ":\n";
            b << "    say pay\n";
            b << "    wait space\n";
            if (form.clearTile) b << "    clear_tile_event\n";
            b << "  else:\n";
            b << "    say refuse\n";
            b << "    wait space\n";
            break;
        }
        case WizardTemplateKind::GemToll: {
            s << stringDef("pay", form.payMsg) << "\n";
            s << stringDef("refuse", form.refuseMsg) << "\n";
            b << "  if gems >= " << form.amount << ":\n";
            b << "    say pay\n";
            b << "    wait space\n";
            if (form.clearTile) b << "    clear_tile_event\n";
            b << "  else:\n";
            b << "    say refuse\n";
            b << "    wait space\n";
            break;
        }
        case WizardTemplateKind::ItemGate: {
            std::string itemCmt;
            if (itemNameOf) {
                auto n = itemNameOf(form.itemId);
                if (!n.empty()) itemCmt = "  # " + n;
            }
            s << stringDef("open", form.payMsg) << "\n";
            s << stringDef("hint", form.refuseMsg) << "\n";
            b << "  if has_item " << hexByte(form.itemId) << ":" << itemCmt << "\n";
            b << "    say open\n";
            b << "    wait space\n";
            if (form.clearTile) b << "    clear_tile_event\n";
            b << "  else:\n";
            b << "    say hint\n";
            b << "    wait space\n";
            break;
        }
        case WizardTemplateKind::ServiceShop: {
            b << "  service_title sign=" << hexByte(form.signIndex)
              << " mode=" << form.serviceMode << "\n";
            if (form.shopSelector > 0) {
                b << "  selector " << hexByte(form.shopSelector) << "\n";
            }
            break;
        }
        case WizardTemplateKind::Encounter: {
            b << "  fight monsters";
            std::string names;
            for (int m : form.monsters) {
                b << " " << hexByte(m);
                if (monsterNameOf) {
                    auto n = monsterNameOf(m);
                    if (!n.empty()) {
                        if (!names.empty()) names += ", ";
                        names += n;
                    }
                }
            }
            b << " flags 0x00";
            if (!names.empty()) b << "  # " << names;
            b << "\n";
            break;
        }
        case WizardTemplateKind::Transition: {
            b << "  go_to screen " << form.screen << " pos " << hexByte(form.pos) << "\n";
            break;
        }
        case WizardTemplateKind::Riddle: {
            s << stringDef("ask", form.question) << "\n";
            s << stringDef("wrong", "That is not the answer.") << "\n";
            b << "  say ask\n";
            b << "  wait space\n";
            b << "  read_answer\n";
            b << "  if answer == \"" << escapeText(form.answer) << "\":\n";
            b << "    clear_tile_event\n";
            b << "  else:\n";
            b << "    say wrong\n";
            b << "    wait space\n";
            break;
        }
        case WizardTemplateKind::Trap: {
            b << "  party_damage member=" << hexByte(form.trapMember)
              << " value=" << form.amount << "\n";
            break;
        }
        default:
            break;
    }

    out.stringsBlock = s.str();
    out.scriptBlock = "script " + scriptName + ":" + evtTag + "\n" + b.str();
    return out;
}

namespace {

bool startsWith(const std::string& s, const char* pfx) {
    return s.compare(0, std::char_traits<char>::length(pfx), pfx) == 0;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

}  // namespace

std::string mergeWizardSnippet(const std::string& buffer, const WizardSnippet& snip) {
    std::vector<std::string> lines = splitLines(buffer);

    // 1. Strings block: find `strings:` header; body = following indented/blank
    //    lines up to the first top-level statement.
    int stringsHeader = -1;
    int stringsEnd = -1;  // exclusive end of strings body
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (startsWith(lines[i], "strings:")) {
            stringsHeader = i;
            stringsEnd = i + 1;
            while (stringsEnd < static_cast<int>(lines.size()) &&
                   (lines[stringsEnd].empty() || lines[stringsEnd][0] == ' '))
                ++stringsEnd;
            break;
        }
    }

    // Last trigger line + first script line positions.
    int lastTrigger = -1;
    int firstScript = -1;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (startsWith(lines[i], "on tile ")) lastTrigger = i;
        if (firstScript < 0 && startsWith(lines[i], "script ")) firstScript = i;
    }

    auto insertLines = [&](int pos, const std::vector<std::string>& add) {
        lines.insert(lines.begin() + pos, add.begin(), add.end());
        // Keep indices of interest stable relative to the insertion point.
        if (stringsHeader >= 0 && pos <= stringsHeader) {
            stringsHeader += static_cast<int>(add.size());
            stringsEnd += static_cast<int>(add.size());
        } else if (stringsHeader >= 0 && pos <= stringsEnd) {
            stringsEnd += static_cast<int>(add.size());
        }
        if (lastTrigger >= 0 && pos <= lastTrigger) lastTrigger += static_cast<int>(add.size());
        if (firstScript >= 0 && pos <= firstScript) firstScript += static_cast<int>(add.size());
    };

    // 2. Script block goes at the end.
    {
        std::vector<std::string> add;
        if (!lines.empty() && !lines.back().empty()) add.push_back("");
        for (auto& l : splitLines(snip.scriptBlock)) add.push_back(std::move(l));
        lines.insert(lines.end(), add.begin(), add.end());
    }

    // 3. Trigger line after the last existing trigger (or before first script).
    {
        int pos = lastTrigger >= 0 ? lastTrigger + 1 : (firstScript >= 0 ? firstScript
                                                                          : static_cast<int>(lines.size()));
        insertLines(pos, {snip.triggerLine});
    }

    // 4. String defs inside the strings: block.
    if (!snip.stringsBlock.empty()) {
        std::vector<std::string> defs = splitLines(snip.stringsBlock);
        if (stringsHeader >= 0) {
            // The parser ends the strings block at the first blank line, so new
            // defs must be inserted immediately after the `strings:` header
            // (consecutive), not after the block's trailing blank.
            insertLines(stringsHeader + 1, defs);
        } else {
            std::vector<std::string> block = {"", "strings:"};
            block.insert(block.end(), defs.begin(), defs.end());
            int pos = firstScript >= 0 ? firstScript : static_cast<int>(lines.size());
            // Triggers may sit before scripts; put strings before triggers too.
            if (lastTrigger >= 0 && lastTrigger < pos) pos = lastTrigger;
            insertLines(pos, block);
        }
    }

    std::string out;
    for (const auto& l : lines) out += l + "\n";
    return out;
}

}  // namespace mm2

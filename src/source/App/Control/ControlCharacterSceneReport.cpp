#include "App/Control/ControlCharacterScene.h"

#include "json.hpp"

namespace
{
using nlohmann::json;

json Optional(const std::optional<bool>& value)
{
    return value.has_value() ? json(*value) : json(nullptr);
}

json SelectedSlot(const App::Control::CharacterSceneFacts& facts)
{
    if (facts.selectedHero < 0 || facts.selectedHero >= facts.characterSlots)
    {
        return nullptr;
    }
    return facts.selectedHero + 1;
}

json ServerMessage(const App::Control::CharacterSceneFacts& facts)
{
    json result;
    result["visible"] = facts.serverMessageVisible;
    const bool known = facts.serverMessageLines >= 0 &&
                       facts.serverMessageLines <= App::Control::CharacterSceneServerMessageLines &&
                       static_cast<int>(facts.serverMessageTexts.size()) >= facts.serverMessageLines;
    if (!known)
    {
        result["lines"] = nullptr;
        result["texts"] = nullptr;
        return result;
    }
    result["lines"] = facts.serverMessageLines;
    result["texts"] = json(std::vector<std::string>(facts.serverMessageTexts.begin(),
                                                    facts.serverMessageTexts.begin() + facts.serverMessageLines));
    return result;
}

json Pointer(const App::Control::CharacterSceneFacts& facts)
{
    if (!facts.windowPointer.has_value() || !facts.logicalPointer.has_value())
    {
        return nullptr;
    }
    return json::array(
        {(*facts.windowPointer)[0], (*facts.windowPointer)[1], (*facts.logicalPointer)[0], (*facts.logicalPointer)[1]});
}
} // namespace

namespace App::Control
{
std::string CharacterSceneObject(const CharacterSceneFacts& facts)
{
    json result;
    result["version"] = CharacterSceneObservationVersion;
    result["scene"] = "character_list";
    result["roster_received"] = facts.rosterReceived;
    result["selected_slot"] = SelectedSlot(facts);
    result["server_message"] = ServerMessage(facts);
    json windows;
    windows["character_select"] = Optional(facts.characterSelectVisible);
    windows["character_make"] = Optional(facts.characterMakeVisible);
    windows["message"] = Optional(facts.messageVisible);
    windows["system_menu"] = Optional(facts.systemMenuVisible);
    windows["option"] = Optional(facts.optionVisible);
    windows["generic_confirm"] = Optional(facts.genericConfirmVisible);
    windows["generic_menu"] = Optional(facts.genericMenuVisible);
    result["windows"] = std::move(windows);
    result["input_focused"] = facts.inputFocused;
    result["input_idle"] = Optional(facts.inputIdle);
    result["pointer"] = Pointer(facts);
    return result.dump();
}
} // namespace App::Control

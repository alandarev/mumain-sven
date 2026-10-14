#include "App/Control/ControlLoginScene.h"

#include "json.hpp"

namespace
{
using nlohmann::json;

json Optional(const std::optional<bool>& value)
{
    return value.has_value() ? json(*value) : json(nullptr);
}

json Pointer(const App::Control::LoginSceneFacts& facts)
{
    if (!facts.windowPointer.has_value() || !facts.logicalPointer.has_value())
    {
        return nullptr;
    }
    return json::array(
        {(*facts.windowPointer)[0], (*facts.windowPointer)[1], (*facts.logicalPointer)[0], (*facts.logicalPointer)[1]});
}

json LoginForm(const App::Control::LoginSceneFacts& facts)
{
    json form;
    form["account"] = facts.account.has_value() ? json(*facts.account) : json(nullptr);
    form["password_empty"] = Optional(facts.passwordEmpty);
    form["focused_field"] = (facts.focusedField == "account" || facts.focusedField == "password")
                                ? json(facts.focusedField)
                                : json(nullptr);
    return form;
}
} // namespace

namespace App::Control
{
std::string LoginSceneObject(const LoginSceneFacts& facts)
{
    json result;
    result["version"] = LoginSceneObservationVersion;
    result["scene"] = "login";
    result["server_groups"] = facts.serverGroups;
    result["login_form_ready"] = facts.loginFormReady;
    json windows;
    windows["server_select"] = Optional(facts.serverSelectVisible);
    windows["login"] = Optional(facts.loginVisible);
    windows["message"] = Optional(facts.messageVisible);
    windows["system_menu"] = Optional(facts.systemMenuVisible);
    windows["option"] = Optional(facts.optionVisible);
    windows["credit"] = Optional(facts.creditVisible);
    windows["generic_confirm"] = Optional(facts.genericConfirmVisible);
    windows["generic_menu"] = Optional(facts.genericMenuVisible);
    result["windows"] = std::move(windows);
    result["login_form"] = LoginForm(facts);
    result["input_focused"] = facts.inputFocused;
    result["input_idle"] = Optional(facts.inputIdle);
    result["pointer"] = Pointer(facts);
    return result.dump();
}
} // namespace App::Control

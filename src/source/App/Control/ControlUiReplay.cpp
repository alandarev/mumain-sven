#include "stdafx.h"
#include "App/Control/ControlUiReplay.h"

#include "json.hpp"

#include <array>
#include <algorithm>

namespace
{
using nlohmann::json;
constexpr std::array OrdinaryPanels = {"character",   "inventory", "myquest",      "party",
                                       "friend",      "movemap",   "command",      "quick_command",
                                       "window_menu", "help",      "chatinputbox", "muhelper"};
constexpr std::array PassiveWindows = {"mainframe",     "chatlogwindow",       "systemlogwindow", "party_info_window",
                                       "buff_window",   "item_endurance_info", "name_window",     "hero_position_info",
                                       "mu_helper_bar", "slidewindow",         "skill_list",      "hotkey"};

bool Is(const json& object, const char* key, bool value)
{
    return object.contains(key) && object[key].is_boolean() && object[key] == value;
}

template <std::size_t Size> bool Contains(const std::array<const char*, Size>& names, std::string_view name)
{
    return std::find(names.begin(), names.end(), name) != names.end();
}

std::string ModalRefusal(const json& observation, bool allowSystemMenu, bool closingChatInput)
{
    if (!observation.is_object() || observation.value("version", 0) != 2)
        return "unsupported UI observability";
    for (const char* key : {"legacy_popup_active", "message_window_active", "friend_children_active",
                            "picked_item_active", "native_events_pending", "helper_active"})
    {
        if (!Is(observation, key, false))
            return std::string("active or unknown prerequisite: ") + key;
    }
    const bool chatOwnsFocus = closingChatInput && Is(observation, "input_focused", true) &&
                               observation.contains("input_focus_owner") &&
                               observation["input_focus_owner"] == "chatinputbox";
    if (!Is(observation, "input_focused", false) && !chatOwnsFocus)
        return "active or unknown prerequisite: input_focused";
    const bool systemMenu = allowSystemMenu && Is(observation, "system_menu_only", true);
    if (Is(observation, "generic_dialogs_supported", true))
    {
        if (!Is(observation, "messagebox_active", false) || !Is(observation, "generic_confirm_active", false) ||
            !(Is(observation, "generic_menu_active", false) ||
              (systemMenu && Is(observation, "generic_menu_active", true))))
            return "active or unknown modal";
    }
    else if (!Is(observation, "generic_dialogs_supported", false) ||
             !(Is(observation, "messagebox_active", false) ||
               (systemMenu && Is(observation, "messagebox_active", true))))
        return "active or unknown native modal";
    return {};
}

std::string WindowRefusal(const json& windows)
{
    if (!windows.is_array() || windows.empty())
        return "missing registry";
    for (const auto& entry : windows)
    {
        if (!entry.is_object() || !entry.contains("window") || !entry["window"].is_string() ||
            !entry.contains("visible") || !entry["visible"].is_boolean() || !entry.contains("registered") ||
            !entry["registered"].is_boolean())
            return "malformed registry entry";
        if (!entry["visible"].get<bool>())
            continue;
        const auto name = entry["window"].get<std::string>();
        if (!entry["registered"].get<bool>())
            return "visible unregistered window";
        // These mechanisms were checked separately, never inferred from manager visibility.
        if (name == "messagebox" || name == "generic_menu_dialog" || name == "generic_confirm_dialog")
            continue;
        if (!Contains(OrdinaryPanels, name) && !Contains(PassiveWindows, name))
            return "unsupported visible window: " + name;
    }
    return {};
}
std::string TargetRefusal(const json& state, bool show, std::string_view window)
{
    const auto& observation = state["observability"];
    if (window == "system_menu")
        return !show && !Is(observation, "system_menu_only", true) ? "no identified system menu" : "";
    if (show && window == "friend" && !Is(observation, "friend_open_allowed", true))
        return "friends route unavailable";
    const json* target = nullptr;
    for (const auto& entry : state["windows"])
    {
        if (entry["window"] != window)
            continue;
        if (target != nullptr)
            return "ambiguous panel";
        target = &entry;
    }
    if (target == nullptr || !Is(*target, "registered", true))
        return "panel unavailable";
    if (Is(*target, "visible", show))
        return "panel already in requested state";
    return {};
}
} // namespace

namespace App::Control
{
std::string UiReplayRefusal(std::string_view snapshot, bool show, std::string_view window, bool allowSystemMenu)
{
    try
    {
        const auto state = json::parse(snapshot);
        if (!state.is_object() || !state.contains("observability") || !state.contains("windows"))
            return "missing UI observations";
        if (window != "system_menu" && !Contains(OrdinaryPanels, window))
            return "unsupported replay panel";
        const auto& observation = state["observability"];
        if (auto reason = ModalRefusal(observation, allowSystemMenu && window == "system_menu" && !show,
                                       !show && window == "chatinputbox");
            !reason.empty())
            return reason;
        if (auto reason = WindowRefusal(state["windows"]); !reason.empty())
            return reason;
        if (show && (window == "inventory" || window == "character"))
        {
            const auto* key = window == "inventory" ? "inventory_open_effect" : "character_open_effect";
            if (!Is(observation, key, false))
                return "opening may change quest state";
        }
        return TargetRefusal(state, show, window);
    }
    catch (const json::exception&)
    {
        return "malformed UI observations";
    }
}
std::string ExecuteGuardedUi(std::string_view guard, std::string_view currentSnapshot, bool worldReady, bool muted,
                             bool show, std::string_view window, const std::function<std::string()>& action)
{
    if (!worldReady || muted || guard != currentSnapshot)
        return "UI state changed or unavailable";
    if (auto reason = UiReplayRefusal(currentSnapshot, show, window, window == "system_menu" && !show); !reason.empty())
        return reason;
    return action();
}
} // namespace App::Control

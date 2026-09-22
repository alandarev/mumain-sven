#include "stdafx.h"
#include "App/Control/ControlQuickPeer.h"
#include "json.hpp"

#include <cmath>

namespace
{
bool FieldsKnown(const nlohmann::json& state)
{
    for (const char* key :
         {"key", "index", "world", "hero_key", "hero_index", "kind", "type", "subtype", "menu_index", "command_index"})
        if (!state.contains(key) || !state[key].is_number_integer())
            return false;
    for (const char* key : {"id", "hero_id"})
        if (!state.contains(key) || !state[key].is_string() || state[key].get<std::string>().empty())
            return false;
    for (const char* key : {"menu_matches", "duel_watch", "chaos_castle", "cursed_temple", "strife"})
        if (!state.contains(key) || !state[key].is_boolean())
            return false;
    for (const auto& [key, count] : {std::pair{"position", 3},
                                     {"hero_position", 3},
                                     {"gens", 2},
                                     {"geometry", 2},
                                     {"transform", 5},
                                     {"menu_transform", 5},
                                     {"pointer", 4},
                                     {"placement", 2}})
    {
        if (!state.contains(key) || !state[key].is_array() || state[key].size() != static_cast<std::size_t>(count))
            return false;
        for (const auto& value : state[key])
            if (!value.is_number() || !std::isfinite(value.get<double>()))
                return false;
    }
    return true;
}
} // namespace

namespace App::Control
{
std::string QuickPeerRefusal(std::string_view observation, bool show)
{
    try
    {
        const auto state = nlohmann::json::parse(observation);
        if (!state.is_object() || !state.contains("version") || !state["version"].is_number_integer() ||
            state["version"] != QuickPeerObservationVersion)
            return "unsupported quick peer observations";
        if (!FieldsKnown(state))
            return "unknown quick peer fields";
        for (const char* field : {"ready", "allowed", "input_idle", "placement_valid"})
        {
            if (!state.contains(field) || !state[field].is_boolean() || state[field] != true)
                return "quick peer prerequisite unavailable";
        }
        if (!state.contains("menu_visible") || !state["menu_visible"].is_boolean() || state["menu_visible"] == show)
            return "quick peer menu state unavailable";
        if (!show && (!state.contains("menu_name") || !state["menu_name"].is_string() ||
                      state["menu_name"] != state["id"] || !state.contains("menu_position") ||
                      !state["menu_position"].is_array() || state["menu_position"].size() != 2 ||
                      !state["menu_position"][0].is_number_integer() || !state["menu_position"][1].is_number_integer()))
            return "unknown quick menu identity";
        if (!show &&
            (!state.contains("menu_matches") || !state["menu_matches"].is_boolean() || state["menu_matches"] != true))
            return "quick peer menu identity changed";
        return {};
    }
    catch (const nlohmann::json::exception&)
    {
        return "malformed quick peer observations";
    }
}
} // namespace App::Control

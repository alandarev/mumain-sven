#include "stdafx.h"
#include "App/Control/ControlQuickPeer.h"
#include "Core/Input/SyntheticInput.h"
#include "Core/Text/Utf8.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/MainScene.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Core/WindowCommon.h"
#include "UI/Scaling/UITransform.h"
#include "World/MapInfra/MapManager.h"
#include "json.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <limits>

extern int LoadingWorld;

namespace
{
using nlohmann::json;
namespace Windows = mu::ui::window;
constexpr float QuickRange = 300.f;
constexpr int QuickWidth = 112;
constexpr int QuickHeight = 140;
constexpr int PointerOffsetX = 10;
constexpr int PointerOffsetY = 50;
constexpr std::size_t QuickNameCapacity = 32; // CQuickCommandWindow::m_strID includes its terminator.

std::string BoundedName(const wchar_t* name, std::size_t capacity)
{
    const auto* end = std::find(name, name + capacity, L'\0');
    if (end == name || end == name + capacity)
        return {};
    return Core::Text::ToUtf8(std::wstring(name, end).c_str());
}

bool FinitePosition(const CHARACTER& character)
{
    return std::isfinite(character.Object.Position[0]) && std::isfinite(character.Object.Position[1]) &&
           std::isfinite(character.Object.Position[2]);
}

bool InputIdle()
{
    if (!SDL_WasInit(SDL_INIT_EVENTS) || !Core::Input::Synthetic::IsIdle() ||
        SDL_HasEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST) || SDL_GetMouseState(nullptr, nullptr) != 0 ||
        g_pNewKeyInput->HasPendingInput() || MouseLButton || MouseLButtonPop || MouseLButtonPush || MouseRButton ||
        MouseRButtonPop || MouseRButtonPush || MouseLButtonDBClick)
        return false;
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    return keys != nullptr && count > 0 && std::none_of(keys, keys + count, [](bool held) { return held; });
}
void ObserveMenu(json& state, int index, std::string_view id)
{
    const bool visible = g_pNewUISystem->IsVisible(Windows::INTERFACE_QUICK_COMMAND);
    state["menu_visible"] = visible;
    state["menu_matches"] = visible && g_pQuickCommand->SelectedCharacterIndex() == index &&
                            BoundedName(g_pQuickCommand->TargetName(), QuickNameCapacity) == id;
    state["menu_index"] = g_pQuickCommand->SelectedCharacterIndex();
    state["command_index"] = g_pQuickCommand->SelectedCommandIndex();
    if (visible)
    {
        state["menu_name"] = g_pQuickCommand->SelectedCharacterIndex() >= 0
                                 ? json(BoundedName(g_pQuickCommand->TargetName(), QuickNameCapacity))
                                 : json(nullptr);
        const auto position = g_pQuickCommand->Position();
        state["menu_position"] = {position.x, position.y};
    }
}

bool KnownTransform(const UI::Scaling::Transform& transform)
{
    return std::isfinite(transform.scaleX) && std::isfinite(transform.scaleY) && std::isfinite(transform.offsetX) &&
           std::isfinite(transform.offsetY) && std::isfinite(transform.typographyScale) && transform.scaleX > 0 &&
           transform.scaleY > 0 && transform.typographyScale > 0;
}

bool ObservePlacement(json& state)
{
    const auto transform = UI::Scaling::TransformForLayout(g_pNewUIHotKey->GetLayoutMode(), WindowWidth, WindowHeight);
    const float logicalX = UI::Scaling::LogicalX(transform, g_fWindowMouseX);
    const float logicalY = UI::Scaling::LogicalY(transform, g_fWindowMouseY);
    if (WindowWidth <= 0 || WindowHeight <= 0 || WindowWidth > std::numeric_limits<int>::max() / 2 ||
        WindowHeight > std::numeric_limits<int>::max() / 2 || !std::isfinite(logicalX) || !std::isfinite(logicalY) ||
        logicalX < 0 || logicalY < 0 || logicalX > WindowWidth || logicalY > WindowHeight || !KnownTransform(transform))
        return false;
    const int x = static_cast<int>(std::floor(logicalX)) + PointerOffsetX;
    const int y = std::max(static_cast<int>(std::floor(logicalY)) - PointerOffsetY, 0);
    state["geometry"] = {WindowWidth, WindowHeight};
    state["transform"] = {transform.scaleX, transform.scaleY, transform.offsetX, transform.offsetY,
                          transform.typographyScale};
    state["pointer"] = {g_fWindowMouseX, g_fWindowMouseY, MouseX, MouseY};
    state["placement"] = {x, y};
    const auto menu = UI::Scaling::TransformForLayout(g_pQuickCommand->GetLayoutMode(), WindowWidth, WindowHeight);
    state["menu_transform"] = {menu.scaleX, menu.scaleY, menu.offsetX, menu.offsetY, menu.typographyScale};
    state["placement_valid"] = KnownTransform(menu) && UI::Scaling::PositionX(menu, x) >= 0 &&
                               UI::Scaling::PositionY(menu, y) >= 0 &&
                               UI::Scaling::PositionX(menu, x + QuickWidth) <= WindowWidth &&
                               UI::Scaling::PositionY(menu, y + QuickHeight) <= WindowHeight;
    return true;
}
} // namespace

namespace App::Control
{
int ResolveQuickPeer(std::span<CHARACTER> storage, const CHARACTER* hero, int key, std::string_view id)
{
    if (storage.empty() || storage.size() > MAX_CHARACTERS_CLIENT || hero == nullptr || key < 0 ||
        key > std::numeric_limits<SHORT>::max() || id.empty())
        return -1;
    bool heroFound = false;
    int found = -1;
    for (std::size_t index = 0; index < storage.size(); ++index)
    {
        const auto& candidate = storage[index];
        heroFound = heroFound || &candidate == hero;
        if (!candidate.Object.Live || candidate.Key != key)
            continue;
        // Duplicate live keys are ambiguous even if only one name matches.
        if (found != -1 || &candidate == hero || BoundedName(candidate.ID, QuickNameCapacity) != id)
            return -1;
        found = static_cast<int>(index);
    }
    return heroFound ? found : -1;
}

bool QuickPeerAllowed(CHARACTER& hero, const CHARACTER& peer)
{
    if (!hero.Object.Live || !peer.Object.Live || !FinitePosition(hero) || !FinitePosition(peer))
        return false;
    const auto& object = peer.Object;
    const double dx = static_cast<double>(object.Position[0]) - hero.Object.Position[0];
    const double dy = static_cast<double>(object.Position[1]) - hero.Object.Position[1];
    return object.Kind == KIND_PLAYER && object.Type == MODEL_PLAYER && object.SubType != MODEL_XMAS_EVENT_CHA_DEER &&
           object.SubType != MODEL_XMAS_EVENT_CHA_SNOWMAN && object.SubType != MODEL_XMAS_EVENT_CHA_SSANTA &&
           dx * dx + dy * dy < QuickRange * QuickRange && !g_isCharacterBuff((&hero.Object), eBuff_DuelWatch) &&
           !gMapManager.InChaosCastle() && !gMapManager.IsCursedTemple() &&
           (!IsStrifeMap(gMapManager.WorldActive) || hero.m_byGensInfluence == peer.m_byGensInfluence);
}

std::string QuickPeerObservation(int key, std::string_view id)
{
    json state = {{"version", QuickPeerObservationVersion}, {"key", key}, {"id", id}, {"ready", false}};
    const auto storage = WorldCharacterStorage();
    const int index = ResolveQuickPeer(storage, Hero, key, id);
    if (SceneFlag != MAIN_SCENE || LoadingWorld >= 30 || index < 0 || !g_pNewUISystem || !g_pNewUIMng ||
        !g_pQuickCommand || !g_pNewUIHotKey ||
        g_pNewUIMng->FindUIObj(Windows::INTERFACE_QUICK_COMMAND) != g_pQuickCommand ||
        g_pNewUIMng->FindUIObj(Windows::INTERFACE_HOTKEY) != g_pNewUIHotKey)
        return state.dump();
    const auto& peer = storage[index];
    const auto& object = peer.Object;
    if (!Hero->Object.Live || Hero->Object.Kind != KIND_PLAYER || Hero->Object.Type != MODEL_PLAYER ||
        BoundedName(Hero->ID, std::size(Hero->ID)).empty() || !FinitePosition(*Hero) || !FinitePosition(peer))
        return state.dump();
    state["index"] = index;
    state["world"] = gMapManager.WorldActive;
    state["hero_key"] = Hero->Key;
    state["hero_index"] = Hero - storage.data();
    state["hero_id"] = BoundedName(Hero->ID, std::size(Hero->ID));
    state["hero_position"] = {Hero->Object.Position[0], Hero->Object.Position[1], Hero->Object.Position[2]};
    state["position"] = {object.Position[0], object.Position[1], object.Position[2]};
    state["kind"] = object.Kind;
    state["type"] = object.Type;
    state["subtype"] = object.SubType;
    state["gens"] = {Hero->m_byGensInfluence, peer.m_byGensInfluence};
    state["allowed"] = QuickPeerAllowed(*Hero, peer);
    state["duel_watch"] = static_cast<bool>(g_isCharacterBuff((&Hero->Object), eBuff_DuelWatch));
    state["chaos_castle"] = gMapManager.InChaosCastle();
    state["cursed_temple"] = gMapManager.IsCursedTemple();
    state["strife"] = IsStrifeMap(gMapManager.WorldActive);
    state["input_idle"] = InputIdle();
    ObserveMenu(state, index, id);
    state["ready"] = ObservePlacement(state);
    return state.dump();
}

std::string ApplyQuickPeer(int key, std::string_view id, bool show, std::string_view expectedObservation)
{
    const auto current = QuickPeerObservation(key, id);
    if (current != expectedObservation)
        return "quick peer changed";
    if (auto reason = QuickPeerRefusal(current, show); !reason.empty())
        return reason;
    const auto storage = WorldCharacterStorage();
    const int index = ResolveQuickPeer(storage, Hero, key, id);
    if (index < 0)
        return "quick peer unavailable";
    if (!show)
        g_pQuickCommand->CloseQuickCommand();
    else
    {
        const auto state = json::parse(current);
        const std::wstring name(storage[index].ID);
        g_pQuickCommand->OpenQuickCommand(name.c_str(), index, state["placement"][0].get<int>(),
                                          state["placement"][1].get<int>());
    }
    return {};
}
} // namespace App::Control

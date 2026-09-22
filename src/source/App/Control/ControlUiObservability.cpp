#include "stdafx.h"
#include "App/Control/ControlUiObservability.h"
#include "App/Control/ControlUiObservation.h"
#include "GameLogic/Quests/QuestMng.h"
#include "MUHelper/MuHelper.h"
#include "Scenes/MainScene.h"
#include "World/GameMaps/GMCrywolf1st.h"

#include "UI/Legacy/UIControls.h"
#include "UI/Windows/MsgWin.h"

#include "UI/Legacy/UIManager.h"
#include "UI/Legacy/UIMng.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Dialogs/NewUIMessageBox.h"
#include "UI/NewUI/Dialogs/NewUICustomMessageBox.h"
#include "UI/NewUI/Inventory/NewUIInventoryCtrl.h"
#include "UI/NewUI/Party/NewUIFriendWindow.h"

#include "json.hpp"

extern int LoadingWorld;

namespace
{
using nlohmann::json;
namespace Windows = SEASON3B;

using EventRegistry = Windows::CNewUIManager;
using CryWolf = Windows::CNewUICryWolf;
using SiegeWarfare = Windows::CNewUISiegeWarfare;
constexpr DWORD InventoryTutorialQuest = 0x1000F;
constexpr DWORD CharacterTutorialQuest = 0x10009;

json ActivityValue(std::optional<bool> activity)
{
    return activity.has_value() ? json(*activity) : json(nullptr);
}

bool Registered(DWORD key, const Windows::CNewUIObj* expected)
{
    return expected != nullptr && g_pNewUISystem != nullptr && g_pNewUIMng != nullptr &&
           g_pNewUIMng->FindUIObj(key) == expected;
}

void ObserveNativeDialogs(json& result)
{
    result["messagebox_active"] = nullptr;
    result["native_events_pending"] = nullptr;
    result["system_menu_only"] = nullptr;
    if (g_pNewUISystem == nullptr || !Registered(Windows::INTERFACE_MESSAGEBOX, g_MessageBox))
        return;
    result["messagebox_active"] = !g_MessageBox->IsEmpty();
    result["native_events_pending"] = g_MessageBox->HasPendingEvents();
    result["system_menu_only"] = g_MessageBox->IsOnlySystemMenu();
}

void ObserveFriendChildren(json& result)
{
    result["friend_children_active"] = nullptr;
    if (g_pNewUISystem == nullptr || g_pNewUIMng == nullptr)
        return;
    const auto* friends =
        dynamic_cast<const Windows::CNewUIFriendWindow*>(g_pNewUIMng->FindUIObj(Windows::INTERFACE_FRIEND));
    if (friends == nullptr)
        return;
    result["friend_children_active"] = ActivityValue(App::Control::ObserveUiActivity(
        friends->GetWindowManager(), true, [](const auto& manager) { return manager.HasReplayBlockingChildren(); }));
}

void ObserveEventManagers(json& result)
{
    const bool worldReady = g_pNewUISystem != nullptr && SceneFlag == MAIN_SCENE && LoadingWorld < 30;
    auto* cryWolf = g_pNewUISystem != nullptr ? g_pCryWolfInterface : nullptr;
    auto* siegeWarfare = g_pNewUISystem != nullptr ? g_pSiegeWarfare : nullptr;
    result["crywolf_event_active"] = ActivityValue(App::Control::ObserveCryWolfEvent(g_pNewUIMng, cryWolf, worldReady));
    result["siegewarfare_child_active"] =
        ActivityValue(App::Control::ObserveSiegeWarfareChild(g_pNewUIMng, siegeWarfare, worldReady));
}

void ObserveVersionDialogs(json& result)
{
    result["message_window_active"] = CUIMng::Instance().m_MsgWin.IsShow();
    result["picked_item_active"] = Windows::CNewUIInventoryCtrl::GetPickedItem() != nullptr;
    result["generic_dialogs_supported"] = false;
    result["generic_confirm_active"] = nullptr;
    result["generic_menu_active"] = nullptr;
}
} // namespace

namespace App::Control
{
std::optional<bool> ObserveCryWolfEvent(EventRegistry* registry, const CryWolf* expected, bool worldReady)
{
    if (!worldReady || registry == nullptr || expected == nullptr ||
        registry->FindUIObj(Windows::INTERFACE_CRYWOLF) != expected)
        return std::nullopt;
    // Even an apparently empty event UI can create a dialog in its render path.
    return M34CryWolf1st::IsCyrWolf1st();
}

std::optional<bool> ObserveSiegeWarfareChild(EventRegistry* registry, SiegeWarfare* expected, bool worldReady)
{
    if (!worldReady || registry == nullptr || expected == nullptr ||
        registry->FindUIObj(Windows::INTERFACE_SIEGEWARFARE) != expected)
        return std::nullopt;
    // Input and update delegate to this child even outside the castle map.
    return expected->GetBase() != nullptr;
}

std::string UiObservabilityObject()
{
    json result;
    result["version"] = UiObservabilityVersion;
    result["helper_active"] = MUHelper::g_MuHelper.IsActive();
    result["input_focused"] = CUITextInputBox::IsAnyInputBoxFocused();
    result["input_focus_owner"] = nullptr;
    if (Registered(Windows::INTERFACE_CHATINPUTBOX, g_pChatInputBox) && g_pChatInputBox->OwnsFocusedInput())
        result["input_focus_owner"] = "chatinputbox";
    constexpr int FriendsMinimumLevel = 6;
    result["friend_open_allowed"] =
        CharacterAttribute != nullptr ? json(CharacterAttribute->Level >= FriendsMinimumLevel) : json(nullptr);
    ObserveNativeDialogs(result);
    ObserveFriendChildren(result);
    ObserveVersionDialogs(result);
    ObserveEventManagers(result);
    result["inventory_open_effect"] = g_QuestMng.IsIndexInCurQuestIndexList(InventoryTutorialQuest) &&
                                      g_QuestMng.IsEPRequestRewardState(InventoryTutorialQuest);
    result["character_open_effect"] = g_QuestMng.IsIndexInCurQuestIndexList(CharacterTutorialQuest) &&
                                      g_QuestMng.IsEPRequestRewardState(CharacterTutorialQuest);
    result["legacy_popup_active"] =
        ActivityValue(ObserveUiActivity(g_pUIPopup, true, [](auto& popup) { return popup.GetPopupID() != 0; }));
    return result.dump();
}
} // namespace App::Control

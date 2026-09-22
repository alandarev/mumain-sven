#include "stdafx.h"
#include "App/Control/ControlUiObservability.h"
#include "App/Control/ControlUiObservation.h"
#include "GameLogic/Quests/QuestMng.h"
#include "MUHelper/MuHelper.h"
#include "UI/Widgets/UIControls.h"
#include "UI/Windows/MsgWin.h"

#if __has_include("UI/Core/WindowSystem.h")
#define MU_OBSERVABILITY_RMLUI 1
#include "UI/Core/UIManager.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include "UI/Dialogs/MessageBox.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Party/FriendWindow.h"
#else
#define MU_OBSERVABILITY_RMLUI 0
#include "UI/Legacy/UIManager.h"
#include "UI/Legacy/UIMng.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Dialogs/NewUIMessageBox.h"
#include "UI/NewUI/Dialogs/NewUICustomMessageBox.h"
#include "UI/NewUI/Inventory/NewUIInventoryCtrl.h"
#include "UI/NewUI/Party/NewUIFriendWindow.h"
#endif

#include "json.hpp"

namespace
{
using nlohmann::json;
#if MU_OBSERVABILITY_RMLUI
namespace Windows = mu::ui::window;
#else
namespace Windows = SEASON3B;
#endif

constexpr int ObservabilityVersion = 2;
constexpr DWORD InventoryTutorialQuest = 0x1000F;
constexpr DWORD CharacterTutorialQuest = 0x10009;

json ActivityValue(std::optional<bool> activity)
{
    return activity.has_value() ? json(*activity) : json(nullptr);
}

bool Registered(DWORD key, const Windows::CObject* expected)
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
#if !MU_OBSERVABILITY_RMLUI
    const auto& boxes = g_MessageBox->GetMessageBoxes();
    result["system_menu_only"] = boxes.size() == 1 &&
                                 dynamic_cast<const Windows::CSystemMenuMsgBox*>(boxes.front()) != nullptr &&
                                 !g_MessageBox->HasPendingEvents();
#endif
}

void ObserveFriendChildren(json& result)
{
    result["friend_children_active"] = nullptr;
    if (g_pNewUISystem == nullptr || g_pNewUIMng == nullptr)
        return;
#if MU_OBSERVABILITY_RMLUI
    const auto* friends =
        dynamic_cast<const Windows::CFriendWindow*>(g_pNewUIMng->FindUIObj(Windows::INTERFACE_FRIEND));
#else
    const auto* friends =
        dynamic_cast<const Windows::CNewUIFriendWindow*>(g_pNewUIMng->FindUIObj(Windows::INTERFACE_FRIEND));
#endif
    if (friends == nullptr)
        return;
    result["friend_children_active"] = ActivityValue(App::Control::ObserveUiActivity(
        friends->GetWindowManager(), true, [](const auto& manager) { return manager.HasReplayBlockingChildren(); }));
}

#if MU_OBSERVABILITY_RMLUI
json RegisteredActivity(DWORD key)
{
    if (g_pNewUISystem == nullptr || g_pNewUIMng == nullptr || g_pNewUIMng->FindUIObj(key) == nullptr)
        return nullptr;
    return g_pNewUIMng->IsInterfaceVisible(key);
}
#endif

void ObserveVersionDialogs(json& result)
{
#if MU_OBSERVABILITY_RMLUI
    result["message_window_active"] = g_MsgWin.IsVisible();
    result["picked_item_active"] = Windows::CInventoryCtrl::GetPickedItem() != nullptr;
    result["system_menu_only"] = ActivityValue(App::Control::ObserveUiActivity(
        Windows::g_pGenericMenuDialog,
        Registered(Windows::INTERFACE_GENERIC_MENU_DIALOG, Windows::g_pGenericMenuDialog),
        [](const auto& menu) { return menu.IsOnlySystemMenu(); }));
    result["generic_dialogs_supported"] = true;
    result["generic_confirm_active"] = RegisteredActivity(Windows::INTERFACE_GENERIC_CONFIRM_DIALOG);
    result["generic_menu_active"] = RegisteredActivity(Windows::INTERFACE_GENERIC_MENU_DIALOG);
#else
    result["message_window_active"] = CUIMng::Instance().m_MsgWin.IsShow();
    result["picked_item_active"] = Windows::CNewUIInventoryCtrl::GetPickedItem() != nullptr;
    result["generic_dialogs_supported"] = false;
    result["generic_confirm_active"] = nullptr;
    result["generic_menu_active"] = nullptr;
#endif
}
} // namespace

namespace App::Control
{
std::string UiObservabilityObject()
{
    json result;
    result["version"] = ObservabilityVersion;
    result["helper_active"] = MUHelper::g_MuHelper.IsActive();
    result["input_focused"] = CUITextInputBox::IsAnyInputBoxFocused();
    result["input_focus_owner"] = nullptr;
#if MU_OBSERVABILITY_RMLUI
    if (Registered(Windows::INTERFACE_CHATINPUTBOX, g_pChatInputBox) && g_pChatInputBox->OwnsFocusedInput())
        result["input_focus_owner"] = "chatinputbox";
#endif
    constexpr int FriendsMinimumLevel = 6;
    result["friend_open_allowed"] =
        CharacterAttribute != nullptr ? json(CharacterAttribute->Level >= FriendsMinimumLevel) : json(nullptr);
    ObserveNativeDialogs(result);
    ObserveFriendChildren(result);
    ObserveVersionDialogs(result);
    result["inventory_open_effect"] = g_QuestMng.IsIndexInCurQuestIndexList(InventoryTutorialQuest) &&
                                      g_QuestMng.IsEPRequestRewardState(InventoryTutorialQuest);
    result["character_open_effect"] = g_QuestMng.IsIndexInCurQuestIndexList(CharacterTutorialQuest) &&
                                      g_QuestMng.IsEPRequestRewardState(CharacterTutorialQuest);
    result["legacy_popup_active"] =
        ActivityValue(ObserveUiActivity(g_pUIPopup, true, [](auto& popup) { return popup.GetPopupID() != 0; }));
    return result.dump();
}
} // namespace App::Control

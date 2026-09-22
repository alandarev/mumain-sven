#include "stdafx.h"
#include "App/Control/ControlUiObservability.h"
#include "App/Control/ControlUiObservation.h"

#if __has_include("UI/Core/WindowSystem.h")
#define MU_OBSERVABILITY_RMLUI 1
#include "UI/Core/UIManager.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/MessageBox.h"
#else
#define MU_OBSERVABILITY_RMLUI 0
#include "UI/Legacy/UIManager.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Dialogs/NewUIMessageBox.h"
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

constexpr int ObservabilityVersion = 1;

json ActivityValue(std::optional<bool> activity)
{
    return activity.has_value() ? json(*activity) : json(nullptr);
}

json MessageBoxActivity()
{
    if (g_pNewUISystem == nullptr || g_pNewUIMng == nullptr)
    {
        return nullptr;
    }
    // Do not treat an absent (or different) registered object as an empty stack.
    auto* registered = g_pNewUIMng->FindUIObj(Windows::INTERFACE_MESSAGEBOX);
    return ActivityValue(App::Control::ObserveUiActivity(g_MessageBox,
                                                         registered != nullptr && registered == g_MessageBox,
                                                         [](auto& manager) { return !manager.IsEmpty(); }));
}

#if MU_OBSERVABILITY_RMLUI
json RegisteredActivity(DWORD key)
{
    if (g_pNewUISystem == nullptr || g_pNewUIMng == nullptr || g_pNewUIMng->FindUIObj(key) == nullptr)
    {
        return nullptr;
    }
    return g_pNewUIMng->IsInterfaceVisible(key);
}
#endif
} // namespace

namespace App::Control
{
std::string UiObservabilityObject()
{
    json result;
    result["version"] = ObservabilityVersion;
    result["messagebox_active"] = MessageBoxActivity();
    result["legacy_popup_active"] =
        ActivityValue(ObserveUiActivity(g_pUIPopup, true, [](auto& popup) { return popup.GetPopupID() != 0; }));
#if MU_OBSERVABILITY_RMLUI
    result["generic_dialogs_supported"] = true;
    result["generic_confirm_active"] = RegisteredActivity(Windows::INTERFACE_GENERIC_CONFIRM_DIALOG);
    result["generic_menu_active"] = RegisteredActivity(Windows::INTERFACE_GENERIC_MENU_DIALOG);
#else
    result["generic_dialogs_supported"] = false;
    result["generic_confirm_active"] = nullptr;
    result["generic_menu_active"] = nullptr;
#endif
    return result.dump();
}
} // namespace App::Control

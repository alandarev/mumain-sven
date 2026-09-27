#include "stdafx.h"
#include "App/Control/ControlLoginScene.h"

#include "App/Control/ControlQuickPeer.h"
#include "Network/Server/ServerListManager.h"
#include "Network/Server/WSclient.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Widgets/UIControls.h"
#include "UI/Windows/CreditWin.h"
#include "UI/Windows/LoginWin.h"
#include "UI/Windows/MsgWin.h"
#include "UI/Windows/ServerSelWin.h"
#include "UI/Windows/SysMenuWin.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include <string>

namespace
{
// login.rml's two stock <input> fields; the login window keeps its document
// private, so the fields are found in the context by their ids.
constexpr const char* AccountFieldId = "input_account";
constexpr const char* PasswordFieldId = "input_password";

Rml::ElementFormControl* LoginField(Rml::Context& context, const char* id)
{
    for (int index = 0; index < context.GetNumDocuments(); ++index)
    {
        Rml::ElementDocument* document = context.GetDocument(index);
        if (document == nullptr)
        {
            continue;
        }
        if (Rml::Element* element = document->GetElementById(id))
        {
            return rmlui_dynamic_cast<Rml::ElementFormControl*>(element);
        }
    }
    return nullptr;
}

void ObserveLoginForm(App::Control::LoginSceneFacts& facts)
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (context == nullptr)
    {
        return;
    }
    if (Rml::ElementFormControl* account = LoginField(*context, AccountFieldId))
    {
        facts.account = account->GetValue();
    }
    if (Rml::ElementFormControl* password = LoginField(*context, PasswordFieldId))
    {
        facts.passwordEmpty = password->GetValue().empty();
    }
    if (Rml::Element* focused = context->GetFocusElement())
    {
        if (focused->GetId() == AccountFieldId)
        {
            facts.focusedField = "account";
        }
        else if (focused->GetId() == PasswordFieldId)
        {
            facts.focusedField = "password";
        }
    }
}
} // namespace

namespace App::Control
{
LoginSceneFacts ObserveLoginScene()
{
    LoginSceneFacts facts;
    facts.serverGroups = g_ServerListManager != nullptr ? g_ServerListManager->GetServerGroupSize() : 0;
    facts.loginFormReady = CurrentProtocolState == RECEIVE_JOIN_SERVER_SUCCESS;
    facts.serverSelectVisible = g_ServerSelWin.IsVisible();
    facts.loginVisible = g_LoginWin.IsVisible();
    facts.messageVisible = g_MsgWin.IsVisible();
    facts.systemMenuVisible = g_SysMenuWin.IsVisible();
    facts.creditVisible = g_CreditWin.IsVisible();
    if (g_pNewUISystem != nullptr)
    {
        facts.optionVisible = g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_OPTION);
        facts.genericConfirmVisible = g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_GENERIC_CONFIRM_DIALOG);
        facts.genericMenuVisible = g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_GENERIC_MENU_DIALOG);
    }
    ObserveLoginForm(facts);
    // Ported windows type into RmlUi <input>s, which the native focus does not see.
    facts.inputFocused = CUITextInputBox::IsAnyInputBoxFocused() || RmlUiRuntime::Instance().IsTextInputActive();
    facts.inputIdle = ControlInputIdle();
    facts.windowPointer = std::array<float, 2>{g_fWindowMouseX, g_fWindowMouseY};
    facts.logicalPointer = std::array<int, 2>{MouseX, MouseY};
    return facts;
}
} // namespace App::Control

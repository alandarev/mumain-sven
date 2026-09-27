#include "stdafx.h"
#include "App/Control/ControlLoginScene.h"

#include "App/Control/ControlQuickPeer.h"
#include "Core/Text/Utf8.h"
#include "Network/Server/ServerListManager.h"
#include "Network/Server/WSclient.h"
#include "UI/Legacy/UIControls.h"
#include "UI/Legacy/UIMng.h"

#include <algorithm>
#include <iterator>
#include <string>

namespace
{
// CLoginWin hands out its two input boxes; of the password box only whether
// it holds any text is reported (the copy is wiped at once).
void ObserveLoginForm(App::Control::LoginSceneFacts& facts, CLoginWin& login)
{
    CUITextInputBox* account = login.GetUsernameInputBox();
    CUITextInputBox* password = login.GetPasswordInputBox();
    if (account != nullptr)
    {
        wchar_t text[MAX_USERNAME_SIZE + 1] = {};
        account->GetText(text, MAX_USERNAME_SIZE + 1);
        facts.account = Core::Text::ToUtf8(text);
        if (account->HaveFocus())
        {
            facts.focusedField = "account";
        }
    }
    if (password != nullptr)
    {
        wchar_t text[MAX_PASSWORD_SIZE + 1] = {};
        password->GetText(text, MAX_PASSWORD_SIZE + 1);
        facts.passwordEmpty = text[0] == L'\0';
        std::fill(std::begin(text), std::end(text), L'\0');
        if (password->HaveFocus())
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
    CUIMng& manager = CUIMng::Instance();
    facts.serverSelectVisible = manager.m_ServerSelWin.IsShow();
    facts.loginVisible = manager.m_LoginWin.IsShow();
    facts.messageVisible = manager.m_MsgWin.IsShow();
    facts.systemMenuVisible = manager.m_SysMenuWin.IsShow();
    facts.optionVisible = manager.m_OptionWin.IsShow();
    facts.creditVisible = manager.m_CreditWin.IsShow();
    ObserveLoginForm(facts, manager.m_LoginWin);
    facts.inputFocused = CUITextInputBox::IsAnyInputBoxFocused();
    facts.inputIdle = ControlInputIdle();
    facts.windowPointer = std::array<float, 2>{g_fWindowMouseX, g_fWindowMouseY};
    facts.logicalPointer = std::array<int, 2>{MouseX, MouseY};
    return facts;
}
} // namespace App::Control

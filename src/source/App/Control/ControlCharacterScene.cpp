#include "stdafx.h"
#include "App/Control/ControlCharacterScene.h"

#include "App/Control/ControlQuickPeer.h"
#include "Core/Text/Utf8.h"
#include "Network/Server/WSclient.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/SceneCommon.h"
#include "UI/Windows/ServerMsgWin.h"

#if __has_include("UI/Core/WindowSystem.h")
#define MU_CHARACTER_SCENE_RMLUI 1
#include "Character/CharMakeWin.h"
#include "Character/CharSelMainWin.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Widgets/UIControls.h"
#include "UI/Windows/MsgWin.h"
#include "UI/Windows/SysMenuWin.h"
#else
#define MU_CHARACTER_SCENE_RMLUI 0
#include "UI/Legacy/UIControls.h"
#include "UI/Legacy/UIMng.h"
#endif

#include <cwchar>
#include <string>

namespace
{
// CServerMsgWin keeps its lines protected and has no reader; this derived
// view only names them, so the window's own code stays untouched.
class ServerMessageLines : public CServerMsgWin
{
public:
    static int Count(const CServerMsgWin& window)
    {
        return window.*(&ServerMessageLines::m_nMsgLine);
    }

    static std::string Text(const CServerMsgWin& window, int line)
    {
        const wchar_t* stored = (window.*(&ServerMessageLines::m_aszMsg))[line];
        return Core::Text::ToUtf8(std::wstring(stored, wcsnlen(stored, SMW_MSG_ROW_MAX)).c_str());
    }
};

#if MU_CHARACTER_SCENE_RMLUI
CServerMsgWin& ServerMessageWindow()
{
    return g_ServerMsgWin;
}

bool ServerMessageVisible()
{
    return g_ServerMsgWin.IsVisible();
}

void ObserveSceneWindows(App::Control::CharacterSceneFacts& facts)
{
    facts.characterSelectVisible = g_CharSelMainWin.IsVisible();
    facts.characterMakeVisible = g_CharMakeWin.IsVisible();
    facts.messageVisible = g_MsgWin.IsVisible();
    facts.systemMenuVisible = g_SysMenuWin.IsVisible();
    if (g_pNewUISystem != nullptr)
    {
        facts.optionVisible = g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_OPTION);
        facts.genericConfirmVisible = g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_GENERIC_CONFIRM_DIALOG);
        facts.genericMenuVisible = g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_GENERIC_MENU_DIALOG);
    }
    // Ported windows type into RmlUi <input>s, which the native focus does not see.
    facts.inputFocused = CUITextInputBox::IsAnyInputBoxFocused() || RmlUiRuntime::Instance().IsTextInputActive();
}
#else
CServerMsgWin& ServerMessageWindow()
{
    return CUIMng::Instance().m_ServerMsgWin;
}

bool ServerMessageVisible()
{
    return CUIMng::Instance().m_ServerMsgWin.IsShow();
}

void ObserveSceneWindows(App::Control::CharacterSceneFacts& facts)
{
    CUIMng& manager = CUIMng::Instance();
    facts.characterSelectVisible = manager.m_CharSelMainWin.IsShow();
    facts.characterMakeVisible = manager.m_CharMakeWin.IsShow();
    facts.messageVisible = manager.m_MsgWin.IsShow();
    facts.systemMenuVisible = manager.m_SysMenuWin.IsShow();
    facts.optionVisible = manager.m_OptionWin.IsShow();
    facts.inputFocused = CUITextInputBox::IsAnyInputBoxFocused();
}
#endif
} // namespace

namespace App::Control
{
CharacterSceneFacts ObserveCharacterScene()
{
    CharacterSceneFacts facts;
    facts.rosterReceived = CurrentProtocolState == RECEIVE_CHARACTERS_LIST;
    facts.selectedHero = SelectedHero;
    facts.characterSlots = MAX_CHARACTERS_PER_ACCOUNT;

    const CServerMsgWin& serverMessage = ServerMessageWindow();
    facts.serverMessageVisible = ServerMessageVisible();
    facts.serverMessageLines = ServerMessageLines::Count(serverMessage);
    if (facts.serverMessageLines >= 0 && facts.serverMessageLines <= CharacterSceneServerMessageLines)
    {
        for (int line = 0; line < facts.serverMessageLines; ++line)
        {
            facts.serverMessageTexts.push_back(ServerMessageLines::Text(serverMessage, line));
        }
    }

    ObserveSceneWindows(facts);
    facts.inputIdle = ControlInputIdle();
    facts.windowPointer = std::array<float, 2>{g_fWindowMouseX, g_fWindowMouseY};
    facts.logicalPointer = std::array<int, 2>{MouseX, MouseY};
    return facts;
}
} // namespace App::Control

#pragma once

#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Combat/NewUISiegeWarObserver.h"
#include "UI/Legacy/UIControls.h"
#include "Scenes/MainScene.h"
#include "Core/Time/FrameTimerScheduler.h"
#include "UI/NewUI/Dialogs/NewUIMessageBox.h"
#include "UI/Legacy/UIManager.h"
#include "UI/Windows/MsgWin.h"
#include "UI/Legacy/UIMng.h"
#include "NativeAssetBoundary.h"

extern int LoadingWorld;

namespace SEASON3B
{
// Test-only ownership bridge. No renderer/texture Create, gameplay input or
// network processing runs. The registered objects and observation methods are real.
class UiLifecycleFixture
{
    // First constructed, last destroyed: verify even after all UI destructors.
    NativeAssetBoundary m_assets;

public:
    UiLifecycleFixture()
        : m_scene(SceneFlag), m_loading(LoadingWorld), m_savedPopup(g_pUIPopup),
          m_savedMessageVisible(CUIMng::Instance().m_MsgWin.IsShow())
    {
        auto* system = CNewUISystem::GetInstance();
        system->m_pNewUIMng = &registry;
        system->m_pNewSiegeWarfare = &siege;
        system->m_pNewChatInputBox = &chat;
        system->m_pNewQuickCommandWindow = &quick;
        system->m_pNewUIHotKey = &hotkey;
        registry.AddUIObj(INTERFACE_SIEGEWARFARE, &siege);
        registry.AddUIObj(INTERFACE_CHATINPUTBOX, &chat);
        registry.AddUIObj(INTERFACE_QUICK_COMMAND, &quick);
        registry.AddUIObj(INTERFACE_HOTKEY, &hotkey);
        chat.m_pChatInputBox = new CUITextInputBox;
        chat.m_pWhsprIDInputBox = new CUITextInputBox;
        quick.SetID(L"Peer");
        quick.Show(false);
        SceneFlag = MAIN_SCENE;
        LoadingWorld = 0;
    }

    ~UiLifecycleFixture()
    {
        chat.m_pChatInputBox->SetState(UISTATE_HIDE);
        chat.m_pWhsprIDInputBox->SetState(UISTATE_HIDE);
        if (m_modalPrerequisites)
        {
            friends.Release();
            Core::Time::FrameTimerScheduler::Instance().Kill(CHATCONNECT_TIMER);
            CNewUISystem::GetInstance()->m_pNewCryWolfInterface = nullptr;
        }
        auto* system = CNewUISystem::GetInstance();
        system->m_pNewUIMng = nullptr;
        system->m_pNewSiegeWarfare = nullptr;
        system->m_pNewChatInputBox = nullptr;
        system->m_pNewQuickCommandWindow = nullptr;
        system->m_pNewUIHotKey = nullptr;
        registry.RemoveAllUIObjs();
        SceneFlag = m_scene;
        LoadingWorld = m_loading;
        g_pUIPopup = m_savedPopup;
        if (m_preparedPopup)
            CUIMng::Instance().m_MsgWin.CWin::Show(m_savedMessageVisible);
    }

    UiLifecycleFixture(const UiLifecycleFixture&) = delete;
    UiLifecycleFixture& operator=(const UiLifecycleFixture&) = delete;

    void PreparePopup()
    {
        g_pUIPopup = &popup;
        // The static native CMsgWin has not run Create. Initialize only base
        // visibility; derived Show reads widgets that have not been created.
        m_preparedPopup = true;
        CUIMng::Instance().m_MsgWin.CWin::Show(false);
    }

    bool PrepareModalPrerequisites()
    {
        // Real empty owners for predicates preceding Siege/focus in modal policy.
        // No messagebox/scene asset startup and no Friends main-window request.
        m_modalPrerequisites = true;
        CNewUISystem::GetInstance()->m_pNewCryWolfInterface = &cryWolf;
        registry.AddUIObj(INTERFACE_CRYWOLF, &cryWolf);
        registry.AddUIObj(INTERFACE_MESSAGEBOX, g_MessageBox);
        return friends.Create(&registry);
    }

    void PopulateSiege()
    {
        // The manager owns this actual child; ordinary InitMiniMapUI/Release
        // retires it. Deliberately bypass asset loading, not the observer.
        siege.m_pSiegeWarUI = new CNewUISiegeWarObserver;
    }

    CUITextInputBox& Input(bool whisper)
    {
        return *(whisper ? chat.m_pWhsprIDInputBox : chat.m_pChatInputBox);
    }

    CNewUIManager registry;
    CNewUISiegeWarfare siege;
    CNewUIChatInputBox chat;
    CNewUIQuickCommandWindow quick;
    CNewUIHotKey hotkey;
    CNewUICryWolf cryWolf;
    CNewUIFriendWindow friends;
    CUIPopup popup;

private:
    bool m_modalPrerequisites = false;
    EGameScene m_scene;
    int m_loading;
    CUIPopup* m_savedPopup;
    bool m_savedMessageVisible;
    bool m_preparedPopup = false;
};
} // namespace SEASON3B
